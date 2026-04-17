#include "Model.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

namespace ku::asset {

namespace {

TextureData extractTextureFromTextureIndex(const tinygltf::Model& model, int textureIndex);

glm::vec2 readVec2FromValue(const tinygltf::Value& value, const glm::vec2& fallback)
{
    if (!value.IsArray() || value.Size() < 2) {
        return fallback;
    }

    return glm::vec2(
        static_cast<float>(value.Get(0).Get<double>()),
        static_cast<float>(value.Get(1).Get<double>()));
}

template <typename TTextureInfo>
MaterialData::TextureTransform readTextureTransform(const TTextureInfo& textureInfo)
{
    MaterialData::TextureTransform transform{};
    if (textureInfo.texCoord > 0) {
        transform.texCoord = static_cast<uint32_t>(textureInfo.texCoord);
    }

    const auto it = textureInfo.extensions.find("KHR_texture_transform");
    if (it == textureInfo.extensions.end()) {
        return transform;
    }

    const tinygltf::Value& ext = it->second;
    if (!ext.IsObject()) {
        return transform;
    }

    if (ext.Has("offset")) {
        transform.offset = readVec2FromValue(ext.Get("offset"), transform.offset);
    }
    if (ext.Has("scale")) {
        transform.scale = readVec2FromValue(ext.Get("scale"), transform.scale);
    }
    if (ext.Has("rotation")) {
        transform.rotation = static_cast<float>(ext.Get("rotation").Get<double>());
    }
    if (ext.Has("texCoord")) {
        const int texCoord = ext.Get("texCoord").Get<int>();
        if (texCoord > 0) {
            transform.texCoord = static_cast<uint32_t>(texCoord);
        } else {
            transform.texCoord = 0;
        }
    }

    return transform;
}

size_t componentSize(int componentType)
{
    switch (componentType) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        case TINYGLTF_COMPONENT_TYPE_BYTE:
            return 1;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        case TINYGLTF_COMPONENT_TYPE_SHORT:
            return 2;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        case TINYGLTF_COMPONENT_TYPE_INT:
        case TINYGLTF_COMPONENT_TYPE_FLOAT:
            return 4;
        case TINYGLTF_COMPONENT_TYPE_DOUBLE:
            return 8;
        default:
            throw std::runtime_error("Unsupported glTF component type");
    }
}

size_t numComponents(int type)
{
    switch (type) {
        case TINYGLTF_TYPE_SCALAR:
            return 1;
        case TINYGLTF_TYPE_VEC2:
            return 2;
        case TINYGLTF_TYPE_VEC3:
            return 3;
        case TINYGLTF_TYPE_VEC4:
            return 4;
        case TINYGLTF_TYPE_MAT2:
            return 4;
        case TINYGLTF_TYPE_MAT3:
            return 9;
        case TINYGLTF_TYPE_MAT4:
            return 16;
        default:
            throw std::runtime_error("Unsupported glTF accessor type");
    }
}

glm::mat4 nodeLocalMatrix(const tinygltf::Node& node)
{
    if (node.matrix.size() == 16) {
        glm::mat4 matrix(1.0f);
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                // glTF stores mat4 in column-major order.
                matrix[col][row] = static_cast<float>(node.matrix[col * 4 + row]);
            }
        }
        return matrix;
    }

    glm::vec3 translation(0.0f, 0.0f, 0.0f);
    if (node.translation.size() == 3) {
        translation = glm::vec3(
            static_cast<float>(node.translation[0]),
            static_cast<float>(node.translation[1]),
            static_cast<float>(node.translation[2]));
    }

    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    if (node.rotation.size() == 4) {
        rotation = glm::quat(
            static_cast<float>(node.rotation[3]),
            static_cast<float>(node.rotation[0]),
            static_cast<float>(node.rotation[1]),
            static_cast<float>(node.rotation[2]));
    }

    glm::vec3 scale(1.0f, 1.0f, 1.0f);
    if (node.scale.size() == 3) {
        scale = glm::vec3(
            static_cast<float>(node.scale[0]),
            static_cast<float>(node.scale[1]),
            static_cast<float>(node.scale[2]));
    }

    const glm::mat4 t = glm::translate(glm::mat4(1.0f), translation);
    const glm::mat4 r = glm::mat4_cast(rotation);
    const glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);
    return t * r * s;
}

