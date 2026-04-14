#pragma once

#include <string_view>
#include <vector>
#include <memory>
#include <type_traits>
#include <utility>
#include <cstdint>
#include <string>
#include <unordered_map>

#include <vulkan/vulkan.h>

#include "RenderGraph.h"
#include "RenderPass.h"

namespace ku {

class RHIDevice;
class CommandList;
struct FrameData;

class RenderPipeline {
public:
    ~RenderPipeline();

    template<typename T, typename... Args>
    T& addPass(Args&&... args) {
        static_assert(std::is_base_of_v<RenderPass, T>, "T must derive from RenderPass");
        auto pass = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = pass.get();
        m_passes.emplace_back(std::move(pass));
        return *ptr;
    }

    void compile(RHIDevice& device);
    void execute(CommandList& cmd, const FrameData& frame);
    void drawUI();
    void onResize(uint32_t width, uint32_t height);
    void setExecuteInsideRendering(bool insideRendering) { m_executeInsideRendering = insideRendering; }
    void bindExternalImage(
        std::string_view resourceName,
        VkImage image,
        VkImageLayout currentLayout,
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);
    void clearExternalResources();

    [[nodiscard]] size_t passCount() const { return m_passes.size(); }

private:
    struct ExternalImageBinding {
        VkImage image = VK_NULL_HANDLE;
        VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    };

    struct CompileDebugInfo {
        size_t passCount = 0;
        size_t resourceCount = 0;
        size_t dependencyCount = 0;
        size_t barrierCount = 0;
        std::string orderSummary;
    };

    struct ExecuteDebugInfo {
        uint64_t frameIndex = 0;
        size_t plannedBarriers = 0;
        size_t appliedBarriers = 0;
        size_t skippedUnbound = 0;
        size_t skippedNoAccess = 0;
        size_t skippedNoop = 0;
        size_t skippedInRendering = 0;
    };

    struct BarrierDebugEvent {
        std::string fromPass;
        std::string toPass;
        std::string resourceName;
        ResourceHazardType hazard = ResourceHazardType::ReadAfterWrite;
        VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        bool applied = false;
        std::string reason;
    };

    std::vector<std::unique_ptr<RenderPass>> m_passes;
    std::vector<size_t> m_compiledExecutionOrder;
    std::unordered_map<std::string, uint32_t> m_resourceNameToId;
    std::unordered_map<uint32_t, ExternalImageBinding> m_externalImageBindings;
    CompileDebugInfo m_compileDebug;
    ExecuteDebugInfo m_executeDebug;
    std::vector<BarrierDebugEvent> m_barrierDebugEvents;
    bool m_executeInsideRendering = false;
    RenderGraph m_renderGraph;
};

} // namespace ku
