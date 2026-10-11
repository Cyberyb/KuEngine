#include "MclarenSceneEditing.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ku {
namespace {
bool validMaterial(const asset::SceneMaterialSlot& m) {
    if (m.shadingModel != asset::ShadingModel::PBR && m.shadingModel != asset::ShadingModel::Unlit) return false;
    for (float v : m.baseColor) if (!std::isfinite(v) || v < 0.f || v > 1.f) return false;
    for (float v : m.emissive) if (!std::isfinite(v) || v < 0.f || v > 64.f) return false;
    return std::isfinite(m.metallic) && m.metallic >= 0.f && m.metallic <= 1.f
        && std::isfinite(m.roughness) && m.roughness >= 0.f && m.roughness <= 1.f;
}
}
bool MclarenSceneEditor::queue(MclarenSceneEdit edit, std::string& error) {
    error.clear();
    if (m_pending) { error = "An instance edit is already pending"; return false; }
    if ((!edit.transform && !edit.material) || (edit.transform && !asset::validSceneTransform(*edit.transform))
        || (edit.material && !validMaterial(*edit.material))) {
        error = "Invalid edit: finite TRS/positive scale, base/metallic/roughness [0,1], emissive [0,64] required";
        return false;
    }
    m_pending = std::move(edit);
    return true;
}
bool MclarenSceneEditor::publish(asset::SceneData& scene,
    const std::function<bool(const std::vector<asset::SceneInstance>&, std::string&)>& prepare,
    std::string& error) {
    if (!m_pending) return false;
    const auto edit = std::move(*m_pending); m_pending.reset(); error.clear();
    if (edit.instance >= scene.instances.size()) { error = "Invalid instance index"; return false; }
    const auto* mesh = scene.findMesh(scene.instances[edit.instance].mesh);
    const size_t slots = mesh ? std::max(size_t(1), mesh->mesh.materials.size()) : 0;
    if (!mesh || (edit.material && edit.material->slot >= slots)) { error = "Invalid material slot"; return false; }
    try {
        auto candidate = scene.instances;
        auto& target = candidate[edit.instance];
        if (edit.transform) target.transform = *edit.transform;
        glm::vec3 minimum{}, maximum{};
        if (!asset::transformBounds(mesh->mesh.boundsMin, mesh->mesh.boundsMax,
                asset::sceneTransformMatrix(target.transform), minimum, maximum)) {
            error = "Edited transform produces non-finite bounds"; return false;
        }
        const auto normal = asset::sceneNormalMatrix(asset::sceneTransformMatrix(target.transform));
        for (int column=0;column<3;++column) for(int row=0;row<3;++row)
            if(!std::isfinite(normal[column][row])) {
                error = "Edited transform produces a non-finite normal matrix"; return false;
            }
        if (edit.material) {
            auto found = std::find_if(target.materialSlots.begin(), target.materialSlots.end(),
                [&](const auto& m) { return m.slot == edit.material->slot; });
            if (found == target.materialSlots.end()) target.materialSlots.push_back(*edit.material);
            else *found = *edit.material;
        }
        if (!prepare(candidate, error)) return false;
        scene.instances.swap(candidate); ++m_generation;
        return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
asset::SceneMaterialSlot editableMaterial(const asset::SceneData& scene, size_t instance, uint32_t slot) {
    if (instance >= scene.instances.size()) throw std::out_of_range("instance");
    const auto& selected = scene.instances[instance]; const auto* mesh = scene.findMesh(selected.mesh);
    if (!mesh || slot >= std::max(size_t(1), mesh->mesh.materials.size())) throw std::out_of_range("slot");
    const asset::MaterialData fallback{};
    const auto resolved = resolveInstanceMaterial(mesh->mesh.materials.empty() ? fallback : mesh->mesh.materials[slot], selected, slot);
    return {slot, resolved.shadingModel, resolved.baseColorFactor, resolved.emissiveFactor, resolved.metallicFactor, resolved.roughnessFactor};
}
}
