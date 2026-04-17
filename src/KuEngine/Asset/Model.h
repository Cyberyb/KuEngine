#pragma once

#include <filesystem>
#include <cstdint>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace ku::asset {

struct MeshVertex {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 uv0{0.0f, 0.0f};
    glm::vec2 uv1{0.0f, 0.0f};
};

struct TextureData {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> rgba8;

    [[nodiscard]] bool valid() const
    {
        return width > 0 && height > 0 && rgba8.size() == static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
    }
};

struct MaterialData {
    struct TextureTransform {
        glm::vec2 offset{0.0f, 0.0f};
        glm::vec2 scale{1.0f, 1.0f};
        float rotation = 0.0f;
        uint32_t texCoord = 0;
    };

    glm::vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    float normalScale = 1.0f;
    float occlusionStrength = 1.0f;

    TextureData baseColorTexture;
    TextureData normalTexture;
    TextureData ormTexture;

    TextureTransform baseColorTransform{};
    TextureTransform normalTransform{};
    TextureTransform ormTransform{};
};

struct SubMeshData {
    uint32_t indexStart = 0;
    uint32_t indexCount = 0;
    uint32_t materialIndex = 0;
};

struct MeshData {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<MaterialData> materials;
    std::vector<SubMeshData> subMeshes;
    glm::vec3 boundsMin{0.0f, 0.0f, 0.0f};
    glm::vec3 boundsMax{0.0f, 0.0f, 0.0f};
    glm::vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    TextureData baseColorTexture;
};

class ModelLoader {
public:
    static MeshData loadFromFile(const std::filesystem::path& path);
};

} // namespace ku::asset
