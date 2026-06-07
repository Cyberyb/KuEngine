#include <gtest/gtest.h>

#include <KuEngine/Asset/AssetConfig.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

struct TempDir {
    std::filesystem::path path;

    TempDir()
    {
        const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() / ("kuengine-asset-config-" + std::to_string(tick));
        std::filesystem::create_directories(path);
    }

    ~TempDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

void writeText(const std::filesystem::path& filePath, const std::string& content)
{
    std::filesystem::create_directories(filePath.parent_path());
    std::ofstream file(filePath, std::ios::binary);
    file << content;
}

} // namespace

TEST(AssetConfigTest, SceneConfigParsesCameraLightingAndNode)
{
    TempDir tempDir;
    const std::filesystem::path scenePath = tempDir.path / "resources/scenes/sandbox/test.scene.json";

    writeText(
        scenePath,
        R"({
  "camera": {
    "position": [1.0, 2.0, 3.0],
    "target": [0.0, 0.0, 0.0],
    "up": [0.0, 1.0, 0.0],
    "fovYDeg": 75.0,
    "near": 0.2,
    "far": 300.0
  },
  "lighting": {
    "direction": [0.2, 1.0, 0.3],
    "color": [0.8, 0.9, 1.0],
    "intensity": 2.5
  },
  "nodes": [
    {
      "id": "car",
      "model": "models/props/mclaren_765lt.glb",
      "material": "materials/pbr/mclaren-765lt.material.json"
    }
  ]
})")
    ;

    ku::asset::SceneConfig sceneConfig{};
    std::string error;
    ASSERT_TRUE(ku::asset::loadSceneConfigFromFile(scenePath, sceneConfig, &error)) << error;

    EXPECT_FLOAT_EQ(sceneConfig.camera.position.x, 1.0f);
    EXPECT_FLOAT_EQ(sceneConfig.camera.position.y, 2.0f);
    EXPECT_FLOAT_EQ(sceneConfig.camera.position.z, 3.0f);
    EXPECT_FLOAT_EQ(sceneConfig.camera.fovYDeg, 75.0f);
    EXPECT_FLOAT_EQ(sceneConfig.camera.nearPlane, 0.2f);
    EXPECT_FLOAT_EQ(sceneConfig.camera.farPlane, 300.0f);

    EXPECT_FLOAT_EQ(sceneConfig.lighting.direction.x, 0.2f);
    EXPECT_FLOAT_EQ(sceneConfig.lighting.color.y, 0.9f);
    EXPECT_FLOAT_EQ(sceneConfig.lighting.intensity, 2.5f);

    ASSERT_EQ(sceneConfig.nodes.size(), 1u);
    EXPECT_EQ(sceneConfig.nodes[0].id, "car");
    EXPECT_EQ(sceneConfig.nodes[0].model, "models/props/mclaren_765lt.glb");
    EXPECT_EQ(sceneConfig.nodes[0].material, "materials/pbr/mclaren-765lt.material.json");

    const std::filesystem::path resourcesRoot = ku::asset::findResourcesRoot(scenePath);
    EXPECT_EQ(resourcesRoot.filename(), "resources");
}

TEST(AssetConfigTest, SceneConfigFallsBackToDefaultsWhenFieldsMissing)
{
    TempDir tempDir;
    const std::filesystem::path scenePath = tempDir.path / "resources/scenes/sandbox/default.scene.json";

    writeText(scenePath, R"({ "nodes": [ { "id": "only-id" } ] })");

    ku::asset::SceneConfig sceneConfig{};
    std::string error;
    ASSERT_TRUE(ku::asset::loadSceneConfigFromFile(scenePath, sceneConfig, &error)) << error;

    EXPECT_FLOAT_EQ(sceneConfig.camera.fovYDeg, 60.0f);
    EXPECT_FLOAT_EQ(sceneConfig.camera.nearPlane, 0.1f);
    EXPECT_FLOAT_EQ(sceneConfig.camera.farPlane, 200.0f);

    EXPECT_FLOAT_EQ(sceneConfig.lighting.direction.x, 0.35f);
    EXPECT_FLOAT_EQ(sceneConfig.lighting.direction.y, 1.0f);
    EXPECT_FLOAT_EQ(sceneConfig.lighting.direction.z, 0.45f);
    EXPECT_FLOAT_EQ(sceneConfig.lighting.intensity, 1.0f);

    ASSERT_EQ(sceneConfig.nodes.size(), 1u);
    EXPECT_EQ(sceneConfig.nodes[0].id, "only-id");
    EXPECT_TRUE(sceneConfig.nodes[0].model.empty());
    EXPECT_TRUE(sceneConfig.nodes[0].material.empty());
}

TEST(AssetConfigTest, MaterialConfigParsesBaseColorFactor)
{
    TempDir tempDir;
    const std::filesystem::path materialPath = tempDir.path / "resources/materials/pbr/test.material.json";

    writeText(materialPath, R"({ "baseColorFactor": [0.25, 0.5, 0.75, 1.0] })");

    ku::asset::MaterialConfig materialConfig{};
    std::string error;
    ASSERT_TRUE(ku::asset::loadMaterialConfigFromFile(materialPath, materialConfig, &error)) << error;

    EXPECT_TRUE(materialConfig.hasBaseColorFactor);
    EXPECT_FLOAT_EQ(materialConfig.baseColorFactor[0], 0.25f);
    EXPECT_FLOAT_EQ(materialConfig.baseColorFactor[1], 0.5f);
    EXPECT_FLOAT_EQ(materialConfig.baseColorFactor[2], 0.75f);
    EXPECT_FLOAT_EQ(materialConfig.baseColorFactor[3], 1.0f);
}

