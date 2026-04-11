#include "RenderPipeline.h"
#include "RenderPass.h"
#include "../RHI/RHIDevice.h"
#include "../RHI/CommandList.h"
#include "../Core/Log.h"

namespace ku {

RenderPipeline::~RenderPipeline()
{
    KU_INFO("RenderPipeline destroyed ({} passes)", m_passes.size());
}

void RenderPipeline::compile(RHIDevice& device)
{
    for (auto& pass : m_passes) {
        pass->initialize(device);
        pass->setup();
        KU_INFO("RenderPass compiled: {}", pass->name());
    }
}

void RenderPipeline::execute(CommandList& cmd, const FrameData& frame)
{
    for (auto& pass : m_passes) {
        if (pass->enabled()) {
            pass->execute(cmd, frame);
        }
    }
}

void RenderPipeline::drawUI()
{
    for (auto& pass : m_passes) {
        if (pass->enabled()) {
            pass->drawUI();
        }
    }
}

void RenderPipeline::onResize(uint32_t width, uint32_t height)
{
    for (auto& pass : m_passes) {
        pass->onResize(width, height);
    }
}

} // namespace ku
