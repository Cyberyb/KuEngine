#include "CornellBoxReadback.h"

#include <KuEngine/Core/Log.h>
#include <KuEngine/RHI/RHIBuffer.h>
#include <KuEngine/RHI/CommandList.h>
#include <KuEngine/Render/RenderPipeline.h>
#include <KuEngine/Render/RenderGraph.h>
#include <KuEngine/Render/ForwardGraphTargets.h>
#include <KuEngine/Render/ScenePasses.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace ku {
namespace {
constexpr std::string_view stagingName = "CornellBoxStaging";
constexpr VkDeviceSize capacity = VkDeviceSize(2048)*2048*24;
float half(uint16_t bits)
{
    const int exponent=(bits>>10)&31, fraction=bits&1023;
    if (exponent==31) return fraction ? NAN : (bits&32768 ? -INFINITY : INFINITY);
    return (bits&32768 ? -1.f : 1.f)*(exponent ? std::ldexp(1.f+fraction/1024.f,exponent-15) : std::ldexp(float(fraction),-24));
}
uint8_t display(float value)
{
    value=std::max(value,0.f); value=value/(1+value);
    value=value<=.0031308f ? 12.92f*value : 1.055f*std::pow(value,1.f/2.4f)-.055f;
    return static_cast<uint8_t>(std::lround(std::clamp(value,0.f,1.f)*255));
}
}
CornellBoxReadback::CornellBoxReadback(RenderPipeline& pipeline,SceneDrawSource& source,std::filesystem::path prefix)
    :m_pipeline(pipeline),m_source(source),m_prefix(std::move(prefix)) {}
