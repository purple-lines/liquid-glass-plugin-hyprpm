#include "LiquidGlassDecoration.hpp"
#include "LiquidGlassPassElement.hpp"
#include "globals.hpp"

#include <GLES3/gl32.h>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprutils/math/Misc.hpp>
#include <hyprutils/math/Region.hpp>
#include <hyprutils/math/Vector2D.hpp>
#include <chrono>

// ============================================================================
// CONSTRUCTOR
// ============================================================================

CLiquidGlassDecoration::CLiquidGlassDecoration(PHLWINDOW pWindow)
    : IHyprWindowDecoration(pWindow), m_pWindow(pWindow) {
    // Disable Hyprland's built-in blur - we handle it ourselves
    // pWindow->m_windowData.noBlur = true;
}

// ============================================================================
// DECORATION INTERFACE IMPLEMENTATION
// ============================================================================

eDecorationLayer CLiquidGlassDecoration::getDecorationLayer() {
    return DECORATION_LAYER_BOTTOM;
}

uint64_t CLiquidGlassDecoration::getDecorationFlags() {
    return DECORATION_NON_SOLID;
}

eDecorationType CLiquidGlassDecoration::getDecorationType() {
    return eDecorationType::DECORATION_CUSTOM;
}

std::string CLiquidGlassDecoration::getDisplayName() {
    return "LiquidGlass";
}

SDecorationPositioningInfo CLiquidGlassDecoration::getPositioningInfo() {
    SDecorationPositioningInfo info;
    info.priority       = 10000;
    info.policy         = DECORATION_POSITION_ABSOLUTE;
    info.desiredExtents = {{0, 0}, {0, 0}};
    return info;
}

void CLiquidGlassDecoration::onPositioningReply(const SDecorationPositioningReply& reply) {
    // No action needed
}

PHLWINDOW CLiquidGlassDecoration::getOwner() {
    return m_pWindow.lock();
}

// ============================================================================
// DRAWING
// ============================================================================

void CLiquidGlassDecoration::draw(PHLMONITOR pMonitor, float const& a) {
    // Check if effect is enabled
    static auto* const PENABLED = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquid-glass:enabled")->getDataStaticPtr();
    if (!**PENABLED)
        return;

    // Add our pass element to the render pass
    CLiquidGlassPassElement::SLiquidGlassData data{this, a};
    g_pHyprRenderer->m_renderPass.add(makeUnique<CLiquidGlassPassElement>(data));
}

// ============================================================================
// BACKGROUND SAMPLING
// ============================================================================

void CLiquidGlassDecoration::sampleBackground(CFramebuffer& sourceFB, CBox box) {
    // Allocate framebuffer if size changed
    if (m_sampleFB.m_size.x != box.width || m_sampleFB.m_size.y != box.height) {
        m_sampleFB.alloc(box.width, box.height, sourceFB.m_drmFormat);
    }

    int x0 = static_cast<int>(box.x);
    int x1 = static_cast<int>(box.x + box.width);
    int y0 = static_cast<int>(box.y);
    int y1 = static_cast<int>(box.y + box.height);

    // Blit the background region to our sample framebuffer
    glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFB.getFBID());
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_sampleFB.getFBID());
    glBlitFramebuffer(x0, y0, x1, y1, 0, 0, 
                      static_cast<int>(box.width), static_cast<int>(box.height),
                      GL_COLOR_BUFFER_BIT, GL_LINEAR);
}

// ============================================================================
// LIQUID GLASS SHADER APPLICATION
// ============================================================================

