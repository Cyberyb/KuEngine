#pragma once

#include "RenderGraphResources.h"

#include <memory>
#include <vector>

namespace ku {

class RHIDevice;
class RHIBuffer;
class RHITexture;

class RenderGraphImageAllocation {
public:
    virtual ~RenderGraphImageAllocation() = default;
    [[nodiscard]] virtual VkImage image() const noexcept = 0;
    [[nodiscard]] virtual VkImageView imageView() const noexcept = 0;
};

class RenderGraphBufferAllocation {
public:
    virtual ~RenderGraphBufferAllocation() = default;
    [[nodiscard]] virtual VkBuffer buffer() const noexcept = 0;
};

class RenderGraphResourceAllocator {
public:
    virtual ~RenderGraphResourceAllocator() = default;
    [[nodiscard]] virtual std::unique_ptr<RenderGraphImageAllocation>
    createImage(const ImageDesc& desc, VkExtent2D extent) = 0;
    [[nodiscard]] virtual std::unique_ptr<RenderGraphBufferAllocation>
    createBuffer(const BufferDesc& desc) = 0;
};

class RHIRenderGraphResourceAllocator final
    : public RenderGraphResourceAllocator {
public:
    explicit RHIRenderGraphResourceAllocator(RHIDevice& device);
    [[nodiscard]] std::unique_ptr<RenderGraphImageAllocation>
    createImage(const ImageDesc& desc, VkExtent2D extent) override;
    [[nodiscard]] std::unique_ptr<RenderGraphBufferAllocation>
    createBuffer(const BufferDesc& desc) override;

private:
    RHIDevice* m_device = nullptr;
};

class RenderGraphResourcePool {
public:
    RenderGraphResourcePool() = default;
    RenderGraphResourcePool(RenderGraphResourcePool&&) noexcept = default;
    RenderGraphResourcePool& operator=(RenderGraphResourcePool&&) noexcept = default;
    RenderGraphResourcePool(const RenderGraphResourcePool&) = delete;
    RenderGraphResourcePool& operator=(const RenderGraphResourcePool&) = delete;

    [[nodiscard]] static RenderGraphResourcePool build(
        const RenderGraph& graph,
        VkExtent2D swapchainExtent,
        uint64_t allocationGeneration,
        RenderGraphResourceAllocator& allocator);
    [[nodiscard]] static RenderGraphResourcePool buildResizeCandidate(
        const RenderGraph& graph,
        VkExtent2D swapchainExtent,
        uint64_t allocationGeneration,
        RenderGraphResourceAllocator& allocator);

    void adoptAbsoluteAllocationsFrom(RenderGraphResourcePool& previous) noexcept;
    void swap(RenderGraphResourcePool& other) noexcept;
    void invalidateTransientContents() noexcept;

    [[nodiscard]] ResolvedImage resolveImage(ImageHandle handle) const;
    [[nodiscard]] ResolvedBuffer resolveBuffer(BufferHandle handle) const;
    void setImageState(
        ImageHandle handle,
        ResourceState2 state,
        bool contentsValid);
    void setBufferState(
        BufferHandle handle,
        ResourceState2 state,
        bool contentsValid);
    [[nodiscard]] VkExtent2D swapchainExtent() const noexcept
    {
        return m_swapchainExtent;
    }

private:
    struct Slot {
        ResourceKind kind = ResourceKind::Image;
        ResourceOwnership ownership = ResourceOwnership::Internal;
        ImageDesc imageDesc{};
        BufferDesc bufferDesc{};
        VkExtent2D extent{0, 0};
        std::unique_ptr<RenderGraphImageAllocation> image;
        std::unique_ptr<RenderGraphBufferAllocation> buffer;
        ResourceState2 physicalState{};
        uint64_t allocationGeneration = 0;
        bool contentsValid = false;
        bool reuseFromPrevious = false;
    };

    [[nodiscard]] const Slot& checkedSlot(ResourceHandle handle) const;
    [[nodiscard]] Slot& checkedSlot(ResourceHandle handle);

    uint64_t m_graphGeneration = 0;
    VkExtent2D m_swapchainExtent{0, 0};
    std::vector<Slot> m_slots;
};

[[nodiscard]] VkExtent2D resolveImageExtent(
    const ImageDesc& desc,
    VkExtent2D swapchainExtent);

} // namespace ku
