/*
 * Liquid Glass Plugin for Hyprland
 * 
 * Apple-style liquid glass effect featuring:
 * - Edge refraction with displacement mapping
 * - Chromatic aberration (RGB channel separation)
 * - Fresnel effect (edge glow)
 * - Specular highlights
 * - Subtle interior blur
 */

#include "LiquidGlassDecoration.hpp"
#include "globals.hpp"
#include "shaders.hpp"

#include <GLES3/gl32.h>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/Shader.hpp>
#include <hyprland/src/helpers/Color.hpp>
#include <chrono>

// ============================================================================
// SHADER MANAGEMENT
// ============================================================================

static std::string loadShader(const char* fileName) {
    if (SHADERS.contains(fileName)) {
        return SHADERS.at(fileName);
    }

    const std::string message = std::format("[{}] Failed to load shader: {}", PLUGIN_NAME, fileName);
    HyprlandAPI::addNotification(PHANDLE, message, CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
    throw std::runtime_error(message);
}

static void initShader() {
    const char* shaderFile = "liquidglass.frag";
    
    GLuint prog = g_pHyprOpenGL->createProgram(
        g_pHyprOpenGL->m_shaders->TEXVERTSRC, 
        loadShader(shaderFile), 
        true
    );

    if (prog == 0) {
        const std::string message = std::format("[{}] Failed to compile shader: {}", PLUGIN_NAME, shaderFile);
        HyprlandAPI::addNotification(PHANDLE, message, CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error(message);
    }

    g_pGlobalState->shader.program = prog;
    
    // Get standard uniform locations
    g_pGlobalState->shader.uniformLocations[SHADER_PROJ]       = glGetUniformLocation(prog, "proj");
    g_pGlobalState->shader.uniformLocations[SHADER_POS_ATTRIB] = glGetAttribLocation(prog, "pos");
    g_pGlobalState->shader.uniformLocations[SHADER_TEX_ATTRIB] = glGetAttribLocation(prog, "texcoord");
    g_pGlobalState->shader.uniformLocations[SHADER_TEX]        = glGetUniformLocation(prog, "tex");
    g_pGlobalState->shader.uniformLocations[SHADER_TOP_LEFT]   = glGetUniformLocation(prog, "topLeft");
    g_pGlobalState->shader.uniformLocations[SHADER_FULL_SIZE]  = glGetUniformLocation(prog, "fullSize");
    g_pGlobalState->shader.uniformLocations[SHADER_RADIUS]     = glGetUniformLocation(prog, "radius");

    // Get liquid glass specific uniform locations
    g_pGlobalState->locTime                  = glGetUniformLocation(prog, "time");
    g_pGlobalState->locBlurStrength          = glGetUniformLocation(prog, "blurStrength");
    g_pGlobalState->locRefractionStrength    = glGetUniformLocation(prog, "refractionStrength");
    g_pGlobalState->locChromaticAberration   = glGetUniformLocation(prog, "chromaticAberration");
    g_pGlobalState->locFresnelStrength       = glGetUniformLocation(prog, "fresnelStrength");
    g_pGlobalState->locSpecularStrength      = glGetUniformLocation(prog, "specularStrength");
    g_pGlobalState->locGlassOpacity          = glGetUniformLocation(prog, "glassOpacity");
    g_pGlobalState->locEdgeThickness         = glGetUniformLocation(prog, "edgeThickness");
    g_pGlobalState->locRedTint               = glGetUniformLocation(prog, "redTint");
    g_pGlobalState->locGreenTint             = glGetUniformLocation(prog, "greenTint");
    g_pGlobalState->locBlueTint              = glGetUniformLocation(prog, "blueTint");
    g_pGlobalState->locFullSizeUntransformed = glGetUniformLocation(prog, "fullSizeUntransformed");

    // Create VAO
    g_pGlobalState->shader.createVao();

    // Store start time for animation
    auto now = std::chrono::steady_clock::now();
    g_pGlobalState->startTime = std::chrono::duration<float>(now.time_since_epoch()).count();

    HyprlandAPI::addNotification(PHANDLE, 
        std::format("[{}] Shader initialized successfully", PLUGIN_NAME),
        CHyprColor{0.2, 0.8, 0.2, 1.0}, 3000);
}

// ============================================================================
// WINDOW CALLBACKS
// ============================================================================

static void onNewWindow(void* self, std::any data) {
    const auto PWINDOW = std::any_cast<PHLWINDOW>(data);

    // Check if decoration already exists
    if (std::ranges::any_of(PWINDOW->m_windowDecorations,
                            [](const auto& d) { return d->getDisplayName() == "LiquidGlass"; }))
        return;

    // Create and attach decoration
    auto deco = makeUnique<CLiquidGlassDecoration>(PWINDOW);
    g_pGlobalState->decorations.emplace_back(deco);
    deco->m_self = deco;
    HyprlandAPI::addWindowDecoration(PHANDLE, PWINDOW, std::move(deco));
}

static void onCloseWindow(void* self, std::any data) {
    const auto PWINDOW = std::any_cast<PHLWINDOW>(data);

    // Remove decoration from our tracking list
    std::erase_if(g_pGlobalState->decorations, [PWINDOW](const auto& deco) {
        auto locked = deco.lock();
        return !locked || locked->getOwner() == PWINDOW;
    });
}

// ============================================================================
// PLUGIN API
// ============================================================================

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    // Version check
    const std::string HASH        = __hyprland_api_get_hash();
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (HASH != CLIENT_HASH) {
        HyprlandAPI::addNotification(
            PHANDLE,
            std::format("[{}] Version mismatch! Plugin headers don't match running Hyprland.", PLUGIN_NAME),
            CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("Version mismatch");
    }

    // Initialize global state
    g_pGlobalState = std::make_unique<SGlobalState>();

    // Initialize shader
    initShader();

    // Register callbacks
    static auto P1 = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "openWindow",
        [&](void* self, SCallbackInfo& info, std::any data) { onNewWindow(self, data); });

    static auto P2 = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "closeWindow",
        [&](void* self, SCallbackInfo& info, std::any data) { onCloseWindow(self, data); });

    // Register configuration values with Apple-tuned defaults
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:liquid-glass:enabled", Hyprlang::INT{1});
    
    // Blur: subtle interior blur for glass thickness feel
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:liquid-glass:blur_strength", Hyprlang::FLOAT{1.5});
    
    // Refraction: edge distortion intensity
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:liquid-glass:refraction_strength", Hyprlang::FLOAT{0.08});
    
    // Chromatic aberration: RGB separation at edges
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:liquid-glass:chromatic_aberration", Hyprlang::FLOAT{0.012});
    
    // Fresnel: edge glow intensity
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:liquid-glass:fresnel_strength", Hyprlang::FLOAT{0.4});
    
    // Specular: sharp highlight intensity
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:liquid-glass:specular_strength", Hyprlang::FLOAT{0.3});
    
    // Glass opacity
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:liquid-glass:glass_opacity", Hyprlang::FLOAT{1.0});
    
    // Edge thickness: how wide the refractive edge is
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:liquid-glass:edge_thickness", Hyprlang::FLOAT{0.15});
    
    // Red tint: apple blue tint default
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:liquid-glass:red_tint", Hyprlang::FLOAT{0.95});
    
    // Green tint: apple blue tint default
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:liquid-glass:green_tint", Hyprlang::FLOAT{0.97});
    
    // Red tint: apple blue tint default
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:liquid-glass:blue_tint", Hyprlang::FLOAT{1.0});

    // Apply to existing windows
    for (auto& w : g_pCompositor->m_windows) {
        if (w->isHidden() || !w->m_isMapped)
            continue;

        onNewWindow(nullptr, std::any(w));
    }

    HyprlandAPI::reloadConfig();

    HyprlandAPI::addNotification(PHANDLE,
        std::format("[{}] Loaded successfully! Enjoy your liquid glass.", PLUGIN_NAME),
        CHyprColor{0.2, 0.8, 0.4, 1.0}, 4000);

    return {PLUGIN_NAME, PLUGIN_DESCRIPTION, PLUGIN_AUTHOR, PLUGIN_VERSION};
}

APICALL EXPORT void PLUGIN_EXIT() {
    // Clean up decorations
    for (auto& deco : g_pGlobalState->decorations) {
        auto locked = deco.lock();
        if (locked) {
            auto owner = locked->getOwner();
            if (owner)
                owner->removeWindowDeco(locked.get());
        }
    }

    // Remove all our pass elements
    g_pHyprRenderer->m_renderPass.removeAllOfType("CLiquidGlassPassElement");
    
    // Destroy shader
    g_pGlobalState->shader.destroy();
    
    // Reset global state
    g_pGlobalState.reset();
}
