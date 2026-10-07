#pragma once

#include "RenderGraphResources.h"

#include <cstdint>
#include <memory>

namespace ku {

class CommandList;
class RHIDevice;
class RHIComputePipeline;
class RHIParameterSet;
struct NativeCommandLifetime;
template<typename Parameters>
class CallbackRenderPass;

enum class NativeCommandCapability : uint32_t {
    None = 0,
    Transfer = 1u << 0,
    Compute = 1u << 1,
    Graphics = 1u << 2,
};

enum class NativeCommandSideEffect : uint32_t {
    None = 0,
    ReadsDeclaredResources = 1u << 0,
    WritesDeclaredResources = 1u << 1,
};

[[nodiscard]] constexpr NativeCommandCapability operator|(
    NativeCommandCapability lhs,
    NativeCommandCapability rhs) noexcept
{
    return static_cast<NativeCommandCapability>(
        static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

[[nodiscard]] constexpr NativeCommandSideEffect operator|(
    NativeCommandSideEffect lhs,
    NativeCommandSideEffect rhs) noexcept
{
    return static_cast<NativeCommandSideEffect>(
        static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

struct NativeCommandContract {
    NativeCommandCapability capabilities = NativeCommandCapability::None;
    NativeCommandSideEffect sideEffects = NativeCommandSideEffect::None;
};

void validateNativeCommandContractDeclaration(NativeCommandContract contract);

void validateNativeCommandContract(
    NativeCommandContract allowed,
    NativeCommandContract requested,
    bool computeSupported);

class NativeCommandScope {
public:
    NativeCommandScope(const NativeCommandScope&) = delete;
    NativeCommandScope& operator=(const NativeCommandScope&) = delete;
    NativeCommandScope(NativeCommandScope&&) noexcept = default;
    NativeCommandScope& operator=(NativeCommandScope&&) noexcept = default;

    [[nodiscard]] VkCommandBuffer commandBuffer() const;
    [[nodiscard]] ResolvedImage resolveImage(
        ImageHandle handle,
        ImageUse use,
        ImageSubresourceRange range = {});
    [[nodiscard]] ResolvedBuffer resolveBuffer(
        BufferHandle handle,
        BufferUse use,
        BufferRange range = {});
    void dispatch(uint32_t x, uint32_t y = 1, uint32_t z = 1) const;

private:
    friend class GraphCommandContext;
    NativeCommandScope(
        CommandList& commands,
        RenderGraphResourceResolver& resources,
        const RHIDevice& device,
        NativeCommandContract contract,
        std::shared_ptr<const NativeCommandLifetime> lifetime);
    void ensureActive() const;

    CommandList* m_commands = nullptr;
    RenderGraphResourceResolver* m_resources = nullptr;
    const RHIDevice* m_device = nullptr;
    NativeCommandContract m_contract{};
    std::shared_ptr<const NativeCommandLifetime> m_lifetime;
};

class GraphCommandContext {
public:
    ~GraphCommandContext();

    GraphCommandContext(const GraphCommandContext&) = delete;
    GraphCommandContext& operator=(const GraphCommandContext&) = delete;

    [[nodiscard]] ResolvedImage resolveImage(
        ImageHandle handle,
        ImageUse use,
        ImageSubresourceRange range = {});
    [[nodiscard]] ResolvedBuffer resolveBuffer(
        BufferHandle handle,
        BufferUse use,
        BufferRange range = {});
    void copyBuffer(
        BufferHandle source,
        BufferHandle destination,
        VkDeviceSize sourceOffset,
        VkDeviceSize destinationOffset,
        VkDeviceSize size);
    void fillBuffer(
        BufferHandle destination,
        VkDeviceSize offset,
        VkDeviceSize size,
        uint32_t data);
    void bindComputePipeline(const RHIComputePipeline& pipeline);
    void bindParameterSet(
        const RHIComputePipeline& pipeline,
        uint32_t setIndex,
        const RHIParameterSet& parameters);
    void dispatch(uint32_t x, uint32_t y = 1, uint32_t z = 1);
    [[nodiscard]] NativeCommandScope nativeCommands(
        NativeCommandContract requested);

private:
    template<typename Parameters>
    friend class CallbackRenderPass;
    GraphCommandContext(
        CommandList& commands,
        RenderGraphResourceResolver& resources,
        const RHIDevice& device,
        NativeCommandContract allowedNative);
    CommandList* m_commands = nullptr;
    RenderGraphResourceResolver* m_resources = nullptr;
    const RHIDevice* m_device = nullptr;
    NativeCommandContract m_allowedNative{};
    std::shared_ptr<NativeCommandLifetime> m_lifetime;
    const RHIComputePipeline* m_boundComputePipeline = nullptr;
    bool m_insideRenderingScope = false;
};

} // namespace ku
