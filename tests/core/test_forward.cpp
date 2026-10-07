#include <gtest/gtest.h>

#include <KuEngine/Asset/AssetConfig.h>
#include <KuEngine/Asset/Model.h>
#include <KuEngine/Asset/Scene.h>
#include <KuEngine/Render/ForwardCommon.h>
#include <KuEngine/Render/PBRResources.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <filesystem>
#include <limits>
#include <string_view>

namespace {
std::filesystem::path sourcePath(const char* relative)
{
    return std::filesystem::path(KUENGINE_SOURCE_DIR) / relative;
}
}

TEST(ForwardAssetTest, LoadsGltfMaterialSemanticsAndIndependentTransforms)
{
    const ku::asset::MeshData mesh = ku::asset::ModelLoader::loadFromFile(
        sourcePath("tests/fixtures/material-semantics.gltf"));
    ASSERT_EQ(mesh.materials.size(), 3u);
    const auto& material = mesh.materials[0];
    EXPECT_EQ(material.shadingModel, ku::asset::ShadingModel::Unlit);
    EXPECT_EQ(material.alphaMode, ku::asset::AlphaMode::Mask);
    EXPECT_FLOAT_EQ(material.alphaCutoff, 0.37f);
    EXPECT_TRUE(material.doubleSided);
    EXPECT_TRUE(material.metallicRoughnessTexture.valid());
    EXPECT_TRUE(material.occlusionTexture.valid());
    EXPECT_TRUE(material.emissiveTexture.valid());
    EXPECT_FLOAT_EQ(material.metallicRoughnessTransform.offset.x, 0.5f);
    EXPECT_FLOAT_EQ(material.metallicRoughnessTransform.rotation, 0.1f);
    EXPECT_EQ(material.metallicRoughnessTransform.texCoord, 1u);
    EXPECT_FLOAT_EQ(material.occlusionTransform.offset.x, 0.3f);
    EXPECT_FLOAT_EQ(material.occlusionTransform.rotation, 0.2f);
    EXPECT_EQ(material.occlusionTransform.texCoord, 0u);
    EXPECT_FLOAT_EQ(material.emissiveTransform.offset.x, 0.11f);
    EXPECT_FLOAT_EQ(material.emissiveTransform.rotation, 0.4f);
    EXPECT_EQ(material.emissiveTransform.texCoord, 1u);
    EXPECT_EQ(mesh.materials[1].alphaMode, ku::asset::AlphaMode::Blend);
    EXPECT_EQ(mesh.materials[2].alphaMode, ku::asset::AlphaMode::Opaque);
}

TEST(ForwardAssetTest, SceneConfigKeepsSharedHandleTrsAndOverrides)
{
    ku::asset::SceneConfig config{};
    std::string error;
    const auto directory = sourcePath("examples/forward_reuse/assets");
    ASSERT_TRUE(ku::asset::loadSceneConfigFromFile(
        directory / "forward-reuse.scene.json", config, &error)) << error;
    ku::asset::SceneLoadDescription description{};
    description.resourcesRoot = directory;
    description.config = config;
    ku::asset::SceneData scene;
    ASSERT_TRUE(ku::asset::SceneLoader::load(description, scene, &error)) << error;
    ASSERT_EQ(scene.meshes.size(), 1u);
    ASSERT_EQ(scene.instances.size(), 2u);
    EXPECT_EQ(scene.instances[0].mesh, scene.instances[1].mesh);
    EXPECT_NE(scene.instances[0].transform.position.x,
        scene.instances[1].transform.position.x);
    EXPECT_EQ(scene.instances[0].materialOverride.shadingModel,
        ku::asset::ShadingModel::PBR);
    EXPECT_EQ(scene.instances[1].materialOverride.shadingModel,
        ku::asset::ShadingModel::Unlit);
    EXPECT_EQ(scene.lighting.pointLights.size(), 1u);
}

