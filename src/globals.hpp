#pragma once

/*
 * Liquid Glass Plugin for Hyprland
 * Apple-style liquid glass effect with refraction, chromatic aberration, and Fresnel highlights
 */

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/render/Shader.hpp>
#include <memory>
#include <vector>

class CLiquidGlassDecoration;

// Custom shader uniform locations (extending Hyprland's built-in ones)
enum eLiquidGlassUniforms {
    LG_UNIFORM_TIME = 100,
    LG_UNIFORM_BLUR_STRENGTH,
    LG_UNIFORM_REFRACTION_STRENGTH,
    LG_UNIFORM_CHROMATIC_ABERRATION,
    LG_UNIFORM_FRESNEL_STRENGTH,
    LG_UNIFORM_SPECULAR_STRENGTH,
    LG_UNIFORM_GLASS_OPACITY,
    LG_UNIFORM_EDGE_THICKNESS,
    LG_UNIFORM_RED_TINT,
    LG_UNIFORM_GREEN_TINT,
    LG_UNIFORM_BLUE_TINT,
    LG_UNIFORM_FULL_SIZE_UNTRANSFORMED,
};

struct SGlobalState {
    std::vector<WP<CLiquidGlassDecoration>> decorations;
    SShader                                  shader;
    float                                    startTime = 0.0f;
    
    // Shader uniform locations
    GLint locTime                  = -1;
    GLint locBlurStrength          = -1;
    GLint locRefractionStrength    = -1;
    GLint locChromaticAberration   = -1;
    GLint locFresnelStrength       = -1;
    GLint locSpecularStrength      = -1;
    GLint locGlassOpacity          = -1;
    GLint locEdgeThickness         = -1;
    GLint locRedTint               = -1;
    GLint locGreenTint             = -1;
    GLint locBlueTint              = -1;
    GLint locFullSizeUntransformed = -1;
};

inline HANDLE                        PHANDLE = nullptr;
inline std::unique_ptr<SGlobalState> g_pGlobalState;

// Plugin info
inline const char* PLUGIN_NAME        = "liquid-glass";
inline const char* PLUGIN_DESCRIPTION = "Apple-style Liquid Glass effect for Hyprland";
inline const char* PLUGIN_AUTHOR      = "xiaoxigua-1";
inline const char* PLUGIN_VERSION     = "1.0.0";
