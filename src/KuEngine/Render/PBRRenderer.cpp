#include "KuEngine/Render/PBRRenderer.h"

#include <KuEngine/Core/Log.h>
#include <KuEngine/RHI/RHIPipeline.h>
#include <KuEngine/RHI/RHIBuffer.h>
#include <vulkan/vulkan.h>

namespace ku {

void PBRRenderer::initialize(RHIDevice& device)
{
    (void)device;
    // 目前基础实现不在此处创建材质资源，后续会逐步迁移资源创建逻辑到此处。
}

void PBRRenderer::execute(CommandList& cmd, const FrameData& /*frame*/)
{
    if (!m_pipeline || !m_vertexBuffer || !m_indexBuffer) {
        return;
    }

    if (m_drawItems.empty()) {
        return;
    }

    // 如果存在 per-draw push 常量，则数量需和 draw items 对齐；否则使用单一 m_push。
    const bool havePerDraw = !m_perDrawPush.empty();
    if (havePerDraw && m_perDrawPush.size() != m_drawItems.size()) {
        KU_WARN("PBRRenderer: per-draw push constant count mismatch");
        return;
    }

    m_pipeline->bind(cmd);

    if (m_frameDescriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipeline->layout(),
            1,
            1,
            &m_frameDescriptorSet,
            0,
            nullptr);
    }

    if (m_environmentDescriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipeline->layout(),
            2,
            1,
            &m_environmentDescriptorSet,
            0,
            nullptr);
    }

    VkBuffer vertexBuffers[] = {m_vertexBuffer->buffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmd, m_indexBuffer->buffer(), 0, VK_INDEX_TYPE_UINT32);

    for (size_t i = 0; i < m_drawItems.size(); ++i) {
        const PBRDrawItem& item = m_drawItems[i];
        if (item.indexCount == 0) {
            continue;
        }

        const PBRMaterialBinding* mat = item.materialBinding;
        // 如果提供了每次绘制的 FrameUniforms，则在绘制前更新 UBO
        if (m_frameUniformBuffer && !m_perDrawFrameUniforms.empty()) {
            if (i < m_perDrawFrameUniforms.size()) {
                void* mapped = m_frameUniformBuffer->map();
                if (mapped != nullptr) {
                    std::memcpy(mapped, &m_perDrawFrameUniforms[i], sizeof(PBRFrameUniforms));
                    m_frameUniformBuffer->flush();
                    m_frameUniformBuffer->unmap();
                }
            }
        }
        if (mat && mat->descriptorSet != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(
                cmd,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_pipeline->layout(),
                0,
                1,
                &mat->descriptorSet,
                0,
                nullptr);
        }

        const PBRPushConstants* pushPtr = nullptr;
        if (havePerDraw) {
            pushPtr = &m_perDrawPush[i];
        } else {
            pushPtr = &m_push;
        }

        vkCmdPushConstants(
            cmd,
            m_pipeline->layout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(PBRPushConstants),
            pushPtr);

        vkCmdDrawIndexed(cmd, item.indexCount, 1, item.indexStart, 0, 0);
    }
}

} // namespace ku
