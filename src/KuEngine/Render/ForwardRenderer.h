// KuEngine public forward renderer: frame/draw UBO ownership and borrowed draw submission.
#pragma once

#include <memory>
#include <span>
#include <string>

#include <vulkan/vulkan.h>

#include <KuEngine/Render/ForwardCommon.h>

namespace ku {

class CommandList;
class ForwardProgram;
class RHIBuffer;
class RHIDevice;

class ForwardRenderer {
public:
    ForwardRenderer();
    ~ForwardRenderer();

    ForwardRenderer(const ForwardRenderer&) = delete;
    ForwardRenderer& operator=(const ForwardRenderer&) = delete;

    [[nodiscard]] bool initialize(
        RHIDevice& device,
        ForwardProgram& program,
        size_t initialCapacity,
        std::string& errorMessage);
    [[nodiscard]] bool ensureCapacity(
        size_t drawCapacity,
        std::string& errorMessage);
    void reset();

    [[nodiscard]] bool render(
        CommandList& commandList,
        const ForwardView& view,
        std::span<const ForwardDraw> draws,
        std::string& errorMessage);
    [[nodiscard]] uint32_t capacity() const { return m_layout.capacity; }
    [[nodiscard]] bool ready() const;

private:
    struct DynamicCandidate;
    [[nodiscard]] bool writeFrame(const ForwardView& view);
    [[nodiscard]] bool writeDraws(
        std::span<const ForwardDraw> draws,
        bool outputGamma,
        float exposure);
    void drawSkybox(CommandList& commandList, const ForwardView& view) const;
    void destroyDescriptors();

    RHIDevice* m_device = nullptr;
    ForwardProgram* m_program = nullptr;
    VkDevice m_deviceHandle = VK_NULL_HANDLE;
    std::unique_ptr<RHIBuffer> m_frameBuffer;
    std::unique_ptr<RHIBuffer> m_drawBuffer;
    ForwardDynamicBufferLayout m_layout{};
    VkDescriptorPool m_framePool = VK_NULL_HANDLE;
    VkDescriptorPool m_drawPool = VK_NULL_HANDLE;
    VkDescriptorSet m_frameSet = VK_NULL_HANDLE;
    VkDescriptorSet m_drawSet = VK_NULL_HANDLE;
};

} // namespace ku
