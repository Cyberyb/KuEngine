// KuEngine 渲染管线模块：管理 RenderPass 集合，编译渲染图并驱动逐帧渲染流程。
#pragma once

#include <string_view>
#include <vector>
#include <memory>
#include <type_traits>
#include <utility>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

#include <vulkan/vulkan.h>

#include "RenderGraph.h"
#include "RenderPass.h"

namespace ku {

class RHIDevice;
class CommandList;
struct FrameData;
struct RenderContext;

class RenderPipeline {
public:
    struct ExternalImageBindingInfo {
        std::string_view resourceName;
        VkImage image = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VkExtent2D extent{0, 0};
        VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        VkClearValue clearValue{};
        VkAttachmentLoadOp defaultLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        VkAttachmentStoreOp defaultStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        VkImageLayout finalLayout = VK_IMAGE_LAYOUT_GENERAL;
        bool contentsValid = false;
    };

    ~RenderPipeline();

    template<typename T, typename... Args>
    T& addPass(Args&&... args) {
        static_assert(std::is_base_of_v<RenderPass, T>, "T must derive from RenderPass");
        auto pass = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = pass.get();
        m_passes.emplace_back(std::move(pass));
        m_compiled = false;
        return *ptr;
    }

    void compile(const RenderContext& context);
    void update(const FrameData& frame);
    void execute(CommandList& cmd, const FrameData& frame);
    void executeOverlay(
        CommandList& cmd,
        const std::function<void(VkCommandBuffer)>& draw);
    void finalizeExternalImages(CommandList& cmd);
    void drawUI();
    void drawUIInline();
    void onResize(uint32_t width, uint32_t height);
    void bindExternalImage(const ExternalImageBindingInfo& info);
    void clearExternalResources();

    [[nodiscard]] size_t passCount() const { return m_passes.size(); }
    [[nodiscard]] bool externalContentsValid(std::string_view resourceName) const;

private:
    void executePassNode(
        CommandList& cmd,
        const FrameData& frame,
        const PassNode& node,
        const std::vector<ResourceDesc>& resources);

    struct ExternalImageBinding {
        VkImage image = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VkExtent2D extent{0, 0};
        VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        VkClearValue clearValue{};
        VkAttachmentLoadOp defaultLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        VkAttachmentStoreOp defaultStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        VkImageLayout finalLayout = VK_IMAGE_LAYOUT_GENERAL;
        bool contentsValid = false;
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
        size_t resourceTransitions = 0;
        size_t renderingScopes = 0;
        size_t skippedUnbound = 0;
        size_t skippedNoAccess = 0;
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
    std::unordered_map<std::string, ExternalImageBinding> m_externalImageBindings;
    CompileDebugInfo m_compileDebug;
    ExecuteDebugInfo m_executeDebug;
    std::vector<BarrierDebugEvent> m_barrierDebugEvents;
    bool m_compiled = false;
    RenderGraph m_renderGraph;
};

} // namespace ku
