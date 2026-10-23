#pragma once

#include <common.hpp>

#include <string>
#include <map>
#include <memory>

struct ShaderSources {
    std::string vertexSource;
    std::string fragmentSource;
};

class Shader {
private:
    ~Shader();

public:
    static void clearCache();
    static Shader* create(const ShaderSources& sources);

    static Shader* create(
        const fs::path& vertexPath,
        const fs::path& fragmentPath,
        std::map<std::string, std::string> macroVariables = {}
    );

    static Shader* createCompute(const fs::path& path, std::map<std::string, std::string> macroVariables = {});

    static inline void destroy(Shader* shader) {
        delete shader;
    }

    inline void use() {
        glUseProgram(program);
    }

    inline i32 location(const char* name) {
        if (auto it = uniformLocations.find(name); it != uniformLocations.end()) return it->second;
        const auto result = glGetUniformLocation(program, name);
        uniformLocations.emplace(name, result);
        return result;
    }

    void setMatrix4(i32 location, const float* data);
    inline void setMatrix4(const char* name, const float* data) {
        setMatrix4(location(name), data);
    }

    void setInt(i32 location, i32 value);
    inline void setInt(const char* name, i32 value) {
        setInt(location(name), value);
    }

    void setUInt(i32 location, u32 value);
    inline void setUInt(const char* name, u32 value) {
        setUInt(location(name), value);
    }

    void setFloat(i32 location, float value);
    inline void setFloat(const char* name, float value) {
        setFloat(location(name), value);
    }

    void setVec2(i32 location, glm::vec2 value);
    inline void setVec2(const char* name, glm::vec2 value) {
        setVec2(location(name), value);
    }

    void setVec3(i32 location, glm::vec3 value);
    inline void setVec3(const char* name, glm::vec3 value) {
        setVec3(location(name), value);
    }

    void setVec4(i32 location, glm::vec4 value);
    inline void setVec4(const char* name, glm::vec4 value) {
        setVec4(location(name), value);
    }

    void setTexture(i32 location, i32 id, u32 texture);
    inline void setTexture(i32 location, i32 id, cocos2d::CCTexture2D* texture) {
        setTexture(location, id, texture->getName());
    }
    inline void setTexture(const char* name, i32 id, cocos2d::CCTexture2D* texture) {
        setTexture(location(name), id, texture);
    }

    void setTextureArray(i32 location, i32 count, u32* textures);
    void setTextureArray(i32 location, i32 count, cocos2d::CCTexture2D** textures);
    inline void setTextureArray(const char* name, i32 count, cocos2d::CCTexture2D** textures) {
        setTextureArray(location(name), count, textures);
    }

private:
    u32 program = 0;
    std::shared_ptr<const u32> programOwner;
    // Programs are immutable after linking, including a uniform's -1 result.
    // Avoid a driver name lookup for the same sampler on every layer draw.
    std::map<std::string, i32, std::less<>> uniformLocations;
};
