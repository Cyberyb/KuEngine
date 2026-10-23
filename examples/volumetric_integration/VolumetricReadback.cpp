#include "VolumetricReadback.h"
#include <KuEngine/Core/Log.h>
#include <KuEngine/RHI/CommandList.h>
#include <KuEngine/Render/RenderPipeline.h>
#include <KuEngine/Render/RenderGraphResources.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace ku::volumetric {
namespace {
constexpr auto stagingName = "VolumetricStaging";
constexpr VkDeviceSize capacity = VkDeviceSize(2048)*2048*8;
float half(uint16_t bits)
{
    const int exponent = (bits>>10)&31, fraction = bits&1023;
    if (exponent == 31) return fraction ? NAN : (bits&32768 ? -INFINITY : INFINITY);
    return (bits&32768 ? -1.f : 1.f)*(exponent ? std::ldexp(1.f+fraction/1024.f,exponent-15) : std::ldexp(float(fraction),-24));
}
uint8_t encode(float value)
{
    return uint8_t(std::lround(std::pow(std::clamp(value,0.f,1.f),1.f/2.2f)*255));
}
}
ReadbackPass::ReadbackPass(RenderPipeline& pipeline,ScenePass& source,std::filesystem::path prefix,std::shared_ptr<CaptureReport> report)
    : m_pipeline(pipeline),m_source(source),m_prefix(std::move(prefix)),m_report(std::move(report)) {}
