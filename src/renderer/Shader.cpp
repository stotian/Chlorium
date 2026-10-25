#include "Shader.hpp"
#include "ShaderPreprocessor.hpp"
#include "ContextEpoch.hpp"
#include "Geode/cocos/platform/win32/CCGL.h"
#include "common.hpp"
#include "glslang/MachineIndependent/Versions.h"

#include <Geode/Geode.hpp>
#include <vector>

#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>
#include <optional>
#include <memory>
#include <cstdint>
#include <new>

using namespace geode;
static std::map<std::string, std::shared_ptr<const u32>> programCache;
static std::map<std::string, std::string> sourceCache;
static std::shared_ptr<const u32> ownProgram(u32 id, std::shared_ptr<chlorium::research::gl::ContextEpoch<>> context) {
    const auto handle = new (std::nothrow) u32(id);
    if (!handle) { glDeleteProgram(id); throw std::bad_alloc(); }
    // shared_ptr invokes this deleter even if its control-block allocation
    // fails. A new GL context may reuse an old program's numeric name.
    return {handle, [context](const u32* value) {
        if (context && context->isCurrent()) glDeleteProgram(*value);
        delete value;
    }};
}
void Shader::clearCache() { programCache.clear(); sourceCache.clear(); }


Shader::~Shader() {
    // All linked programs have a shared owner with a context generation.
    // Never delete an unowned numeric name in whichever context happens to run.
}

void printErrorLog(std::string str) {
    char* log = str.data();
    char* start = log;

    while (*log) {
        if (*log == '\n') {
            *log = 0;
            log::error("    {}", start);
            start = log + 1;
        }
        log++;
    }

    if (*start != 0)
        log::error("    {}", start);
}

static std::optional<std::string> preprocessShader(const fs::path& path, std::string preCode, EShLanguage stage) {
    const auto cacheKey = Mod::get()->getResourcesDir().string() + "/" + path.string() + "|" +
        std::to_string(static_cast<int>(stage)) + "|" + preCode;
    if (auto it = sourceCache.find(cacheKey); it != sourceCache.end()) return it->second;
    auto source = readResourceFile(path);
    if (!source) {
        log::error("Missing shader resource: {}", path);
        return std::nullopt;
    }
    std::string error;
    auto code = chlorium::preprocessShaderSource(stage, preCode + *source, path.string(),
        [](std::string_view name) { return readResourceFile(fs::path(name)); }, error);
    if (!code) log::error("Shader preprocessing failed ({}): {}", path, error);
    else {
        if (sourceCache.size() >= 16) sourceCache.clear();
        sourceCache.emplace(cacheKey, *code);
    }
    return code;
}

#define GL_COMPUTE_SHADER 0x91B9

static u32 createShader(i32 type, const char* source) {
    u32 shader = glCreateShader(type);
    if (!shader) {
        geode::log::error("Failed to allocate shader object");
        return 0;
    }
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    i32 success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, NULL, log);
        const char* shaderType = "unknown";
        switch (type) {
        case GL_VERTEX_SHADER: shaderType = "vertex"; break;
        case GL_FRAGMENT_SHADER: shaderType = "fragment"; break;
        case GL_COMPUTE_SHADER: shaderType = "compute"; break;
        }
        geode::log::error("failed to compile {} shader:", shaderType);
        printErrorLog(log);
        glDeleteShader(shader);
        return 0;
    };

    return shader;
}

Shader* Shader::create(const ShaderSources& sources) {
    auto context = chlorium::research::gl::acquireContextEpoch();
    if (!context) return nullptr;
    const auto key = std::to_string(context->epoch()) + ":draw:" + sources.vertexSource + '\0' + sources.fragmentSource;
    if (auto it = programCache.find(key); it != programCache.end()) {
        auto* shader = new Shader(); shader->programOwner = it->second;
        shader->program = *it->second; return shader;
    }
    u32 vertexShader   = createShader(GL_VERTEX_SHADER, sources.vertexSource.c_str());
    u32 fragmentShader = createShader(GL_FRAGMENT_SHADER, sources.fragmentSource.c_str());

    if (!vertexShader || !fragmentShader) {
        if (vertexShader)   glDeleteShader(vertexShader);
        if (fragmentShader) glDeleteShader(fragmentShader);
        return nullptr;
    }

    u32 program = glCreateProgram();
    if (!program) {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        geode::log::error("Failed to allocate shader program");
        return nullptr;
    }
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, 512, NULL, log);
        geode::log::error("Failed to link shader program:");
        printErrorLog(log);
        glDeleteProgram(program);
        return nullptr;
    }

    auto owner = ownProgram(program, std::move(context));
    std::unique_ptr<Shader, decltype(&Shader::destroy)> shader(new Shader(), &Shader::destroy);
    shader->programOwner = std::move(owner);
    shader->program = program;
    if (programCache.size() >= 8) programCache.clear();
    programCache.emplace(key, shader->programOwner);
    return shader.release();
}

