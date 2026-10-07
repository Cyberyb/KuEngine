// KuEngine graph executor: owns passes and graph-internal resource allocations.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <vulkan/vulkan.h>

#include "RenderGraph.h"
#include "RenderGraphResourcePool.h"
#include "RenderGraphResources.h"
#include "RenderGraphStatePlanner.h"
#include "RenderGraphCallbackPass.h"
#include "RenderPass.h"

namespace ku {

class RHIDevice;
class CommandList;
struct FrameData;
struct RenderContext;

class RenderPipeline final : public RenderGraphResourceResolver {
public:
    struct ExternalImageBindingInfo {
        std::string_view resourceName;
        VkImage image = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VkExtent2D extent{0, 0};
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkImageUsageFlags usage = 0;
        ResourceState2 initialState{};
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        VkClearValue clearValue{};
        VkAttachmentLoadOp defaultLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        VkAttachmentStoreOp defaultStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        ResourceState2 finalState{};
        bool contentsValid = false;
    };

    struct ExternalBufferBindingInfo {
        std::string_view resourceName;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize size = 0;
        VkBufferUsageFlags usage = 0;
        ResourceState2 initialState{};
        ResourceState2 finalState{};
        bool contentsValid = false;
    };

    RenderPipeline();
    ~RenderPipeline();

    template<typename T, typename... Args>
    T& addPass(Args&&... args)
    {
        static_assert(std::is_base_of_v<RenderPass, T>);
        auto pass = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = pass.get();
        m_passes.emplace_back(std::move(pass));
        m_compiled = false;
        invalidateExports();
        return *ptr;
    }

    template<typename Parameters>
    CallbackRenderPass<Parameters>& addCallbackPass(
        CallbackPassDesc<Parameters> desc)
    {
        auto pass = std::make_unique<CallbackRenderPass<Parameters>>(
            std::move(desc));
        auto* ptr = pass.get();
        m_passes.emplace_back(std::move(pass));
        m_compiled = false;
        invalidateExports();
        return *ptr;
    }

    void compile(const RenderContext& context);
    void update(const FrameData& frame);
    void execute(CommandList& cmd, const FrameData& frame);
    void executeOverlay(
        CommandList& cmd,
        const std::function<void(VkCommandBuffer)>& draw);
    void finalizeExternalImages(CommandList& cmd);
    void drawPassUIContent();
    void drawRenderGraphUIContent();
    void onResize(uint32_t width, uint32_t height);

    void bindExternalImage(ImageHandle handle, const ExternalImageBindingInfo& info);
    void bindExternalImage(const ExternalImageBindingInfo& info);
    void bindExternalBuffer(BufferHandle handle, const ExternalBufferBindingInfo& info);
    void bindExternalBuffer(const ExternalBufferBindingInfo& info);
    void clearExternalResources();

    // Pure descriptor checks are public so callers/tests can reject bad
    // bindings before command recording without requiring a live pipeline.
    static void validateExternalImageBindingInfo(
        const ImageDesc& expected,
        VkExtent2D swapchainExtent,
        const ExternalImageBindingInfo& info,
        std::string_view logicalName = {});
    static void validateExternalBufferBindingInfo(
        const BufferDesc& expected,
        const ExternalBufferBindingInfo& info,
        std::string_view logicalName = {});

    [[nodiscard]] ResolvedImage resolveImage(
        ImageHandle handle,
        ImageUse declaredUse,
        ImageSubresourceRange range = {}) override;
    [[nodiscard]] ResolvedBuffer resolveBuffer(
        BufferHandle handle,
        BufferUse declaredUse,
        BufferRange range = {}) override;
    [[nodiscard]] bool insideRenderingScope() const noexcept override
    {
        return m_currentPass != nullptr && !m_currentPass->attachments.empty();
    }
    [[nodiscard]] ExportedImage exportImage(ImageHandle handle) const;
    [[nodiscard]] ExportedBuffer exportBuffer(BufferHandle handle) const;

    [[nodiscard]] size_t passCount() const { return m_passes.size(); }
    [[nodiscard]] bool externalContentsValid(std::string_view resourceName) const;
    [[nodiscard]] std::optional<CommandListStatistics>
    expectedFrameStatistics() const;
    [[nodiscard]] bool compiled() const noexcept { return m_compiled; }

private:
    struct ExternalImageBinding {
        ImageHandle handle{};
        VkImage image = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VkExtent2D extent{0, 0};
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkImageUsageFlags usage = 0;
        ResourceState2 state{};
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        VkClearValue clearValue{};
        VkAttachmentLoadOp defaultLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        VkAttachmentStoreOp defaultStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        ResourceState2 finalState{};
        uint64_t allocationGeneration = 0;
        bool contentsValid = false;
    };

    struct ExternalBufferBinding {
        BufferHandle handle{};
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize size = 0;
        VkBufferUsageFlags usage = 0;
        ResourceState2 state{};
        ResourceState2 finalState{};
        uint64_t allocationGeneration = 0;
        bool contentsValid = false;
    };

    void executePassNode(
        CommandList& cmd,
        const FrameData& frame,
        const PassNode& node,
        const std::vector<ResourceDesc>& resources);
    void beginFrame();
    void invalidateExports() noexcept;
    void setResolvedImageState(
        ImageHandle handle,
        ResourceState2 state,
        bool contentsValid);
    void setResolvedBufferState(
        BufferHandle handle,
        ResourceState2 state,
        bool contentsValid);
    [[nodiscard]] ResolvedImage resolveImagePhysical(ImageHandle handle);
    [[nodiscard]] ResolvedBuffer resolveBufferPhysical(BufferHandle handle);
    [[nodiscard]] bool currentPassDeclares(
        ResourceHandle resource,
        ImageUse use,
        ImageSubresourceRange range) const;
    [[nodiscard]] bool currentPassDeclares(
        ResourceHandle resource,
        BufferUse use,
        BufferRange range) const;

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
    std::unordered_map<uint32_t, ExternalImageBinding> m_externalImageBindings;
    std::unordered_map<uint32_t, ExternalBufferBinding> m_externalBufferBindings;
    CompileDebugInfo m_compileDebug;
    ExecuteDebugInfo m_executeDebug;
    std::vector<BarrierDebugEvent> m_barrierDebugEvents;
    bool m_compiled = false;
    bool m_executeCompleted = false;
    uint64_t m_nextAllocationGeneration = 1;
    uint64_t m_nextExternalBindingGeneration = 1;
    RHIDevice* m_device = nullptr;
    RenderGraph m_renderGraph;
    RenderGraphResourcePool m_resourcePool;
    std::shared_ptr<ExportLifetimeState> m_exportLifetime;
    const PassNode* m_currentPass = nullptr;
    std::vector<PlannedResourceFinalState> m_executionResourceStates;
};

} // namespace ku