TEST(ForwardAssetTest, ReuseFixtureFeedsEveryMaterialSemanticIntoDrawPlan)
{
    const auto directory = sourcePath("examples/forward_reuse/assets");
    ku::asset::SceneConfig config{};
    std::string error;
    ASSERT_TRUE(ku::asset::loadSceneConfigFromFile(
        directory / "forward-reuse.scene.json", config, &error)) << error;
    ku::asset::SceneLoadDescription description{};
    description.resourcesRoot = directory;
    description.config = config;
    ku::asset::SceneData scene;
    ASSERT_TRUE(ku::asset::SceneLoader::load(description, scene, &error)) << error;
    ASSERT_EQ(scene.meshes.size(), 1u);
    ASSERT_EQ(scene.instances.size(), 2u);
    const ku::asset::MeshData& mesh = scene.meshes.front().mesh;
    ASSERT_EQ(mesh.materials.size(), 4u);
    ASSERT_EQ(mesh.subMeshes.size(), 4u);
    EXPECT_EQ(mesh.indices.size(), 21u);

    const auto& semantic = mesh.materials[0];
    EXPECT_TRUE(semantic.metallicRoughnessTexture.valid());
    EXPECT_TRUE(semantic.occlusionTexture.valid());
    EXPECT_TRUE(semantic.emissiveTexture.valid());
    EXPECT_NE(semantic.metallicRoughnessTexture.rgba8,
        semantic.occlusionTexture.rgba8);
    EXPECT_NE(semantic.occlusionTexture.rgba8,
        semantic.emissiveTexture.rgba8);
    EXPECT_EQ(semantic.metallicRoughnessTransform.texCoord, 1u);
    EXPECT_EQ(semantic.occlusionTransform.texCoord, 0u);
    EXPECT_EQ(semantic.emissiveTransform.texCoord, 1u);
    EXPECT_FLOAT_EQ(semantic.metallicRoughnessTransform.rotation, 0.35f);
    EXPECT_FLOAT_EQ(semantic.occlusionTransform.rotation, -0.15f);
    EXPECT_FLOAT_EQ(semantic.emissiveTransform.rotation, -0.25f);
    EXPECT_EQ(mesh.materials[1].alphaMode, ku::asset::AlphaMode::Mask);
    EXPECT_FLOAT_EQ(mesh.materials[1].alphaCutoff, 0.5f);
    ASSERT_TRUE(mesh.materials[1].baseColorTexture.valid());
    EXPECT_NE(std::find(mesh.materials[1].baseColorTexture.rgba8.begin(),
                  mesh.materials[1].baseColorTexture.rgba8.end(), 0u),
        mesh.materials[1].baseColorTexture.rgba8.end());
    EXPECT_EQ(mesh.materials[2].alphaMode, ku::asset::AlphaMode::Blend);
    EXPECT_TRUE(mesh.materials[3].doubleSided);

    const ku::MaterialGpuVariantPlan variants =
        ku::buildMaterialGpuVariantPlan(scene.instances);
    ASSERT_EQ(variants.variants.size(), 2u);
    ASSERT_EQ(variants.instanceVariantIndices.size(), 2u);
    EXPECT_NE(variants.instanceVariantIndices[0],
        variants.instanceVariantIndices[1]);
    const auto& unlitVariant = variants.variants[
        variants.instanceVariantIndices[1]];
    EXPECT_TRUE(unlitVariant.textureBindings[4].hasSource);
    EXPECT_EQ(unlitVariant.textureBindings[4].source, "disabled");

    std::vector<ku::ForwardPipelineKey> pipelineKeys;
    std::vector<ku::ForwardDraw> drawPlan;
    std::vector<ku::ForwardSortKey> sortKeys;
    const glm::vec3 cameraPosition(0.0f, 0.0f, 3.2f);
    for (const ku::asset::SceneInstance& instance : scene.instances) {
        const glm::mat4 world = ku::asset::sceneTransformMatrix(instance.transform);
        const glm::vec3 center = glm::vec3(world * glm::vec4(
            0.5f * (mesh.boundsMin + mesh.boundsMax), 1.0f));
        const float distanceSquared = glm::dot(
            center - cameraPosition, center - cameraPosition);
        for (const ku::asset::SubMeshData& subMesh : mesh.subMeshes) {
            ASSERT_LT(subMesh.materialIndex, mesh.materials.size());
            const ku::ResolvedMaterial material = ku::resolveMaterial(
                mesh.materials[subMesh.materialIndex],
                &instance.materialOverride);
            ku::ForwardDraw draw{};
            draw.subMeshIndex = static_cast<uint32_t>(drawPlan.size() % 4u);
            draw.materialIndex = subMesh.materialIndex;
            draw.modelMatrix = world;
            draw.boundsCenterWorld = center;
            draw.material = material;
            drawPlan.push_back(draw);
            pipelineKeys.push_back(ku::pipelineKeyFor(draw.material));
            sortKeys.push_back({material.alphaMode, distanceSquared,
                sortKeys.size()});
        }
    }
    ASSERT_EQ(pipelineKeys.size(), 8u);
    EXPECT_EQ(pipelineKeys[0].shadingModel, ku::asset::ShadingModel::PBR);
    EXPECT_EQ(pipelineKeys[4].shadingModel, ku::asset::ShadingModel::Unlit);
    EXPECT_EQ(pipelineKeys[1].alphaMode, ku::asset::AlphaMode::Mask);
    EXPECT_EQ(pipelineKeys[2].alphaMode, ku::asset::AlphaMode::Blend);
    EXPECT_TRUE(pipelineKeys[3].doubleSided);
    EXPECT_EQ(drawPlan[0].material.textureTransforms[2].texCoord, 1u);
    EXPECT_FLOAT_EQ(
        drawPlan[0].material.textureTransforms[3].rotation, -0.15f);
    EXPECT_FLOAT_EQ(
        drawPlan[0].material.textureTransforms[4].rotation, -0.25f);
    EXPECT_EQ(ku::buildForwardDrawOrder(sortKeys),
        (std::vector<size_t>{0,1,3,4,5,7,2,6}));
}

