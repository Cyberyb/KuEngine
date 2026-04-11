#include "RenderPipeline.h"
#include "RenderPass.h"
#include "RHIDevice.h"

namespace ku {

RenderPipeline::~RenderPipeline() {
    m_passes.clear();
}

void RenderPipeline::compile(RHIDevice& device) {
    for (auto& pass : m_passes) {
        pass->initialize(device);
    }
    KU_INFO("RenderPipeline: compiled {} passes", m_passes.size());
}

void RenderPipeline::execute(CommandList& cmd, const FrameData& frame) {
    for (auto& pass : m_passes) {
        if (pass->enabled()) {
            pass->execute(cmd, frame);
        }
    }
}

void RenderPipeline::drawUI() {
    if (ImGui::CollapsingHeader("Render Passes")) {
        for (auto& pass : m_passes) {
            ImGui::PushID(pass->name().data());
            if (ImGui::CollapsingHeader(pass->name().data())) {
                static bool enabled = pass->enabled();
                ImGui::Checkbox("Enabled", &enabled);
                pass->drawUI();
            }
            ImGui::PopID();
        }
    }
}

void RenderPipeline::onResize(uint32_t width, uint32_t height) {
    for (auto& pass : m_passes) {
        pass->onResize(width, height);
    }
}

} // namespace ku
