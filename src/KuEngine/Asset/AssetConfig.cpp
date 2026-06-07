#include "AssetConfig.h"

#include <fstream>

#include <nlohmann/json.hpp>

namespace ku::asset {

namespace {

using Json = nlohmann::json;

bool readVec3(const Json& value, glm::vec3& outValue)
{
    if (!value.is_array() || value.size() != 3) {
        return false;
    }

    for (size_t i = 0; i < 3; ++i) {
        if (!value[i].is_number()) {
            return false;
        }
    }

    outValue = glm::vec3(
        value[0].get<float>(),
        value[1].get<float>(),
        value[2].get<float>());
    return true;
}

bool readFloat4(const Json& value, std::array<float, 4>& outValue)
{
    if (!value.is_array() || value.size() != 4) {
        return false;
    }

    for (size_t i = 0; i < 4; ++i) {
        if (!value[i].is_number()) {
            return false;
        }

        outValue[i] = value[i].get<float>();
    }

    return true;
}

void readTextureBinding(const Json& json, MaterialConfig::TextureBindingConfig& outBinding)
{
    if (!json.is_object()) {
        return;
    }

    if (json.contains("source") && json["source"].is_string()) {
        outBinding.source = json["source"].get<std::string>();
        outBinding.hasSource = true;
    }
    if (json.contains("colorSpace") && json["colorSpace"].is_string()) {
        outBinding.colorSpace = json["colorSpace"].get<std::string>();
        outBinding.hasColorSpace = true;
    }
    if (json.contains("channelMapping") && json["channelMapping"].is_string()) {
        outBinding.channelMapping = json["channelMapping"].get<std::string>();
        outBinding.hasChannelMapping = true;
    }
    if (json.contains("uvSet") && json["uvSet"].is_number_integer()) {
        outBinding.uvSet = json["uvSet"].get<int>();
        outBinding.hasUvSet = true;
    }
}

void setError(std::string* errorMessage, const std::string& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

} // namespace

std::filesystem::path findResourcesRoot(const std::filesystem::path& path)
{
    std::filesystem::path current = path;
    while (!current.empty()) {
        if (current.filename() == "resources") {
            return current;
        }

        const std::filesystem::path parent = current.parent_path();
        if (parent == current) {
            break;
        }

        current = parent;
    }

    return {};
}

bool loadSceneConfigFromFile(
    const std::filesystem::path& path,
    SceneConfig& outConfig,
    std::string* errorMessage)
{
    outConfig = SceneConfig{};

    if (!std::filesystem::exists(path)) {
        setError(errorMessage, "Scene config not found: " + path.string());
        return false;
    }

    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            setError(errorMessage, "Failed to open scene config: " + path.string());
            return false;
        }

        Json sceneJson;
        file >> sceneJson;

        if (sceneJson.contains("camera") && sceneJson["camera"].is_object()) {
            const Json& camera = sceneJson["camera"];
            if (camera.contains("position")) {
                (void)readVec3(camera["position"], outConfig.camera.position);
            }
            if (camera.contains("target")) {
                (void)readVec3(camera["target"], outConfig.camera.target);
            }
            if (camera.contains("up")) {
                (void)readVec3(camera["up"], outConfig.camera.up);
            }
            if (camera.contains("fovYDeg") && camera["fovYDeg"].is_number()) {
                outConfig.camera.fovYDeg = camera["fovYDeg"].get<float>();
            }
            if (camera.contains("near") && camera["near"].is_number()) {
                outConfig.camera.nearPlane = camera["near"].get<float>();
            }
            if (camera.contains("far") && camera["far"].is_number()) {
                outConfig.camera.farPlane = camera["far"].get<float>();
            }
        }

        if (sceneJson.contains("lighting") && sceneJson["lighting"].is_object()) {
            const Json& lighting = sceneJson["lighting"];
            if (lighting.contains("direction")) {
                (void)readVec3(lighting["direction"], outConfig.lighting.direction);
            }
            if (lighting.contains("color")) {
                (void)readVec3(lighting["color"], outConfig.lighting.color);
            }
            if (lighting.contains("intensity") && lighting["intensity"].is_number()) {
                outConfig.lighting.intensity = lighting["intensity"].get<float>();
            }
        }