TEST(ForwardMaterialVariantTest,
    MapsTextureSourcesByInstanceIdIndependentOfSceneOrderAndDeduplicates)
{
    ku::asset::SceneInstance a{};
    a.id = "A";
    a.mesh = 7;
    a.materialReference = "materials/a.json";
    a.hasMaterialOverride = true;
    a.materialOverride.baseColorBinding.source = "gltf:baseColorTexture";
    a.materialOverride.baseColorBinding.hasSource = true;
    a.materialOverride.baseColorBinding.uvSet = 0;
    a.materialOverride.baseColorBinding.hasUvSet = true;
    a.materialOverride.ormBinding.source =
        "gltf:occlusionTexture|metallicRoughnessTexture";
    a.materialOverride.ormBinding.hasSource = true;
    a.materialOverride.ormBinding.channelMapping =
        "R=occlusion,G=roughness,B=metallic";
    a.materialOverride.ormBinding.hasChannelMapping = true;

    ku::asset::SceneInstance b{};
    b.id = "B";
    b.mesh = 7;
    b.materialReference = "materials/b.json";
    b.hasMaterialOverride = true;
    b.materialOverride.baseColorBinding.source = "disabled";
    b.materialOverride.baseColorBinding.hasSource = true;
    b.materialOverride.metallicRoughnessBinding.source =
        "gltf:metallicRoughnessTexture";
    b.materialOverride.metallicRoughnessBinding.hasSource = true;
    b.materialOverride.occlusionBinding.source = "gltf:occlusionTexture";
    b.materialOverride.occlusionBinding.hasSource = true;

    const std::vector<ku::asset::SceneInstance> ab{a, b};
    const std::vector<ku::asset::SceneInstance> ba{b, a};
    const ku::MaterialGpuVariantPlan planAB =
        ku::buildMaterialGpuVariantPlan(ab);
    const ku::MaterialGpuVariantPlan planBA =
        ku::buildMaterialGpuVariantPlan(ba);
    ASSERT_EQ(planAB.variants.size(), 2u);
    ASSERT_EQ(planBA.variants.size(), 2u);
    EXPECT_EQ(planAB.variants, planBA.variants);
    EXPECT_EQ(planAB.variants[planAB.instanceVariantIndices[0]],
        planBA.variants[planBA.instanceVariantIndices[1]]);
    EXPECT_EQ(planAB.variants[planAB.instanceVariantIndices[1]],
        planBA.variants[planBA.instanceVariantIndices[0]]);
    EXPECT_NE(planAB.instanceVariantIndices[0],
        planAB.instanceVariantIndices[1]);

    ku::asset::SceneInstance sameDescriptor = a;
    sameDescriptor.id = "A-scalar-only";
    sameDescriptor.materialReference = "materials/a-scalar.json";
    sameDescriptor.materialOverride.roughnessFactor = 0.2f;
    sameDescriptor.materialOverride.hasRoughnessFactor = true;
    sameDescriptor.materialOverride.baseColorBinding.uvSet = 1;
    const ku::MaterialGpuVariantPlan deduplicated =
        ku::buildMaterialGpuVariantPlan(
            std::vector<ku::asset::SceneInstance>{a, sameDescriptor});
    ASSERT_EQ(deduplicated.variants.size(), 1u);
    EXPECT_EQ(deduplicated.instanceVariantIndices[0],
        deduplicated.instanceVariantIndices[1]);
}