TEST(AssetConfigTest, MaterialConfigParsesExtendedFields)
{
        TempDir tempDir;
        const std::filesystem::path materialPath = tempDir.path / "resources/materials/pbr/extended.material.json";

        writeText(
                materialPath,
                R"({
    "id": "test-material",
    "version": "1.2.3",
    "pipeline": "pbr-metallic-roughness",
    "doubleSided": true,
    "alphaMode": "MASK",
    "alphaCutoff": 0.42,
    "baseColorFactor": [0.1, 0.2, 0.3, 0.4],
    "metallicFactor": 0.8,
    "roughnessFactor": 0.6,
    "normalScale": 0.5,
    "occlusionStrength": 0.7,
    "textureBindings": {
        "baseColor": { "source": "gltf:baseColorTexture", "colorSpace": "sRGB", "uvSet": 1 },
        "normal": { "source": "gltf:normalTexture", "colorSpace": "Linear", "uvSet": 0 },
        "orm": { "source": "gltf:metallicRoughnessTexture", "channelMapping": "R=occlusion,G=roughness,B=metallic", "colorSpace": "Linear", "uvSet": 0 }
    }
})")
        ;

        ku::asset::MaterialConfig materialConfig{};
        std::string error;
        ASSERT_TRUE(ku::asset::loadMaterialConfigFromFile(materialPath, materialConfig, &error)) << error;

        EXPECT_EQ(materialConfig.id, "test-material");
        EXPECT_EQ(materialConfig.version, "1.2.3");
        EXPECT_EQ(materialConfig.pipeline, "pbr-metallic-roughness");
        EXPECT_TRUE(materialConfig.doubleSided);
        EXPECT_EQ(materialConfig.alphaMode, "MASK");
        EXPECT_TRUE(materialConfig.hasAlphaCutoff);
        EXPECT_FLOAT_EQ(materialConfig.alphaCutoff, 0.42f);

        EXPECT_TRUE(materialConfig.hasBaseColorFactor);
        EXPECT_FLOAT_EQ(materialConfig.baseColorFactor[0], 0.1f);
        EXPECT_FLOAT_EQ(materialConfig.metallicFactor, 0.8f);
        EXPECT_FLOAT_EQ(materialConfig.roughnessFactor, 0.6f);
        EXPECT_FLOAT_EQ(materialConfig.normalScale, 0.5f);
        EXPECT_FLOAT_EQ(materialConfig.occlusionStrength, 0.7f);

        EXPECT_TRUE(materialConfig.baseColorBinding.hasSource);
        EXPECT_EQ(materialConfig.baseColorBinding.source, "gltf:baseColorTexture");
        EXPECT_TRUE(materialConfig.baseColorBinding.hasColorSpace);
        EXPECT_EQ(materialConfig.baseColorBinding.colorSpace, "sRGB");
        EXPECT_TRUE(materialConfig.baseColorBinding.hasUvSet);
        EXPECT_EQ(materialConfig.baseColorBinding.uvSet, 1);

        EXPECT_TRUE(materialConfig.normalBinding.hasSource);
        EXPECT_EQ(materialConfig.normalBinding.source, "gltf:normalTexture");

        EXPECT_TRUE(materialConfig.ormBinding.hasChannelMapping);
        EXPECT_EQ(materialConfig.ormBinding.channelMapping, "R=occlusion,G=roughness,B=metallic");
}

TEST(AssetConfigTest, MaterialConfigKeepsDefaultWhenFieldMissing)
{
    TempDir tempDir;
    const std::filesystem::path materialPath = tempDir.path / "resources/materials/pbr/default.material.json";

    writeText(materialPath, R"({ "id": "no-color" })");

    ku::asset::MaterialConfig materialConfig{};
    std::string error;
    ASSERT_TRUE(ku::asset::loadMaterialConfigFromFile(materialPath, materialConfig, &error)) << error;

    EXPECT_FALSE(materialConfig.hasBaseColorFactor);
    EXPECT_FLOAT_EQ(materialConfig.baseColorFactor[0], 1.0f);
    EXPECT_FLOAT_EQ(materialConfig.baseColorFactor[1], 1.0f);
    EXPECT_FLOAT_EQ(materialConfig.baseColorFactor[2], 1.0f);
    EXPECT_FLOAT_EQ(materialConfig.baseColorFactor[3], 1.0f);
    EXPECT_FALSE(materialConfig.hasMetallicFactor);
    EXPECT_FLOAT_EQ(materialConfig.metallicFactor, 1.0f);
    EXPECT_FALSE(materialConfig.hasRoughnessFactor);
    EXPECT_FLOAT_EQ(materialConfig.roughnessFactor, 1.0f);
    EXPECT_FALSE(materialConfig.hasNormalScale);
    EXPECT_FLOAT_EQ(materialConfig.normalScale, 1.0f);
    EXPECT_FALSE(materialConfig.hasOcclusionStrength);
    EXPECT_FLOAT_EQ(materialConfig.occlusionStrength, 1.0f);
    EXPECT_FALSE(materialConfig.baseColorBinding.hasSource);
    EXPECT_FALSE(materialConfig.normalBinding.hasSource);
    EXPECT_FALSE(materialConfig.ormBinding.hasSource);
}

TEST(AssetConfigTest, MissingConfigFileReturnsFalse)
{
    ku::asset::SceneConfig sceneConfig{};
    std::string sceneError;
    EXPECT_FALSE(ku::asset::loadSceneConfigFromFile("not-exist.scene.json", sceneConfig, &sceneError));
    EXPECT_FALSE(sceneError.empty());

    ku::asset::MaterialConfig materialConfig{};
    std::string materialError;
    EXPECT_FALSE(ku::asset::loadMaterialConfigFromFile("not-exist.material.json", materialConfig, &materialError));
    EXPECT_FALSE(materialError.empty());
}