void CLiquidGlassDecoration::applyLiquidGlassEffect(CFramebuffer& sourceFB, CFramebuffer& targetFB,
                                                      CBox& rawBox, CBox& transformedBox, float windowAlpha) {
    // Get config values
    static auto* const PBLUR       = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquid-glass:blur_strength")->getDataStaticPtr();
    static auto* const PREFRACT    = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquid-glass:refraction_strength")->getDataStaticPtr();
    static auto* const PCHROMATIC  = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquid-glass:chromatic_aberration")->getDataStaticPtr();
    static auto* const PFRESNEL    = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquid-glass:fresnel_strength")->getDataStaticPtr();
    static auto* const PSPECULAR   = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquid-glass:specular_strength")->getDataStaticPtr();
    static auto* const POPACITY    = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquid-glass:glass_opacity")->getDataStaticPtr();
    static auto* const PEDGE       = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquid-glass:edge_thickness")->getDataStaticPtr();
    static auto* const REDTINT     = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquid-glass:red_tint")->getDataStaticPtr();
    static auto* const GREENTINT   = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquid-glass:green_tint")->getDataStaticPtr();
    static auto* const BLUETINT    = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquid-glass:blue_tint")->getDataStaticPtr();

    // Calculate transformation matrix
    const auto TR = Math::wlTransformToHyprutils(
        Math::invertTransform(g_pHyprOpenGL->m_renderData.pMonitor->m_transform));

    Mat3x3 matrix = g_pHyprOpenGL->m_renderData.monitorProjection.projectBox(rawBox, TR, rawBox.rot);
    Mat3x3 glMatrix = g_pHyprOpenGL->m_renderData.projection.copy().multiply(matrix);
    auto tex = sourceFB.getTexture();

    glMatrix.transpose();
    
    // Bind target framebuffer and source texture
    glBindFramebuffer(GL_FRAMEBUFFER, targetFB.getFBID());
    glActiveTexture(GL_TEXTURE0);
    tex->bind();
    
    // Use our liquid glass shader
    g_pHyprOpenGL->useProgram(g_pGlobalState->shader.program);

    // Set standard uniforms
    g_pGlobalState->shader.setUniformMatrix3fv(SHADER_PROJ, 1, GL_FALSE, glMatrix.getMatrix());
    g_pGlobalState->shader.setUniformInt(SHADER_TEX, 0);

    // Set position and size uniforms
    const auto TOPLEFT  = Vector2D(transformedBox.x, transformedBox.y);
    const auto FULLSIZE = Vector2D(transformedBox.width, transformedBox.height);

    g_pGlobalState->shader.setUniformFloat2(SHADER_TOP_LEFT, 
        static_cast<float>(TOPLEFT.x), static_cast<float>(TOPLEFT.y));
    g_pGlobalState->shader.setUniformFloat2(SHADER_FULL_SIZE, 
        static_cast<float>(FULLSIZE.x), static_cast<float>(FULLSIZE.y));

    // Set liquid glass specific uniforms
    auto now = std::chrono::steady_clock::now();
    float time = std::chrono::duration<float>(now.time_since_epoch()).count() - g_pGlobalState->startTime;
    
    glUniform1f(g_pGlobalState->locTime, time);
    glUniform1f(g_pGlobalState->locBlurStrength, static_cast<float>(**PBLUR));
    glUniform1f(g_pGlobalState->locRefractionStrength, static_cast<float>(**PREFRACT));
    glUniform1f(g_pGlobalState->locChromaticAberration, static_cast<float>(**PCHROMATIC));
    glUniform1f(g_pGlobalState->locFresnelStrength, static_cast<float>(**PFRESNEL));
    glUniform1f(g_pGlobalState->locSpecularStrength, static_cast<float>(**PSPECULAR));
    glUniform1f(g_pGlobalState->locGlassOpacity, static_cast<float>(**POPACITY) * windowAlpha);
    glUniform1f(g_pGlobalState->locEdgeThickness, static_cast<float>(**PEDGE));
    glUniform1f(g_pGlobalState->locRedTint, static_cast<float>(**REDTINT));
    glUniform1f(g_pGlobalState->locGreenTint, static_cast<float>(**GREENTINT));
    glUniform1f(g_pGlobalState->locBlueTint, static_cast<float>(**BLUETINT));
    
    // Untransformed size for proper calculations
    glUniform2f(g_pGlobalState->locFullSizeUntransformed, 
        static_cast<float>(rawBox.width), static_cast<float>(rawBox.height));

    // Set window corner radius
    const auto PWINDOW = m_pWindow.lock();
    float cornerRadius = PWINDOW ? PWINDOW->rounding() : 0.0f;
    g_pGlobalState->shader.setUniformFloat(SHADER_RADIUS, cornerRadius);

    // Draw
    glBindVertexArray(g_pGlobalState->shader.uniformLocations[SHADER_SHADER_VAO]);
    g_pHyprOpenGL->scissor(rawBox);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    g_pHyprOpenGL->scissor(nullptr);
}

// ============================================================================
// RENDER PASS
// ============================================================================

void CLiquidGlassDecoration::renderPass(PHLMONITOR pMonitor, const float& a) {
    const auto PWINDOW = m_pWindow.lock();
    if (!PWINDOW)
        return;

    const auto PWORKSPACE = PWINDOW->m_workspace;
    const auto WORKSPACEOFFSET = PWORKSPACE && !PWINDOW->m_pinned 
        ? PWORKSPACE->m_renderOffset->value() 
        : Vector2D();
    
    const auto SOURCE = g_pHyprOpenGL->m_renderData.currentFB;

    // Calculate window box
    auto thisbox = PWINDOW->getWindowMainSurfaceBox();

    CBox wlrbox = thisbox.translate(WORKSPACEOFFSET)
                      .translate(-pMonitor->m_position + PWINDOW->m_floatingOffset)
                      .scale(pMonitor->m_scale)
                      .round();
    CBox transformBox = wlrbox;

    // Apply monitor transform
    const auto TR = Math::wlTransformToHyprutils(
        Math::invertTransform(g_pHyprOpenGL->m_renderData.pMonitor->m_transform));
    transformBox.transform(TR, 
        g_pHyprOpenGL->m_renderData.pMonitor->m_transformedSize.x,
        g_pHyprOpenGL->m_renderData.pMonitor->m_transformedSize.y);

    // Sample background and apply effect
    sampleBackground(*SOURCE, transformBox);
    applyLiquidGlassEffect(m_sampleFB, *SOURCE, wlrbox, transformBox, a);
}

// ============================================================================
// WINDOW UPDATES
// ============================================================================

void CLiquidGlassDecoration::updateWindow(PHLWINDOW pWindow) {
    damageEntire();
}

void CLiquidGlassDecoration::damageEntire() {
    const auto PWINDOW = m_pWindow.lock();
    if (!PWINDOW)
        return;

    const auto PWINDOWWORKSPACE = PWINDOW->m_workspace;
    auto surfaceBox = PWINDOW->getWindowMainSurfaceBox();

    if (PWINDOWWORKSPACE && PWINDOWWORKSPACE->m_renderOffset->isBeingAnimated() && !PWINDOW->m_pinned)
        surfaceBox.translate(PWINDOWWORKSPACE->m_renderOffset->value());
    surfaceBox.translate(PWINDOW->m_floatingOffset);

    g_pHyprRenderer->damageBox(surfaceBox);
}