TEST(ForwardMaterialVariantTest,
    IgnoresUvOnlyPresenceButStillSplitsTextureSourceOverrides)
{
    ku::asset::SceneInstance legacyDisabled{};
    legacyDisabled.id = "legacy-disabled";
    legacyDisabled.mesh = 11;
    legacyDisabled.hasMaterialOverride = true;
    legacyDisabled.materialOverride.ormBinding.source = "disabled";
    legacyDisabled.materialOverride.ormBinding.hasSource = true;
    legacyDisabled.materialOverride.ormBinding.channelMapping =
        "R=occlusion,G=roughness,B=metallic";
    legacyDisabled.materialOverride.ormBinding.hasChannelMapping = true;

    ku::asset::SceneInstance aoUvOnly = legacyDisabled;
    aoUvOnly.id = "ao-uv-only";
    aoUvOnly.materialOverride.occlusionBinding.uvSet = 1;
    aoUvOnly.materialOverride.occlusionBinding.hasUvSet = true;

    const std::vector<ku::asset::SceneInstance> forward{
        legacyDisabled, aoUvOnly};
    const std::vector<ku::asset::SceneInstance> reverse{
        aoUvOnly, legacyDisabled};
    const ku::MaterialGpuVariantPlan forwardPlan =
        ku::buildMaterialGpuVariantPlan(forward);
    const ku::MaterialGpuVariantPlan reversePlan =
        ku::buildMaterialGpuVariantPlan(reverse);
    ASSERT_EQ(forwardPlan.variants.size(), 1u);
    ASSERT_EQ(reversePlan.variants.size(), 1u);
    EXPECT_EQ(forwardPlan.variants, reversePlan.variants);
    EXPECT_EQ(forwardPlan.instanceVariantIndices[0],
        reversePlan.instanceVariantIndices[1]);
    EXPECT_EQ(forwardPlan.instanceVariantIndices[1],
        reversePlan.instanceVariantIndices[0]);

    ku::asset::SceneInstance embedded{};
    embedded.id = "embedded";
    embedded.mesh = 11;
    embedded.hasMaterialOverride = true;
    ku::asset::SceneInstance embeddedUvOnly = embedded;
    embeddedUvOnly.id = "embedded-uv-only";
    embeddedUvOnly.materialOverride.baseColorBinding.uvSet = 1;
    embeddedUvOnly.materialOverride.baseColorBinding.hasUvSet = true;
    embeddedUvOnly.materialOverride.normalBinding.uvSet = 1;
    embeddedUvOnly.materialOverride.normalBinding.hasUvSet = true;
    embeddedUvOnly.materialOverride.metallicRoughnessBinding.uvSet = 1;
    embeddedUvOnly.materialOverride.metallicRoughnessBinding.hasUvSet = true;
    embeddedUvOnly.materialOverride.occlusionBinding.uvSet = 1;
    embeddedUvOnly.materialOverride.occlusionBinding.hasUvSet = true;
    embeddedUvOnly.materialOverride.emissiveBinding.uvSet = 1;
    embeddedUvOnly.materialOverride.emissiveBinding.hasUvSet = true;
    const ku::MaterialGpuVariantPlan embeddedPlan =
        ku::buildMaterialGpuVariantPlan(
            std::vector<ku::asset::SceneInstance>{embedded, embeddedUvOnly});
    ASSERT_EQ(embeddedPlan.variants.size(), 1u);

    ku::asset::SceneInstance disabledBase = embedded;
    disabledBase.id = "disabled-base";
    disabledBase.materialOverride.baseColorBinding.source = "disabled";
    disabledBase.materialOverride.baseColorBinding.hasSource = true;
    const ku::MaterialGpuVariantPlan sourceDifference =
        ku::buildMaterialGpuVariantPlan(
            std::vector<ku::asset::SceneInstance>{embedded, disabledBase});
    ASSERT_EQ(sourceDifference.variants.size(), 2u);
    EXPECT_NE(sourceDifference.instanceVariantIndices[0],
        sourceDifference.instanceVariantIndices[1]);
}