ReadbackPass::~ReadbackPass()
{
    // Engine destruction waits the GPU before destroying its passes. Preserve
    // final-slot errors outside this noexcept destructor so main can fail.
    try { inspectAll(); }
    catch (const std::exception& error) { m_report->error = error.what(); }
}
void ReadbackPass::inspectAll()
{
    for (uint32_t i = 0; i < m_captures.size(); ++i) inspect(i);
}
void ReadbackPass::initialize(const RenderContext& context)
{
    inspectAll();
    m_captures.clear(); m_captures.resize(context.framesInFlight);
    RHIBuffer::CreateInfo info;
    info.size = capacity; info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.allocationFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT|VMA_ALLOCATION_CREATE_MAPPED_BIT;
    for (auto& capture : m_captures) capture.staging = std::make_unique<RHIBuffer>(context.device,info);
}
void ReadbackPass::setup(RenderGraphBuilder& builder)
{
    m_image = builder.createImage(sceneColorName,sceneColorDesc());
    builder.useImage(m_image,ImageUse::TransferSource);
    m_buffer = builder.importBuffer(stagingName,BufferDesc{capacity,VK_BUFFER_USAGE_TRANSFER_DST_BIT,VMA_MEMORY_USAGE_AUTO,InitialContent::Undefined});
    builder.useBuffer(m_buffer,BufferUse::TransferDestination);
    builder.dependsOn("VolumetricDisplay");
}
void ReadbackPass::update(const FrameData& frame)
{
    inspect(frame.frameIndex);
    m_pipeline.bindExternalBuffer({.resourceName=stagingName,
        .buffer=m_captures.at(frame.frameIndex).staging->buffer(),.size=capacity,.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .initialState={VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT,VK_IMAGE_LAYOUT_UNDEFINED},
        .finalState={VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT,VK_IMAGE_LAYOUT_UNDEFINED},.contentsValid=false});
}
void ReadbackPass::onResize(uint32_t,uint32_t) { inspectAll(); }
void ReadbackPass::execute(CommandList& cmd,const FrameData& frame,RenderGraphResourceResolver& resources)
{
    auto& capture = m_captures.at(frame.frameIndex);
    const auto image = resources.resolveImage(m_image,ImageUse::TransferSource);
    const auto buffer = resources.resolveBuffer(m_buffer,BufferUse::TransferDestination);
    if (!image.contentsValid || VkDeviceSize(image.extent.width)*image.extent.height*8 > capacity)
        throw std::runtime_error("Volumetric readback: invalid content or capture exceeds 2048x2048 pixel capacity");
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
    copy.imageExtent = {image.extent.width,image.extent.height,1};
    vkCmdCopyImageToBuffer(cmd,image.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffer.buffer,1,&copy);
    capture.extent = image.extent;
    capture.rect = frame.viewerLayout.sceneFramebuffer;
    capture.constants = m_source.constants(frame);
    capture.submit = frame.submittedFrame;
    capture.pending = true;
}
void ReadbackPass::inspect(uint32_t slot)
{
    auto& capture = m_captures.at(slot);
    if (!capture.pending) return;
    capture.staging->invalidate();
    const auto* data = static_cast<const uint16_t*>(capture.staging->map());
    if (!data) throw std::runtime_error("Volumetric staging map failed");
    try {
        std::ostringstream suffix;
        suffix << "_submit" << std::setw(6) << std::setfill('0') << capture.submit << "_slot" << slot
               << '_' << capture.rect.width << 'x' << capture.rect.height;
        const auto base = m_prefix.string()+suffix.str();
        if (!m_prefix.parent_path().empty()) std::filesystem::create_directories(m_prefix.parent_path());
        std::ofstream ppm(base+".ppm",std::ios::binary);
        std::ofstream raw(base+".rgba16f",std::ios::binary);
        std::ofstream metadata(base+".txt");
        ppm << "P6\n" << capture.rect.width << ' ' << capture.rect.height << "\n255\n";
        double sum = 0;
        uint64_t hash = 14695981039346656037ull;
        for (uint32_t y = 0; y < capture.rect.height; ++y) {
            const auto* row = data+(size_t(y+capture.rect.y)*capture.extent.width+capture.rect.x)*4;
            raw.write(reinterpret_cast<const char*>(row),std::streamsize(capture.rect.width)*8);
            for (uint32_t x = 0; x < capture.rect.width; ++x) {
                char rgb[3];
                for (uint32_t c = 0; c < 4; ++c) {
                    const auto bits = row[x*4+c];
                    const float value = half(bits);
                    if (!std::isfinite(value) || value < 0 || (c == 3 && value != 1))
                        throw std::runtime_error("Volumetric capture contains invalid radiance/alpha");
                    hash = (hash ^ (bits & 255))*1099511628211ull;
                    hash = (hash ^ (bits >> 8))*1099511628211ull;
                    if (c < 3) { sum += value; rgb[c] = char(encode(value)); }
                }
                ppm.write(rgb,3);
            }
        }
        if (capture.constants.medium[1] > 0 && capture.constants.medium[2] > 0 && sum < .001)
            throw std::runtime_error("Volumetric capture is unexpectedly black");
        metadata << std::setprecision(9) << "submit=" << capture.submit << "\nslot=" << slot
            << "\nwidth=" << capture.rect.width << "\nheight=" << capture.rect.height
            << "\ntime=" << capture.constants.medium[0] << "\ndensity=" << capture.constants.medium[1]
            << "\nlight=" << capture.constants.medium[2] << "\nstrongFog=" << capture.constants.medium[3]
            << "\nsteps=" << capture.constants.quality[0] << "\nshadowSteps=" << capture.constants.quality[1]
            << "\nintegration=" << capture.constants.quality[2] << "\nflags=" << capture.constants.quality[3]
            << "\nhash=" << hash << "\nradianceSum=" << sum << '\n';
        if (!ppm || !raw || !metadata) throw std::runtime_error("Volumetric evidence export failed");
        KU_INFO("KUENGINE_VOLUMETRIC_PIXELS_OK slot={} submit={} time={} density={} integration={} sum={} hash={} output={}.ppm",
            slot,capture.submit,capture.constants.medium[0],capture.constants.medium[1],capture.constants.quality[2],sum,hash,base);
        ++m_report->completed;
    } catch (...) { capture.staging->unmap(); throw; }
    capture.staging->unmap(); capture.pending = false;
}
} // namespace ku::volumetric
