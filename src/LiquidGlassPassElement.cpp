#include "LiquidGlassPassElement.hpp"
#include "LiquidGlassDecoration.hpp"
#include "globals.hpp"

#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/render/OpenGL.hpp>

CLiquidGlassPassElement::CLiquidGlassPassElement(const SLiquidGlassData& data) 
    : m_data(data) {}

void CLiquidGlassPassElement::draw(const CRegion& damage) {
    if (!m_data.deco)
        return;
    
    m_data.deco->renderPass(g_pHyprOpenGL->m_renderData.pMonitor.lock(), m_data.a);
}

std::optional<CBox> CLiquidGlassPassElement::boundingBox() {
    if (!m_data.deco)
        return std::nullopt;

    const auto PWINDOW = m_data.deco->getOwner();
    if (!PWINDOW)
        return std::nullopt;

    const auto PWINDOWWORKSPACE = PWINDOW->m_workspace;
    auto surfaceBox = PWINDOW->getWindowMainSurfaceBox();

    if (PWINDOWWORKSPACE && PWINDOWWORKSPACE->m_renderOffset->isBeingAnimated() && !PWINDOW->m_pinned)
        surfaceBox.translate(PWINDOWWORKSPACE->m_renderOffset->value());
    surfaceBox.translate(PWINDOW->m_floatingOffset);

    return surfaceBox;
}

bool CLiquidGlassPassElement::needsLiveBlur() {
    // We handle our own blur in the shader
    return false;
}

bool CLiquidGlassPassElement::needsPrecomputeBlur() {
    return false;
}