TEST(PBRMaterialTextureSourcePlanTest,
    UvOnlyOverridesStayPerDrawAndCannotBypassLegacyDisabledSource)
{
    ku::asset::MaterialData embedded{};
    ku::asset::MaterialConfig disabledOrm{};
    disabledOrm.ormBinding.source = "disabled";
    disabledOrm.ormBinding.hasSource = true;
    disabledOrm.ormBinding.channelMapping =
        "R=occlusion,G=roughness,B=metallic";
    disabledOrm.ormBinding.hasChannelMapping = true;
    disabledOrm.occlusionBinding.uvSet = 1;
    disabledOrm.occlusionBinding.hasUvSet = true;

    EXPECT_FALSE(ku::hasTextureSourceOverride(
        disabledOrm.occlusionBinding));
    const ku::PBRMaterialTextureSourcePlan disabledPlan =
        ku::buildPBRMaterialTextureSourcePlan(embedded, &disabledOrm);
    EXPECT_EQ(disabledPlan.baseColor, &embedded.baseColorTexture);
    EXPECT_EQ(disabledPlan.normal, &embedded.normalTexture);
    EXPECT_EQ(disabledPlan.metallicRoughness, nullptr);
    EXPECT_EQ(disabledPlan.occlusion, nullptr);
    EXPECT_EQ(disabledPlan.emissive, &embedded.emissiveTexture);
    EXPECT_EQ(disabledPlan.unsupportedOverrides,
        (std::array<bool, ku::pbr_material_binding::count>{}));
    const ku::ResolvedMaterial disabledResolved =
        ku::resolveMaterial(embedded, &disabledOrm);
    EXPECT_EQ(disabledResolved.textureTransforms[3].texCoord, 1u);

    ku::asset::MaterialConfig uvOnly{};
    uvOnly.baseColorBinding.hasUvSet = true;
    uvOnly.baseColorBinding.uvSet = 1;
    uvOnly.normalBinding.hasUvSet = true;
    uvOnly.normalBinding.uvSet = 1;
    uvOnly.metallicRoughnessBinding.hasUvSet = true;
    uvOnly.metallicRoughnessBinding.uvSet = 1;
    uvOnly.occlusionBinding.hasUvSet = true;
    uvOnly.occlusionBinding.uvSet = 1;
    uvOnly.emissiveBinding.hasUvSet = true;
    uvOnly.emissiveBinding.uvSet = 1;
    const ku::PBRMaterialTextureSourcePlan embeddedPlan =
        ku::buildPBRMaterialTextureSourcePlan(embedded, &uvOnly);
    EXPECT_EQ(embeddedPlan.baseColor, &embedded.baseColorTexture);
    EXPECT_EQ(embeddedPlan.normal, &embedded.normalTexture);
    EXPECT_EQ(embeddedPlan.metallicRoughness,
        &embedded.metallicRoughnessTexture);
    EXPECT_EQ(embeddedPlan.occlusion, &embedded.occlusionTexture);
    EXPECT_EQ(embeddedPlan.emissive, &embedded.emissiveTexture);
    const ku::ResolvedMaterial uvResolved =
        ku::resolveMaterial(embedded, &uvOnly);
    for (const auto& transform : uvResolved.textureTransforms) {
        EXPECT_EQ(transform.texCoord, 1u);
    }
}