const tinygltf::Accessor& requireAccessor(const tinygltf::Model& model, int accessorIndex)
{
    if (accessorIndex < 0 || accessorIndex >= static_cast<int>(model.accessors.size())) {
        throw std::runtime_error("Invalid glTF accessor index");
    }
    return model.accessors[static_cast<size_t>(accessorIndex)];
}

const unsigned char* accessorDataBegin(const tinygltf::Model& model, const tinygltf::Accessor& accessor)
{
    if (accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(model.bufferViews.size())) {
        throw std::runtime_error("Accessor references invalid bufferView");
    }

    const tinygltf::BufferView& view = model.bufferViews[static_cast<size_t>(accessor.bufferView)];
    if (view.buffer < 0 || view.buffer >= static_cast<int>(model.buffers.size())) {
        throw std::runtime_error("BufferView references invalid buffer");
    }

    const tinygltf::Buffer& buffer = model.buffers[static_cast<size_t>(view.buffer)];
    const size_t offset = static_cast<size_t>(view.byteOffset + accessor.byteOffset);
    if (offset >= buffer.data.size()) {
        throw std::runtime_error("Accessor offset out of range");
    }

    return buffer.data.data() + offset;
}

glm::vec3 readVec3(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    size_t index)
{
    if (accessor.type != TINYGLTF_TYPE_VEC3) {
        throw std::runtime_error("Accessor is not VEC3");
    }
    if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
        throw std::runtime_error("VEC3 accessor must be float");
    }

    const tinygltf::BufferView& view = model.bufferViews[static_cast<size_t>(accessor.bufferView)];
    const size_t defaultStride = componentSize(accessor.componentType) * numComponents(accessor.type);
    const size_t stride = accessor.ByteStride(view) > 0 ? static_cast<size_t>(accessor.ByteStride(view)) : defaultStride;

    const unsigned char* dataBegin = accessorDataBegin(model, accessor);
    const unsigned char* ptr = dataBegin + stride * index;

    glm::vec3 value{};
    std::memcpy(&value[0], ptr, sizeof(float) * 3);
    return value;
}

float readComponentAsFloat(const unsigned char* ptr, int componentType, bool normalized)
{
    switch (componentType) {
        case TINYGLTF_COMPONENT_TYPE_FLOAT:
            return *reinterpret_cast<const float*>(ptr);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
            const uint8_t v = *reinterpret_cast<const uint8_t*>(ptr);
            return normalized ? static_cast<float>(v) / 255.0f : static_cast<float>(v);
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
            const uint16_t v = *reinterpret_cast<const uint16_t*>(ptr);
            return normalized ? static_cast<float>(v) / 65535.0f : static_cast<float>(v);
        }
        case TINYGLTF_COMPONENT_TYPE_BYTE: {
            const int8_t v = *reinterpret_cast<const int8_t*>(ptr);
            if (!normalized) {
                return static_cast<float>(v);
            }
            return std::max(-1.0f, static_cast<float>(v) / 127.0f);
        }
        case TINYGLTF_COMPONENT_TYPE_SHORT: {
            const int16_t v = *reinterpret_cast<const int16_t*>(ptr);
            if (!normalized) {
                return static_cast<float>(v);
            }
            return std::max(-1.0f, static_cast<float>(v) / 32767.0f);
        }
        default:
            throw std::runtime_error("Unsupported accessor component conversion");
    }
}

