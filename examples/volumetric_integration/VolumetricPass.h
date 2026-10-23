#pragma once
#include "VolumetricOptions.h"
#include <KuEngine/Render/RenderPass.h>
#include <KuEngine/RHI/RHIShader.h>
#include <KuEngine/RHI/RHIPipeline.h>
#include <KuEngine/RHI/RHIParameterSet.h>
#include <memory>
#include <vector>

namespace ku::volumetric {
inline constexpr std::string_view sceneColorName = "VolumetricSceneColor";
ImageDesc sceneColorDesc();
struct alignas(16) SceneConstants {
    std::array<float, 4> viewport{};
    std::array<float, 4> medium{}; // time, density, light, strong fog
    std::array<int32_t, 4> quality{}; // view steps, shadow steps, integration, flags
};
static_assert(sizeof(SceneConstants) == 48);
class ScenePass final : public RenderPass {
public:
    explicit ScenePass(Settings settings) : m_settings(settings) {}
    ~ScenePass() override;
    std::string_view name() const override { return "VolumetricIntegration"; }
    void initialize(const RenderContext&) override;
    void setup(RenderGraphBuilder&) override;
    void update(const FrameData&) override;
    void execute(CommandList&, const FrameData&) override;
    void drawUI() override;
    std::optional<CommandListStatistics> expectedFrameStatistics() const override { return CommandListStatistics{1,3}; }
    SceneConstants constants(const FrameData&) const;
private:
    Settings m_settings;
    std::unique_ptr<RHIShader> m_vert, m_frag;
    std::unique_ptr<RHIPipeline> m_pipeline;
};
class DisplayPass final : public RenderPass {
public:
    ~DisplayPass() override;
    std::string_view name() const override { return "VolumetricDisplay"; }
    void initialize(const RenderContext&) override;
    void setup(RenderGraphBuilder&) override;
    void execute(CommandList&, const FrameData&, RenderGraphResourceResolver&) override;
    std::optional<CommandListStatistics> expectedFrameStatistics() const override { return CommandListStatistics{1,3}; }
private:
    void reset();
    RHIDevice* m_device = nullptr;
    VkSampler m_sampler = VK_NULL_HANDLE;
    ImageDesc m_outputDesc;
    ImageHandle m_scene;
    bool m_srgb = false;
    std::vector<std::unique_ptr<RHIParameterSet>> m_parameters;
    std::unique_ptr<RHIShader> m_vert, m_frag;
    std::unique_ptr<RHIPipeline> m_pipeline;
};
} // namespace ku::volumetric
