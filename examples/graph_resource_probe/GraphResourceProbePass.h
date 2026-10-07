#pragma once

#include <KuEngine/Render/RenderGraph.h>
#include <KuEngine/Render/RenderGraphCallbackPass.h>
#include <KuEngine/Render/RenderPass.h>
#include <KuEngine/RHI/RHIParameterSet.h>
#include <KuEngine/RHI/RHIPipeline.h>
#include <KuEngine/RHI/RHIShader.h>

#include <cstdint>
#include <memory>

namespace ku {

class RHIDevice;

class GraphResourceImageClearPass final : public RenderPass {
public:
    [[nodiscard]] std::string_view name() const override
    {
        return "GraphResourceImageClear";
    }
    void initialize(const RenderContext& context) override;
    void setup(RenderGraphBuilder& builder) override;
    void execute(CommandList&, const FrameData&, RenderGraphResourceResolver&) override;
    [[nodiscard]] std::optional<CommandListStatistics>
    expectedFrameStatistics() const override { return CommandListStatistics{0, 0}; }

private:
    ImageDesc m_imageDesc{};
    ImageHandle m_image{};
};

class GraphResourceClearPass final : public RenderPass {
public:
    [[nodiscard]] std::string_view name() const override
    {
        return "GraphResourceBufferFill";
    }
    void initialize(const RenderContext& context) override;
    void setup(RenderGraphBuilder& builder) override;
    void execute(CommandList&, const FrameData&, RenderGraphResourceResolver&) override;
    [[nodiscard]] std::optional<CommandListStatistics>
    expectedFrameStatistics() const override { return CommandListStatistics{0, 0}; }

private:
    BufferDesc m_sourceDesc{};
    BufferHandle m_source{};
};

struct GraphResourceCopyParameters {
    BufferDesc m_sourceDesc{};
    BufferDesc m_computeDesc{};
    BufferHandle m_source{};
    BufferHandle m_compute{};
};

[[nodiscard]] CallbackPassDesc<GraphResourceCopyParameters>
makeGraphResourceCopyCallback();

struct GraphResourceComputeParameters {
    BufferDesc m_computeDesc{};
    BufferDesc m_drawDesc{};
    BufferHandle m_compute{};
    BufferHandle m_draw{};
    std::unique_ptr<RHIShader> m_shader;
    std::unique_ptr<RHIParameterSet> m_parameters;
    std::unique_ptr<RHIComputePipeline> m_pipeline;
};

[[nodiscard]] CallbackPassDesc<GraphResourceComputeParameters>
makeGraphResourceComputeCallback();

class GraphResourceDrawPass final : public RenderPass {
public:
    GraphResourceDrawPass();
    ~GraphResourceDrawPass() override;
    [[nodiscard]] std::string_view name() const override
    {
        return "GraphResourceSampleDraw";
    }
    void initialize(const RenderContext& context) override;
    void setup(RenderGraphBuilder& builder) override;
    void execute(CommandList&, const FrameData&, RenderGraphResourceResolver&) override;
    [[nodiscard]] std::optional<CommandListStatistics>
    expectedFrameStatistics() const override { return CommandListStatistics{1, 3}; }

private:
    void resetGpuResources() noexcept;

    ImageDesc m_imageDesc{};
    ImageDesc m_swapchainDesc{};
    BufferDesc m_drawDesc{};
    ImageHandle m_image{};
    BufferHandle m_draw{};
    RHIDevice* m_device = nullptr;
    std::unique_ptr<RHIShader> m_vertexShader;
    std::unique_ptr<RHIShader> m_fragmentShader;
    std::unique_ptr<RHIParameterSet> m_parameters;
    std::unique_ptr<RHIPipeline> m_pipeline;
    VkSampler m_sampler = VK_NULL_HANDLE;
    uint64_t m_boundImageGeneration = 0;
    uint64_t m_initialImageGeneration = 0;
    uint64_t m_initialBufferGeneration = 0;
    VkExtent2D m_initialExtent{0, 0};
    bool m_resizeReported = false;
};

class GraphResourcePostDrawCopyPass final : public RenderPass {
public:
    [[nodiscard]] std::string_view name() const override
    {
        return "GraphResourcePostDrawCopy";
    }
    void initialize(const RenderContext& context) override;
    void setup(RenderGraphBuilder& builder) override;
    void execute(CommandList&, const FrameData&, RenderGraphResourceResolver&) override;
    [[nodiscard]] std::optional<CommandListStatistics>
    expectedFrameStatistics() const override { return CommandListStatistics{0, 0}; }

private:
    BufferDesc m_drawDesc{};
    BufferDesc m_resultDesc{};
    BufferHandle m_draw{};
    BufferHandle m_result{};
};

} // namespace ku