glm::vec2 readVec2(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    size_t index)
{
    if (accessor.type != TINYGLTF_TYPE_VEC2) {
        throw std::runtime_error("Accessor is not VEC2");
    }

    const tinygltf::BufferView& view = model.bufferViews[static_cast<size_t>(accessor.bufferView)];
    const size_t defaultStride = componentSize(accessor.componentType) * numComponents(accessor.type);
    const size_t stride = accessor.ByteStride(view) > 0 ? static_cast<size_t>(accessor.ByteStride(view)) : defaultStride;
    const size_t componentBytes = componentSize(accessor.componentType);

    const unsigned char* dataBegin = accessorDataBegin(model, accessor);
    const unsigned char* ptr = dataBegin + stride * index;

    glm::vec2 value{};
    value.x = readComponentAsFloat(ptr + componentBytes * 0, accessor.componentType, accessor.normalized);
    value.y = readComponentAsFloat(ptr + componentBytes * 1, accessor.componentType, accessor.normalized);
    return value;
}

std::vector<uint32_t> readIndices(const tinygltf::Model& model, const tinygltf::Accessor& accessor)
{
    if (accessor.type != TINYGLTF_TYPE_SCALAR) {
        throw std::runtime_error("Index accessor must be scalar");
    }

    const tinygltf::BufferView& view = model.bufferViews[static_cast<size_t>(accessor.bufferView)];
    const size_t defaultStride = componentSize(accessor.componentType);
    const size_t stride = accessor.ByteStride(view) > 0 ? static_cast<size_t>(accessor.ByteStride(view)) : defaultStride;
    const unsigned char* dataBegin = accessorDataBegin(model, accessor);

    std::vector<uint32_t> indices;
    indices.resize(accessor.count);

    for (size_t i = 0; i < accessor.count; ++i) {
        const unsigned char* ptr = dataBegin + stride * i;
        switch (accessor.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                indices[i] = static_cast<uint32_t>(*reinterpret_cast<const uint8_t*>(ptr));
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                indices[i] = static_cast<uint32_t>(*reinterpret_cast<const uint16_t*>(ptr));
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                indices[i] = *reinterpret_cast<const uint32_t*>(ptr);
                break;
            default:
                throw std::runtime_error("Unsupported index component type");
        }
    }

    return indices;
}

void buildNormalsForRange(MeshData& mesh, size_t indexBegin, size_t indexEnd)
{
    if (indexEnd <= indexBegin || (indexEnd - indexBegin) < 3) {
        return;
    }

    for (size_t i = indexBegin; i + 2 < indexEnd; i += 3) {
        const uint32_t ia = mesh.indices[i + 0];
        const uint32_t ib = mesh.indices[i + 1];
        const uint32_t ic = mesh.indices[i + 2];

        const glm::vec3& a = mesh.vertices[ia].position;
        const glm::vec3& b = mesh.vertices[ib].position;
        const glm::vec3& c = mesh.vertices[ic].position;

        const glm::vec3 n = glm::cross(b - a, c - a);
        if (glm::dot(n, n) < 1e-12f) {
            continue;
        }

        mesh.vertices[ia].normal += n;
        mesh.vertices[ib].normal += n;
        mesh.vertices[ic].normal += n;
    }

    for (size_t i = indexBegin; i < indexEnd; ++i) {
        const uint32_t idx = mesh.indices[i];
        const float len2 = glm::dot(mesh.vertices[idx].normal, mesh.vertices[idx].normal);
        if (len2 < 1e-12f) {
            mesh.vertices[idx].normal = glm::vec3(0.0f, 1.0f, 0.0f);
        } else {
            mesh.vertices[idx].normal = glm::normalize(mesh.vertices[idx].normal);
        }
    }
}

