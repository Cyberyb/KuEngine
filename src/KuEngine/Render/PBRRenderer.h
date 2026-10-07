// KuEngine PBR 渲染模块：组织 PBR 绘制项，并负责管线、描述符与网格绘制命令的提交。
#pragma once

#include <cstdint>
#include <vector>
#include <utility>

#include <vulkan/vulkan.h>

#include <KuEngine/RHI/CommandList.h>
#include <KuEngine/RHI/RHIDevice.h>
#include <KuEngine/Render/MaterialInstance.h>
#include <KuEngine/Render/RenderPass.h>
#include <KuEngine/RHI/RHIPipeline.h>
#include <KuEngine/RHI/RHIBuffer.h>

namespace ku {

struct PBRDrawItem {
    uint32_t indexStart = 0;
    uint32_t indexCount = 0;
    uint32_t materialIndex = 0;
};

class PBRRenderer {
public:
    PBRRenderer() = default;

    void setDepthFormat(VkFormat depthFormat) { m_depthFormat = depthFormat; }
    [[nodiscard]] VkFormat depthFormat() const { return m_depthFormat; }

    void setDrawItems(std::vector<PBRDrawItem> drawItems) { m_drawItems = std::move(drawItems); }
    [[nodiscard]] const std::vector<PBRDrawItem>& drawItems() const { return m_drawItems; }

    void initialize(RHIDevice& device);

    void setPipeline(RHIPipeline* pipeline) { m_pipeline = pipeline; }
    void setFrameDescriptorSet(VkDescriptorSet set) { m_frameDescriptorSet = set; }
    void setEnvironmentDescriptorSet(VkDescriptorSet set) noexcept
    {
        m_environmentDescriptorSet = set;
    }
    void setMaterialBindings(
        const std::vector<PBRMaterialBinding>* bindings)
    {
        // Keep draw-time material indices independent from caller vector
        // storage. Descriptor ownership still remains with the GPU asset.
        m_materialBindings = bindings != nullptr
            ? *bindings
            : std::vector<PBRMaterialBinding>{};
    }
    void setVertexIndexBuffers(RHIBuffer* vb, RHIBuffer* ib) { m_vertexBuffer = vb; m_indexBuffer = ib; }

    void setPushConstants(const PBRPushConstants& push) { m_push = push; }
    void setPerDrawPushConstants(std::vector<PBRPushConstants> pushes) { m_perDrawPush = std::move(pushes); }
    void setFrameUniformBuffer(
        RHIBuffer* buffer,
        uint32_t stride,
        uint32_t firstDrawOffset)
    {
        m_frameUniformBuffer = buffer;
        m_frameUniformStride = stride;
        m_firstDrawUniformOffset = firstDrawOffset;
    }
    void setPerDrawFrameUniforms(std::vector<PBRFrameUniforms> frames) { m_perDrawFrameUniforms = std::move(frames); }

    void execute(CommandList& cmd, const FrameData& frame);

private:
    VkFormat m_depthFormat = VK_FORMAT_D32_SFLOAT;
    std::vector<PBRDrawItem> m_drawItems;
    std::vector<PBRMaterialBinding> m_materialBindings;
    RHIPipeline* m_pipeline = nullptr;
    VkDescriptorSet m_frameDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSet m_environmentDescriptorSet = VK_NULL_HANDLE;
    RHIBuffer* m_vertexBuffer = nullptr;
    RHIBuffer* m_indexBuffer = nullptr;
    PBRPushConstants m_push{};
    std::vector<PBRPushConstants> m_perDrawPush;
    RHIBuffer* m_frameUniformBuffer = nullptr;
    uint32_t m_frameUniformStride = 0;
    uint32_t m_firstDrawUniformOffset = 0;
    std::vector<PBRFrameUniforms> m_perDrawFrameUniforms;
};

} // namespace ku
