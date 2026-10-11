#include <gtest/gtest.h>
#include "MclarenSceneEditing.h"
#include "MclarenSceneAsset.h"
#include "MclarenAssetReplacement.h"
#include <limits>

namespace {
const auto fixtures = std::filesystem::path(KUENGINE_SOURCE_DIR) / "tests/fixtures";
ku::MclarenSceneAsset load() {
    ku::MclarenSceneAsset result; std::string error;
    if (!result.load(error, fixtures / "viewer-two-instances.scene.json")) throw std::runtime_error(error);
    return result;
}
const auto prepared = [](const auto&, std::string&) { return true; };
}
TEST(MclarenViewer, StrictSceneCLI) {
    const std::vector<std::string_view> args{"--scene","some.json","--smoke-frames","3"};
    const auto options = ku::parseMclarenCommandLine(args);
    EXPECT_EQ(options.scenePath, "some.json"); EXPECT_EQ(options.forwardedArguments.size(), 2);
    for (const auto& invalid : std::vector<std::vector<std::string_view>>{{"--scene"},{"--scene","--deferred"},{"--scene","a","--scene","b"}})
        EXPECT_THROW((void)ku::parseMclarenCommandLine(invalid), std::invalid_argument);
}
TEST(MclarenViewer, ExplicitSceneFailsWithoutDefaultFallback) {
    ku::MclarenSceneAsset scene; std::string error;
    for (const auto path : {"missing.scene.json","render-path-invalid.json","viewer-bad-model.scene.json","viewer-bad-hdr.scene.json","viewer-empty-node.scene.json"}) {
        EXPECT_FALSE(scene.load(error, fixtures / path)) << path; EXPECT_FALSE(error.empty());
    }
}
TEST(MclarenViewer, SharedMeshAndPayloadReleasePreservesEditingMetadata) {
    auto scene = load(); const auto& data = scene.sceneData();
    ASSERT_EQ(data.meshes.size(),1); ASSERT_EQ(data.instances.size(),2);
    EXPECT_EQ(data.instances[0].mesh, data.instances[1].mesh);
    ASSERT_EQ(data.meshes[0].mesh.subMeshes.size(),3); ASSERT_EQ(data.meshes[0].mesh.materials.size(),2);
    EXPECT_TRUE(scene.environmentImage().valid()); EXPECT_TRUE(scene.environmentPath().empty());
    auto& mesh = scene.sceneData().meshes[0].mesh;
    for(auto& material:mesh.materials) for(auto* texture:{&material.baseColorTexture,&material.normalTexture,&material.metallicRoughnessTexture,&material.occlusionTexture,&material.emissiveTexture}) {
        texture->width=texture->height=2;texture->rgba8.resize(16,255);
    }
    const auto bounds = data.meshes[0].mesh.boundsMax;
    scene.releaseCpuMesh();
    EXPECT_EQ(data.meshes[0].mesh.boundsMax,bounds); EXPECT_EQ(data.meshes[0].mesh.subMeshes.size(),3);
    EXPECT_EQ(data.meshes[0].mesh.materials.size(),2); EXPECT_TRUE(data.meshes[0].mesh.vertices.empty());
    EXPECT_TRUE(data.meshes[0].mesh.indices.empty()); EXPECT_TRUE(scene.environmentImage().rgba32f.empty());
    EXPECT_EQ(mesh.vertices.capacity(),0);EXPECT_EQ(mesh.indices.capacity(),0);EXPECT_EQ(scene.environmentImage().rgba32f.capacity(),0);
    for(const auto& material:mesh.materials) for(const auto* texture:{&material.baseColorTexture,&material.normalTexture,&material.metallicRoughnessTexture,&material.occlusionTexture,&material.emissiveTexture}) {
        EXPECT_EQ(texture->rgba8.capacity(),0);EXPECT_EQ(texture->width,2);
    }
    EXPECT_FLOAT_EQ(ku::editableMaterial(data,0,0).metallic,.2f);
}
TEST(MclarenViewer, InstanceSlotEditsAndUnlitPreserveOtherSlotsAndValues) {
    auto asset = load(); asset.releaseCpuMesh(); auto& scene = asset.sceneData();
    ku::MclarenSceneEditor editor; std::string error;
    auto material = ku::editableMaterial(scene,0,0); material.baseColor={.1f,.8f,.2f,1};
    material.emissive={.2f,.3f,.4f}; material.metallic=.8f; material.roughness=.3f; material.shadingModel=ku::asset::ShadingModel::Unlit;
    auto transform=scene.instances[0].transform; transform.position.y=.2f; transform.scale={.8f,.9f,1.f}; transform.rotationEulerDeg.z=10;
    ASSERT_TRUE(editor.queue({0,transform,material},error));
    EXPECT_FLOAT_EQ(ku::editableMaterial(scene,0,0).metallic,.2f);
    ASSERT_TRUE(editor.publish(scene,prepared,error)); EXPECT_EQ(editor.generation(),1);
    EXPECT_FLOAT_EQ(ku::editableMaterial(scene,0,0).metallic,.8f);
    EXPECT_FLOAT_EQ(ku::editableMaterial(scene,1,0).metallic,.2f);
    EXPECT_FLOAT_EQ(ku::editableMaterial(scene,0,1).metallic,.1f);
    EXPECT_EQ(scene.instances[1].transform.position.y,0);
    for (const auto& primitive:scene.meshes[0].mesh.subMeshes) {
        auto resolved=ku::resolveInstanceMaterial(scene.meshes[0].mesh.materials[primitive.materialIndex],scene.instances[0],primitive.materialIndex);
        if (primitive.materialIndex==0) EXPECT_EQ(resolved.baseColorFactor,material.baseColor);
        else EXPECT_FLOAT_EQ(resolved.metallicFactor,.1f);
    }
    material.shadingModel=ku::asset::ShadingModel::PBR;
    ASSERT_TRUE(editor.queue({0,{},material},error)); ASSERT_TRUE(editor.publish(scene,prepared,error));
    EXPECT_FLOAT_EQ(ku::editableMaterial(scene,0,0).roughness,.3f);
}
TEST(MclarenViewer, InvalidEditsAndCandidatePreparationFailureRetainScene) {
    auto asset=load(); auto& scene=asset.sceneData(); ku::MclarenSceneEditor editor; std::string error;
    auto material=ku::editableMaterial(scene,0,0); auto bad=material; bad.metallic=2;
    EXPECT_FALSE(editor.queue({0,{},bad},error)); bad=material; bad.emissive[0]=std::numeric_limits<float>::infinity();
    EXPECT_FALSE(editor.queue({0,{},bad},error)); auto trs=scene.instances[0].transform; trs.scale.x=0;
    EXPECT_FALSE(editor.queue({0,trs,{}},error));
    ASSERT_TRUE(editor.queue({99,{},material},error)); EXPECT_FALSE(editor.publish(scene,prepared,error));
    bad=material; bad.slot=99; ASSERT_TRUE(editor.queue({0,{},bad},error)); EXPECT_FALSE(editor.publish(scene,prepared,error));
    material.metallic=.9f; ASSERT_TRUE(editor.queue({0,{},material},error));
    EXPECT_FALSE(editor.publish(scene,[](const auto&,std::string& e){e="Controlled CPU preparation failure (not GPU OOM)";return false;},error));
    EXPECT_EQ(editor.generation(),0); EXPECT_TRUE(scene.instances[0].materialSlots.empty());
}
TEST(MclarenViewer, CompleteSecondInstanceEditsRetainFirstInstanceAndOtherSlot) {
    auto asset=load(); asset.releaseCpuMesh(); auto& scene=asset.sceneData();
    ku::MclarenSceneEditor editor; std::string error;
    auto left=ku::editableMaterial(scene,0,0);left.baseColor={.15f,.65f,.25f,1};
    left.metallic=.85f;left.roughness=.2f;left.emissive={.1f,.2f,.05f};
    auto leftTrs=scene.instances[0].transform;leftTrs.position.y=.2f;leftTrs.rotationEulerDeg.z=9;leftTrs.scale={.9f,.95f,1};
    ASSERT_TRUE(editor.queue({0,leftTrs,left},error));ASSERT_TRUE(editor.publish(scene,prepared,error));
    auto right=ku::editableMaterial(scene,1,1);
    for(size_t operation=1;operation<=7;++operation) {
        ku::MclarenSceneEdit edit;edit.instance=1;
        switch(operation) {
        case 1:right.baseColor={.8f,.15f,.6f,1};break;
        case 2:right.metallic=.7f;break;
        case 3:right.roughness=.25f;break;
        case 4:right.emissive={.05f,.1f,.3f};break;
        case 5:right.shadingModel=ku::asset::ShadingModel::Unlit;break;
        case 6:right.shadingModel=ku::asset::ShadingModel::PBR;break;
        case 7:{auto trs=scene.instances[1].transform;trs.position.y=-.2f;trs.rotationEulerDeg.z=-11;trs.scale={1.05f,.9f,1};edit.transform=trs;break;}
        }
        if(operation!=7)edit.material=right;
        ASSERT_TRUE(editor.queue(edit,error));ASSERT_TRUE(editor.publish(scene,prepared,error));
        const auto actual=ku::editableMaterial(scene,1,1);
        EXPECT_EQ(actual.baseColor,right.baseColor);EXPECT_EQ(actual.emissive,right.emissive);
        EXPECT_FLOAT_EQ(actual.metallic,right.metallic);EXPECT_FLOAT_EQ(actual.roughness,right.roughness);
        EXPECT_EQ(actual.shadingModel,right.shadingModel);
        const auto preserved=ku::editableMaterial(scene,0,0);
        EXPECT_EQ(preserved.baseColor,left.baseColor);EXPECT_EQ(preserved.emissive,left.emissive);
        EXPECT_FLOAT_EQ(preserved.metallic,.85f);EXPECT_FLOAT_EQ(preserved.roughness,.2f);
        EXPECT_EQ(preserved.shadingModel,ku::asset::ShadingModel::PBR);
        EXPECT_EQ(scene.instances[0].transform.position,leftTrs.position);
        EXPECT_EQ(scene.instances[0].transform.rotationEulerDeg,leftTrs.rotationEulerDeg);
        EXPECT_EQ(scene.instances[0].transform.scale,leftTrs.scale);
        const auto other=ku::editableMaterial(scene,1,0);
        EXPECT_EQ(other.baseColor,(std::array<float,4>{.6f,.2f,.1f,1}));
        EXPECT_EQ(other.emissive,(std::array<float,3>{0,0,0}));
        EXPECT_FLOAT_EQ(other.metallic,.2f);EXPECT_FLOAT_EQ(other.roughness,.5f);
        EXPECT_TRUE(scene.instances[0].mesh==scene.instances[1].mesh);
    }
    EXPECT_EQ(scene.instances[1].transform.position,(glm::vec3{1.8f,-.2f,0}));
    EXPECT_EQ(scene.instances[1].transform.rotationEulerDeg,(glm::vec3{0,0,-11}));
    EXPECT_EQ(scene.instances[1].transform.scale,(glm::vec3{1.05f,.9f,1}));
    EXPECT_EQ(editor.generation(),8);
}
TEST(MclarenViewer, NonFiniteDerivedTransformIsRejectedBeforePreparation) {
    auto asset=load(); asset.releaseCpuMesh(); auto& scene=asset.sceneData();
    const auto original=scene.instances[0].transform;
    ku::MclarenSceneEditor editor; std::string error; bool preparedCandidate=false;
    const auto prepare=[&](const auto&,std::string&) { preparedCandidate=true; return true; };
    // Inputs are finite/positive, but inverse-normal and transformed bounds
    // must also be representable. Do not relax import SceneData::valid().
    auto transform=original; transform.scale={1.e20f,1.e20f,1.e20f};
    ASSERT_TRUE(editor.queue({0,transform,{}},error));
    EXPECT_FALSE(editor.publish(scene,prepare,error));
    EXPECT_FALSE(preparedCandidate); EXPECT_FALSE(error.empty());
    EXPECT_EQ(editor.generation(),0); EXPECT_EQ(scene.instances[0].transform.scale,original.scale);
    transform=original; transform.scale={std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max()};
    ASSERT_TRUE(editor.queue({0,transform,{}},error));
    EXPECT_FALSE(editor.publish(scene,prepare,error));
    EXPECT_FALSE(preparedCandidate); EXPECT_EQ(editor.generation(),0);
    EXPECT_EQ(scene.instances[0].transform.scale,original.scale);
    EXPECT_FALSE(scene.valid()); // payload-free editing is not import validity
}
TEST(MclarenViewer, ClearIsIndependentRequestWithTransactionalStatus) {
    ku::MclarenReplacementController controller; ku::MclarenPublishedAssetState live{};
    live.environmentPath="live.hdr";live.environmentGeneration=3;live.environmentIdentity=8;controller.initialize(live);
    std::string error; EXPECT_FALSE(controller.queue(ku::MclarenAssetKind::ClearEnvironment,"bad",&error));
    ASSERT_TRUE(controller.queue(ku::MclarenAssetKind::ClearEnvironment,{},&error)); auto request=*controller.takePending();
    controller.publishFailure(request,ku::MclarenReplacementStage::GpuUpload,ku::MclarenReplacementCategory::GpuUploadAllocation,"controlled");
    EXPECT_EQ(controller.live(),live);
    controller.publishSuccess(request,{},9,0,0,false,live.modelGlobalBaseColorFactor);
    EXPECT_TRUE(controller.live().environmentPath.empty()); EXPECT_EQ(controller.live().environmentGeneration,4);
    std::filesystem::path path; EXPECT_FALSE(ku::resolveMclarenAssetPath(request.kind,{},fixtures,path,error));
}
