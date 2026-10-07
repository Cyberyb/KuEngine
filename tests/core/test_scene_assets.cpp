#include <gtest/gtest.h>

#include <KuEngine/Asset/AssetPath.h>
#include <KuEngine/Asset/HDRImage.h>
#include <KuEngine/Asset/Scene.h>
#include <KuEngine/KuEngine.h>
#include <KuEngine/Render/PBRResources.h>

#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

ku::asset::MeshData makeDrawableMesh(uint32_t materialCount = 1)
{
    ku::asset::MeshData mesh{};
    mesh.vertices.resize(3);
    mesh.indices = {0, 1, 2};
    mesh.materials.resize(materialCount);
    mesh.subMeshes.push_back(ku::asset::SubMeshData{0, 3, 0});
    return mesh;
}

ku::asset::SceneLoadDescription makeSceneDescription()
{
    ku::asset::SceneLoadDescription description{};
    description.resourcesRoot =
        std::filesystem::current_path() / "virtual-resources";
    description.environmentPath = "environments/studio.hdr";
    description.config.nodes = {
        ku::asset::SceneNodeConfig{
            "first",
            "models/props/../props/car.glb",
            {},
        },
        ku::asset::SceneNodeConfig{
            "second",
            "models/props/car.glb",
            {},
        },
    };
    return description;
}

} // namespace

TEST(SceneLoaderTest, DeduplicatesNormalizedPathsAndKeepsStableHandles)
{
    ku::asset::SceneData scene{};
    int loadCalls = 0;
    std::string error;
    ASSERT_TRUE(ku::asset::SceneLoader::load(
        makeSceneDescription(),
        scene,
        &error,
        [&](const std::filesystem::path&) {
            ++loadCalls;
            return makeDrawableMesh();
        })) << error;

    EXPECT_EQ(loadCalls, 1);
    ASSERT_EQ(scene.meshes.size(), 1u);
    ASSERT_EQ(scene.instances.size(), 2u);
    EXPECT_EQ(scene.meshes[0].handle, 0u);
    EXPECT_EQ(scene.instances[0].mesh, 0u);
    EXPECT_EQ(scene.instances[1].mesh, 0u);
    EXPECT_EQ(scene.findMesh(0), &scene.meshes[0]);
    EXPECT_EQ(scene.findMesh(ku::asset::invalidMeshHandle), nullptr);
    EXPECT_EQ(scene.findMesh(1), nullptr);
    EXPECT_TRUE(scene.valid());
    scene.instances[1].mesh = ku::asset::invalidMeshHandle;
    EXPECT_FALSE(scene.valid());
    scene.instances[1].mesh = 0;
    EXPECT_EQ(
        scene.environment.sourcePath,
        ku::asset::resolveAssetPath(
            makeSceneDescription().resourcesRoot,
            makeSceneDescription().environmentPath));
}

TEST(SceneLoaderTest, AssignsOneHandlePerUniqueModelInFirstSeenOrder)
{
    ku::asset::SceneLoadDescription description = makeSceneDescription();
    description.config.nodes.push_back(
        ku::asset::SceneNodeConfig{"third", "models/other.glb", {}});

    ku::asset::SceneData scene{};
    std::string error;
    ASSERT_TRUE(ku::asset::SceneLoader::load(
        description,
        scene,
        &error,
        [](const std::filesystem::path&) {
            return makeDrawableMesh();
        })) << error;

    ASSERT_EQ(scene.meshes.size(), 2u);
    ASSERT_EQ(scene.instances.size(), 3u);
    EXPECT_EQ(scene.meshes[0].handle, 0u);
    EXPECT_EQ(scene.meshes[1].handle, 1u);
    EXPECT_EQ(scene.instances[2].mesh, 1u);
}

TEST(SceneLoaderTest, FailureDoesNotPublishPartialScene)
{
    ku::asset::SceneLoadDescription description = makeSceneDescription();
    description.config.nodes.push_back(
        ku::asset::SceneNodeConfig{"broken", "models/broken.glb", {}});

    ku::asset::SceneData published{};
    published.camera.fovYDeg = 17.0f;
    std::string error;
    EXPECT_FALSE(ku::asset::SceneLoader::load(
        description,
        published,
        &error,
        [](const std::filesystem::path& path) {
            if (path.filename() == "broken.glb") {
                throw std::runtime_error("controlled model failure");
            }
            return makeDrawableMesh();
        }));
    EXPECT_NE(error.find("controlled model failure"), std::string::npos);
    EXPECT_FLOAT_EQ(published.camera.fovYDeg, 17.0f);
    EXPECT_TRUE(published.meshes.empty());
    EXPECT_TRUE(published.instances.empty());
}

TEST(SceneLoaderTest, RejectsEmptyOrNonDrawableNodesWithoutPublishing)
{
    ku::asset::SceneLoadDescription description = makeSceneDescription();
    description.config.nodes[0].model.clear();

    ku::asset::SceneData published{};
    published.lighting.intensity = 9.0f;
    std::string error;
    EXPECT_FALSE(ku::asset::SceneLoader::load(
        description,
        published,
        &error,
        [](const std::filesystem::path&) {
            return ku::asset::MeshData{};
        }));
    EXPECT_FLOAT_EQ(published.lighting.intensity, 9.0f);
}