void appendPrimitive(
    const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    const glm::mat4& worldMatrix,
    uint32_t materialIndex,
    MeshData& outMesh)
{
    if (primitive.mode != -1 && primitive.mode != TINYGLTF_MODE_TRIANGLES) {
        return;
    }

    const auto posIt = primitive.attributes.find("POSITION");
    if (posIt == primitive.attributes.end()) {
        return;
    }

    const tinygltf::Accessor& positionAccessor = requireAccessor(model, posIt->second);
    const tinygltf::Accessor* normalAccessor = nullptr;
    const tinygltf::Accessor* uv0Accessor = nullptr;
    const tinygltf::Accessor* uv1Accessor = nullptr;

    auto findTexCoordAccessor = [&](int requestedSet) -> const tinygltf::Accessor* {
        const auto findBySet = [&](int setIndex) -> const tinygltf::Accessor* {
            const std::string key = "TEXCOORD_" + std::to_string(setIndex);
            const auto it = primitive.attributes.find(key);
            if (it == primitive.attributes.end()) {
                return nullptr;
            }
            return &requireAccessor(model, it->second);
        };

        if (const tinygltf::Accessor* acc = findBySet(requestedSet)) {
            return acc;
        }
        if (requestedSet != 0) {
            if (const tinygltf::Accessor* acc = findBySet(0)) {
                return acc;
            }
        }
        if (requestedSet != 1) {
            if (const tinygltf::Accessor* acc = findBySet(1)) {
                return acc;
            }
        }

        for (const auto& [attrName, accessorIndex] : primitive.attributes) {
            if (std::string_view(attrName).rfind("TEXCOORD_", 0) == 0) {
                return &requireAccessor(model, accessorIndex);
            }
        }

        return nullptr;
    };
    const auto normalIt = primitive.attributes.find("NORMAL");
    if (normalIt != primitive.attributes.end()) {
        normalAccessor = &requireAccessor(model, normalIt->second);
    }
    uv0Accessor = findTexCoordAccessor(0);
    uv1Accessor = findTexCoordAccessor(1);
    if (uv1Accessor == nullptr) {
        uv1Accessor = uv0Accessor;
    }

    const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(worldMatrix)));

    const size_t baseVertex = outMesh.vertices.size();
    outMesh.vertices.reserve(outMesh.vertices.size() + positionAccessor.count);

    for (size_t i = 0; i < positionAccessor.count; ++i) {
        const glm::vec3 localPos = readVec3(model, positionAccessor, i);
        const glm::vec3 worldPos = glm::vec3(worldMatrix * glm::vec4(localPos, 1.0f));

        MeshVertex vertex{};
        vertex.position = worldPos;

        if (normalAccessor != nullptr && i < normalAccessor->count) {
            glm::vec3 localNormal = readVec3(model, *normalAccessor, i);
            const glm::vec3 worldNormal = normalMatrix * localNormal;
            if (glm::dot(worldNormal, worldNormal) > 1e-12f) {
                vertex.normal = glm::normalize(worldNormal);
            } else {
                vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }
        } else {
            vertex.normal = glm::vec3(0.0f, 0.0f, 0.0f);
        }

        if (uv0Accessor != nullptr && i < uv0Accessor->count) {
            vertex.uv0 = readVec2(model, *uv0Accessor, i);
        } else {
            vertex.uv0 = glm::vec2(0.0f, 0.0f);
        }

        if (uv1Accessor != nullptr && i < uv1Accessor->count) {
            vertex.uv1 = readVec2(model, *uv1Accessor, i);
        } else {
            vertex.uv1 = vertex.uv0;
        }

        outMesh.vertices.push_back(vertex);

        outMesh.boundsMin = glm::min(outMesh.boundsMin, worldPos);
        outMesh.boundsMax = glm::max(outMesh.boundsMax, worldPos);
    }

    const size_t indexBegin = outMesh.indices.size();
    if (primitive.indices >= 0) {
        const tinygltf::Accessor& indexAccessor = requireAccessor(model, primitive.indices);
        std::vector<uint32_t> localIndices = readIndices(model, indexAccessor);
        outMesh.indices.reserve(outMesh.indices.size() + localIndices.size());
        for (const uint32_t idx : localIndices) {
            outMesh.indices.push_back(static_cast<uint32_t>(baseVertex) + idx);
        }
    } else {
        outMesh.indices.reserve(outMesh.indices.size() + positionAccessor.count);
        for (size_t i = 0; i < positionAccessor.count; ++i) {
            outMesh.indices.push_back(static_cast<uint32_t>(baseVertex + i));
        }
    }
    const size_t indexEnd = outMesh.indices.size();

    if (indexEnd > indexBegin) {
        outMesh.subMeshes.push_back(SubMeshData{
            static_cast<uint32_t>(indexBegin),
            static_cast<uint32_t>(indexEnd - indexBegin),
            materialIndex,
        });
    }

    if (normalAccessor == nullptr) {
        buildNormalsForRange(outMesh, indexBegin, indexEnd);
    }
}