CornellBoxReadback::~CornellBoxReadback() = default;
void CornellBoxReadback::initialize(const RenderContext& context)
{
    const auto gbuffer=makeGBufferTargets(context.depthFormat,context.clearDepthStencil);
    m_descriptors={gbuffer.colors[0],gbuffer.colors[2],makeDeferredSceneColor(context)};
    RHIBuffer::CreateInfo info{};
    info.size=capacity; info.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.allocationFlags=VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT|VMA_ALLOCATION_CREATE_MAPPED_BIT;
    m_frames.clear();
    m_frames.resize(context.framesInFlight);
    for (auto& capture : m_frames) capture.staging=std::make_unique<RHIBuffer>(context.device,info);
}
void CornellBoxReadback::setup(RenderGraphBuilder& builder)
{
    const std::array<std::string_view,3> names{gbufferNames[0],gbufferNames[2],deferredSceneColorName};
    for (size_t i=0;i<3;++i) {
        m_images[i]=builder.createImage(names[i],m_descriptors[i]);
        builder.useImage(m_images[i],ImageUse::TransferSource);
    }
    m_buffer=builder.importBuffer(stagingName,BufferDesc{capacity,VK_BUFFER_USAGE_TRANSFER_DST_BIT,VMA_MEMORY_USAGE_AUTO,InitialContent::Undefined});
    builder.useBuffer(m_buffer,BufferUse::TransferDestination);
    builder.dependsOn("ForwardDisplay");
}
void CornellBoxReadback::update(const FrameData& frame)
{
    m_activeFrame = frame.frameIndex;
    inspect();
    m_pipeline.bindExternalBuffer({.resourceName=stagingName,.buffer=m_frames[m_activeFrame].staging->buffer(),.size=capacity,
        .usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .initialState={VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT,VK_IMAGE_LAYOUT_UNDEFINED},
        .finalState={VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT,VK_IMAGE_LAYOUT_UNDEFINED},.contentsValid=false});
}
void CornellBoxReadback::onResize(uint32_t,uint32_t) {
    for (uint32_t slot=0;slot<m_frames.size();++slot) { m_activeFrame=slot; inspect(); }
}
void CornellBoxReadback::execute(CommandList& commands,const FrameData& frame,RenderGraphResourceResolver& resources)
{
    m_activeFrame = frame.frameIndex;
    m_frames[m_activeFrame].submit = frame.submittedFrame;
    const auto buffer=resources.resolveBuffer(m_buffer,BufferUse::TransferDestination);
    for (size_t i=0;i<3;++i) {
        const auto image=resources.resolveImage(m_images[i],ImageUse::TransferSource);
        const VkDeviceSize bytes=VkDeviceSize(image.extent.width)*image.extent.height*8;
        if (!image.contentsValid || bytes*3>capacity) throw std::runtime_error("Cornell readback invalid content or staging capacity exceeded");
        if (i && (image.extent.width!=m_frames[m_activeFrame].extent.width || image.extent.height!=m_frames[m_activeFrame].extent.height))
            throw std::runtime_error("Cornell readback images differ in extent");
        m_frames[m_activeFrame].extent=image.extent;
        VkBufferImageCopy copy{};
        copy.bufferOffset=bytes*i; copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
        copy.imageExtent={image.extent.width,image.extent.height,1};
        vkCmdCopyImageToBuffer(commands,image.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffer.buffer,1,&copy);
    }
    m_frames[m_activeFrame].rect=frame.viewerLayout.sceneFramebuffer;
    m_frames[m_activeFrame].exposure=m_source.checkedSceneSnapshot(frame).view.environmentExposure;
    m_frames[m_activeFrame].pending=true;
}
void CornellBoxReadback::inspect()
{
    if (!m_frames[m_activeFrame].pending) return;
    m_frames[m_activeFrame].staging->invalidate();
    const auto* data=static_cast<const uint16_t*>(m_frames[m_activeFrame].staging->map());
    if (!data) throw std::runtime_error("Cornell staging map failed");
    try {
        const size_t count=size_t(m_frames[m_activeFrame].extent.width)*m_frames[m_activeFrame].extent.height;
        const auto* position=data+count*4;
        const auto* hdr=data+count*8;
        size_t pbr=0,red=0,green=0,white=0,bright=0;
        double luminance=0;
        size_t isolationPixels=0;
        double redWallRadiance=0;
        const bool odd = m_frames[m_activeFrame].submit % 2 != 0;
        for (size_t i=0;i<count;++i) {
            const float code=half(position[i*4+3]);
            if (code!=0 && code!=1) throw std::runtime_error("Cornell GBuffer contains a non-PBR surface code");
            if (!code) continue;
            ++pbr;
            const float r=half(data[i*4]),g=half(data[i*4+1]),b=half(data[i*4+2]);
            if (m_verifyFrameIsolation && std::abs(half(position[i*4])+1.f)<.001f && g<.1f && b<.1f) {
                ++isolationPixels;
                if (std::abs(r-(odd?.63f:.875f))>.001f)
                    throw std::runtime_error("Draw UBO data belongs to another in-flight submission");
                redWallRadiance+=half(hdr[i*4])+half(hdr[i*4+1])+half(hdr[i*4+2]);
            }
            red+=r>.5f && g<.1f && b<.1f;
            green+=g>.3f && r<.1f && b<.1f;
            white+=r>.6f && g>.6f && b>.6f;
            for (size_t c=0;c<3;++c) {
                const float value=half(hdr[i*4+c]);
                if (!std::isfinite(value) || value<0) throw std::runtime_error("Cornell HDR contains invalid radiance");
                luminance+=value;
            }
            bright+=r==0 && g==0 && b==0 && std::abs(half(position[i*4+1])-1.985f)<.003f && half(hdr[i*4])>4;
        }
        if (pbr<count/20 || red<count/10000 || green<count/10000 || white<count/1000 || bright<4 || luminance<1)
            throw std::runtime_error("Cornell GPU frame is missing room walls, white geometry, ceiling emission or lighting");
        if (m_verifyFrameIsolation) {
            if (isolationPixels < 8 || (odd ? redWallRadiance > .001 : redWallRadiance < 1))
                throw std::runtime_error("Frame UBO lighting belongs to another in-flight submission");
            KU_INFO("KUENGINE_FRAME_ISOLATION_OK slot={} submit={} redBase={} redHdrSum={} pixels={}",
                m_activeFrame,m_frames[m_activeFrame].submit,odd?.63f:.875f,redWallRadiance,isolationPixels);
        }
        if (!m_prefix.parent_path().empty()) std::filesystem::create_directories(m_prefix.parent_path());
        const auto base=m_prefix.string()+"_"+std::to_string(m_frames[m_activeFrame].extent.width)+"x"+std::to_string(m_frames[m_activeFrame].extent.height);
        std::ofstream raw(base+".rawbin",std::ios::binary);
        raw.write(reinterpret_cast<const char*>(&m_frames[m_activeFrame].extent.width),4);
        raw.write(reinterpret_cast<const char*>(&m_frames[m_activeFrame].extent.height),4);
        raw.write(reinterpret_cast<const char*>(data),static_cast<std::streamsize>(count*24));
        std::ofstream image(base+".ppm",std::ios::binary);
        image<<"P6\n"<<m_frames[m_activeFrame].rect.width<<' '<<m_frames[m_activeFrame].rect.height<<"\n255\n";
        for (uint32_t y=0;y<m_frames[m_activeFrame].rect.height;++y) for (uint32_t x=0;x<m_frames[m_activeFrame].rect.width;++x) {
            const size_t index=(size_t(y+m_frames[m_activeFrame].rect.y)*m_frames[m_activeFrame].extent.width+x+m_frames[m_activeFrame].rect.x)*4;
            const char rgb[3]{char(display(half(hdr[index])*m_frames[m_activeFrame].exposure)),char(display(half(hdr[index+1])*m_frames[m_activeFrame].exposure)),char(display(half(hdr[index+2])*m_frames[m_activeFrame].exposure))};
            image.write(rgb,3);
        }
        if (!raw || !image) throw std::runtime_error("Cornell evidence export failed");
        KU_INFO("KUENGINE_CORNELL_SLOT_OK slot={} submit={} staging={}",m_activeFrame,m_frames[m_activeFrame].submit,reinterpret_cast<uintptr_t>(m_frames[m_activeFrame].staging->buffer()));
        KU_INFO("KUENGINE_CORNELL_PIXELS_OK extent={}x{} pbr={} red={} green={} white={} emissive={} output={}.ppm",
            m_frames[m_activeFrame].extent.width,m_frames[m_activeFrame].extent.height,pbr,red,green,white,bright,base);
    } catch (...) {m_frames[m_activeFrame].staging->unmap();throw;}
    m_frames[m_activeFrame].staging->unmap();
    m_frames[m_activeFrame].pending=false;
}
} // namespace ku
