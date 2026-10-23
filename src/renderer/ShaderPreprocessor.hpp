#pragma once
#include <glslang/Public/ShaderLang.h>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace chlorium {
using ShaderLoader = std::function<std::optional<std::string>(std::string_view)>;
std::optional<std::string> preprocessShaderSource(EShLanguage stage, const std::string& source,
    const std::string& name, const ShaderLoader& loader, std::string& error);
}
