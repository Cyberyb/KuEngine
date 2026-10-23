#pragma once

#include <KuEngine/Asset/Model.h>

#include <array>
#include <string_view>

namespace ku::cornell {
enum Material : uint32_t { Floor, Ceiling, BackWall, RedWall, GreenWall, ShortBox, TallBox, Lamp, MaterialCount };
inline constexpr std::array<std::string_view, MaterialCount> materialNames{
    "Floor", "Ceiling", "Back wall", "Red left wall", "Green right wall", "Short box", "Tall box", "Ceiling lamp"};

// World-space geometry, room x/z [-1,1], y [0,2], opening towards +Z.
// Every surface has its own vertices so corners retain flat normals.
[[nodiscard]] asset::MeshData makeScene();
[[nodiscard]] asset::SceneLightingConfig makeLighting(float strength = 1.0f);
[[nodiscard]] asset::SceneCameraConfig defaultCamera();
} // namespace ku::cornell
