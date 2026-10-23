#include "ShaderPreprocessor.hpp"
#include <glslang/Public/ResourceLimits.h>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>

namespace chlorium {
class Includer final : public glslang::TShader::Includer {
    struct Payload {
        std::string source;
        IncludeResult result;
        Payload(const std::string& name, std::string text)
            : source(std::move(text)), result(name, source.data(), source.size(), nullptr) {}
    };
    const ShaderLoader& loader;
    std::map<IncludeResult*, std::unique_ptr<Payload>> active;
public:
    explicit Includer(const ShaderLoader& loader) : loader(loader) {}
    IncludeResult* includeLocal(const char* header, const char*, size_t depth) override {
        // Shipped headers are basenames. Do not allow absolute paths, parent
        // traversal or arbitrarily deep includes in a mod resource.
        if (depth > 32 || std::string_view(header).find_first_of("/\\:") != std::string_view::npos)
            return nullptr;
        auto text = loader(header);
        if (!text) return nullptr;
        auto payload = std::make_unique<Payload>(header, std::move(*text));
        auto* result = &payload->result;
        active.emplace(result, std::move(payload));
        return result;
    }
    void releaseInclude(IncludeResult* result) override { active.erase(result); }
};

std::optional<std::string> preprocessShaderSource(EShLanguage stage, const std::string& source,
    const std::string& name, const ShaderLoader& loader, std::string& error) {
    static std::once_flag initialized;
    static bool initializationOK = false;
    std::call_once(initialized, [] { initializationOK = glslang::InitializeProcess(); });
    if (!initializationOK) { error = "glslang initialization failed"; return std::nullopt; }
    glslang::TShader shader(stage);
    const char* text = source.c_str();
    const char* filename = name.c_str();
    const int length = static_cast<int>(source.size());
    shader.setStringsWithLengthsAndNames(&text, &length, &filename, 1);
    Includer includer(loader);
    std::string expanded;
    if (!shader.preprocess(GetDefaultResources(), 460, ECoreProfile, false, false,
                           EShMsgDefault, &expanded, includer)) {
        error = shader.getInfoLog();
        return std::nullopt;
    }
    // Includes are already expanded. Avoid driver-specific include extensions
    // and duplicate #version directives; handle directives on the first line.
    std::istringstream input(expanded);
    std::string output = "#version 460 core\n", line;
    while (std::getline(input, line)) {
        auto start = line.find_first_not_of(" \t\r");
        auto trimmed = start == std::string::npos ? std::string_view{} : std::string_view(line).substr(start);
        if (trimmed.starts_with("#version") || trimmed.starts_with("#line") ||
            trimmed.starts_with("#extension GL_ARB_shading_language_include")) continue;
        output += line + '\n';
    }
    return output;
}
}