TEST(ForwardTransformTest, AppliesDocumentedEulerOrderAndNormalMatrix)
{
    ku::asset::SceneTransform transform{};
    transform.position = {1.0f, 2.0f, 3.0f};
    transform.rotationEulerDeg = {90.0f, 90.0f, 0.0f};
    transform.scale = {2.0f, 3.0f, 4.0f};
    const glm::mat4 actual = ku::asset::sceneTransformMatrix(transform);
    glm::mat4 expected = glm::translate(glm::mat4(1.0f), transform.position);
    expected = glm::rotate(expected, glm::radians(90.0f), {0,1,0});
    expected = glm::rotate(expected, glm::radians(90.0f), {1,0,0});
    expected = glm::scale(expected, transform.scale);
    for (int c=0;c<4;++c) for(int r=0;r<4;++r)
        EXPECT_NEAR(actual[c][r], expected[c][r], 1e-5f);
    const glm::mat3 normal = ku::asset::sceneNormalMatrix(actual);
    const glm::mat3 expectedNormal = glm::transpose(glm::inverse(glm::mat3(actual)));
    for (int c=0;c<3;++c) for(int r=0;r<3;++r)
        EXPECT_NEAR(normal[c][r], expectedNormal[c][r], 1e-5f);
    transform.scale.y = 0.0f;
    EXPECT_FALSE(ku::asset::validSceneTransform(transform));
}

TEST(ForwardTransformTest, TransformsAllEightBoundsCorners)
{
    ku::asset::SceneTransform transform{};
    transform.position = {3,0,0};
    transform.rotationEulerDeg.z = 90;
    transform.scale = {2,1,1};
    glm::vec3 minimum, maximum;
    ASSERT_TRUE(ku::asset::transformBounds({-1,-2,-1},{1,2,1},
        ku::asset::sceneTransformMatrix(transform), minimum, maximum));
    EXPECT_NEAR(minimum.x, 1.0f, 1e-5f);
    EXPECT_NEAR(maximum.x, 5.0f, 1e-5f);
    EXPECT_NEAR(minimum.y, -2.0f, 1e-5f);
    EXPECT_NEAR(maximum.y, 2.0f, 1e-5f);
}