MaterialData extractMaterialData(const tinygltf::Model& model, int materialIndex)
{
    MaterialData out;
    if (materialIndex < 0 || materialIndex >= static_cast<int>(model.materials.size())) {
        return out;
    }

    const tinygltf::Material& material = model.materials[static_cast<size_t>(materialIndex)];

    const auto& factor = material.pbrMetallicRoughness.baseColorFactor;
    if (factor.size() == 4) {
        out.baseColorFactor = glm::vec4(
            static_cast<float>(factor[0]),
            static_cast<float>(factor[1]),
            static_cast<float>(factor[2]),
            static_cast<float>(factor[3]));
    }

    out.metallicFactor = static_cast<float>(material.pbrMetallicRoughness.metallicFactor);
    out.roughnessFactor = static_cast<float>(material.pbrMetallicRoughness.roughnessFactor);

    if (material.normalTexture.index >= 0) {
        out.normalScale = static_cast<float>(material.normalTexture.scale);
    }
    if (material.occlusionTexture.index >= 0) {
        out.occlusionStrength = static_cast<float>(material.occlusionTexture.strength);
    }

    out.baseColorTransform = readTextureTransform(material.pbrMetallicRoughness.baseColorTexture);
    out.normalTransform = readTextureTransform(material.normalTexture);
    out.ormTransform = readTextureTransform(material.pbrMetallicRoughness.metallicRoughnessTexture);

    out.baseColorTexture = extractTextureFromTextureIndex(
        model,
        material.pbrMetallicRoughness.baseColorTexture.index);
    out.normalTexture = extractTextureFromTextureIndex(model, material.normalTexture.index);
    out.ormTexture = extractTextureFromTextureIndex(
        model,
        material.pbrMetallicRoughness.metallicRoughnessTexture.index);

    // If metallicRoughness texture is missing but occlusion texture exists, reuse it as ORM input.
    if (!out.ormTexture.valid() && material.occlusionTexture.index >= 0) {
        out.ormTexture = extractTextureFromTextureIndex(model, material.occlusionTexture.index);
        out.ormTransform = readTextureTransform(material.occlusionTexture);
    }

    return out;
}

uint32_t resolveMaterialIndex(const tinygltf::Model& model, const tinygltf::Primitive& primitive)
{
    if (!model.materials.empty() && primitive.material >= 0
        && primitive.material < static_cast<int>(model.materials.size())) {
        return static_cast<uint32_t>(primitive.material);
    }
    return 0;
}

TextureData extractTextureFromTextureIndex(const tinygltf::Model& model, int textureIndex)
{
    if (textureIndex < 0 || textureIndex >= static_cast<int>(model.textures.size())) {
        return {};
    }

    const tinygltf::Texture& texture = model.textures[static_cast<size_t>(textureIndex)];
    if (texture.source < 0 || texture.source >= static_cast<int>(model.images.size())) {
        return {};
    }

    const tinygltf::Image& image = model.images[static_cast<size_t>(texture.source)];
    if (image.width <= 0 || image.height <= 0 || image.image.empty()) {
        return {};
    }
    if (image.bits != 8) {
        throw std::runtime_error("Only 8-bit baseColor textures are supported");
    }

    int channels = image.component;
    if (channels <= 0) {
        throw std::runtime_error("Invalid glTF image component count");
    }

    const size_t pixelCount = static_cast<size_t>(image.width) * static_cast<size_t>(image.height);
    const size_t inputStride = static_cast<size_t>(channels);
    if (image.image.size() < pixelCount * inputStride) {
        throw std::runtime_error("glTF image payload is truncated");
    }

    TextureData out;
    out.width = static_cast<uint32_t>(image.width);
    out.height = static_cast<uint32_t>(image.height);
    out.rgba8.resize(pixelCount * 4);

    for (size_t i = 0; i < pixelCount; ++i) {
        const uint8_t* src = &image.image[i * inputStride];
        uint8_t r = 255;
        uint8_t g = 255;
        uint8_t b = 255;
        uint8_t a = 255;

        switch (channels) {
            case 1:
                r = g = b = src[0];
                break;
            case 2:
                r = g = b = src[0];
                a = src[1];
                break;
            case 3:
                r = src[0];
                g = src[1];
                b = src[2];
                break;
            default:
                r = src[0];
                g = src[1];
                b = src[2];
                a = src[3];
                break;
        }

        uint8_t* dst = &out.rgba8[i * 4];
        dst[0] = r;
        dst[1] = g;
        dst[2] = b;
        dst[3] = a;
    }

    return out;
}

} // namespace

