#include "ScenePathControl.h"
#include "DeferredCommon.h"
#include "RenderPass.h"
#include <KuEngine/Core/Log.h>
#include <imgui.h>
#include <stdexcept>
namespace ku {
const char* gbufferViewLabel(GBufferView view) noexcept {
    constexpr const char* labels[]{"Scene", "Base RGB", "Metallic A", "World Normal RGB", "Roughness A", "World Position RGB", "ShadingModel A", "Emissive RGB", "AO A"};
    const auto index = static_cast<uint32_t>(view);
    return index < 9 ? labels[index] : "Invalid";
}
SceneBranchMask sceneBranchMask(asset::RenderPath path, GBufferView view) {
    if ((path != asset::RenderPath::Forward && path != asset::RenderPath::Deferred) || static_cast<uint32_t>(view) > 8)
        throw std::invalid_argument("Invalid scene path/debug selection");
    if (path == asset::RenderPath::Forward && view != GBufferView::Scene)
        throw std::invalid_argument("GBuffer is unavailable in Forward");
    const bool deferred = path == asset::RenderPath::Deferred;
    return {!deferred, deferred, deferred, view == GBufferView::Scene, view != GBufferView::Scene};
}
void ScenePathControl::setDeferredCapability(bool available, std::string reason) { m_available = available; m_reason = std::move(reason); }
void ScenePathControl::bind(RenderPass& f, RenderPass* g, RenderPass* l, RenderPass& d, RenderPass* debug) {
    m_passes = {&f,g,l,&d,debug}; m_bound = true; apply();
}
void ScenePathControl::initialize(asset::RenderPath path) {
    if (!request(path)) throw std::runtime_error(m_status);
    publishPending();
}
bool ScenePathControl::request(asset::RenderPath path, GBufferView view) {
    try {
        (void)sceneBranchMask(path,view);
        if (path == asset::RenderPath::Deferred && !m_available) throw std::runtime_error("Deferred unavailable: " + m_reason);
        if (path == asset::RenderPath::Deferred && m_bound && (!m_passes[1] || !m_passes[2] || !m_passes[4])) throw std::runtime_error("Deferred branches are not completely bound");
    } catch (const std::exception& error) { m_status = error.what(); return false; }
    m_requestedPath = path; m_requestedView = view; m_pending = true; m_status = "Pending next fence-safe update";
    return true;
}
void ScenePathControl::apply() {
    const auto mask = sceneBranchMask(m_path,m_view);
    const std::array enabled{mask.forwardOpaque,mask.geometry,mask.lighting,mask.display,mask.debug};
    for (size_t i=0;i<m_passes.size();++i) if (m_passes[i]) m_passes[i]->setEnabled(enabled[i]);
}
bool ScenePathControl::publishPending() {
    if (!m_pending) return false;
    m_path=m_requestedPath; m_view=m_requestedView; m_pending=false; ++m_generation; apply();
    m_status = std::string(asset::toString(m_path)) + " / " + gbufferViewLabel(m_view);
    KU_INFO("KUENGINE_PATH_PUBLISHED path={} view={} generation={}",asset::toString(m_path),static_cast<uint32_t>(m_view),m_generation);
    return true;
}
void ScenePathControl::drawUI() {
    if (!ImGui::CollapsingHeader("Rendering",ImGuiTreeNodeFlags_DefaultOpen)) return;
    int selected = m_path == asset::RenderPath::Deferred ? 1 : 0;
    if (ImGui::BeginCombo("Render Path", selected ? "Deferred" : "Forward")) {
        if (ImGui::Selectable("Forward",selected==0)) (void)request(asset::RenderPath::Forward);
        ImGui::BeginDisabled(!m_available);
        if (ImGui::Selectable("Deferred",selected==1)) (void)request(asset::RenderPath::Deferred);
        ImGui::EndDisabled(); ImGui::EndCombo();
    }
    if (!m_available) ImGui::TextWrapped("Deferred unavailable: %s",m_reason.c_str());
    ImGui::BeginDisabled(m_path != asset::RenderPath::Deferred);
    if (ImGui::BeginCombo("GBuffer View",gbufferViewLabel(m_view))) {
        for (uint32_t i=0;i<9;++i) if (ImGui::Selectable(gbufferViewLabel(static_cast<GBufferView>(i)),i==static_cast<uint32_t>(m_view)))
            (void)request(m_path,static_cast<GBufferView>(i));
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    if (m_path == asset::RenderPath::Forward) ImGui::TextDisabled("GBuffer is available only in Deferred.");
    ImGui::TextWrapped("Position: world [-2,+2] -> [0,1], outside clamped (not world reconstruction); Normal: n*0.5+0.5; Emissive: clamp [0,1]. Empty: black. ShadingModel: PBR red, Unlit green, unknown magenta. Debug bypasses exposure/tone/Blend.");
    ImGui::TextWrapped("%s",m_status.c_str());
}
bool deferredPathAvailable(const RHIDevice& device, VkFormat depth, std::string& reason) {
    try { validateGBufferFormats(device,depth); reason.clear(); return true; }
    catch (const std::exception& error) { reason=error.what(); return false; }
}
}
