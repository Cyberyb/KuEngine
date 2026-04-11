#include "RHIPipeline.h"
#include "RHIDevice.h"
#include "../Core/Log.h"

namespace ku {

RHIPipeline::RHIPipeline(const RHIDevice& device, const GraphicsPipelineDesc& desc)
    : m_device(device.device())
{
    (void)desc;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    VK_CHECK(vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_layout));

    KU_INFO("Graphics pipeline created");
}

RHIPipeline::~RHIPipeline()
{
    if (m_pipeline) vkDestroyPipeline(m_device, m_pipeline, nullptr);
    if (m_layout) vkDestroyPipelineLayout(m_device, m_layout, nullptr);
}

void RHIPipeline::bind(VkCommandBuffer cmd) const
{
    if (m_pipeline == VK_NULL_HANDLE) {
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
}

} // namespace ku
