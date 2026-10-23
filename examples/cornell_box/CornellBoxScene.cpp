#include "CornellBoxScene.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <limits>

namespace ku::cornell {
namespace {
void quad(asset::MeshData& mesh, glm::vec3 center, glm::vec3 u, glm::vec3 v,
    const glm::mat4& world = glm::mat4(1.0f))
{
    const glm::vec3 normal = glm::normalize(glm::mat3(world) * glm::cross(u, v));
    const glm::vec3 tangent = glm::normalize(glm::mat3(world) * u);
    const uint32_t start = static_cast<uint32_t>(mesh.vertices.size());
    const std::array<glm::vec2, 4> corners{{{-1,-1},{1,-1},{1,1},{-1,1}}};
    for (auto corner : corners) {
        asset::MeshVertex vertex{};
        vertex.position = glm::vec3(world * glm::vec4(center + corner.x*u + corner.y*v, 1));
        vertex.normal = normal;
        vertex.tangent = glm::vec4(tangent, 1);
        vertex.uv0 = vertex.uv1 = (corner + glm::vec2(1)) * .5f;
        mesh.vertices.push_back(vertex);
    }
    for (uint32_t i : {0u,1u,2u,0u,2u,3u}) mesh.indices.push_back(start+i);
}
void beginPart(asset::MeshData& mesh, uint32_t material)
{
    mesh.subMeshes.push_back({static_cast<uint32_t>(mesh.indices.size()),0,material});
}
void endPart(asset::MeshData& mesh)
{
    auto& part = mesh.subMeshes.back();
    part.indexCount = static_cast<uint32_t>(mesh.indices.size()) - part.indexStart;
}
void box(asset::MeshData& mesh, uint32_t material, glm::vec3 center, glm::vec3 size, float yaw)
{
    const glm::mat4 world = glm::translate(glm::mat4(1),center)
        * glm::rotate(glm::mat4(1),glm::radians(yaw),glm::vec3(0,1,0));
    const glm::vec3 h = size*.5f;
    beginPart(mesh,material);
    quad(mesh,{h.x,0,0},{0,0,-h.z},{0,h.y,0},world);
    quad(mesh,{-h.x,0,0},{0,0,h.z},{0,h.y,0},world);
    quad(mesh,{0,h.y,0},{h.x,0,0},{0,0,-h.z},world);
    quad(mesh,{0,-h.y,0},{h.x,0,0},{0,0,h.z},world);
    quad(mesh,{0,0,h.z},{h.x,0,0},{0,h.y,0},world);
    quad(mesh,{0,0,-h.z},{-h.x,0,0},{0,h.y,0},world);
    endPart(mesh);
}
}

asset::MeshData makeScene()
{
    asset::MeshData mesh;
    mesh.materials.resize(MaterialCount);
    for (auto& material : mesh.materials) {
        material.baseColorFactor = {.73f,.73f,.70f,1};
        material.metallicFactor = 0;
        material.roughnessFactor = .85f;
        material.shadingModel = asset::ShadingModel::PBR;
    }
    mesh.materials[RedWall].baseColorFactor = {.63f,.035f,.025f,1};
    mesh.materials[GreenWall].baseColorFactor = {.045f,.43f,.055f,1};
    mesh.materials[ShortBox].roughnessFactor = .36f;
    mesh.materials[TallBox].roughnessFactor = .72f;
    mesh.materials[Lamp].baseColorFactor = {0,0,0,1};
    mesh.materials[Lamp].emissiveFactor = {12,10.8f,9.0f};

    const auto wall = [&](uint32_t material,glm::vec3 center,glm::vec3 u,glm::vec3 v) {
        beginPart(mesh,material); quad(mesh,center,u,v); endPart(mesh);
    };
    wall(Floor,{0,0,0},{1,0,0},{0,0,-1});
    wall(Ceiling,{0,2,0},{1,0,0},{0,0,1});
    wall(BackWall,{0,1,-1},{1,0,0},{0,1,0});
    wall(RedWall,{-1,1,0},{0,0,-1},{0,1,0});
    wall(GreenWall,{1,1,0},{0,0,1},{0,1,0});
    box(mesh,ShortBox,{-.40f,.325f,.30f},{.58f,.65f,.58f},-17);
    box(mesh,TallBox,{.36f,.60f,-.30f},{.58f,1.20f,.58f},19);
    wall(Lamp,{0,1.985f,-.12f},{.30f,0,0},{0,0,.24f});

    mesh.boundsMin = glm::vec3(std::numeric_limits<float>::max());
    mesh.boundsMax = glm::vec3(std::numeric_limits<float>::lowest());
    for (const auto& vertex : mesh.vertices) {
        mesh.boundsMin = glm::min(mesh.boundsMin,vertex.position);
        mesh.boundsMax = glm::max(mesh.boundsMax,vertex.position);
    }
    return mesh;
}

asset::SceneLightingConfig makeLighting(float strength)
{
    asset::SceneLightingConfig lighting;
    lighting.intensity = 0;
    // Four point emitters approximate the ceiling source using the public
    // direct-light model; the emissive quad itself does not cast light.
    for (float x : {-.18f,.18f}) for (float z : {-.27f,.03f})
        lighting.pointLights.push_back({{x,1.82f,z},{1,.91f,.78f},3.2f*std::max(strength,0.f),6});
    return lighting;
}

asset::SceneCameraConfig defaultCamera()
{
    asset::SceneCameraConfig camera;
    camera.position = {0,1,4.1f};
    camera.target = {0,1,0};
    camera.fovYDeg = 40;
    camera.nearPlane = .05f;
    camera.farPlane = 20;
    return camera;
}
} // namespace ku::cornell