static u32 createShaderProgram(const std::vector<u32>& shaders) {
    u32 program = glCreateProgram();
    if (!program) {
        geode::log::error("Failed to allocate compute shader program");
        return 0;
    }
    for (auto shader : shaders)
        glAttachShader(program, shader);
    glLinkProgram(program);

    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, 512, NULL, log);
        geode::log::error("Failed to link shader program:");
        printErrorLog(log);
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

static inline std::string generatePreCode(std::map<std::string, std::string> macroVariables) {
    std::string preCode = "#define GLSL\n";
    for (auto [k, v] : macroVariables)
        preCode += "#define " + k + " " + v + "\n";
    return preCode;
}

Shader* Shader::create(
    const fs::path& vertexPath,
    const fs::path& fragmentPath,
    std::map<std::string, std::string> macroVariables
) {
    auto preCode = generatePreCode(macroVariables);

    auto vertexSource   = preprocessShader(vertexPath, preCode, EShLangVertex);
    auto fragmentSource = preprocessShader(fragmentPath, preCode, EShLangFragment);

    if (!vertexSource || !fragmentSource)
        return nullptr;

    return create(ShaderSources{*vertexSource, *fragmentSource});
}

Shader* Shader::createCompute(const fs::path& path, std::map<std::string, std::string> macroVariables) {
    auto context = chlorium::research::gl::acquireContextEpoch();
    if (!context) return nullptr;
    auto preCode = generatePreCode(macroVariables);

    auto source = preprocessShader(path, preCode, EShLangCompute);
    if (!source)
        return nullptr;

    const auto key = std::to_string(context->epoch()) + ":compute:" + *source;
    if (auto it = programCache.find(key); it != programCache.end()) {
        auto* shader = new Shader(); shader->programOwner = it->second;
        shader->program = *it->second; return shader;
    }
    u32 shader = createShader(GL_COMPUTE_SHADER, source.value().c_str());
    if (!shader)
        return nullptr;

    u32 program = createShaderProgram({shader});

    glDeleteShader(shader);

    if (program == 0)
        return nullptr;

    auto owner = ownProgram(program, std::move(context));
    std::unique_ptr<Shader, decltype(&Shader::destroy)> ret(new Shader(), &Shader::destroy);
    ret->programOwner = std::move(owner);
    ret->program = program;
    if (programCache.size() >= 8) programCache.clear();
    programCache.emplace(key, ret->programOwner);
    return ret.release();
}

void Shader::setMatrix4(i32 location, const float* data) {
    use();
    glUniformMatrix4fv(location, 1, GL_FALSE, data);
}

void Shader::setInt(i32 location, i32 value) {
    use();
    glUniform1i(location, value);
}

void Shader::setUInt(i32 location, u32 value) {
    use();
    glUniform1ui(location, value);
}

void Shader::setFloat(i32 location, float value) {
    use();
    glUniform1f(location, value);
}

void Shader::setVec2(i32 location, glm::vec2 value) {
    use();
    glUniform2f(location, value.x, value.y);
}

void Shader::setVec3(i32 location, glm::vec3 value) {
    use();
    glUniform3f(location, value.x, value.y, value.z);
}

void Shader::setVec4(i32 location, glm::vec4 value) {
    use();
    glUniform4f(location, value.x, value.y, value.z, value.w);
}

void Shader::setTexture(i32 location, i32 id, u32 texture) {
    use();
    glActiveTexture(GL_TEXTURE0 + id);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(location, id);
}

void Shader::setTextureArray(i32 location, i32 count, u32* textures) {
    use();
    if (count <= 0 || !textures) return;
    std::vector<i32> units(count);
    for (i32 i = 0; i < count; ++i) units[i] = i;
    glUniform1iv(location, count, units.data());

    for (i32 i = 0; i < count; i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, textures[i]);
    }
}

void Shader::setTextureArray(i32 location, i32 count, cocos2d::CCTexture2D** textures) {
    if (count <= 0 || !textures) return;
    use();
    std::vector<i32> integers;
    integers.resize(count);

    for (i32 i = 0; i < count; i++)
        integers[i] = i;

    glUniform1iv(location, count, integers.data());

    for (i32 i = 0; i < count; i++) {
        if (textures[i] == nullptr)
            continue;

        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, textures[i]->getName());
    }
}

