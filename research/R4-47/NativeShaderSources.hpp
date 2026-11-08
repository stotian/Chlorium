#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace chlorium::research::resident {
struct ShaderSources {std::string vertex,fragment;};
inline std::uint64_t sourceHash(std::string_view source) noexcept {
    std::uint64_t value=14695981039346656037ull;
    for(unsigned char byte:source)value=(value^byte)*1099511628211ull;
    return value;
}

// Exact reviewed text only. Attached source text is NOT proof of the currently
// linked native executable, nor proof of framebuffer parity after recompilation.
inline std::optional<ShaderSources> reviewedNativeSources(
    std::string_view residentVertex,std::string_view nativeVertex,
    std::string_view nativeFragment,std::string& error) {
    error.clear();
    if(residentVertex.size()!=2056 || sourceHash(residentVertex)!=0x59251a3c19c8d368ull){
        error="unreviewed resident vertex source";return std::nullopt;
    }
    if(nativeVertex.size()!=709 || sourceHash(nativeVertex)!=0xa2318967aee60148ull){
        error="unreviewed native vertex source";return std::nullopt;
    }
    if(nativeFragment.size()!=516 || sourceHash(nativeFragment)!=0xa9c8e0b64e142972ull){
        error="unreviewed native fragment source";return std::nullopt;
    }
    ShaderSources result{std::string(residentVertex),std::string(nativeFragment)};
    auto replace=[&](std::string& text,std::string_view before,std::string_view after){
        const auto at=text.find(before);
        if(at==std::string::npos || text.find(before,at+before.size())!=std::string::npos)return false;
        text.replace(at,before.size(),after);return true;
    };
    // Keep the resident position graph and the native varying names. The pinned
    // native vertex uses exactly CC_MVPMatrix * vec4(position,1), with no effect.
    if(!replace(result.vertex,"out vec4 renderColor;","out vec4 v_fragmentColor;") ||
       !replace(result.vertex,"out vec2 texCoord;","out vec2 v_texCoord;") ||
       !replace(result.vertex,"renderColor=unpackUnorm4x8(nativeColor);texCoord=uintBitsToFloat(nativeUV);",
                "v_fragmentColor=unpackUnorm4x8(nativeColor);v_texCoord=uintBitsToFloat(nativeUV);") ||
       !replace(result.fragment,"varying vec4 v_fragmentColor;","in vec4 v_fragmentColor;") ||
       !replace(result.fragment,"varying vec2 v_texCoord;","in vec2 v_texCoord;") ||
       !replace(result.fragment,"gl_FragColor = v_fragmentColor * texture2D(CC_Texture0, v_texCoord);",
                "chloriumFragment = v_fragmentColor * texture(CC_Texture0, v_texCoord);")){
        error="reviewed shader rewrite boundary changed";return std::nullopt;
    }
    result.fragment.insert(0,"#version 430 core\nlayout(location=0) out vec4 chloriumFragment;\n");
    return result;
}
}
