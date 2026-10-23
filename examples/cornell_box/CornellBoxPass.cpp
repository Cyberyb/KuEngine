#include "CornellBoxPass.h"

#include <KuEngine/Core/Log.h>
#include <KuEngine/RHI/ResourceUploader.h>
#include <KuEngine/Render/ForwardGraphTargets.h>
#include <KuEngine/Render/ForwardProgram.h>
#include <KuEngine/Render/ForwardRenderer.h>
#include <KuEngine/Render/GpuModelAsset.h>
#include <KuEngine/Render/PBRResources.h>
#include <KuEngine/Render/TextureFactory.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ku {
namespace {
size_t phaseIndex(SceneDrawPhase phase) { return static_cast<size_t>(phase); }
}
CornellBoxPass::CornellBoxPass(asset::RenderPath path) : m_initialPath(path),m_mesh(cornell::makeScene()) {}
CornellBoxPass::~CornellBoxPass()
{
    for (auto& renderer : m_renderers) renderer.reset();
    m_materials.reset(); m_environment.reset(); m_model.reset();
    m_geometryProgram.reset(); m_program.reset();
}
ForwardRenderer& CornellBoxPass::sceneRenderer(SceneDrawPhase phase)
{
    return *m_renderers.at(phaseIndex(phase));
}
void CornellBoxPass::initialize(const RenderContext& context)
{
    if (m_program && m_renderers[0] && m_renderers[0]->ready()) return;
    std::string error;
    ResourceUploader uploader(context.device);
    TextureFactory textures(context.device,uploader);
    m_program = std::make_unique<ForwardProgram>();
    if (!m_program->initialize(context.device,sceneHdrFormat,context.depthFormat,context.depthCompareOp,error))
        throw std::runtime_error(error);
    std::vector<ForwardPipelineKey> keys;
    for (const auto& material : m_mesh.materials) keys.push_back(pipelineKeyFor(resolveMaterial(material)));
    if (!m_program->ensurePipelines(keys,error)) throw std::runtime_error(error);
    if (pathControl().deferredAvailable()) {
        m_geometryProgram = std::make_unique<ForwardProgram>();
        if (!m_geometryProgram->initializeGeometry(context.device,context.depthFormat,context.depthCompareOp,error)
            || !m_geometryProgram->ensurePipelines(keys,error)) throw std::runtime_error(error);
    }
    m_model = std::make_unique<GpuModelAsset>();
    if (!m_model->initialize(context.device,uploader,m_mesh,error)) throw std::runtime_error(error);
    m_materials = std::make_unique<PBRMaterialResources>();
    if (!m_materials->initialize(context.device,textures,m_mesh,nullptr,error,m_program->setLayout(forward_set::material)))
        throw std::runtime_error(error);
    asset::HDRImageData neutral;
    neutral.width = neutral.height = 1;
    neutral.rgba32f = {1,1,1,1};
    m_environment = std::make_unique<PBREnvironmentResources>();
    if (!m_environment->initialize(context.device,textures,neutral,error,m_program->setLayout(forward_set::environment)))
        throw std::runtime_error(error);
    for (size_t i=0;i<m_renderers.size();++i) {
        if (i == phaseIndex(SceneDrawPhase::Geometry) && !m_geometryProgram) continue;
        m_renderers[i] = std::make_unique<ForwardRenderer>();
        auto& program = i == phaseIndex(SceneDrawPhase::Geometry) ? *m_geometryProgram : *m_program;
        if (!m_renderers[i]->initialize(context.device,program,m_mesh.subMeshes.size(),error, context.framesInFlight)) throw std::runtime_error(error);
    }
    pathControl().initialize(m_initialPath);
    KU_INFO("KUENGINE_CORNELL_SCENE path={} procedural=1 submeshes={} indices={} pointLights=4 model=PBR",
        asset::toString(pathControl().path()),m_mesh.subMeshes.size(),m_mesh.indices.size());
}
void CornellBoxPass::update(const FrameData&) { pathControl().publishPending(); }
void CornellBoxPass::execute(CommandList&,const FrameData& frame)
{
    const auto camera = cornell::defaultCamera();
    const float yaw = glm::radians(m_cameraYaw), pitch = glm::radians(m_cameraPitch);
    const glm::vec3 eye = camera.target + m_cameraDistance*glm::vec3(std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch));
    const auto rect = frame.viewerLayout.sceneFramebuffer;
    const float aspect = rect.height ? float(rect.width)/float(rect.height) : 1.f;
    auto projection = glm::perspectiveRH_ZO(glm::radians(camera.fovYDeg),std::max(aspect,.01f),camera.nearPlane,camera.farPlane);
    projection[1][1] *= -1;
    ForwardView view;
    view.viewProjection = projection*glm::lookAt(eye,camera.target,camera.up);
    view.inverseViewProjection = glm::inverse(view.viewProjection);
    view.cameraPosition = eye;
    view.lighting = cornell::makeLighting(m_lightStrength);
    view.environment = m_environment.get();
    view.environmentEnabled = m_fill > 0;
    view.environmentIntensity = m_fill;
    view.environmentExposure = m_exposure;
    if (m_verifyFrameIsolation) {
        view.environmentEnabled = false;
        view.lighting = cornell::makeLighting(frame.submittedFrame % 2 ? 0.f : 1.f);
    }
    std::vector<ForwardDraw> draws;
    for (size_t i=0;i<m_mesh.subMeshes.size();++i) {
        const auto& part = m_mesh.subMeshes[i];
        ForwardDraw draw;
        draw.model = m_model.get(); draw.materials = m_materials.get();
        draw.subMeshIndex = static_cast<uint32_t>(i); draw.materialIndex = part.materialIndex;
        draw.material = resolveMaterial(m_mesh.materials[part.materialIndex]);
        if (m_verifyFrameIsolation && part.materialIndex == cornell::RedWall)
            draw.material.baseColorFactor[0] = frame.submittedFrame % 2 ? .63f : .875f;
        draws.push_back(draw);
    }
    m_snapshot = {view,std::move(draws),frame.totalTime};
}
void CornellBoxPass::drawUI()
{
    pathControl().drawUI();
    ImGui::TextWrapped("Procedural Cornell box: red / green walls, two white boxes and an emissive ceiling panel.");
    ImGui::SliderFloat("Ceiling light",&m_lightStrength,0,3,"%.2f");
    ImGui::SliderFloat("Exposure",&m_exposure,.1f,3,"%.2f");
    ImGui::SliderFloat("Neutral fill",&m_fill,0,.5f,"%.2f");
    const char* boxes[] = {"Short box","Tall box"};
    ImGui::Combo("Material",&m_selectedBox,boxes,2);
    auto& material = m_mesh.materials[m_selectedBox == 0 ? cornell::ShortBox : cornell::TallBox];
    ImGui::SliderFloat("Roughness",&material.roughnessFactor,.04f,1,"%.2f");
    ImGui::SliderFloat("Metallic",&material.metallicFactor,0,1,"%.2f");
    ImGui::Separator();
    ImGui::SliderFloat("Camera yaw",&m_cameraYaw,-16,16,"%.1f deg");
    ImGui::SliderFloat("Camera pitch",&m_cameraPitch,-12,12,"%.1f deg");
    ImGui::SliderFloat("Camera distance",&m_cameraDistance,3.2f,5.5f,"%.2f");
    if (ImGui::Button("Reset view")) {m_cameraYaw=0;m_cameraPitch=0;m_cameraDistance=4.1f;}
    ImGui::TextWrapped("Direct PBR lighting uses four point lights. Neutral fill is an approximation; shadows and global illumination are not implemented.");
}
} // namespace ku
