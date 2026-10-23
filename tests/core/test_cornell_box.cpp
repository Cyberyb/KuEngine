#include <gtest/gtest.h>
#include "../../examples/cornell_box/CornellBoxScene.h"
#include <glm/geometric.hpp>
#include <algorithm>
#include <cmath>

TEST(CornellBoxScene, HasRoomTwoBoxesAndSeparateCeilingEmitter)
{
    const auto mesh=ku::cornell::makeScene();
    ASSERT_EQ(mesh.subMeshes.size(),8u);
    ASSERT_EQ(mesh.materials.size(),ku::cornell::MaterialCount);
    EXPECT_EQ(mesh.indices.size(),108u);
    EXPECT_EQ(mesh.boundsMin,glm::vec3(-1,0,-1));
    EXPECT_EQ(mesh.boundsMax,glm::vec3(1,2,1));
    for (const auto& part:mesh.subMeshes) {
        EXPECT_LE(size_t(part.indexStart)+part.indexCount,mesh.indices.size());
        EXPECT_LT(part.materialIndex,mesh.materials.size());
    }
    for (auto index:mesh.indices) EXPECT_LT(index,mesh.vertices.size());
}
TEST(CornellBoxScene, TriangleWindingAgreesWithFlatNormalsAndTangents)
{
    const auto mesh=ku::cornell::makeScene();
    for (size_t i=0;i<mesh.indices.size();i+=3) {
        const auto& a=mesh.vertices[mesh.indices[i]];
        const auto& b=mesh.vertices[mesh.indices[i+1]];
        const auto& c=mesh.vertices[mesh.indices[i+2]];
        EXPECT_GT(glm::dot(glm::cross(b.position-a.position,c.position-a.position),a.normal),.01f);
        EXPECT_NEAR(glm::length(a.normal),1,1e-5f);
        EXPECT_NEAR(glm::dot(a.normal,glm::vec3(a.tangent)),0,1e-5f);
        EXPECT_EQ(a.normal,b.normal); EXPECT_EQ(a.normal,c.normal);
    }
}
TEST(CornellBoxScene, RoomFacesInwardWhileBoxesAreGroundedAndRotate)
{
    const auto mesh=ku::cornell::makeScene();
    for (size_t i=0;i<5;++i) {
        const auto& part=mesh.subMeshes[i];
        const auto& vertex=mesh.vertices[mesh.indices[part.indexStart]];
        EXPECT_GT(glm::dot(glm::vec3(0,1,0)-vertex.position,vertex.normal),.9f);
    }
    for (auto slot : {ku::cornell::ShortBox,ku::cornell::TallBox}) {
        const auto& part=mesh.subMeshes[slot];
        glm::vec3 lo(10),hi(-10);
        for (size_t i=part.indexStart;i<size_t(part.indexStart)+part.indexCount;++i) {
            const auto p=mesh.vertices[mesh.indices[i]].position;
            lo=glm::min(lo,p);hi=glm::max(hi,p);
        }
        EXPECT_NEAR(lo.y,0,1e-5f);
        EXPECT_GT(hi.y,.6f);EXPECT_LT(hi.y,2);
        EXPECT_GT(lo.x,-1);EXPECT_LT(hi.x,1);
        EXPECT_GT(lo.z,-1);EXPECT_LT(hi.z,1);
        const auto n=mesh.vertices[mesh.indices[part.indexStart]].normal;
        EXPECT_GT(std::abs(n.x),.1f);EXPECT_GT(std::abs(n.z),.1f);
    }
    const auto& lamp=mesh.vertices[mesh.indices[mesh.subMeshes[ku::cornell::Lamp].indexStart]];
    EXPECT_LT(lamp.position.y,2);EXPECT_GT(lamp.position.y,1.9f);
    EXPECT_LT(lamp.normal.y,-.99f);
}
TEST(CornellBoxScene, UsesUntexturedNonMetallicPbrWithRedLeftAndGreenRight)
{
    const auto mesh=ku::cornell::makeScene();
    for (const auto& m:mesh.materials) {
        EXPECT_EQ(m.shadingModel,ku::asset::ShadingModel::PBR);
        EXPECT_EQ(m.alphaMode,ku::asset::AlphaMode::Opaque);
        EXPECT_FLOAT_EQ(m.metallicFactor,0);
        EXPECT_GT(m.roughnessFactor,0);EXPECT_LE(m.roughnessFactor,1);
        EXPECT_FALSE(m.baseColorTexture.valid());
        EXPECT_FALSE(m.doubleSided);
    }
    EXPECT_GT(mesh.materials[ku::cornell::RedWall].baseColorFactor.r,.6f);
    EXPECT_GT(mesh.materials[ku::cornell::GreenWall].baseColorFactor.g,.4f);
    EXPECT_GT(mesh.materials[ku::cornell::Lamp].emissiveFactor.r,1);
    EXPECT_NE(mesh.materials[ku::cornell::ShortBox].roughnessFactor,mesh.materials[ku::cornell::TallBox].roughnessFactor);
}
TEST(CornellBoxScene, CeilingPointLightsStayInsideRoomAndScaleToZero)
{
    const auto lighting=ku::cornell::makeLighting();
    ASSERT_EQ(lighting.pointLights.size(),4u);
    EXPECT_FLOAT_EQ(lighting.intensity,0);
    for (const auto& point:lighting.pointLights) {
        EXPECT_GT(point.position.y,1.5f);EXPECT_LT(point.position.y,2);
        EXPECT_GT(point.intensity,0);EXPECT_GT(point.range,2);
    }
    for (const auto& point:ku::cornell::makeLighting(0).pointLights) EXPECT_FLOAT_EQ(point.intensity,0);
    const auto camera=ku::cornell::defaultCamera();
    EXPECT_GT(camera.position.z,1);
    EXPECT_GT(camera.nearPlane,0);EXPECT_GT(camera.farPlane,camera.position.z+1);
}