MeshData ModelLoader::loadFromFile(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Model file does not exist: " + path.string());
    }

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;

    std::string warnings;
    std::string errors;

    bool ok = false;
    if (path.extension() == ".glb") {
        ok = loader.LoadBinaryFromFile(&model, &errors, &warnings, path.string());
    } else {
        ok = loader.LoadASCIIFromFile(&model, &errors, &warnings, path.string());
    }

    if (!warnings.empty()) {
        // Non-fatal; ignored by loader caller.
    }

    if (!ok) {
        throw std::runtime_error("Failed to load model: " + errors);
    }

    MeshData result{};
    result.boundsMin = glm::vec3(std::numeric_limits<float>::max());
    result.boundsMax = glm::vec3(std::numeric_limits<float>::lowest());

    if (model.materials.empty()) {
        result.materials.push_back(MaterialData{});
    } else {
        result.materials.reserve(model.materials.size());
        for (size_t i = 0; i < model.materials.size(); ++i) {
            result.materials.push_back(extractMaterialData(model, static_cast<int>(i)));
        }
    }

    int sceneIndex = model.defaultScene;
    if (sceneIndex < 0) {
        sceneIndex = 0;
    }

    if (sceneIndex < 0 || sceneIndex >= static_cast<int>(model.scenes.size())) {
        throw std::runtime_error("glTF scene is missing");
    }

    std::function<void(int, const glm::mat4&)> visitNode;
    visitNode = [&](int nodeIndex, const glm::mat4& parentMatrix) {
        if (nodeIndex < 0 || nodeIndex >= static_cast<int>(model.nodes.size())) {
            return;
        }

        const tinygltf::Node& node = model.nodes[static_cast<size_t>(nodeIndex)];
        const glm::mat4 worldMatrix = parentMatrix * nodeLocalMatrix(node);

        if (node.mesh >= 0 && node.mesh < static_cast<int>(model.meshes.size())) {
            const tinygltf::Mesh& mesh = model.meshes[static_cast<size_t>(node.mesh)];
            for (const tinygltf::Primitive& primitive : mesh.primitives) {
                const uint32_t materialIndex = resolveMaterialIndex(model, primitive);
                appendPrimitive(model, primitive, worldMatrix, materialIndex, result);
            }
        }

        for (const int child : node.children) {
            visitNode(child, worldMatrix);
        }
    };

    const tinygltf::Scene& scene = model.scenes[static_cast<size_t>(sceneIndex)];
    for (const int nodeIndex : scene.nodes) {
        visitNode(nodeIndex, glm::mat4(1.0f));
    }

    if (result.vertices.empty() || result.indices.empty()) {
        throw std::runtime_error("No drawable mesh primitive found in model");
    }

    if (result.subMeshes.empty()) {
        result.subMeshes.push_back(SubMeshData{
            0,
            static_cast<uint32_t>(result.indices.size()),
            0,
        });
    }

    // Backward-compatible summary fields for single-material callers.
    result.baseColorFactor = result.materials[0].baseColorFactor;
    result.baseColorTexture = result.materials[0].baseColorTexture;
    for (const MaterialData& material : result.materials) {
        if (material.baseColorTexture.valid()) {
            result.baseColorFactor = material.baseColorFactor;
            result.baseColorTexture = material.baseColorTexture;
            break;
        }
    }

    return result;
}

} // namespace ku::asset
