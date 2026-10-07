// KuEngine render graph: typed logical resources, validation, dependencies, and execution order.
#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace ku {

class RenderPass;

enum class ResourceHazardType { ReadAfterWrite, WriteAfterRead, WriteAfterWrite };
enum class ResourceKind { Image, Buffer };
enum class ResourceOwnership { Internal, Imported };
enum class InitialContent { Undefined, Cleared, Preserved };
enum class ImageExtentMode { Absolute, SwapchainRelative };
enum class AttachmentType { Color, Depth };
enum class AttachmentLoadPolicy { RuntimeDefault, Load, Clear, DontCare };
enum class AttachmentStorePolicy { RuntimeDefault, Store, DontCare };

struct ResourceState2 {
    VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_NONE;
    VkAccessFlags2 access = VK_ACCESS_2_NONE;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

    auto operator<=>(const ResourceState2&) const = default;
};

struct ImageSubresourceRange {
    VkImageAspectFlags aspect = 0;
    uint32_t baseMipLevel = 0;
    uint32_t levelCount = VK_REMAINING_MIP_LEVELS;
    uint32_t baseArrayLayer = 0;
    uint32_t layerCount = VK_REMAINING_ARRAY_LAYERS;

    auto operator<=>(const ImageSubresourceRange&) const = default;
};

struct BufferRange {
    VkDeviceSize offset = 0;
    VkDeviceSize size = VK_WHOLE_SIZE;

    auto operator<=>(const BufferRange&) const = default;
};

enum class ImageUse {
    ColorAttachment,
    DepthAttachment,
    FragmentSampled,
    FragmentStorageRead,
    FragmentStorageWrite,
    ComputeSampled,
    ComputeStorageRead,
    ComputeStorageWrite,
    ComputeStorageReadWrite,
    TransferSource,
    TransferDestination,
};

enum class BufferUse {
    Vertex,
    Index,
    VertexUniform,
    FragmentUniform,
    Indirect,
    FragmentStorageRead,
    FragmentStorageWrite,
    ComputeUniform,
    ComputeStorageRead,
    ComputeStorageWrite,
    ComputeStorageReadWrite,
    TransferSource,
    TransferDestination,
};

struct ImageExtentDesc {
    ImageExtentMode mode = ImageExtentMode::Absolute;
    VkExtent2D absolute{0, 0};

    [[nodiscard]] static ImageExtentDesc absoluteExtent(
        uint32_t width,
        uint32_t height) noexcept
    {
        return ImageExtentDesc{ImageExtentMode::Absolute, {width, height}};
    }

    [[nodiscard]] static ImageExtentDesc swapchainRelative() noexcept
    {
        return ImageExtentDesc{ImageExtentMode::SwapchainRelative, {0, 0}};
    }

    [[nodiscard]] bool operator==(const ImageExtentDesc& other) const noexcept
    {
        return mode == other.mode
            && absolute.width == other.absolute.width
            && absolute.height == other.absolute.height;
    }
};

struct ImageDesc {
    ImageExtentDesc extent{};
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageUsageFlags usage = 0;
    VkImageAspectFlags aspect = 0;
    uint32_t mipLevels = 1;
    uint32_t arrayLayers = 1;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkClearValue clearValue{};
    InitialContent initialContent = InitialContent::Undefined;
};

[[nodiscard]] bool operator==(const ImageDesc& lhs, const ImageDesc& rhs) noexcept;

struct BufferDesc {
    VkDeviceSize size = 0;
    VkBufferUsageFlags usage = 0;
    VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO;
    InitialContent initialContent = InitialContent::Undefined;

    auto operator<=>(const BufferDesc&) const = default;
};

struct ImageHandle {
    static constexpr uint32_t kInvalidIndex = UINT32_MAX;
    uint32_t index = kInvalidIndex;
    uint64_t graphGeneration = 0;

    [[nodiscard]] bool isValid() const noexcept
    {
        return index != kInvalidIndex && graphGeneration != 0;
    }
    auto operator<=>(const ImageHandle&) const = default;
};

struct BufferHandle {
    static constexpr uint32_t kInvalidIndex = UINT32_MAX;
    uint32_t index = kInvalidIndex;
    uint64_t graphGeneration = 0;

    [[nodiscard]] bool isValid() const noexcept
    {
        return index != kInvalidIndex && graphGeneration != 0;
    }
    auto operator<=>(const BufferHandle&) const = default;
};

struct ResourceHandle {
    uint32_t index = UINT32_MAX;
    uint64_t graphGeneration = 0;
    ResourceKind kind = ResourceKind::Image;

    [[nodiscard]] bool isValid() const noexcept
    {
        return index != UINT32_MAX && graphGeneration != 0;
    }
    auto operator<=>(const ResourceHandle&) const = default;
};

[[nodiscard]] ResourceHandle resourceHandle(ImageHandle handle) noexcept;
[[nodiscard]] ResourceHandle resourceHandle(BufferHandle handle) noexcept;

struct ResourceDesc {
    std::string name;
    ResourceKind kind = ResourceKind::Image;
    ResourceOwnership ownership = ResourceOwnership::Internal;
    std::variant<ImageDesc, BufferDesc> description{ImageDesc{}};
    bool exported = false;

    [[nodiscard]] bool external() const noexcept
    {
        return ownership == ResourceOwnership::Imported;
    }
};

struct CompiledResourceUse {
    ResourceHandle resource;
    ResourceState2 state{};
    ImageSubresourceRange imageRange{};
    BufferRange bufferRange{};
    ImageUse imageUse = ImageUse::FragmentSampled;
    BufferUse bufferUse = BufferUse::FragmentStorageRead;
    bool reads = false;
    bool writes = false;
    bool attachment = false;
    size_t attachmentIndex = SIZE_MAX;
};

