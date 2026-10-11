#include "MclarenEditSmoke.h"
#include <KuEngine/Core/Log.h>
#include <KuEngine/Core/Window.h>
#include <KuEngine/UI/UIOverlay.h>
#include <KuEngine/Render/ForwardGraphTargets.h>
#include <KuEngine/Render/ForwardProgram.h>
#include <KuEngine/Render/ForwardRenderer.h>
#include <KuEngine/Render/GpuModelAsset.h>
#include <KuEngine/Render/GpuMesh.h>
#include <KuEngine/Render/PBRResources.h>
#include <KuEngine/RHI/RHIBuffer.h>
#include <KuEngine/RHI/CommandList.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <glm/gtc/type_ptr.hpp>

namespace ku {
namespace {
constexpr size_t maximumPixels=2048*2048;
enum Step : size_t {
    Baseline, LeftBase, LeftMetallic, LeftRoughness, LeftEmissive, LeftUnlit, LeftPbr, LeftTrs,
    RightBase, RightMetallic, RightRoughness, RightEmissive, RightUnlit, RightPbr, RightTrs,
    EditedForward, EditedDeferred, RightUnlitAgain, UnlitForward, UnlitDeferred, RightPbrAgain,
    LoadA, BadHdr, Clear, ClearForward, ClearGBuffer, ResizeRecompile, LoadB, BadEdit, Count
};
bool editing(size_t step) { return (step>=LeftBase&&step<=RightTrs)||step==RightUnlitAgain||step==RightPbrAgain; }
size_t editInstance(size_t step) { return step>=RightBase?1:0; }
size_t editOperation(size_t step) {
    if(step==RightUnlitAgain)return 5;
    if(step==RightPbrAgain)return 6;
    return (step-1)%7+1;
}
template<class T> void append(std::vector<uint64_t>& out, const T& v) {
    const auto* data=reinterpret_cast<const uint8_t*>(&v);
    for(size_t i=0;i<sizeof(v);++i) out.push_back(data[i]);
}
void require(bool value, const char* reason) { if(!value) throw std::runtime_error(reason); }
// Independent sky-only sampling of the active descriptor, even while normal
// contribution toggles are off. Own UBO prevents any phase offset overwrite.
class EnvironmentProbe final : public RenderPass {
public:
    explicit EnvironmentProbe(MclarenPass& source):source(source) {}
    std::string_view name() const override { return "MclarenEnvironmentProbe"; }
    void initialize(const RenderContext& c) override {
        color=makeForwardGraphTargets(c).color;
        if(renderer.ready()) return;
        std::string error;
        require(program.initialize(c.device,sceneHdrFormat,c.depthFormat,c.depthCompareOp,error),error.c_str());
        require(renderer.initialize(c.device,program,1,error),error.c_str());
    }
    void setup(RenderGraphBuilder& b) override {
        source.declarePrepareDependency(b);
        b.colorAttachment(b.createImage(name(),color),AttachmentLoadPolicy::Clear,AttachmentStorePolicy::Store);
    }
    void execute(CommandList& cmd,const FrameData& f) override {
        auto view=source.checkedSceneSnapshot(f).view;view.skyboxEnabled=true;
        std::string error;require(renderer.render(cmd,view,{},error),error.c_str());
    }
    std::optional<CommandListStatistics> expectedFrameStatistics() const override {return CommandListStatistics{1,3};}
private:
    MclarenPass& source;ImageDesc color{};ForwardProgram program;ForwardRenderer renderer;
};
}
class MclarenEditReadback final : public RenderPass {
public:
    MclarenEditReadback(Engine& e,MclarenEditSmoke& owner,bool mrt):engine(e),owner(owner),mrt(mrt) {}
    std::string_view name()const override{return mrt?"MclarenEditMrtReadback":"MclarenEditHdrReadback";}
    bool requiresOutsideRenderingScope()const noexcept override{return true;}
    void initialize(const RenderContext& c) override {
        if(staging) return;
        hdr=makeForwardGraphTargets(c).color;targets=makeGBufferTargets(c.depthFormat,c.clearDepthStencil);
        RHIBuffer::CreateInfo info{};info.size=VkDeviceSize(maximumPixels)*(mrt?32:16);
        info.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;info.allocationFlags=VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT|VMA_ALLOCATION_CREATE_MAPPED_BIT;
        staging=std::make_unique<RHIBuffer>(c.device,info);
    }
    void setup(RenderGraphBuilder& b)override {
        if(mrt) for(size_t i=0;i<4;++i) images[i]=b.createImage(gbufferNames[i],targets.colors[i]);
        else {images[0]=b.createImage(forward_graph_resource::sceneColor,hdr);images[1]=b.createImage("MclarenEnvironmentProbe",hdr);}
        for(size_t i=0;i<(mrt?4u:2u);++i)b.useImage(images[i],ImageUse::TransferSource);
        buffer=b.importBuffer(name(),BufferDesc{VkDeviceSize(maximumPixels)*(mrt?32:16),VK_BUFFER_USAGE_TRANSFER_DST_BIT,VMA_MEMORY_USAGE_AUTO,InitialContent::Undefined});
        b.useBuffer(buffer,BufferUse::TransferDestination);
    }
    void update(const FrameData&)override {
        engine.renderPipeline().bindExternalBuffer({.resourceName=name(),.buffer=staging->buffer(),.size=VkDeviceSize(maximumPixels)*(mrt?32:16),.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .initialState={VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT,VK_IMAGE_LAYOUT_UNDEFINED},
            .finalState={VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT,VK_IMAGE_LAYOUT_UNDEFINED},.contentsValid=false});
    }
    void execute(CommandList& cmd,const FrameData&,RenderGraphResourceResolver& r)override {
        if(!owner.capture())return;
        require(!pending,"Edit staging overwritten before completed submit");
        const auto destination=r.resolveBuffer(buffer,BufferUse::TransferDestination);
        VkDeviceSize offset=0;
        for(size_t i=0;i<(mrt?4u:2u);++i) {
            const auto image=r.resolveImage(images[i],ImageUse::TransferSource);
            require(image.complete() && image.contentsValid && image.format==sceneHdrFormat && size_t(image.extent.width)*image.extent.height<=maximumPixels,"Invalid bounded edit capture");
            if(i)require(image.extent.width==extent.width&&image.extent.height==extent.height,"Capture extent mismatch");
            extent=image.extent;VkBufferImageCopy copy{};copy.bufferOffset=offset;copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={extent.width,extent.height,1};
            vkCmdCopyImageToBuffer(cmd,image.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,destination.buffer,1,&copy);
            offset+=VkDeviceSize(extent.width)*extent.height*8;
        }
        after=owner.completed();step=owner.step();bytes=static_cast<size_t>(offset);pending=true;
    }
    void inspect() {
        if(!pending||owner.completed()<=after)return;
        staging->invalidate();const auto* data=static_cast<const uint8_t*>(staging->map());require(data,"Edit staging map failed");
        try{owner.sample(mrt,extent,data,bytes,after,step);}catch(...){staging->unmap();throw;}
        staging->unmap();pending=false;
    }
    std::optional<CommandListStatistics> expectedFrameStatistics()const override{return CommandListStatistics{0,0};}
private:
    Engine& engine;MclarenEditSmoke& owner;bool mrt,pending=false;
    std::unique_ptr<RHIBuffer> staging;ImageDesc hdr{};GBufferTargets targets{};
    std::array<ImageHandle,4> images{};BufferHandle buffer{};VkExtent2D extent{};
    uint64_t after=0;size_t step=0,bytes=0;
};
MclarenEditSmoke::MclarenEditSmoke(Engine& e,MclarenPass& s,std::filesystem::path prefix):m_engine(e),m_source(s),m_prefix(std::move(prefix)) {
    require(!m_prefix.parent_path().empty(),"Edit prefix requires a directory");
}
uint64_t MclarenEditSmoke::completed()const {const auto& c=m_engine.completedFrameStatistics();return c?c->submittedFrame:0;}
void MclarenEditSmoke::bind(MclarenEditReadback& a,MclarenEditReadback& b){m_readbacks={&a,&b};}
void MclarenEditSmoke::requestStep() {
    m_after=completed();m_editGeneration=m_source.editGeneration();m_environmentGeneration=m_source.publishedAssets().environmentGeneration;m_oldToken=m_source.environmentToken();
    const auto path=(m_step==EditedForward||m_step==UnlitForward||m_step==ClearForward)?asset::RenderPath::Forward:asset::RenderPath::Deferred;
    require(m_source.pathControl().request(path,m_step==ClearGBuffer?GBufferView::ShadingModel:GBufferView::Scene),"Edit test path request failed");
    if(editing(m_step)) {
        const size_t instance=editInstance(m_step),operation=editOperation(m_step);
        auto material=editableMaterial(m_source.editableScene(),instance,static_cast<uint32_t>(instance));
        MclarenSceneEdit edit;edit.instance=instance;
        switch(operation) {
        case 1:material.baseColor=instance?std::array<float,4>{.8f,.15f,.6f,1}:std::array<float,4>{.15f,.65f,.25f,1};break;
        case 2:material.metallic=instance?.7f:.85f;break;
        case 3:material.roughness=instance?.25f:.2f;break;
        case 4:material.emissive=instance?std::array<float,3>{.05f,.1f,.3f}:std::array<float,3>{.1f,.2f,.05f};break;
        case 5:material.shadingModel=asset::ShadingModel::Unlit;break;
        case 6:material.shadingModel=asset::ShadingModel::PBR;break;
        case 7:{auto transform=m_source.editableScene().instances[instance].transform;transform.position.y=instance?-.2f:.2f;transform.rotationEulerDeg.z=instance?-11.f:9.f;transform.scale=instance?glm::vec3{1.05f,.9f,1}:glm::vec3{.9f,.95f,1};edit.transform=transform;break;}
        }
        if(operation!=7)edit.material=material;
        require(m_source.requestEdit(edit),"Edit test request rejected");
    }
    const auto source=std::filesystem::path(KUENGINE_SOURCE_DIR);
    if(m_step==LoadA||m_step==LoadB) {
        m_oldToken=m_source.environmentToken();
        require(m_source.requestAsset(MclarenAssetKind::Environment,(source/"resources/environments/hdr"/(m_step==LoadA?"citrus_orchard_road_puresky_4k.hdr":"derelict_airfield_01_4k.hdr")).string()),"HDR request rejected");
    }
    if(m_step==BadHdr) require(m_source.requestAsset(MclarenAssetKind::Environment,(source/"tests/fixtures/invalid-replacement.hdr").string()),"Bad HDR queue failed");
    if(m_step==Clear) {m_oldToken=m_source.environmentToken();require(m_source.requestAsset(MclarenAssetKind::ClearEnvironment),"Clear queue failed");}
    if(m_step==ResizeRecompile) {m_engine.compile();KU_INFO("KUENGINE_EDIT_RECOMPILE_AFTER_CLEAR state_preserved=1");}
    if(m_step==BadEdit) {auto mat=editableMaterial(m_source.editableScene(),0,0);require(m_source.requestEdit({999,{},mat}),"Invalid edit queue failed");}
    m_requested=true;m_hdrDone=false;m_mrtDone=path==asset::RenderPath::Forward;
    KU_INFO("KUENGINE_EDIT_REQUEST step={} afterCompletedSubmit={}",m_step,m_after);
}
void MclarenEditSmoke::update(const FrameData& frame) {
    for(auto* readback:m_readbacks){readback->inspect();readback->update(frame);}
    m_capture=false;
    if(m_done)return;
    if(!m_requested && completed()>=2)requestStep();
    else if(m_requested&&completed()>m_after) {
        m_capture=true;
        if(editing(m_step))require(m_source.editGeneration()==m_editGeneration+1,"Edit not published at next fence");
        if(m_step!=LoadA&&m_step!=Clear&&m_step!=LoadB)
            require(m_source.publishedAssets().environmentGeneration==m_environmentGeneration&&m_source.environmentToken()==m_oldToken,"Edit/path/recompile/failed operation reuploaded the HDR resource");
        if(m_step==LoadA||m_step==Clear||m_step==LoadB) {
            require(m_source.publishedAssets().environmentGeneration==m_environmentGeneration+1,"Environment candidate not published");
            require(m_source.environmentTokenReleased(m_oldToken),"Old environment GPU ownership not released");
        }
        if(m_step==BadHdr)require(m_source.publishedAssets().environmentGeneration==m_environmentGeneration,"Bad HDR changed live state");
        if(m_step==BadEdit)require(m_source.editGeneration()==m_editGeneration,"Bad edit published");
    }
    m_readbacks[1]->setEnabled(m_source.pathControl().path()==asset::RenderPath::Deferred);
}
void MclarenEditSmoke::checkSnapshot(const SceneRenderSnapshot& s) {
    require(m_source.editableScene().instances.size()==2 && m_source.editableScene().meshes.size()==1 && s.draws.size()==6,"Final viewer must share one multi-slot mesh between two instances");
    std::vector<uint64_t> gpu,preserved;
    append(preserved,s.view.cameraPosition);append(preserved,s.view.lighting.direction);append(preserved,s.view.lighting.color);append(preserved,s.view.lighting.intensity);
    for(const auto& p:s.view.lighting.pointLights){append(preserved,p.position);append(preserved,p.color);append(preserved,p.intensity);append(preserved,p.range);}
    append(preserved,m_engine.ui().sidebarExpanded());
    for(size_t i=0;i<s.draws.size();++i) {
        const auto& d=s.draws[i];append(gpu,d.model);append(gpu,d.model->mesh().vertexBuffer().buffer());append(gpu,d.model->mesh().indexBuffer().buffer());append(gpu,d.materials);append(gpu,d.materials->material(d.materialIndex).descriptorSet);
        require(d.model==s.draws[0].model,"Mesh reuploaded per instance");
        const size_t instance=i/3;const auto slot=d.materialIndex;
        const auto expected=editableMaterial(m_source.editableScene(),instance,slot);
        require(d.material.baseColorFactor==expected.baseColor&&d.material.emissiveFactor==expected.emissive&&d.material.metallicFactor==expected.metallic&&d.material.roughnessFactor==expected.roughness&&d.material.shadingModel==expected.shadingModel,"Snapshot lost SceneData slot semantics");
        if(!m_previousMaterials.empty()&&editing(m_step)) {
            const size_t selectedInstance=editInstance(m_step);
            const bool selected=instance==selectedInstance&&(editOperation(m_step)==7||slot==selectedInstance);
            if(!selected) {
                const auto& old=m_previousMaterials[i];require(old.baseColorFactor==d.material.baseColorFactor&&old.emissiveFactor==d.material.emissiveFactor&&old.metallicFactor==d.material.metallicFactor&&old.roughnessFactor==d.material.roughnessFactor&&old.shadingModel==d.material.shadingModel&&m_previousTransforms[i]==d.modelMatrix,"Unselected instance/slot changed");
            }
        }
    }
    if(m_gpuIdentity.empty())m_gpuIdentity=gpu;else require(gpu==m_gpuIdentity,"Scalar/TRS/environment operation reuploaded mesh or textures");
    if(m_preserved.empty())m_preserved=preserved;else require(preserved==m_preserved,"Asset operation changed camera/light/sidebar");
    if(m_step>=Clear&&m_step<=ResizeRecompile)require(m_source.publishedAssets().environmentPath.empty()&&!s.view.skyboxEnabled&&!s.view.environmentEnabled,"Clear did not disable contributions/empty path");
    if(m_step==LoadB)require(!m_source.publishedAssets().environmentPath.empty()&&s.view.skyboxEnabled&&s.view.environmentEnabled,"LoadB did not restore contributions");
    m_previousMaterials.clear();m_previousTransforms.clear();for(const auto& d:s.draws){m_previousMaterials.push_back(d.material);m_previousTransforms.push_back(d.modelMatrix);}
}
void MclarenEditSmoke::execute(CommandList&,const FrameData& f){if(m_capture){
    const auto& snapshot=m_source.checkedSceneSnapshot(f);
    checkSnapshot(snapshot);
    std::copy_n(glm::value_ptr(snapshot.view.viewProjection),16,m_captureMatrices.begin());
    for(size_t i=0;i<6;++i)std::copy_n(glm::value_ptr(snapshot.draws[i].modelMatrix),16,m_captureMatrices.begin()+16+i*16);
    const auto& rect=f.viewerLayout.sceneFramebuffer;
    m_captureViewport={static_cast<uint32_t>(rect.x),static_cast<uint32_t>(rect.y),rect.width,rect.height};
}}
void MclarenEditSmoke::sample(bool mrt,VkExtent2D extent,const uint8_t* data,size_t bytes,uint64_t after,size_t step) {
    require(step==m_step&&completed()==after+1,"Edit readback not actual completed submission");
    std::filesystem::create_directories(m_prefix.parent_path());
    std::ofstream out(m_prefix.string()+"-step"+std::to_string(step)+(mrt?".mrtbin":".hdrbin"),std::ios::binary);
    out.write(reinterpret_cast<const char*>(&extent.width),4);out.write(reinterpret_cast<const char*>(&extent.height),4);out.write(reinterpret_cast<const char*>(data),bytes);require(bool(out),"Edit evidence write failed");
    if(!mrt){std::ofstream roi(m_prefix.string()+"-step"+std::to_string(step)+".roi",std::ios::binary);roi.write(reinterpret_cast<const char*>(m_captureViewport.data()),16);require(bool(roi),"Viewport evidence write failed");
        std::ofstream matrices(m_prefix.string()+"-step"+std::to_string(step)+".matrices",std::ios::binary);matrices.write(reinterpret_cast<const char*>(m_captureMatrices.data()),sizeof(m_captureMatrices));require(bool(matrices),"Snapshot matrices evidence write failed");}
    if(mrt)m_mrtDone=true;else m_hdrDone=true;
    if(!m_hdrDone||!m_mrtDone)return;
    KU_INFO("KUENGINE_EDIT_SAMPLE_OK step={} completedSubmit={} extent={}x{} sharedMesh=1 noMeshUpload=1",step,completed(),extent.width,extent.height);
    m_requested=false;m_capture=false;++m_step;
    if(m_step==Count){m_done=true;KU_INFO("KUENGINE_EDIT_SMOKE_OK samples={} edits=16 instances=2 completedSubmit={} sharedMesh=1 clearReleased=1",static_cast<size_t>(Count),completed());}
}
void addMclarenEditSmoke(Engine& e,MclarenPass& source,const std::filesystem::path& prefix) {
    auto& owner=e.addPass<MclarenEditSmoke>(e,source,prefix);
    e.addPass<EnvironmentProbe>(source);
    auto& hdr=e.addPass<MclarenEditReadback>(e,owner,false);auto& mrt=e.addPass<MclarenEditReadback>(e,owner,true);owner.bind(hdr,mrt);
}
}
