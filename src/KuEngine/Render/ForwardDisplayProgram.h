#pragma once

#include "ForwardGraphTargets.h"
#include "RenderGraphResources.h"

#include <cstdint>
#include <vector>
#include <memory>

namespace ku {

class CommandList;
class RHIDevice;
class RHIPipeline;
class RHIShader;
class RHIParameterSet;

class ForwardDisplayProgram final {
public:
    ForwardDisplayProgram();
    ~ForwardDisplayProgram();

    ForwardDisplayProgram(const ForwardDisplayProgram&) = delete;
    ForwardDisplayProgram& operator=(const ForwardDisplayProgram&) = delete;

    void initialize(
        const RHIDevice& device,
        VkFormat sceneColorFormat,
        VkFormat outputFormat, uint32_t framesInFlight = 1);
    void draw(
        CommandList& commands,
        const ResolvedImage& sceneColor,
        const ForwardDisplayUvTransform& uvTransform,
        float exposure = 1.0f, bool encodeSrgb = true, uint32_t frameSlot = 0);

    [[nodiscard]] uint64_t descriptorRebindCount() const noexcept
    {
        return m_descriptorRebindCount;
    }

private:
    void reset() noexcept;
    std::vector<std::unique_ptr<ForwardDisplayProgram>> m_additionalFrames;

    const RHIDevice* m_device = nullptr;
    VkFormat m_sceneColorFormat = VK_FORMAT_UNDEFINED;
    VkFormat m_outputFormat = VK_FORMAT_UNDEFINED;
    VkSampler m_sampler = VK_NULL_HANDLE;
    std::unique_ptr<RHIParameterSet> m_parameters;
    std::unique_ptr<RHIShader> m_vertexShader;
    std::unique_ptr<RHIShader> m_fragmentShader;
    std::unique_ptr<RHIPipeline> m_pipeline;
    uint64_t m_boundGeneration = 0;
    VkImageView m_boundView = VK_NULL_HANDLE;
    uint64_t m_descriptorRebindCount = 0;
};

} // namespace ku
