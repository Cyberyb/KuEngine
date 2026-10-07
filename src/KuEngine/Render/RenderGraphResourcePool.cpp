#include "RenderGraphResourcePool.h"

#include <KuEngine/RHI/RHIBuffer.h>
#include <KuEngine/RHI/RHIDevice.h>
#include <KuEngine/RHI/RHITexture.h>

#include <stdexcept>
#include <utility>

namespace ku {
namespace {

class RHIImageAllocation final : public RenderGraphImageAllocation {
public:
    explicit RHIImageAllocation(std::unique_ptr<RHITexture> texture)
        : m_texture(std::move(texture))
    {
        if (!m_texture) {
            throw std::runtime_error("RenderGraph image allocator returned null");
        }
    }
    [[nodiscard]] VkImage image() const noexcept override
    {
        return m_texture->image();
    }
    [[nodiscard]] VkImageView imageView() const noexcept override
    {
        return m_texture->imageView();
    }

private:
    std::unique_ptr<RHITexture> m_texture;
};

class RHIBufferAllocation final : public RenderGraphBufferAllocation {
public:
    explicit RHIBufferAllocation(std::unique_ptr<RHIBuffer> buffer)
        : m_buffer(std::move(buffer))
    {
        if (!m_buffer) {
            throw std::runtime_error("RenderGraph buffer allocator returned null");
        }
    }
    [[nodiscard]] VkBuffer buffer() const noexcept override
    {
        return m_buffer->buffer();
    }

private:
    std::unique_ptr<RHIBuffer> m_buffer;
};

} // namespace

VkExtent2D resolveImageExtent(
    const ImageDesc& desc,
    VkExtent2D swapchainExtent)
{
    const VkExtent2D extent = desc.extent.mode == ImageExtentMode::Absolute
        ? desc.extent.absolute
        : swapchainExtent;
    if (extent.width == 0 || extent.height == 0) {
        throw std::runtime_error(
            "RenderGraph image resolved to a zero-sized extent");
    }
    return extent;
}

RHIRenderGraphResourceAllocator::RHIRenderGraphResourceAllocator(
    RHIDevice& device)
    : m_device(&device)
{
}

std::unique_ptr<RenderGraphImageAllocation>
RHIRenderGraphResourceAllocator::createImage(
    const ImageDesc& desc,
    VkExtent2D extent)
{
    RHITexture::CreateInfo info{};
    info.width = extent.width;
    info.height = extent.height;
    info.format = desc.format;
    info.usage = desc.usage;
    info.aspect = desc.aspect;
    info.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
    return std::make_unique<RHIImageAllocation>(
        std::make_unique<RHITexture>(*m_device, info));
}

std::unique_ptr<RenderGraphBufferAllocation>
RHIRenderGraphResourceAllocator::createBuffer(const BufferDesc& desc)
{
    RHIBuffer::CreateInfo info{};
    info.size = desc.size;
    info.usage = desc.usage;
    info.memoryUsage = desc.memoryUsage;
    return std::make_unique<RHIBufferAllocation>(
        std::make_unique<RHIBuffer>(*m_device, info));
}

RenderGraphResourcePool RenderGraphResourcePool::build(
    const RenderGraph& graph,
    VkExtent2D swapchainExtent,
    uint64_t allocationGeneration,
    RenderGraphResourceAllocator& allocator)
{
    RenderGraphResourcePool candidate{};
    candidate.m_graphGeneration = graph.generation();
    candidate.m_swapchainExtent = swapchainExtent;
    candidate.m_slots.resize(graph.resources().size());

    for (size_t index = 0; index < graph.resources().size(); ++index) {
        const ResourceDesc& resource = graph.resources()[index];
        Slot& slot = candidate.m_slots[index];
        slot.kind = resource.kind;
        slot.ownership = resource.ownership;
        if (resource.kind == ResourceKind::Image) {
            slot.imageDesc = std::get<ImageDesc>(resource.description);
            slot.extent = resolveImageExtent(slot.imageDesc, swapchainExtent);
            if (!resource.external()) {
                slot.image = allocator.createImage(slot.imageDesc, slot.extent);
                if (!slot.image || slot.image->image() == VK_NULL_HANDLE
                    || slot.image->imageView() == VK_NULL_HANDLE) {
                    throw std::runtime_error(
                        "RenderGraph allocator produced an incomplete image: "
                        + resource.name);
                }
                slot.allocationGeneration = allocationGeneration;
            }
        } else {
            slot.bufferDesc = std::get<BufferDesc>(resource.description);
            if (!resource.external()) {
                slot.buffer = allocator.createBuffer(slot.bufferDesc);
                if (!slot.buffer || slot.buffer->buffer() == VK_NULL_HANDLE) {
                    throw std::runtime_error(
                        "RenderGraph allocator produced an incomplete buffer: "
                        + resource.name);
                }
                slot.allocationGeneration = allocationGeneration;
            }
        }
    }
    return candidate;
}

RenderGraphResourcePool RenderGraphResourcePool::buildResizeCandidate(
    const RenderGraph& graph,
    VkExtent2D swapchainExtent,
    uint64_t allocationGeneration,
    RenderGraphResourceAllocator& allocator)
{
    RenderGraphResourcePool candidate{};
    candidate.m_graphGeneration = graph.generation();
    candidate.m_swapchainExtent = swapchainExtent;
    candidate.m_slots.resize(graph.resources().size());

    for (size_t index = 0; index < graph.resources().size(); ++index) {
        const ResourceDesc& resource = graph.resources()[index];
        Slot& slot = candidate.m_slots[index];
        slot.kind = resource.kind;
        slot.ownership = resource.ownership;
        if (resource.kind == ResourceKind::Image) {
            slot.imageDesc = std::get<ImageDesc>(resource.description);
            slot.extent = resolveImageExtent(slot.imageDesc, swapchainExtent);
            if (!resource.external()
                && slot.imageDesc.extent.mode
                    == ImageExtentMode::SwapchainRelative) {
                slot.image = allocator.createImage(slot.imageDesc, slot.extent);
                if (!slot.image || slot.image->image() == VK_NULL_HANDLE
                    || slot.image->imageView() == VK_NULL_HANDLE) {
                    throw std::runtime_error(
                        "RenderGraph resize allocator produced an incomplete image: "
                        + resource.name);
                }
                slot.allocationGeneration = allocationGeneration;
            } else if (!resource.external()) {
                slot.reuseFromPrevious = true;
            }
        } else {
            slot.bufferDesc = std::get<BufferDesc>(resource.description);
            if (!resource.external()) {
                slot.reuseFromPrevious = true;
            }
        }
    }
    return candidate;
}

void RenderGraphResourcePool::adoptAbsoluteAllocationsFrom(
    RenderGraphResourcePool& previous) noexcept
{
    if (previous.m_graphGeneration != m_graphGeneration
        || previous.m_slots.size() != m_slots.size()) {
        return;
    }
    for (size_t index = 0; index < m_slots.size(); ++index) {
        Slot& destination = m_slots[index];
        Slot& source = previous.m_slots[index];
        if (!destination.reuseFromPrevious
            || destination.kind != source.kind
            || destination.ownership != source.ownership) {
            continue;
        }
        destination.image = std::move(source.image);
        destination.buffer = std::move(source.buffer);
        destination.allocationGeneration = source.allocationGeneration;
        destination.physicalState = source.physicalState;
        destination.contentsValid = false;
        destination.reuseFromPrevious = false;
    }
}

void RenderGraphResourcePool::swap(RenderGraphResourcePool& other) noexcept
{
    using std::swap;
    swap(m_graphGeneration, other.m_graphGeneration);
    swap(m_swapchainExtent, other.m_swapchainExtent);
    swap(m_slots, other.m_slots);
}

void RenderGraphResourcePool::invalidateTransientContents() noexcept
{
    for (Slot& slot : m_slots) {
        if (slot.ownership == ResourceOwnership::Internal) {
            slot.contentsValid = false;
        }
    }
}

ResolvedImage RenderGraphResourcePool::resolveImage(ImageHandle handle) const
{
    const Slot& slot = checkedSlot(resourceHandle(handle));
    if (slot.ownership != ResourceOwnership::Internal || !slot.image) {
        throw std::runtime_error(
            "RenderGraphResourcePool cannot resolve an external image");
    }
    return ResolvedImage{
        handle,
        slot.image->image(),
        slot.image->imageView(),
        slot.extent,
        slot.imageDesc.format,
        slot.imageDesc.usage,
        slot.imageDesc.aspect,
        slot.physicalState,
        slot.physicalState.layout,
        slot.imageDesc.clearValue,
        slot.allocationGeneration,
        slot.contentsValid,
        false,
    };
}

ResolvedBuffer RenderGraphResourcePool::resolveBuffer(BufferHandle handle) const
{
    const Slot& slot = checkedSlot(resourceHandle(handle));
    if (slot.ownership != ResourceOwnership::Internal || !slot.buffer) {
        throw std::runtime_error(
            "RenderGraphResourcePool cannot resolve an external buffer");
    }
    return ResolvedBuffer{
        handle,
        slot.buffer->buffer(),
        slot.bufferDesc.size,
        slot.bufferDesc.usage,
        slot.physicalState,
        slot.allocationGeneration,
        slot.contentsValid,
        false,
    };
}

void RenderGraphResourcePool::setImageState(
    ImageHandle handle,
    ResourceState2 state,
    bool contentsValid)
{
    Slot& slot = checkedSlot(resourceHandle(handle));
    if (slot.ownership != ResourceOwnership::Internal || !slot.image) {
        throw std::runtime_error("RenderGraph internal image state is unavailable");
    }
    slot.physicalState = state;
    slot.contentsValid = contentsValid;
}

void RenderGraphResourcePool::setBufferState(
    BufferHandle handle,
    ResourceState2 state,
    bool contentsValid)
{
    Slot& slot = checkedSlot(resourceHandle(handle));
    if (slot.ownership != ResourceOwnership::Internal || !slot.buffer) {
        throw std::runtime_error("RenderGraph internal buffer state is unavailable");
    }
    slot.physicalState = state;
    slot.contentsValid = contentsValid;
}

const RenderGraphResourcePool::Slot& RenderGraphResourcePool::checkedSlot(
    ResourceHandle handle) const
{
    if (!handle.isValid() || handle.graphGeneration != m_graphGeneration) {
        throw std::invalid_argument(
            "RenderGraphResourcePool received an invalid or stale handle");
    }
    if (handle.index >= m_slots.size()) {
        throw std::out_of_range("RenderGraphResourcePool handle index out of range");
    }
    if (m_slots[handle.index].kind != handle.kind) {
        throw std::invalid_argument("RenderGraphResourcePool handle has wrong kind");
    }
    return m_slots[handle.index];
}

RenderGraphResourcePool::Slot& RenderGraphResourcePool::checkedSlot(
    ResourceHandle handle)
{
    return const_cast<Slot&>(
        static_cast<const RenderGraphResourcePool&>(*this).checkedSlot(handle));
}

} // namespace ku
