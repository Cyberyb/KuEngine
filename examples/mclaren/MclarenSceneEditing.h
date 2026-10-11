#pragma once
#include <KuEngine/Asset/Scene.h>
#include <KuEngine/Render/ForwardCommon.h>
#include <functional>
#include <optional>

namespace ku {
struct MclarenSceneEdit {
    size_t instance = 0;
    std::optional<asset::SceneTransform> transform;
    std::optional<asset::SceneMaterialSlot> material;
};
// CPU-only queue. Preparation runs at the caller's fence-safe point. A failure
// leaves SceneData and generation unchanged; no geometry/pixels are copied.
class MclarenSceneEditor {
public:
    bool queue(MclarenSceneEdit edit, std::string& error);
    bool publish(asset::SceneData& scene,
        const std::function<bool(const std::vector<asset::SceneInstance>&, std::string&)>& prepare,
        std::string& error);
    bool pending() const { return m_pending.has_value(); }
    uint64_t generation() const { return m_generation; }
private:
    std::optional<MclarenSceneEdit> m_pending;
    uint64_t m_generation = 0;
};
asset::SceneMaterialSlot editableMaterial(const asset::SceneData&, size_t instance, uint32_t slot);
}