TEST(ForwardMaterialTest, OverridePresencePreservesBaseAndLegacyOrmMapsBothUvs)
{
    ku::asset::MaterialData base{};
    base.baseColorFactor = {0.2f,0.3f,0.4f,0.5f};
    base.metallicFactor = 0.7f;
    base.metallicRoughnessTransform.texCoord = 0;
    base.occlusionTransform.texCoord = 0;
    base.emissiveTransform.texCoord = 0;
    ku::asset::MaterialConfig overrides{};
    overrides.roughnessFactor = 0.25f;
    overrides.hasRoughnessFactor = true;
    overrides.ormBinding.uvSet = 1;
    overrides.ormBinding.hasUvSet = true;
    overrides.ormBinding.channelMapping =
        "R=occlusion,G=roughness,B=metallic";
    overrides.ormBinding.hasChannelMapping = true;
    overrides.emissiveBinding.uvSet = 1;
    overrides.emissiveBinding.hasUvSet = true;
    const ku::ResolvedMaterial resolved = ku::resolveMaterial(base, &overrides);
    EXPECT_FLOAT_EQ(resolved.baseColorFactor[0], 0.2f);
    EXPECT_FLOAT_EQ(resolved.metallicFactor, 0.7f);
    EXPECT_FLOAT_EQ(resolved.roughnessFactor, 0.25f);
    EXPECT_EQ(resolved.textureTransforms[2].texCoord, 1u);
    EXPECT_EQ(resolved.textureTransforms[3].texCoord, 1u);
    EXPECT_EQ(resolved.textureTransforms[4].texCoord, 1u);

    overrides.ormBinding.hasChannelMapping = false;
    const ku::ResolvedMaterial nonCombined =
        ku::resolveMaterial(base, &overrides);
    EXPECT_EQ(nonCombined.textureTransforms[2].texCoord, 0u);
    EXPECT_EQ(nonCombined.textureTransforms[3].texCoord, 0u);
}

TEST(ForwardPipelineTest, ClassifiesKeysAndSortsBlendBackToFront)
{
    ku::ResolvedMaterial pbr{};
    ku::ResolvedMaterial unlit{};
    unlit.shadingModel = ku::asset::ShadingModel::Unlit;
    unlit.alphaMode = ku::asset::AlphaMode::Blend;
    unlit.doubleSided = true;
    EXPECT_NE(ku::pipelineKeyFor(pbr), ku::pipelineKeyFor(unlit));
    const ku::ForwardSortKey keys[] = {
        {ku::asset::AlphaMode::Blend, 4.0f, 0},
        {ku::asset::AlphaMode::Opaque, 100.0f, 1},
        {ku::asset::AlphaMode::Mask, 1.0f, 2},
        {ku::asset::AlphaMode::Blend, 25.0f, 3}};
    EXPECT_EQ(ku::buildForwardDrawOrder(keys),
        (std::vector<size_t>{1,2,3,0}));
}

TEST(ForwardBufferTest, ValidatesAlignmentOffsetsAndOverflow)
{
    ku::ForwardDynamicBufferLayout layout{};
    ASSERT_TRUE(ku::calculateForwardDynamicBufferLayout(
        3, sizeof(ku::ForwardDrawUniforms), 256, layout));
    EXPECT_EQ(layout.stride % 256, 0u);
    EXPECT_EQ(layout.totalSize, layout.stride * 3u);
    const auto preserved = layout;
    EXPECT_FALSE(ku::calculateForwardDynamicBufferLayout(
        static_cast<size_t>(std::numeric_limits<uint32_t>::max()) + 1u,
        sizeof(ku::ForwardDrawUniforms), 256, layout));
    EXPECT_EQ(layout.totalSize, preserved.totalSize);
}

TEST(ForwardLightingTest, ClampsCountIntensityAndRange)
{
    ku::asset::SceneLightingConfig lighting{};
    lighting.intensity = -2.0f;
    lighting.pointLights.resize(7);
    lighting.pointLights[0].range = -1.0f;
    lighting.pointLights[0].intensity = -3.0f;
    ku::asset::sanitizeSceneLighting(lighting);
    EXPECT_EQ(lighting.pointLights.size(), ku::asset::maximumPointLights);
    EXPECT_FLOAT_EQ(lighting.intensity, 0.0f);
    EXPECT_GT(lighting.pointLights[0].range, 0.0f);
    EXPECT_FLOAT_EQ(lighting.pointLights[0].intensity, 0.0f);
}
