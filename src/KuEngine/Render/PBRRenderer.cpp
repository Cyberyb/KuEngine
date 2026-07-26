#include "KuEngine/Render/PBRRenderer.h"

#include <KuEngine/Core/Log.h>
#include <KuEngine/RHI/RHIPipeline.h>
#include <KuEngine/RHI/RHIBuffer.h>

#include <cstddef>
#include <cstring>
#include <limits>

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

    const bool havePerDrawFrames = !m_perDrawFrameUniforms.empty();
    if (havePerDrawFrames && m_perDrawFrameUniforms.size() != m_drawItems.size()) {
        KU_WARN("PBRRenderer: per-draw frame uniform count mismatch");
        return;
    }

    if (havePerDrawFrames) {
        if (!m_frameUniformBuffer || m_frameDescriptorSet == VK_NULL_HANDLE
            || m_frameUniformStride < sizeof(PBRFrameUniforms)) {
            KU_WARN("PBRRenderer: dynamic frame uniform buffer is not configured");
            return;
        }

        const uint64_t lastOffset =
            static_cast<uint64_t>(m_firstDrawUniformOffset)
            + static_cast<uint64_t>(m_drawItems.size() - 1) * m_frameUniformStride;
        const uint64_t requiredSize = lastOffset + sizeof(PBRFrameUniforms);
        if (lastOffset > std::numeric_limits<uint32_t>::max()
            || requiredSize > m_frameUniformBuffer->size()) {
            KU_WARN("PBRRenderer: dynamic frame uniform buffer is too small");
            return;
        }

        auto* mapped = static_cast<std::byte*>(m_frameUniformBuffer->map());
        if (!mapped) {
            KU_WARN("PBRRenderer: failed to map dynamic frame uniform buffer");
            return;
        }

        for (size_t i = 0; i < m_perDrawFrameUniforms.size(); ++i) {
            const size_t offset =
                static_cast<size_t>(m_firstDrawUniformOffset)
                + i * static_cast<size_t>(m_frameUniformStride);
            std::memcpy(
                mapped + offset,
                &m_perDrawFrameUniforms[i],
                sizeof(PBRFrameUniforms));
        }
        m_frameUniformBuffer->flush();
        m_frameUniformBuffer->unmap();
    }

    m_pipeline->bind(cmd);

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
        if (m_frameDescriptorSet != VK_NULL_HANDLE) {
            const uint32_t dynamicOffset = havePerDrawFrames
                ? m_firstDrawUniformOffset
                    + static_cast<uint32_t>(i) * m_frameUniformStride
                : 0;
            vkCmdBindDescriptorSets(
                cmd,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_pipeline->layout(),
                1,
                1,
                &m_frameDescriptorSet,
                1,
                &dynamicOffset);
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
