// Mclaren 示例 Pass：只负责 RenderGraph 声明、逐帧编排、Draw Item 构造与调试 UI。
#pragma once

#include <array>
#include <string>
#include <string_view>

#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif
#include <glm/vec3.hpp>

#include <KuEngine/Render/RenderPass.h>

#include "MclarenRenderResources.h"
#include "MclarenSceneAsset.h"
#include "OrbitCameraController.h"

namespace ku {

class MclarenPass final : public RenderPass {
public:
    MclarenPass() = default;
    ~MclarenPass() override = default;

    [[nodiscard]] std::string_view name() const override
    {
        return "Mclaren";
    }

    void initialize(const RenderContext& context) override;
    void setup(RenderGraphBuilder& builder) override;
    void update(const FrameData& frame) override;
    void execute(CommandList& cmd, const FrameData& frame) override;
    void drawUI() override;
    [[nodiscard]] bool supportsInlineUI() const override { return true; }
    void drawUIInline() override;
    void onResize(uint32_t width, uint32_t height) override;

    void addRotation(float deltaYaw, float deltaPitch);
private:
    MclarenSceneAsset m_scene;
    MclarenRenderResources m_resources;
    OrbitCameraController m_camera;

    std::string m_loadError;
    bool m_hasDepth = false;

    glm::vec3 m_lightDirection{0.35f, 1.0f, 0.45f};
    glm::vec3 m_lightColor{1.0f, 1.0f, 1.0f};
    float m_lightIntensity = 1.0f;

    bool m_enableTextureSampling = true;
    bool m_enableNormalMap = true;
    bool m_enableOrmMap = true;
    bool m_enableEnvironmentMap = true;
    bool m_enableSkybox = true;
    bool m_flipUvY = true;
    bool m_enableOutputGamma = true;
    float m_environmentIntensity = 0.7f;
    float m_environmentExposure = 1.0f;

    std::array<float, 4> m_globalBaseColorFactor{
        1.0f,
        1.0f,
        1.0f,
        1.0f,
    };
};

} // namespace ku
