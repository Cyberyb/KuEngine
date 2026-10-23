#include "DeferredReadbackPass.h"
#include <KuEngine/RHI/CommandList.h>
#include <KuEngine/RHI/RHIBuffer.h>
#include <KuEngine/RHI/RHIDevice.h>
#include <KuEngine/Core/Log.h>
#include <KuEngine/Render/RenderGraphResources.h>
#include <KuEngine/Render/ForwardGraphTargets.h>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace ku {
namespace {
constexpr std::string_view stagingName = "DeferredReadbackStaging";
// Fixed logical size means resize need not recompile buffer descriptor; only
// source relative image allocations change. The staging capacity is bounded.
constexpr VkDeviceSize stagingSize = VkDeviceSize(2048) * 2048 * 56;
ImageDesc probeDesc(bool srgb) {
    ImageDesc result{};
    result.extent = ImageExtentDesc::swapchainRelative();
    result.format = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    result.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    result.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    result.initialContent = InitialContent::Cleared;
    return result;
}
float halfFloat(uint16_t bits) {
    const int exponent = (bits >> 10) & 31;
    const int fraction = bits & 1023;
    const float sign = bits & 32768 ? -1.0f : 1.0f;
    if (exponent == 31) return fraction ? NAN : sign * INFINITY;
    return sign * (exponent ? std::ldexp(1.0f + float(fraction) / 1024.0f, exponent - 15)
        : std::ldexp(float(fraction), -24));
}
}
void HdrDisplayProbePass::initialize(const RenderContext& context) {
    m_scene = makeForwardGraphTargets(context).color;
    m_output = probeDesc(m_srgb);
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(context.device.physicalDevice(), m_output.format, &properties);
    constexpr auto required = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
    if ((properties.optimalTilingFeatures & required) != required) throw std::runtime_error("HDR display probe format unavailable");
    m_program.initialize(context.device, m_scene.format, m_output.format, context.framesInFlight);
}
void HdrDisplayProbePass::setup(RenderGraphBuilder& b) {
    b.dependsOn(m_source.scenePrepareName());
    m_input = b.createImage(forward_graph_resource::sceneColor, m_scene);
    b.useImage(m_input, ImageUse::FragmentSampled);
    b.colorAttachment(b.createImage(name(),m_output),AttachmentLoadPolicy::Clear,AttachmentStorePolicy::Store);
}
void HdrDisplayProbePass::execute(CommandList& c, const FrameData& frame, RenderGraphResourceResolver& r) {
    const auto& view = m_source.checkedSceneSnapshot(frame).view;
    // Probe covers the entire image, preserving source pixel coordinates/ROI.
    const auto image = r.resolveImage(m_input,ImageUse::FragmentSampled);
    m_program.draw(c,image,calculateForwardDisplayUvTransform(frame.viewerLayout.sceneFramebuffer,image.extent),view.environmentExposure,true,frame.frameIndex);
}
DeferredReadbackPass::DeferredReadbackPass(RenderPipeline& pipeline,
    std::filesystem::path prefix, bool backface, bool deferred, bool semantic)
    : m_pipeline(pipeline), m_prefix(std::move(prefix)), m_backface(backface), m_deferred(deferred), m_semantic(semantic) {}