struct PassAttachment {
    ImageHandle resource;
    AttachmentType type = AttachmentType::Color;
    AttachmentLoadPolicy loadPolicy = AttachmentLoadPolicy::RuntimeDefault;
    AttachmentStorePolicy storePolicy = AttachmentStorePolicy::RuntimeDefault;
};

struct PassNode {
    std::string name;
    RenderPass* pass = nullptr;
    std::vector<CompiledResourceUse> uses;
    std::vector<PassAttachment> attachments;
    std::vector<std::string> explicitDependencies;
};

struct PassDependencyEdge {
    size_t fromPass = 0;
    size_t toPass = 0;
    bool explicitDependency = false;
};

struct BarrierPlanItem {
    size_t fromPass = 0;
    size_t toPass = 0;
    ResourceHandle resource;
    ResourceHazardType hazard = ResourceHazardType::ReadAfterWrite;
};

class RenderGraph;

class RenderGraphBuilder {
public:
    RenderGraphBuilder() = default;

    [[nodiscard]] ImageHandle createImage(std::string_view name, const ImageDesc& desc);
    [[nodiscard]] ImageHandle importImage(std::string_view name, const ImageDesc& desc);
    [[nodiscard]] BufferHandle createBuffer(std::string_view name, const BufferDesc& desc);
    [[nodiscard]] BufferHandle importBuffer(std::string_view name, const BufferDesc& desc);

    void useImage(
        ImageHandle resource,
        ImageUse use,
        ImageSubresourceRange range = {});
    void useBuffer(
        BufferHandle resource,
        BufferUse use,
        BufferRange range = {});
    void colorAttachment(
        ImageHandle resource,
        AttachmentLoadPolicy loadPolicy = AttachmentLoadPolicy::RuntimeDefault,
        AttachmentStorePolicy storePolicy = AttachmentStorePolicy::RuntimeDefault);
    void depthAttachment(
        ImageHandle resource,
        AttachmentLoadPolicy loadPolicy = AttachmentLoadPolicy::RuntimeDefault,
        AttachmentStorePolicy storePolicy = AttachmentStorePolicy::RuntimeDefault);
    ImageHandle exportImage(ImageHandle resource);
    BufferHandle exportBuffer(BufferHandle resource);
    void dependsOn(std::string_view passName);

private:
    friend class RenderGraph;
    RenderGraphBuilder(RenderGraph& graph, size_t passIndex);

    RenderGraph* m_graph = nullptr;
    size_t m_passIndex = 0;
};

class RenderGraph {
public:
    RenderGraph();
    void reset();

    [[nodiscard]] size_t registerPass(RenderPass& pass);
    [[nodiscard]] RenderGraphBuilder buildPass(size_t passIndex);
    void compile();

    [[nodiscard]] uint64_t generation() const noexcept { return m_generation; }
    [[nodiscard]] const std::vector<PassNode>& passes() const { return m_passes; }
    [[nodiscard]] const std::vector<ResourceDesc>& resources() const { return m_resources; }
    [[nodiscard]] const std::vector<size_t>& executionOrder() const { return m_executionOrder; }
    [[nodiscard]] const std::vector<PassDependencyEdge>& dependencies() const { return m_dependencies; }
    [[nodiscard]] const std::vector<BarrierPlanItem>& barrierPlan() const { return m_barrierPlan; }
    [[nodiscard]] std::optional<ImageHandle> findImage(std::string_view name) const;
    [[nodiscard]] std::optional<BufferHandle> findBuffer(std::string_view name) const;
    [[nodiscard]] const ResourceDesc& resource(ResourceHandle handle) const;
    [[nodiscard]] const ResourceDesc& resource(ImageHandle handle) const;
    [[nodiscard]] const ResourceDesc& resource(BufferHandle handle) const;
    [[nodiscard]] static ImageSubresourceRange normalizeImageRange(
        const ImageDesc& desc,
        ImageSubresourceRange range);
    [[nodiscard]] static BufferRange normalizeBufferRange(
        const BufferDesc& desc,
        BufferRange range);
    [[nodiscard]] static bool overlaps(
        const ImageSubresourceRange& lhs,
        const ImageSubresourceRange& rhs) noexcept;
    [[nodiscard]] static bool overlaps(
        const BufferRange& lhs,
        const BufferRange& rhs) noexcept;

private:
    friend class RenderGraphBuilder;

    ImageHandle createOrGetImage(
        std::string_view name,
        ResourceOwnership ownership,
        const ImageDesc& desc);
    BufferHandle createOrGetBuffer(
        std::string_view name,
        ResourceOwnership ownership,
        const BufferDesc& desc);
    void addImageUse(
        size_t passIndex,
        ImageHandle resource,
        ImageUse use,
        ImageSubresourceRange range);
    void addBufferUse(
        size_t passIndex,
        BufferHandle resource,
        BufferUse use,
        BufferRange range);
    void addAttachment(
        size_t passIndex,
        ImageHandle resource,
        AttachmentType type,
        AttachmentLoadPolicy loadPolicy,
        AttachmentStorePolicy storePolicy);
    void markExported(ResourceHandle resource);
    void addExplicitDependency(size_t passIndex, std::string_view passName);
    void validateHandle(ResourceHandle handle, std::string_view operation) const;
    void validateResourcesAndUses() const;

    uint64_t m_generation = 0;
    std::vector<PassNode> m_passes;
    std::vector<ResourceDesc> m_resources;
    std::vector<size_t> m_executionOrder;
    std::vector<PassDependencyEdge> m_dependencies;
    std::vector<BarrierPlanItem> m_barrierPlan;
};

} // namespace ku
