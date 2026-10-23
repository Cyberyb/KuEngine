#include "ViewerPathSmoke.h"
#include <KuEngine/Render/GBufferDisplayPass.h>
#include <KuEngine/Render/ForwardDisplayProgram.h>
#include <KuEngine/Render/ForwardGraphTargets.h>
#include <KuEngine/Render/GpuModelAsset.h>
#include <KuEngine/Render/GpuMesh.h>
#include <KuEngine/Render/PBRResources.h>
#include <KuEngine/RHI/RHIBuffer.h>
#include <KuEngine/RHI/CommandList.h>
#include <KuEngine/Core/Log.h>
#include <KuEngine/UI/UIOverlay.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
namespace ku {
namespace {
constexpr VkDeviceSize capacity=VkDeviceSize(2048)*2048*48;
ImageDesc probe(VkFormat format) {ImageDesc d{};d.extent=ImageExtentDesc::swapchainRelative();d.format=format;d.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;d.aspect=VK_IMAGE_ASPECT_COLOR_BIT;d.initialContent=InitialContent::Cleared;return d;}
class SceneProbe final : public RenderPass {
public:
    SceneProbe(SceneDrawSource& source,bool srgb):source(source),srgb(srgb){}
    std::string_view name()const override{return srgb?"ViewerSceneSRGB":"ViewerSceneUNORM";}
    void initialize(const RenderContext& c)override {color=makeForwardGraphTargets(c).color;program.initialize(c.device,color.format,srgb?VK_FORMAT_R8G8B8A8_SRGB:VK_FORMAT_R8G8B8A8_UNORM,c.framesInFlight);}
    void setup(RenderGraphBuilder& b)override {source.declarePrepareDependency(b);input=b.createImage(forward_graph_resource::sceneColor,color);b.useImage(input,ImageUse::FragmentSampled);b.colorAttachment(b.createImage(name(),probe(srgb?VK_FORMAT_R8G8B8A8_SRGB:VK_FORMAT_R8G8B8A8_UNORM)),AttachmentLoadPolicy::Clear,AttachmentStorePolicy::Store);}
    void execute(CommandList& c,const FrameData& f,RenderGraphResourceResolver& r)override {auto image=r.resolveImage(input,ImageUse::FragmentSampled);auto view=source.checkedSceneSnapshot(f).view;program.draw(c,image,calculateForwardDisplayUvTransform(f.viewerLayout.sceneFramebuffer,image.extent),view.environmentExposure,true,f.frameIndex);}
    std::optional<CommandListStatistics> expectedFrameStatistics()const override{return CommandListStatistics{1,3};}
private:SceneDrawSource& source;bool srgb;ImageDesc color{};ImageHandle input{};ForwardDisplayProgram program;
};
float half(uint16_t bits) {int e=(bits>>10)&31,m=bits&1023;if(e==31)return m?NAN:(bits&32768?-INFINITY:INFINITY);return (bits&32768?-1.f:1.f)*(e?std::ldexp(1.f+m/1024.f,e-15):std::ldexp(float(m),-24));}
float srgb(float c) {return c<=.0031308f?12.92f*c:1.055f*std::pow(c,1.f/2.4f)-.055f;}
void ppm(const std::string& path,VkExtent2D extent,const uint8_t* rgba) {std::ofstream out(path,std::ios::binary);out<<"P6\n"<<extent.width<<' '<<extent.height<<"\n255\n";for(size_t i=0;i<size_t(extent.width)*extent.height;++i)out.write(reinterpret_cast<const char*>(rgba+i*4),3);if(!out)throw std::runtime_error("Path evidence write failed");}
template<class T>void append(std::vector<uint64_t>& bytes,const T& object){static_assert(std::is_trivially_copyable_v<T>);const auto* data=reinterpret_cast<const uint8_t*>(&object);for(size_t n=0;n<sizeof(T);++n)bytes.push_back(data[n]);}
std::vector<uint64_t> identity(const SceneRenderSnapshot& s) {
    std::vector<uint64_t> result;
    append(result,s.view.cameraPosition);append(result,s.view.lighting.direction);append(result,s.view.lighting.color);append(result,s.view.lighting.intensity);
    for(const auto& point:s.view.lighting.pointLights){append(result,point.position);append(result,point.color);append(result,point.intensity);append(result,point.range);}
    append(result,s.view.environment);append(result,s.view.environment->descriptorSet());append(result,s.view.environmentEnabled);append(result,s.view.skyboxEnabled);append(result,s.view.environmentIntensity);append(result,s.view.outputGamma);
    for(const auto& draw:s.draws) {
        append(result,draw.model);append(result,draw.model->mesh().vertexBuffer().buffer());append(result,draw.model->mesh().indexBuffer().buffer());append(result,draw.materials);append(result,draw.materials->material(draw.materialIndex).descriptorSet);
        append(result,draw.modelMatrix);append(result,draw.subMeshIndex);append(result,draw.materialIndex);append(result,draw.material.shadingModel);append(result,draw.material.metallicFactor);append(result,draw.material.roughnessFactor);append(result,draw.material.emissiveFactor);append(result,draw.textureEnabled);append(result,draw.tint);
        append(result,draw.material.alphaMode);append(result,draw.material.alphaCutoff);append(result,draw.material.doubleSided);append(result,draw.material.normalScale);append(result,draw.material.occlusionStrength);append(result,draw.material.textureTransforms);
        auto base=draw.material.baseColorFactor;if(draw.material.alphaMode==asset::AlphaMode::Blend)base[3]=1;append(result,base);
    }
    return result;
}
}
ViewerPathSmoke::ViewerPathSmoke(Engine& engine,SceneDrawSource& source,std::filesystem::path prefix,std::function<void(bool)> variant,uint64_t startAfter,std::function<std::vector<uint64_t>()> state)
    :m_engine(engine),m_source(source),m_prefix(std::move(prefix)),m_variant(std::move(variant)),m_state(std::move(state)),m_startAfter(startAfter) {
    if(m_prefix.parent_path().empty())throw std::invalid_argument("Path evidence prefix requires a parent directory");
    for(int i=0;i<3;++i){m_steps.push_back({asset::RenderPath::Forward,GBufferView::Scene,false});m_steps.push_back({asset::RenderPath::Deferred,GBufferView::Scene,false});}
    for(uint32_t i=1;i<=8;++i){m_steps.push_back({asset::RenderPath::Deferred,static_cast<GBufferView>(i),false});m_steps.push_back({asset::RenderPath::Deferred,static_cast<GBufferView>(i),true});}
    m_steps.push_back({asset::RenderPath::Forward,GBufferView::Scene,false});m_steps.push_back({asset::RenderPath::Deferred,GBufferView::Scene,false});m_steps.push_back({asset::RenderPath::Forward,GBufferView::Scene,false});
}
ViewerPathSmoke::~ViewerPathSmoke()=default;
void ViewerPathSmoke::bind(RenderPass& a,RenderPass& b,RenderPass& c,RenderPass& d,ViewerPathReadback& scene,ViewerPathReadback& debug){m_probes={&a,&b,&c,&d};m_readbacks={&scene,&debug};for(auto* p:m_probes)p->setEnabled(false);for(auto* p:m_readbacks)p->setEnabled(false);}
uint64_t ViewerPathSmoke::completedSerial()const {const auto& completed=m_engine.completedFrameStatistics();return completed?completed->submittedFrame:0;}
const ViewerPathSmoke::Selection& ViewerPathSmoke::selection()const{return m_steps.at(m_step);}
void ViewerPathSmoke::update(const FrameData& frame) {
    m_engine.waitForAllFrames(); // optional single-staging verification coordinator

    // External Graph bindings must exist even when their consuming node is
    // disabled. Keep the import alive; only enabled readbacks record copies.
    for(auto* readback:m_readbacks){readback->inspect();readback->update(frame);}
    m_active=false;
    if(m_requested && m_source.pathControl().generation()>m_requestedGeneration) {
        const auto& desired=selection();
        if(m_source.pathControl().path()!=desired.path || m_source.pathControl().view()!=desired.view)throw std::runtime_error("UI path request was not published as requested");
        m_variant(desired.variant);m_active=true;
    }
    const bool debug=m_active && selection().view!=GBufferView::Scene;
    m_probes[0]->setEnabled(m_active&&!debug);m_probes[1]->setEnabled(m_active&&!debug);
    m_probes[2]->setEnabled(debug);m_probes[3]->setEnabled(debug);
    m_readbacks[0]->setEnabled(m_active&&!debug);m_readbacks[1]->setEnabled(debug);
    // Same CPU-only request used by the real control; Source update has already
    // run, so this request cannot publish until the NEXT fence-safe update.
    // Do not depend on an expanded sidebar for finite automation to progress.
    drawUI();
}
void ViewerPathSmoke::drawUI() {
    if(!m_done&&!m_requested&&completedSerial()>=m_startAfter) {
        m_requestedGeneration=m_source.pathControl().generation();
        const auto& desired=selection();
        if(!m_source.pathControl().request(desired.path,desired.view))throw std::runtime_error(m_source.pathControl().status());
        m_requested=true;
        KU_INFO("KUENGINE_PATH_REQUEST step={} afterCompletedSubmit={} path={} view={} variant={}",m_step,completedSerial(),asset::toString(desired.path),static_cast<uint32_t>(desired.view),desired.variant);
    }
}
void ViewerPathSmoke::execute(CommandList&,const FrameData& frame) {
    if(!m_active)return;
    const auto& snapshot=m_source.checkedSceneSnapshot(frame);
    const VkExtent2D extent{frame.viewerLayout.sceneFramebuffer.width,frame.viewerLayout.sceneFramebuffer.height};
    std::vector<uint64_t> projection;append(projection,snapshot.view.viewProjection);
    if(m_sceneExtent.width==extent.width && m_sceneExtent.height==extent.height && m_projection!=projection)
        throw std::runtime_error("Path switch changed projection without a viewport change");
    if(m_sceneExtent.width && (m_sceneExtent.width!=extent.width || m_sceneExtent.height!=extent.height))
        KU_INFO("KUENGINE_PATH_LAYOUT_CHANGED oldScene={}x{} newScene={}x{} stableStateStillChecked=1",m_sceneExtent.width,m_sceneExtent.height,extent.width,extent.height);
    m_sceneExtent=extent;m_projection=std::move(projection);
    auto actual=identity(snapshot);
    append(actual,m_engine.ui().sidebarExpanded());append(actual,m_engine.ui().compactStatsVisible());
    if(m_state) {const auto state=m_state();actual.insert(actual.end(),state.begin(),state.end());}
    if(m_identity.empty())m_identity=actual;
    else if(m_identity!=actual)throw std::runtime_error("Path switch changed shared asset/material/camera/light identity/state");
}
void ViewerPathSmoke::onResize(uint32_t,uint32_t){for(auto* readback:m_readbacks)readback->inspect();}
void ViewerPathSmoke::acceptSample(size_t step,uint64_t recordedAfter,VkExtent2D extent,const uint8_t* data,bool debug) {
    if(step!=m_step || completedSerial()!=recordedAfter+1)throw std::runtime_error("Readback is not associated with the actual completed submission");
    const size_t count=size_t(extent.width)*extent.height;
    const auto* hdr=reinterpret_cast<const uint16_t*>(data+(debug?32:0)*count);
    const auto* unorm=data+(debug?40:8)*count;const auto* hardware=unorm+4*count;
    std::filesystem::create_directories(m_prefix.parent_path());
    const auto base=m_prefix.string()+"-step"+std::to_string(step)+"_"+std::to_string(extent.width)+"x"+std::to_string(extent.height);
    ppm(base+".ppm",extent,unorm);ppm(base+".srgb.ppm",extent,hardware);
    std::ofstream raw(base+".hdrbin",std::ios::binary);raw.write(reinterpret_cast<const char*>(&extent.width),4);raw.write(reinterpret_cast<const char*>(&extent.height),4);raw.write(reinterpret_cast<const char*>(hdr),count*8);if(!raw)throw std::runtime_error("Path HDR export failed");
    int encodingMax=0;for(size_t n=0;n<count;++n)for(size_t c=0;c<3;++c)encodingMax=std::max(encodingMax,std::abs(int(unorm[n*4+c])-hardware[n*4+c]));
    if(encodingMax>1)throw std::runtime_error("Path display UNORM/SRGB encoding mismatch");
    if(debug) {
        const auto* mrt=reinterpret_cast<const uint16_t*>(data);
        std::ofstream mrtFile(base+".mrtbin",std::ios::binary);mrtFile.write(reinterpret_cast<const char*>(&extent.width),4);mrtFile.write(reinterpret_cast<const char*>(&extent.height),4);mrtFile.write(reinterpret_cast<const char*>(data),count*32);if(!mrtFile)throw std::runtime_error("MRT evidence export failed");
        size_t empty=0,visible=0;int mappingMax=0;
        for(size_t n=0;n<count;++n) {
            const int code=int(half(mrt[count*8+n*4+3]));if(code==0)++empty;else ++visible;
            std::array<float,3> ref{};
            const auto view=selection().view;
            for(size_t c=0;c<3;++c)if(code) {
                if(view==GBufferView::Base)ref[c]=half(mrt[n*4+c]);
                if(view==GBufferView::Metallic)ref[c]=half(mrt[n*4+3]);
                if(view==GBufferView::Normal)ref[c]=half(mrt[count*4+n*4+c])*.5f+.5f;
                if(view==GBufferView::Roughness)ref[c]=half(mrt[count*4+n*4+3]);
                if(view==GBufferView::Position)ref[c]=(half(mrt[count*8+n*4+c])+2.f)/4.f;
                if(view==GBufferView::ShadingModel)ref[c]=code==1?float(c==0):code==2?float(c==1):float(c!=1);
                if(view==GBufferView::Emissive)ref[c]=half(mrt[count*12+n*4+c]);
                if(view==GBufferView::Ao)ref[c]=half(mrt[count*12+n*4+3]);
                if(!std::isfinite(ref[c]))throw std::runtime_error("Nonfinite MRT debug reference");
            }
            for(size_t c=0;c<3;++c) {int expected=int(std::round(srgb(std::clamp(ref[c],0.f,1.f))*255));mappingMax=std::max(mappingMax,std::abs(int(unorm[n*4+c])-expected));}
        }
        if(!empty||!visible||mappingMax>1)throw std::runtime_error("Independent MRT debug reference failed/empty ROI");
        if(!selection().variant) {m_debugBaseline.assign(unorm,unorm+count*4);m_hdrBaseline.assign(hdr,hdr+count*4);}
        else {
            if(m_debugBaseline.size()!=count*4)throw std::runtime_error("Debug invariant comparison extent changed between paired samples");
            if(!std::equal(m_debugBaseline.begin(),m_debugBaseline.end(),unorm))throw std::runtime_error("Exposure/Blend polluted GBuffer debug output");
            // Exposure changes display only; disabled Blend alpha should alter
            // SceneColor in the semantic fixture, checked by the external test.
            bool hdrChanged=!std::equal(m_hdrBaseline.begin(),m_hdrBaseline.end(),hdr);
            KU_INFO("KUENGINE_DEBUG_INVARIANT view={} debugUnchanged=1 sceneHdrChanged={}",static_cast<uint32_t>(selection().view),hdrChanged);
        }
        // Codes for the independently rendered normal Deferred sample. The
        // mapping only uses opaque Geometry, independent of alpha test variant.
        if(step==6) {
            const auto visibility=m_prefix.string()+"-step1_"+std::to_string(extent.width)+"x"+std::to_string(extent.height)+".codesbin";
            std::ofstream codes(visibility,std::ios::binary);codes.write(reinterpret_cast<const char*>(&extent.width),4);codes.write(reinterpret_cast<const char*>(&extent.height),4);for(size_t n=0;n<count;++n){const char code=char(half(mrt[count*8+n*4+3]));codes.write(&code,1);}if(!codes)throw std::runtime_error("Path codes export failed");
        }
        KU_INFO("KUENGINE_GBUFFER_REFERENCE_OK view={} empty={} visible={} maxError={}/255 encodingMax={}/255",static_cast<uint32_t>(selection().view),empty,visible,mappingMax,encodingMax);
    }
    KU_INFO("KUENGINE_PATH_SAMPLE_OK step={} completedSubmit={} path={} view={} extent={}x{} identityPreserved=1 prefix={}",step,completedSerial(),asset::toString(selection().path),static_cast<uint32_t>(selection().view),extent.width,extent.height,base);
    m_requested=false;++m_step;
    if(m_step==m_steps.size()){m_done=true;m_variant(false);KU_INFO("KUENGINE_PATH_SMOKE_OK samples={} completedSubmit={} identityPreserved=1",m_steps.size(),completedSerial());}
}
ViewerPathReadback::ViewerPathReadback(Engine& e,ViewerPathSmoke& owner,bool debug):m_engine(e),m_owner(owner),m_debug(debug){}
ViewerPathReadback::~ViewerPathReadback()=default;
void ViewerPathReadback::initialize(const RenderContext& c) {
    m_targets=makeGBufferTargets(c.depthFormat,c.clearDepthStencil);m_hdr=makeForwardGraphTargets(c).color;
    RHIBuffer::CreateInfo info{};info.size=capacity;info.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;info.allocationFlags=VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT|VMA_ALLOCATION_CREATE_MAPPED_BIT;
    m_staging=std::make_unique<RHIBuffer>(c.device,info);m_pending=false;
}
void ViewerPathReadback::setup(RenderGraphBuilder& b) {
    size_t index=0;
    if(m_debug)for(size_t i=0;i<4;++i)m_images[index++]=b.createImage(gbufferNames[i],m_targets.colors[i]);
    m_images[index++]=b.createImage(forward_graph_resource::sceneColor,m_hdr);
    m_images[index++]=b.createImage(m_debug?"ViewerDebugUNORM":"ViewerSceneUNORM",probe(VK_FORMAT_R8G8B8A8_UNORM));
    m_images[index++]=b.createImage(m_debug?"ViewerDebugSRGB":"ViewerSceneSRGB",probe(VK_FORMAT_R8G8B8A8_SRGB));
    for(size_t n=0;n<index;++n)b.useImage(m_images[n],ImageUse::TransferSource);
    m_buffer=b.importBuffer(name(),BufferDesc{capacity,VK_BUFFER_USAGE_TRANSFER_DST_BIT,VMA_MEMORY_USAGE_AUTO,InitialContent::Undefined});b.useBuffer(m_buffer,BufferUse::TransferDestination);
}
void ViewerPathReadback::update(const FrameData&) {
    m_engine.renderPipeline().bindExternalBuffer({.resourceName=name(),.buffer=m_staging->buffer(),.size=capacity,.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT,.initialState={VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT,VK_IMAGE_LAYOUT_UNDEFINED},.finalState={VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT,VK_IMAGE_LAYOUT_UNDEFINED},.contentsValid=false});
}
void ViewerPathReadback::execute(CommandList& commands,const FrameData&,RenderGraphResourceResolver& r) {
    const auto buffer=r.resolveBuffer(m_buffer,BufferUse::TransferDestination);VkDeviceSize offset=0;
    const size_t count=m_debug?7:3;
    for(size_t i=0;i<count;++i) {
        const auto image=r.resolveImage(m_images[i],ImageUse::TransferSource);
        if(!image.complete()||!image.contentsValid||uint64_t(image.extent.width)*image.extent.height>2048*2048||(i && (image.extent.width!=m_extent.width||image.extent.height!=m_extent.height)))throw std::runtime_error("Invalid bounded path readback extent/contents");
        m_extent=image.extent;VkBufferImageCopy copy{};copy.bufferOffset=offset;copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={m_extent.width,m_extent.height,1};
        vkCmdCopyImageToBuffer(commands,image.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffer.buffer,1,&copy);
        const bool hdr=i<count-2;if(image.format!=(hdr?VK_FORMAT_R16G16B16A16_SFLOAT:i==count-2?VK_FORMAT_R8G8B8A8_UNORM:VK_FORMAT_R8G8B8A8_SRGB))throw std::runtime_error("Path readback unexpected image format");
        offset+=VkDeviceSize(m_extent.width)*m_extent.height*(hdr?8:4);
    }
    m_pending=true;m_step=m_owner.step();m_recordedAfter=m_owner.completedSerial();
}
void ViewerPathReadback::inspect() {
    if(!m_pending)return;
    // Swapchain recreation may have waited for GPU idle without publishing the
    // pending statistics yet. Defer to the next fence-safe update; retain the
    // old copy's extent and staging allocation, never label it as a new submit.
    if(m_owner.completedSerial()<=m_recordedAfter)return;
    m_staging->invalidate();auto* data=static_cast<const uint8_t*>(m_staging->map());if(!data)throw std::runtime_error("Path staging map failed");
    try {m_owner.acceptSample(m_step,m_recordedAfter,m_extent,data,m_debug);}
    catch(...) {m_staging->unmap();throw;}
    m_staging->unmap();m_pending=false;
}
void addViewerPathSmoke(Engine& e,SceneDrawSource& source,std::filesystem::path prefix,std::function<void(bool)> variant,uint64_t startAfter,std::function<std::vector<uint64_t>()> state) {
    if(!source.pathControl().deferredAvailable())throw std::runtime_error("Viewer path smoke needs Deferred capability");
    auto& coordinator=e.addPass<ViewerPathSmoke>(e,source,std::move(prefix),std::move(variant),startAfter,std::move(state));
    auto& a=e.addPass<SceneProbe>(source,false);auto& b=e.addPass<SceneProbe>(source,true);
    auto& c=e.addPass<GBufferDisplayPass>(source,VK_FORMAT_R8G8B8A8_UNORM,"ViewerDebugUNORM");auto& d=e.addPass<GBufferDisplayPass>(source,VK_FORMAT_R8G8B8A8_SRGB,"ViewerDebugSRGB");
    auto& scene=e.addPass<ViewerPathReadback>(e,coordinator,false);auto& debug=e.addPass<ViewerPathReadback>(e,coordinator,true);coordinator.bind(a,b,c,d,scene,debug);
}
}