        if (sceneJson.contains("nodes") && sceneJson["nodes"].is_array()) {
            for (const Json& nodeJson : sceneJson["nodes"]) {
                if (!nodeJson.is_object()) {
                    continue;
                }

                SceneNodeConfig node{};
                if (nodeJson.contains("id") && nodeJson["id"].is_string()) {
                    node.id = nodeJson["id"].get<std::string>();
                }
                if (nodeJson.contains("model") && nodeJson["model"].is_string()) {
                    node.model = nodeJson["model"].get<std::string>();
                }
                if (nodeJson.contains("material") && nodeJson["material"].is_string()) {
                    node.material = nodeJson["material"].get<std::string>();
                }

                if (!node.id.empty() || !node.model.empty() || !node.material.empty()) {
                    outConfig.nodes.push_back(std::move(node));
                }
            }
        }

        return true;
    } catch (const std::exception& e) {
        setError(errorMessage, std::string("Failed to parse scene config: ") + e.what());
        return false;
    }
}

bool loadMaterialConfigFromFile(
    const std::filesystem::path& path,
    MaterialConfig& outConfig,
    std::string* errorMessage)
{
    outConfig = MaterialConfig{};

    if (!std::filesystem::exists(path)) {
        setError(errorMessage, "Material config not found: " + path.string());
        return false;
    }

    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            setError(errorMessage, "Failed to open material config: " + path.string());
            return false;
        }

        Json materialJson;
        file >> materialJson;

        if (materialJson.contains("id") && materialJson["id"].is_string()) {
            outConfig.id = materialJson["id"].get<std::string>();
        }
        if (materialJson.contains("version") && materialJson["version"].is_string()) {
            outConfig.version = materialJson["version"].get<std::string>();
        }
        if (materialJson.contains("pipeline") && materialJson["pipeline"].is_string()) {
            outConfig.pipeline = materialJson["pipeline"].get<std::string>();
        }
        if (materialJson.contains("alphaMode") && materialJson["alphaMode"].is_string()) {
            outConfig.alphaMode = materialJson["alphaMode"].get<std::string>();
        }
        if (materialJson.contains("doubleSided") && materialJson["doubleSided"].is_boolean()) {
            outConfig.doubleSided = materialJson["doubleSided"].get<bool>();
        }
        if (materialJson.contains("alphaCutoff") && materialJson["alphaCutoff"].is_number()) {
            outConfig.alphaCutoff = materialJson["alphaCutoff"].get<float>();
            outConfig.hasAlphaCutoff = true;
        }

        if (materialJson.contains("baseColorFactor") && readFloat4(materialJson["baseColorFactor"], outConfig.baseColorFactor)) {
            outConfig.hasBaseColorFactor = true;
        }

        if (materialJson.contains("metallicFactor") && materialJson["metallicFactor"].is_number()) {
            outConfig.metallicFactor = materialJson["metallicFactor"].get<float>();
            outConfig.hasMetallicFactor = true;
        }
        if (materialJson.contains("roughnessFactor") && materialJson["roughnessFactor"].is_number()) {
            outConfig.roughnessFactor = materialJson["roughnessFactor"].get<float>();
            outConfig.hasRoughnessFactor = true;
        }
        if (materialJson.contains("normalScale") && materialJson["normalScale"].is_number()) {
            outConfig.normalScale = materialJson["normalScale"].get<float>();
            outConfig.hasNormalScale = true;
        }
        if (materialJson.contains("occlusionStrength") && materialJson["occlusionStrength"].is_number()) {
            outConfig.occlusionStrength = materialJson["occlusionStrength"].get<float>();
            outConfig.hasOcclusionStrength = true;
        }

        if (materialJson.contains("textureBindings") && materialJson["textureBindings"].is_object()) {
            const Json& bindings = materialJson["textureBindings"];
            if (bindings.contains("baseColor")) {
                readTextureBinding(bindings["baseColor"], outConfig.baseColorBinding);
            }
            if (bindings.contains("normal")) {
                readTextureBinding(bindings["normal"], outConfig.normalBinding);
            }
            if (bindings.contains("orm")) {
                readTextureBinding(bindings["orm"], outConfig.ormBinding);
            }
        }

        return true;
    } catch (const std::exception& e) {
        setError(errorMessage, std::string("Failed to parse material config: ") + e.what());
        return false;
    }
}

} // namespace ku::asset