TEST(SceneLoaderTest, RejectsOutOfRangeGeometryWithoutPublishing)
{
    ku::asset::SceneLoadDescription description = makeSceneDescription();
    ku::asset::SceneData published{};
    published.camera.fovYDeg = 23.0f;
    std::string error;
    EXPECT_FALSE(ku::asset::SceneLoader::load(
        description,
        published,
        &error,
        [](const std::filesystem::path&) {
            ku::asset::MeshData mesh = makeDrawableMesh();
            mesh.indices[1] = 99;
            return mesh;
        }));
    EXPECT_NE(error.find("no drawable mesh"), std::string::npos);
    EXPECT_FLOAT_EQ(published.camera.fovYDeg, 23.0f);
    EXPECT_TRUE(published.meshes.empty());
    EXPECT_TRUE(published.instances.empty());
}

TEST(SceneLoaderTest, RejectsMeshesWithoutDrawableSubMeshRanges)
{
    ku::asset::SceneLoadDescription description = makeSceneDescription();
    ku::asset::SceneData published{};
    published.camera.fovYDeg = 31.0f;
    std::string error;
    EXPECT_FALSE(ku::asset::SceneLoader::load(
        description,
        published,
        &error,
        [](const std::filesystem::path&) {
            ku::asset::MeshData mesh = makeDrawableMesh();
            mesh.subMeshes[0].indexCount = 0;
            return mesh;
        }));
    EXPECT_NE(error.find("no drawable mesh"), std::string::npos);
    EXPECT_FLOAT_EQ(published.camera.fovYDeg, 31.0f);
    EXPECT_TRUE(published.meshes.empty());
    EXPECT_TRUE(published.instances.empty());
}

TEST(HDRImageTest, ValidatesDimensionsPayloadAndOverflow)
{
    ku::asset::HDRImageData image{};
    image.width = 2;
    image.height = 3;
    image.rgba32f.resize(24);
    EXPECT_TRUE(image.valid());
    image.rgba32f.pop_back();
    EXPECT_FALSE(image.valid());

    size_t count = 123;
    EXPECT_FALSE(ku::asset::checkedHdrElementCount(0, 4, count));
    EXPECT_EQ(count, 0u);
    EXPECT_FALSE(ku::asset::checkedHdrElementCount(
        std::numeric_limits<uint64_t>::max(),
        2,
        count));
    EXPECT_EQ(count, 0u);
}

TEST(HDRImageTest, DecodeFailureDoesNotPublishOutput)
{
    ku::asset::HDRImageData published{};
    published.width = 1;
    published.height = 1;
    published.rgba32f = {1.0f, 2.0f, 3.0f, 4.0f};
    std::string error;
    EXPECT_FALSE(ku::asset::HDRImageLoader::loadFromFile(
        "definitely-missing-kuengine.hdr",
        published,
        &error));
    EXPECT_FALSE(error.empty());
    EXPECT_TRUE(published.valid());
    EXPECT_FLOAT_EQ(published.rgba32f[2], 3.0f);
}

TEST(PBRResourcePlanTest, SuppliesFallbackForEmptyMaterials)
{
    EXPECT_EQ(ku::pbr_material_binding::baseColor, 0u);
    EXPECT_EQ(ku::pbr_material_binding::normal, 1u);
    EXPECT_EQ(ku::pbr_material_binding::metallicRoughness, 2u);
    EXPECT_EQ(ku::pbr_material_binding::occlusion, 3u);
    EXPECT_EQ(ku::pbr_material_binding::emissive, 4u);
    EXPECT_EQ(ku::pbr_material_binding::count, 5u);
    EXPECT_EQ(ku::pbr_environment_binding::environment, 0u);

    ku::asset::MeshData mesh = makeDrawableMesh(0);
    mesh.subMeshes[0].materialIndex = 9;
    const ku::PBRMaterialBuildPlan plan = ku::buildPBRMaterialPlan(mesh);
    EXPECT_EQ(plan.materialCount, 1u);
    ASSERT_EQ(plan.subMeshMaterialIndices.size(), 1u);
    EXPECT_EQ(plan.subMeshMaterialIndices[0], 0u);
}

TEST(PBRResourcePlanTest, PreservesValidIndicesAndFallsBackInvalidOnes)
{
    ku::asset::MeshData mesh = makeDrawableMesh(3);
    mesh.subMeshes = {
        ku::asset::SubMeshData{0, 3, 2},
        ku::asset::SubMeshData{0, 3, 7},
    };
    const ku::PBRMaterialBuildPlan plan = ku::buildPBRMaterialPlan(mesh);
    EXPECT_EQ(plan.materialCount, 3u);
    EXPECT_EQ(plan.subMeshMaterialIndices, (std::vector<uint32_t>{2, 0}));
}

TEST(PBRResourcePlanTest, DistinguishesMissingInvalidAndReadyOptionalTextures)
{
    ku::asset::TextureData texture{};
    EXPECT_EQ(
        ku::classifyOptionalTexturePayload(texture),
        ku::PBROptionalTexturePayload::Missing);

    texture.width = 1;
    texture.height = 1;
    EXPECT_EQ(
        ku::classifyOptionalTexturePayload(texture),
        ku::PBROptionalTexturePayload::Invalid);

    texture.rgba8 = {255, 255, 255, 255};
    EXPECT_EQ(
        ku::classifyOptionalTexturePayload(texture),
        ku::PBROptionalTexturePayload::Ready);
}
