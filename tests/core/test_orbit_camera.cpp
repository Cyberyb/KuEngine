#include <gtest/gtest.h>

#include <cmath>

#include "OrbitCameraController.h"

TEST(OrbitCameraControllerTest, BuildsClampedViewportRegion)
{
    ku::OrbitCameraController camera;
    camera.onResize(1000, 500);
    camera.setViewportRegion(0.25f, 0.5f);

    const ku::OrbitViewportRegion region = camera.viewportRegion();
    EXPECT_FLOAT_EQ(region.x, 250.0f);
    EXPECT_FLOAT_EQ(region.width, 500.0f);
    EXPECT_EQ(region.scissorX, 250u);
    EXPECT_EQ(region.scissorWidth, 500u);
    EXPECT_EQ(region.height, 500u);

    camera.setViewportRegion(0.9f, 0.5f);
    EXPECT_NEAR(camera.visibleWidth(), 0.1f, 1e-6f);
}

TEST(OrbitCameraControllerTest, BuildsFiniteVulkanProjection)
{
    ku::asset::SceneCameraConfig config{};
    config.position = {0.0f, 0.0f, 5.0f};
    config.target = {0.0f, 0.0f, 0.0f};

    ku::OrbitCameraController camera;
    camera.reset(config);
    camera.onResize(1280, 720);
    camera.setProjection(60.0f, 0.1f, 200.0f);

    const ku::OrbitCameraFrame frame = camera.frame();
    EXPECT_FLOAT_EQ(camera.distance(), 5.0f);
    EXPECT_LT(frame.projection[1][1], 0.0f);
    EXPECT_TRUE(std::isfinite(frame.viewProjection[0][0]));
    EXPECT_TRUE(std::isfinite(frame.inverseViewProjection[0][0]));
}

TEST(OrbitCameraControllerTest, ResetsModelRotation)
{
    ku::OrbitCameraController camera;
    const glm::mat4 initial =
        camera.modelMatrix(1.0f, glm::vec3(0.0f));

    camera.addRotation(0.5f, 0.25f);
    const glm::mat4 rotated =
        camera.modelMatrix(1.0f, glm::vec3(0.0f));
    EXPECT_NE(initial[0][0], rotated[0][0]);

    camera.resetRotation();
    const glm::mat4 reset =
        camera.modelMatrix(1.0f, glm::vec3(0.0f));
    EXPECT_FLOAT_EQ(initial[0][0], reset[0][0]);
    EXPECT_FLOAT_EQ(initial[2][2], reset[2][2]);
}
