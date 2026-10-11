// KuEngine public forward program: fixed descriptor schema and finite pipeline cache.
#pragma once

#include <map>
#include <memory>
#include <span>
#include <string>

#include <vulkan/vulkan.h>

#include <KuEngine/Render/ForwardCommon.h>

namespace ku {

class RHIDevice;
class RHIPipeline;
class RHIShader;

namespace forward_set {
inline constexpr uint32_t material = 0;
inline constexpr uint32_t frame = 1;
inline constexpr uint32_t environment = 2;
inline constexpr uint32_t draw = 3;
inline constexpr uint32_t count = 4;
} // namespace forward_set

class ForwardProgram {
public:
    ForwardProgram();
    ~ForwardProgram();

    ForwardProgram(const ForwardProgram&) = delete;
    ForwardProgram& operator=(const ForwardProgram&) = delete;

    [[nodiscard]] bool initialize(
        RHIDevice& device,
        VkFormat colorFormat,
        VkFormat depthFormat,
        VkCompareOp depthCompareOp,
        std::string& errorMessage);
    void reset();
    // Geometry uses the same material/frame/environment/draw set schema.
    [[nodiscard]] bool initializeGeometry(RHIDevice& device, VkFormat depthFormat,
        VkCompareOp depthCompareOp, std::string& errorMessage);
    [[nodiscard]] bool geometryMode() const noexcept { return m_geometryMode; }

    [[nodiscard]] bool ensurePipelines(
        std::span<const ForwardPipelineKey> keys,
        std::string& errorMessage);
    [[nodiscard]] RHIPipeline* pipeline(const ForwardPipelineKey& key) const;
    [[nodiscard]] RHIPipeline* skyboxPipeline() const
    {
        return m_skyboxPipeline.get();
    }
    [[nodiscard]] VkDescriptorSetLayout setLayout(uint32_t set) const;
    [[nodiscard]] bool ready() const;

private:
    [[nodiscard]] std::unique_ptr<RHIPipeline> createPipeline(
        const ForwardPipelineKey& key) const;
    void createLayouts();
    void destroyLayouts();

    RHIDevice* m_device = nullptr;
    VkDevice m_deviceHandle = VK_NULL_HANDLE;
    VkFormat m_colorFormat = VK_FORMAT_UNDEFINED;
    VkFormat m_depthFormat = VK_FORMAT_UNDEFINED;
    VkCompareOp m_depthCompareOp = VK_COMPARE_OP_LESS;
    bool m_geometryMode = false;
    VkDescriptorSetLayout m_setLayouts[forward_set::count]{};

    std::unique_ptr<RHIShader> m_vertexShader;
    std::unique_ptr<RHIShader> m_pbrShader;
    std::unique_ptr<RHIShader> m_unlitShader;
    std::unique_ptr<RHIShader> m_skyboxVertexShader;
    std::unique_ptr<RHIShader> m_skyboxFragmentShader;
    std::map<ForwardPipelineKey, std::unique_ptr<RHIPipeline>> m_pipelines;
    std::unique_ptr<RHIPipeline> m_skyboxPipeline;
};

} // namespace ku
