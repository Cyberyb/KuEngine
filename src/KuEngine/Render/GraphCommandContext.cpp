#include "GraphCommandContext.h"

#include <KuEngine/RHI/CommandList.h>
#include <KuEngine/RHI/RHIDevice.h>
#include <KuEngine/RHI/RHIPipeline.h>
#include <KuEngine/RHI/RHIParameterSet.h>

#include <stdexcept>

namespace ku {
struct NativeCommandLifetime {
    bool active = true;
};

namespace {

template<typename T>
[[nodiscard]] bool containsFlags(T available, T requested) noexcept
{
    const auto availableBits = static_cast<uint32_t>(available);
    const auto requestedBits = static_cast<uint32_t>(requested);
    return (availableBits & requestedBits) == requestedBits;
}

constexpr uint32_t kKnownNativeCapabilityBits = static_cast<uint32_t>(
    NativeCommandCapability::Transfer
    | NativeCommandCapability::Compute
    | NativeCommandCapability::Graphics);

constexpr uint32_t kKnownNativeSideEffectBits = static_cast<uint32_t>(
    NativeCommandSideEffect::ReadsDeclaredResources
    | NativeCommandSideEffect::WritesDeclaredResources);

[[nodiscard]] bool imageUseReads(ImageUse use) noexcept
{
    return use == ImageUse::FragmentSampled
        || use == ImageUse::FragmentStorageRead
        || use == ImageUse::ComputeSampled
        || use == ImageUse::ComputeStorageRead
        || use == ImageUse::ComputeStorageReadWrite
        || use == ImageUse::TransferSource
        || use == ImageUse::ColorAttachment
        || use == ImageUse::DepthAttachment;
}

[[nodiscard]] bool imageUseWrites(ImageUse use) noexcept
{
    return use == ImageUse::FragmentStorageWrite
        || use == ImageUse::ComputeStorageWrite
        || use == ImageUse::ComputeStorageReadWrite
        || use == ImageUse::TransferDestination
        || use == ImageUse::ColorAttachment
        || use == ImageUse::DepthAttachment;
}

[[nodiscard]] bool bufferUseReads(BufferUse use) noexcept
{
    return use == BufferUse::Vertex || use == BufferUse::Index
        || use == BufferUse::VertexUniform || use == BufferUse::FragmentUniform
        || use == BufferUse::Indirect || use == BufferUse::FragmentStorageRead
        || use == BufferUse::ComputeUniform || use == BufferUse::ComputeStorageRead
        || use == BufferUse::ComputeStorageReadWrite
        || use == BufferUse::TransferSource;
}

[[nodiscard]] bool bufferUseWrites(BufferUse use) noexcept
{
    return use == BufferUse::FragmentStorageWrite
        || use == BufferUse::ComputeStorageWrite
        || use == BufferUse::ComputeStorageReadWrite
        || use == BufferUse::TransferDestination;
}

void validateSideEffects(
    NativeCommandContract contract,
    bool reads,
    bool writes)
{
    if (reads && !containsFlags(
            contract.sideEffects,
            NativeCommandSideEffect::ReadsDeclaredResources)) {
        throw std::runtime_error(
            "Native command scope did not declare resource-read side effects");
    }
    if (writes && !containsFlags(
            contract.sideEffects,
            NativeCommandSideEffect::WritesDeclaredResources)) {
        throw std::runtime_error(
            "Native command scope did not declare resource-write side effects");
    }
}

} // namespace

void validateNativeCommandContractDeclaration(NativeCommandContract contract)
{
    const uint32_t capabilityBits =
        static_cast<uint32_t>(contract.capabilities);
    const uint32_t sideEffectBits =
        static_cast<uint32_t>(contract.sideEffects);
    if ((capabilityBits & ~kKnownNativeCapabilityBits) != 0) {
        throw std::invalid_argument(
            "Native command contract contains unknown capability bits");
    }
    if ((sideEffectBits & ~kKnownNativeSideEffectBits) != 0) {
        throw std::invalid_argument(
            "Native command contract contains unknown side-effect bits");
    }
}

void validateNativeCommandContract(
    NativeCommandContract allowed,
    NativeCommandContract requested,
    bool computeSupported)
{
    validateNativeCommandContractDeclaration(allowed);
    validateNativeCommandContractDeclaration(requested);
    if (!containsFlags(allowed.capabilities, requested.capabilities)
        || !containsFlags(allowed.sideEffects, requested.sideEffects)) {
        throw std::invalid_argument(
            "Native command request exceeds the callback pass contract");
    }
    if (containsFlags(requested.capabilities, NativeCommandCapability::Compute)
        && !computeSupported) {
        throw std::runtime_error(
            "Native compute commands are unavailable on the graphics queue");
    }
    if (requested.capabilities == NativeCommandCapability::None) {
        throw std::invalid_argument(
            "Native command request must name at least one capability");
    }
}

NativeCommandScope::NativeCommandScope(
    CommandList& commands,
    RenderGraphResourceResolver& resources,
    const RHIDevice& device,
    NativeCommandContract contract,
    std::shared_ptr<const NativeCommandLifetime> lifetime)
    : m_commands(&commands)
    , m_resources(&resources)
    , m_device(&device)
    , m_contract(contract)
    , m_lifetime(std::move(lifetime))
{
}

void NativeCommandScope::ensureActive() const
{
    if (!m_lifetime || !m_lifetime->active) {
        throw std::runtime_error(
            "Native command scope escaped its callback execution lifetime");
    }
}

VkCommandBuffer NativeCommandScope::commandBuffer() const
{
    ensureActive();
    return m_commands->cmd();
}

ResolvedImage NativeCommandScope::resolveImage(
    ImageHandle handle,
    ImageUse use,
    ImageSubresourceRange range)
{
    ensureActive();
    validateSideEffects(m_contract, imageUseReads(use), imageUseWrites(use));
    return m_resources->resolveImage(handle, use, range);
}

ResolvedBuffer NativeCommandScope::resolveBuffer(
    BufferHandle handle,
    BufferUse use,
    BufferRange range)
{
    ensureActive();
    validateSideEffects(m_contract, bufferUseReads(use), bufferUseWrites(use));
    return m_resources->resolveBuffer(handle, use, range);
}

void NativeCommandScope::dispatch(uint32_t x, uint32_t y, uint32_t z) const
{
    ensureActive();
    if (!containsFlags(m_contract.capabilities, NativeCommandCapability::Compute)) {
        throw std::runtime_error(
            "Native command scope does not allow compute dispatch");
    }
    validateDispatchCount(x, y, z, m_device->properties().limits);
    vkCmdDispatch(commandBuffer(), x, y, z);
}

GraphCommandContext::GraphCommandContext(
    CommandList& commands,
    RenderGraphResourceResolver& resources,
    const RHIDevice& device,
    NativeCommandContract allowedNative)
    : m_commands(&commands)
    , m_resources(&resources)
    , m_device(&device)
    , m_allowedNative(allowedNative)
    , m_lifetime(std::make_shared<NativeCommandLifetime>())
    , m_insideRenderingScope(resources.insideRenderingScope())
{
}

GraphCommandContext::~GraphCommandContext()
{
    m_lifetime->active = false;
}

ResolvedImage GraphCommandContext::resolveImage(
    ImageHandle handle,
    ImageUse use,
    ImageSubresourceRange range)
{
    return m_resources->resolveImage(handle, use, range);
}

ResolvedBuffer GraphCommandContext::resolveBuffer(
    BufferHandle handle,
    BufferUse use,
    BufferRange range)
{
    return m_resources->resolveBuffer(handle, use, range);
}

void GraphCommandContext::copyBuffer(
    BufferHandle source,
    BufferHandle destination,
    VkDeviceSize sourceOffset,
    VkDeviceSize destinationOffset,
    VkDeviceSize size)
{
    if (m_insideRenderingScope) {
        throw std::runtime_error(
            "Buffer copy cannot execute inside dynamic rendering");
    }
    const ResolvedBuffer resolvedSource = resolveBuffer(
        source, BufferUse::TransferSource, {sourceOffset, size});
    const ResolvedBuffer resolvedDestination = resolveBuffer(
        destination, BufferUse::TransferDestination, {destinationOffset, size});
    m_commands->copyBuffer(
        resolvedSource.buffer, resolvedDestination.buffer,
        sourceOffset, destinationOffset, size);
}

void GraphCommandContext::fillBuffer(
    BufferHandle destination,
    VkDeviceSize offset,
    VkDeviceSize size,
    uint32_t data)
{
    if (m_insideRenderingScope) {
        throw std::runtime_error(
            "Buffer fill cannot execute inside dynamic rendering");
    }
    const ResolvedBuffer resolved = resolveBuffer(
        destination, BufferUse::TransferDestination, {offset, size});
    m_commands->fillBuffer(resolved.buffer, offset, size, data);
}

void GraphCommandContext::bindComputePipeline(
    const RHIComputePipeline& pipeline)
{
    if (m_insideRenderingScope) {
        throw std::runtime_error(
            "Compute pipeline cannot bind inside dynamic rendering");
    }
    if (!m_device->graphicsQueueSupportsCompute()) {
        throw std::runtime_error(
            "Compute commands are unavailable on the graphics queue");
    }
    if (pipeline.pipeline() == VK_NULL_HANDLE) {
        throw std::invalid_argument("Cannot bind an invalid compute pipeline");
    }
    pipeline.bind(m_commands->cmd());
    m_boundComputePipeline = &pipeline;
}

void GraphCommandContext::bindParameterSet(
    const RHIComputePipeline& pipeline,
    uint32_t setIndex,
    const RHIParameterSet& parameters)
{
    if (m_boundComputePipeline != &pipeline) {
        throw std::runtime_error(
            "Compute parameter set requires its pipeline to be bound first");
    }
    validateComputeParameterSetBinding(
        pipeline.descriptorSetLayouts(), setIndex,
        parameters.layout(), parameters.set());
    const VkDescriptorSet set = parameters.set();
    vkCmdBindDescriptorSets(
        m_commands->cmd(), VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline.layout(), setIndex, 1, &set, 0, nullptr);
}

void GraphCommandContext::dispatch(uint32_t x, uint32_t y, uint32_t z)
{
    if (m_boundComputePipeline == nullptr) {
        throw std::runtime_error(
            "Compute dispatch requires a bound compute pipeline");
    }
    validateDispatchCount(x, y, z, m_device->properties().limits);
    vkCmdDispatch(m_commands->cmd(), x, y, z);
}

NativeCommandScope GraphCommandContext::nativeCommands(
    NativeCommandContract requested)
{
    validateNativeCommandContract(
        m_allowedNative, requested, m_device->graphicsQueueSupportsCompute());
    const auto outsideOnly = static_cast<uint32_t>(
        NativeCommandCapability::Transfer | NativeCommandCapability::Compute);
    if (m_insideRenderingScope
        && (static_cast<uint32_t>(requested.capabilities) & outsideOnly) != 0) {
        throw std::runtime_error(
            "Native transfer/compute commands cannot execute inside dynamic rendering");
    }
    return NativeCommandScope(
        *m_commands, *m_resources, *m_device, requested, m_lifetime);
}

} // namespace ku
