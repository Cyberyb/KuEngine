#pragma once

#include "RenderGraph.h"

#include <memory>
#include <string>

namespace ku {

struct ResolvedImage {
    ImageHandle handle{};
    VkImage image = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkExtent2D extent{0, 0};
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageUsageFlags usage = 0;
    VkImageAspectFlags aspect = 0;
    ResourceState2 state{};
    VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkClearValue clearValue{};
    uint64_t allocationGeneration = 0;
    bool contentsValid = false;
    bool external = false;

    [[nodiscard]] bool complete() const noexcept
    {
        return handle.isValid() && image != VK_NULL_HANDLE
            && imageView != VK_NULL_HANDLE
            && extent.width != 0 && extent.height != 0;
    }
};

struct ResolvedBuffer {
    BufferHandle handle{};
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    VkBufferUsageFlags usage = 0;
    ResourceState2 state{};
    uint64_t allocationGeneration = 0;
    bool contentsValid = false;
    bool external = false;

    [[nodiscard]] bool complete() const noexcept
    {
        return handle.isValid() && buffer != VK_NULL_HANDLE && size != 0;
    }
};

struct ExportLifetimeState {
    bool alive = true;
    uint64_t epoch = 1;
    uint64_t frameSerial = 0;
};

struct ExportedImage {
    ResolvedImage view{};
    uint64_t epoch = 0;
    uint64_t frameSerial = 0;
    std::weak_ptr<const ExportLifetimeState> lifetime;

    [[nodiscard]] bool valid(std::string* error = nullptr) const
    {
        const auto state = lifetime.lock();
        if (!state) {
            if (error) *error = "export owner no longer exists";
            return false;
        }
        if (!state->alive) {
            if (error) *error = "export owner was destroyed";
            return false;
        }
        if (state->epoch != epoch) {
            if (error) *error = "export is stale after resize or recompile";
            return false;
        }
        if (state->frameSerial != frameSerial) {
            if (error) *error = "export is stale after the next frame began";
            return false;
        }
        if (!view.complete() || !view.contentsValid) {
            if (error) *error = "export does not contain a complete stored image";
            return false;
        }
        return true;
    }
};

struct ExportedBuffer {
    ResolvedBuffer view{};
    uint64_t epoch = 0;
    uint64_t frameSerial = 0;
    std::weak_ptr<const ExportLifetimeState> lifetime;

    [[nodiscard]] bool valid(std::string* error = nullptr) const
    {
        const auto state = lifetime.lock();
        if (!state) {
            if (error) *error = "export owner no longer exists";
            return false;
        }
        if (!state->alive) {
            if (error) *error = "export owner was destroyed";
            return false;
        }
        if (state->epoch != epoch) {
            if (error) *error = "export is stale after resize or recompile";
            return false;
        }
        if (state->frameSerial != frameSerial) {
            if (error) *error = "export is stale after the next frame began";
            return false;
        }
        if (!view.complete() || !view.contentsValid) {
            if (error) *error = "export does not contain a complete stored buffer";
            return false;
        }
        return true;
    }
};

[[nodiscard]] inline bool passDeclaresUse(
    const PassNode& pass,
    ResourceHandle resource,
    ImageUse use,
    ImageSubresourceRange requested) noexcept
{
    for (const CompiledResourceUse& declared : pass.uses) {
        if (declared.resource == resource
            && declared.resource.kind == ResourceKind::Image
            && declared.imageUse == use
            && (requested.aspect & ~declared.imageRange.aspect) == 0
            && requested.baseMipLevel >= declared.imageRange.baseMipLevel
            && uint64_t(requested.baseMipLevel) + requested.levelCount
                <= uint64_t(declared.imageRange.baseMipLevel)
                    + declared.imageRange.levelCount
            && requested.baseArrayLayer >= declared.imageRange.baseArrayLayer
            && uint64_t(requested.baseArrayLayer) + requested.layerCount
                <= uint64_t(declared.imageRange.baseArrayLayer)
                    + declared.imageRange.layerCount) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] inline bool passDeclaresUse(
    const PassNode& pass,
    ResourceHandle resource,
    BufferUse use,
    BufferRange requested) noexcept
{
    for (const CompiledResourceUse& declared : pass.uses) {
        if (declared.resource == resource
            && declared.resource.kind == ResourceKind::Buffer
            && declared.bufferUse == use
            && requested.offset >= declared.bufferRange.offset
            && requested.size <= declared.bufferRange.size
            && requested.offset - declared.bufferRange.offset
                <= declared.bufferRange.size - requested.size) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] inline bool passDeclaresUse(
    const PassNode& pass,
    ResourceHandle resource,
    ImageUse use) noexcept
{
    for (const CompiledResourceUse& declared : pass.uses) {
        if (declared.resource == resource
            && declared.resource.kind == ResourceKind::Image
            && declared.imageUse == use) return true;
    }
    return false;
}

[[nodiscard]] inline bool passDeclaresUse(
    const PassNode& pass,
    ResourceHandle resource,
    BufferUse use) noexcept
{
    for (const CompiledResourceUse& declared : pass.uses) {
        if (declared.resource == resource
            && declared.resource.kind == ResourceKind::Buffer
            && declared.bufferUse == use) return true;
    }
    return false;
}

class RenderGraphResourceResolver {
public:
    virtual ~RenderGraphResourceResolver() = default;
    [[nodiscard]] virtual bool insideRenderingScope() const noexcept = 0;
    [[nodiscard]] virtual ResolvedImage resolveImage(
        ImageHandle handle,
        ImageUse declaredUse,
        ImageSubresourceRange range = {}) = 0;
    [[nodiscard]] virtual ResolvedBuffer resolveBuffer(
        BufferHandle handle,
        BufferUse declaredUse,
        BufferRange range = {}) = 0;
};

} // namespace ku