DeferredReadbackPass::~DeferredReadbackPass() = default;
void DeferredReadbackPass::initialize(const RenderContext& context) {
    m_device = &context.device;
    m_targets = makeGBufferTargets(context.depthFormat, context.clearDepthStencil);
    m_color = makeDeferredSceneColor(context);
    if (context.depthFormat != VK_FORMAT_D32_SFLOAT)
        throw std::runtime_error("HDR fixture depth readback requires D32_SFLOAT");
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(context.device.physicalDevice(), context.depthFormat, &properties);
    if (!(properties.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT)) throw std::runtime_error("HDR fixture depth transfer unavailable");
    RHIBuffer::CreateInfo info{};
    info.size = stagingSize;
    info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.allocationFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    auto candidate = std::make_unique<RHIBuffer>(context.device, info);
    m_staging = std::move(candidate);
    m_pending = false;
    allocate(context.initialExtent.width, context.initialExtent.height);
}
void DeferredReadbackPass::allocate(uint32_t width, uint32_t height) {
    if (uint64_t(width) * height * 56 > stagingSize)
        throw std::runtime_error("Deferred readback exceeds bounded staging capacity");
    m_extent = {width, height};
}
void DeferredReadbackPass::setup(RenderGraphBuilder& builder) {
    for (size_t i = 0; m_deferred && i < 4; ++i) {
        m_images[i] = builder.createImage(gbufferNames[i], m_targets.colors[i]);
        builder.useImage(m_images[i], ImageUse::TransferSource);
    }
    m_images[4] = builder.createImage(deferredSceneColorName, m_color);
    builder.useImage(m_images[4], ImageUse::TransferSource);
    for (size_t i = 5; i < 7; ++i) {
        m_images[i] = builder.createImage(i==5 ? "HdrDisplayUNORM" : "HdrDisplaySRGB", probeDesc(i==6));
        builder.useImage(m_images[i],ImageUse::TransferSource);
    }
    m_images[7] = builder.createImage(forward_graph_resource::sceneDepth,m_targets.depth);
    builder.useImage(m_images[7],ImageUse::TransferSource);
    m_buffer = builder.importBuffer(stagingName, BufferDesc{stagingSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO, InitialContent::Undefined});
    builder.useBuffer(m_buffer, BufferUse::TransferDestination);
    builder.dependsOn("ForwardDisplay");
}
void DeferredReadbackPass::setupDepthCapture(RenderGraphBuilder& b) {
    m_depthBefore = b.createImage(forward_graph_resource::sceneDepth,m_targets.depth);
    b.useImage(m_depthBefore,ImageUse::TransferSource);
    m_beforeBuffer = b.importBuffer(stagingName, BufferDesc{stagingSize,VK_BUFFER_USAGE_TRANSFER_DST_BIT,VMA_MEMORY_USAGE_AUTO,InitialContent::Undefined});
    b.useBuffer(m_beforeBuffer,BufferUse::TransferDestination);
}
void DeferredReadbackPass::captureDepth(CommandList& c, RenderGraphResourceResolver& r) {
    const auto image = r.resolveImage(m_depthBefore,ImageUse::TransferSource);
    const auto buffer = r.resolveBuffer(m_beforeBuffer,BufferUse::TransferDestination);
    VkBufferImageCopy copy{};
    copy.bufferOffset = VkDeviceSize(image.extent.width)*image.extent.height*48;
    copy.imageSubresource = {VK_IMAGE_ASPECT_DEPTH_BIT,0,0,1};
    copy.imageExtent = {image.extent.width,image.extent.height,1};
    vkCmdCopyImageToBuffer(c,image.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffer.buffer,1,&copy);
}
void DeferredReadbackPass::update(const FrameData&) {
    // Bounded readback tool owns one staging allocation: wait before host access.
    if (m_pending) m_device->waitIdle();
    inspect();
    m_pipeline.bindExternalBuffer({.resourceName = stagingName,
        .buffer = m_staging->buffer(), .size = stagingSize, .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .initialState = {VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED},
        .finalState = {VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED},
        .contentsValid = false});
}
void DeferredReadbackPass::onResize(uint32_t width, uint32_t height) {
    inspect(); // Runtime deviceWaitIdle makes pending copies complete here.
    allocate(width, height);
}
void DeferredReadbackPass::execute(CommandList& commands, const FrameData&, RenderGraphResourceResolver& resources) {
    const auto buffer = resources.resolveBuffer(m_buffer, BufferUse::TransferDestination);
    const std::array<size_t,8> offsets{0,8,16,24,32,40,44,52};
    for (size_t i = m_deferred ? 0 : 4; i < 8; ++i) {
        const auto image = resources.resolveImage(m_images[i], ImageUse::TransferSource);
        if (!image.contentsValid || image.extent.width != m_extent.width || image.extent.height != m_extent.height)
            throw std::runtime_error("Deferred readback resolved incorrect extent or contents");
        VkBufferImageCopy copy{};
        copy.bufferOffset = VkDeviceSize(m_extent.width)*m_extent.height*offsets[i];
        copy.imageSubresource = {VkImageAspectFlags(i==7 ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT), 0, 0, 1};
        copy.imageExtent = {m_extent.width, m_extent.height, 1};
        vkCmdCopyImageToBuffer(commands, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer.buffer, 1, &copy);
    }
    m_copiedExtent = m_extent;
    m_pending = true;
}
void DeferredReadbackPass::inspect() {
    if (!m_pending) return;
    m_staging->invalidate();
    const auto* data = static_cast<const uint8_t*>(m_staging->map());
    if (!data) throw std::runtime_error("Deferred readback staging map failed");
    const size_t count = size_t(m_copiedExtent.width) * m_copiedExtent.height;
    const auto* position = reinterpret_cast<const uint16_t*>(data + count * 16);
    const auto* output = data + count * 40;
    if (std::memcmp(data+count*48,data+count*52,count*4)!=0)
        throw std::runtime_error("HDR Blend unexpectedly changed scene depth");
    std::array<uint64_t, 3> pixels{}, sums{}, mask{};
    for (size_t i = 0; m_deferred && i < count; ++i) {
        const float codeValue = halfFloat(position[i * 4 + 3]);
        const int code = static_cast<int>(codeValue);
        if (code < 0 || code > 2 || codeValue != float(code))
            throw std::runtime_error("Deferred readback found invalid ShadingModel encoding");
        ++pixels[code];
        if (code) {
            sums[code] += uint64_t(output[i * 4]) + output[i * 4 + 1] + output[i * 4 + 2];
            const float z = halfFloat(position[i * 4 + 2]);
            const float maskZ = code == 1 ? -0.25f : 0.25f;
            if (std::abs(z - maskZ) < 0.002f) ++mask[code];
        }
    }
    if (m_deferred && m_semantic && (!pixels[0] || !pixels[1] || !pixels[2]
        || (!m_backface && (!mask[1] || !mask[2]))))
        throw std::runtime_error("Deferred readback missing Empty/PBR/Unlit/Mask geometry");
    if (!m_prefix.parent_path().empty()) std::filesystem::create_directories(m_prefix.parent_path());
    const auto base = m_prefix.string() + "_" + std::to_string(m_copiedExtent.width)
        + "x" + std::to_string(m_copiedExtent.height);
    const auto filename = base + ".ppm";
    std::ofstream file(filename, std::ios::binary);
    file << "P6\n" << m_copiedExtent.width << ' ' << m_copiedExtent.height << "\n255\n";
    for (size_t i = 0; i < count; ++i) {
        const char rgb[3]{char(output[i * 4]), char(output[i * 4 + 1]), char(output[i * 4 + 2])};
        file.write(rgb, 3);
    }
    std::ofstream hardware(base+".srgb.ppm",std::ios::binary);
    hardware << "P6\n" << m_copiedExtent.width << ' ' << m_copiedExtent.height << "\n255\n";
    for (size_t i=0;i<count;++i) hardware.write(reinterpret_cast<const char*>(data+count*44+i*4),3);
    std::ofstream hdr(base+".hdrbin",std::ios::binary);
    hdr.write(reinterpret_cast<const char*>(&m_copiedExtent.width),4);
    hdr.write(reinterpret_cast<const char*>(&m_copiedExtent.height),4);
    hdr.write(reinterpret_cast<const char*>(data+count*32),static_cast<std::streamsize>(count*8));
    if (m_deferred) {
        std::ofstream codes(base+".codesbin",std::ios::binary);
        codes.write(reinterpret_cast<const char*>(&m_copiedExtent.width),4);
        codes.write(reinterpret_cast<const char*>(&m_copiedExtent.height),4);
        for (size_t n=0;n<count;++n) {const char code=char(halfFloat(position[n*4+3])); codes.write(&code,1);}
        if (!codes) throw std::runtime_error("GBuffer visibility export failed");
    }
    m_staging->unmap();
    if (!file || !hardware || !hdr) throw std::runtime_error("HDR readback image export failed");
    KU_INFO("KUENGINE_HDR_READBACK_OK extent={}x{} depthUnchanged=1 linearFormat=RGBA16F outputs=UNORM,SRGB prefix={}",m_copiedExtent.width,m_copiedExtent.height,base);
    if (m_deferred && m_semantic) KU_INFO("KUENGINE_DEFERRED_PIXELS_OK extent={}x{} empty={} pbr={} unlit={} maskPbr={} maskUnlit={} pbrSum={} unlitSum={} output={}",
        m_copiedExtent.width, m_copiedExtent.height, pixels[0], pixels[1], pixels[2], mask[1], mask[2], sums[1], sums[2], filename);
    m_pending = false;
}
} // namespace ku
