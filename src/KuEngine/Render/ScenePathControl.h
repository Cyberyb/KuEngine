#pragma once
#include <KuEngine/Asset/AssetConfig.h>
#include <array>
#include <string>
#include <vulkan/vulkan.h>

namespace ku {
class RenderPass;
class RHIDevice;
enum class GBufferView : uint32_t { Scene, Base, Metallic, Normal, Roughness, Position, ShadingModel, Emissive, Ao };
[[nodiscard]] const char* gbufferViewLabel(GBufferView view) noexcept;
struct SceneBranchMask {
    bool forwardOpaque = true, geometry = false, lighting = false, display = true, debug = false;
    bool operator==(const SceneBranchMask&) const = default;
};
[[nodiscard]] SceneBranchMask sceneBranchMask(asset::RenderPath path, GBufferView view);
// Non-owning static branch controller. request() is CPU-only; publishPending()
// must be called by the scene coordinator at the existing fence-safe update.
class ScenePathControl {
public:
    void setDeferredCapability(bool available, std::string reason = {});
    void bind(RenderPass& forward, RenderPass* geometry, RenderPass* lighting, RenderPass& display, RenderPass* debug);
    void initialize(asset::RenderPath path);
    [[nodiscard]] bool request(asset::RenderPath path, GBufferView view = GBufferView::Scene);
    bool publishPending();
    void drawUI();
    [[nodiscard]] asset::RenderPath path() const noexcept { return m_path; }
    [[nodiscard]] GBufferView view() const noexcept { return m_view; }
    [[nodiscard]] bool deferredAvailable() const noexcept { return m_available; }
    [[nodiscard]] const std::string& status() const noexcept { return m_status; }
    [[nodiscard]] uint64_t generation() const noexcept { return m_generation; }
private:
    void apply();
    std::array<RenderPass*,5> m_passes{};
    asset::RenderPath m_path = asset::RenderPath::Forward, m_requestedPath = asset::RenderPath::Forward;
    GBufferView m_view = GBufferView::Scene, m_requestedView = GBufferView::Scene;
    bool m_available = true, m_pending = false, m_bound = false;
    uint64_t m_generation = 0;
    std::string m_reason, m_status;
};
[[nodiscard]] bool deferredPathAvailable(const RHIDevice&, VkFormat depthFormat, std::string& reason);
} // namespace ku
