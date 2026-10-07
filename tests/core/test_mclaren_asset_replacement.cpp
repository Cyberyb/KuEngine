#include <gtest/gtest.h>

#include "MclarenAssetReplacement.h"
#include "MclarenSceneAsset.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct TempDir {
    std::filesystem::path path;

    TempDir()
    {
        const auto tick =
            std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path()
            / ("kuengine-mclaren-replacement-" + std::to_string(tick));
        std::filesystem::create_directories(path);
    }

    ~TempDir()
    {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

void touch(const std::filesystem::path& path)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << "fixture";
}

ku::MclarenPublishedAssetState initialState()
{
    return ku::MclarenPublishedAssetState{
        .modelPath = "models/live.glb",
        .environmentPath = "environments/live.hdr",
        .modelGeneration = 7,
        .environmentGeneration = 11,
        .modelIdentity = 0x1234,
        .environmentIdentity = 0x5678,
        .expectedDraws = 19,
        .expectedVertices = 370,
        .modelGlobalBaseColorFactor = {0.2f, 0.4f, 0.6f, 0.8f},
    };
}

} // namespace

TEST(MclarenReplacementPathTest, ResolvesRelativeAndAbsoluteSupportedPaths)
{
    TempDir temp;
    const std::filesystem::path resources = temp.path / "resources";
    const std::filesystem::path model = resources / "models" / "car.GLB";
    const std::filesystem::path environment = temp.path / "sky.HdR";
    touch(model);
    touch(environment);

    std::filesystem::path resolved;
    std::string error;
    EXPECT_TRUE(ku::resolveMclarenAssetPath(
        ku::MclarenAssetKind::Model,
        "models/car.GLB",
        resources,
        resolved,
        error)) << error;
    EXPECT_EQ(resolved, model.lexically_normal());

    EXPECT_TRUE(ku::resolveMclarenAssetPath(
        ku::MclarenAssetKind::Environment,
        environment.string(),
        resources,
        resolved,
        error)) << error;
    EXPECT_EQ(resolved, environment.lexically_normal());
}

TEST(MclarenReplacementPathTest, RejectsEmptyWrongExtensionAndMissingFiles)
{
    TempDir temp;
    const std::filesystem::path resources = temp.path / "resources";
    std::filesystem::create_directories(resources);
    std::filesystem::path resolved;
    std::string error;

    EXPECT_FALSE(ku::resolveMclarenAssetPath(
        ku::MclarenAssetKind::Model,
        {},
        resources,
        resolved,
        error));
    EXPECT_NE(error.find("empty"), std::string::npos);
    EXPECT_FALSE(ku::resolveMclarenAssetPath(
        ku::MclarenAssetKind::Model,
        "models/car.obj",
        resources,
        resolved,
        error));
    EXPECT_NE(error.find(".gltf"), std::string::npos);
    EXPECT_FALSE(ku::resolveMclarenAssetPath(
        ku::MclarenAssetKind::Environment,
        "environments/missing.hdr",
        resources,
        resolved,
        error));
    EXPECT_NE(error.find("does not exist"), std::string::npos);
}

TEST(MclarenReplacementControllerTest, PendingRequestIsImmutableAndIdsIncrease)
{
    ku::MclarenReplacementController controller;
    controller.initialize(initialState());
    std::string error;
    ASSERT_TRUE(controller.queue(
        ku::MclarenAssetKind::Model,
        "draft.glb",
        &error));
    EXPECT_TRUE(controller.pending());
    EXPECT_EQ(controller.nextRequestId(), 2u);
    EXPECT_FALSE(controller.queue(
        ku::MclarenAssetKind::Environment,
        "later.hdr",
        &error));

    const auto first = controller.takePending();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->id, 1u);
    EXPECT_EQ(first->kind, ku::MclarenAssetKind::Model);
    EXPECT_EQ(first->path, "draft.glb");
    EXPECT_FALSE(controller.pending());

    ASSERT_TRUE(controller.queue(
        ku::MclarenAssetKind::Environment,
        "later.hdr",
        &error));
    const auto second = controller.takePending();
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->id, 2u);
}

TEST(MclarenReplacementControllerTest,
    EveryFailureStageRetainsIdentityGenerationAndStatistics)
{
    const std::vector<ku::MclarenReplacementStage> stages{
        ku::MclarenReplacementStage::Input,
        ku::MclarenReplacementStage::CpuDecode,
        ku::MclarenReplacementStage::GpuUpload,
        ku::MclarenReplacementStage::Publish,
    };
    const std::vector<ku::MclarenReplacementCategory> categories{
        ku::MclarenReplacementCategory::InputPath,
        ku::MclarenReplacementCategory::CpuDecodeCompatibility,
        ku::MclarenReplacementCategory::GpuUploadAllocation,
    };

    for (const auto stage : stages) {
        for (const auto category : categories) {
            ku::MclarenReplacementController controller;
            const auto expected = initialState();
            controller.initialize(expected);
            ASSERT_TRUE(controller.queue(
                ku::MclarenAssetKind::Model,
                "candidate.glb"));
            const auto request = controller.takePending();
            ASSERT_TRUE(request.has_value());
            controller.publishFailure(
                *request,
                stage,
                category,
                "injected failure");
            EXPECT_EQ(controller.live(), expected);
            EXPECT_EQ(controller.live().modelIdentity, expected.modelIdentity);
            EXPECT_EQ(controller.live().modelGeneration,
                expected.modelGeneration);
            EXPECT_EQ(controller.live().expectedDraws,
                expected.expectedDraws);
            EXPECT_EQ(controller.live().expectedVertices,
                expected.expectedVertices);
            EXPECT_EQ(controller.statusCategory(), category);
            EXPECT_NE(
                controller.status().find("injected failure"),
                std::string_view::npos);
        }
    }
}

TEST(MclarenReplacementControllerTest, SuccessChangesOnlyRequestedAsset)
{
    ku::MclarenReplacementController controller;
    controller.initialize(initialState());

    ASSERT_TRUE(controller.queue(
        ku::MclarenAssetKind::Model,
        "candidate.glb"));
    auto request = controller.takePending();
    ASSERT_TRUE(request.has_value());
    controller.publishSuccess(
        *request,
        "models/candidate.glb",
        0x9abc,
        23,
        811,
        true,
        {1.0f, 1.0f, 1.0f, 1.0f});
    EXPECT_EQ(controller.live().modelGeneration, 8u);
    EXPECT_EQ(controller.live().environmentGeneration, 11u);
    EXPECT_EQ(controller.live().modelIdentity, 0x9abcu);
    EXPECT_EQ(controller.live().environmentIdentity, 0x5678u);
    EXPECT_EQ(controller.live().expectedDraws, 23u);
    EXPECT_EQ(controller.live().expectedVertices, 811u);
    EXPECT_EQ(
        controller.live().modelGlobalBaseColorFactor,
        (std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f}));
    EXPECT_EQ(
        controller.statusCategory(),
        ku::MclarenReplacementCategory::OptionalTextureFallback);

    ASSERT_TRUE(controller.queue(
        ku::MclarenAssetKind::Environment,
        "candidate.hdr"));
    request = controller.takePending();
    ASSERT_TRUE(request.has_value());
    controller.publishSuccess(
        *request,
        "environments/candidate.hdr",
        0xdef0,
        999,
        999,
        false,
        {0.0f, 0.0f, 0.0f, 0.0f});
    EXPECT_EQ(controller.live().modelGeneration, 8u);
    EXPECT_EQ(controller.live().environmentGeneration, 12u);
    EXPECT_EQ(controller.live().modelIdentity, 0x9abcu);
    EXPECT_EQ(controller.live().environmentIdentity, 0xdef0u);
    EXPECT_EQ(controller.live().expectedDraws, 23u);
    EXPECT_EQ(controller.live().expectedVertices, 811u);
    EXPECT_EQ(
        controller.live().modelGlobalBaseColorFactor,
        (std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f}));
}

TEST(MclarenReplacementControllerTest, FailureRetainsNonUnitModelPolicy)
{
    ku::MclarenReplacementController controller;
    const auto expected = initialState();
    controller.initialize(expected);
    ASSERT_TRUE(controller.queue(
        ku::MclarenAssetKind::Model,
        "candidate.glb"));
    const auto request = controller.takePending();
    ASSERT_TRUE(request.has_value());
    controller.publishFailure(
        *request,
        ku::MclarenReplacementStage::GpuUpload,
        ku::MclarenReplacementCategory::GpuUploadAllocation,
        "injected upload failure");
    EXPECT_EQ(controller.live(), expected);
    EXPECT_EQ(
        controller.live().modelGlobalBaseColorFactor,
        (std::array<float, 4>{0.2f, 0.4f, 0.6f, 0.8f}));
}

TEST(MclarenReplacementCliTest, StripsOptionsAndPreservesRequestOrder)
{
    const std::vector<std::string_view> arguments{
        "--smoke-frames",
        "9",
        "--mclaren-replace-model",
        "models/first.glb",
        "--mclaren-replace-hdr",
        "environments/sky.hdr",
        "--mclaren-replace-model",
        "models/second.gltf",
        "--mclaren-replace-after-updates",
        "3",
        "--smoke-require-validation",
    };
    const auto parsed = ku::parseMclarenCommandLine(arguments);
    ASSERT_EQ(parsed.replacements.size(), 3u);
    EXPECT_EQ(parsed.replacements[0].kind, ku::MclarenAssetKind::Model);
    EXPECT_EQ(parsed.replacements[0].path, "models/first.glb");
    EXPECT_EQ(
        parsed.replacements[1].kind,
        ku::MclarenAssetKind::Environment);
    EXPECT_EQ(parsed.replacements[2].path, "models/second.gltf");
    EXPECT_EQ(parsed.replaceAfterUpdates, 3u);
    EXPECT_EQ(
        parsed.forwardedArguments,
        (std::vector<std::string>{
            "--smoke-frames",
            "9",
            "--smoke-require-validation",
        }));
}

TEST(MclarenReplacementCliTest, RejectsMissingAndInvalidValues)
{
    EXPECT_THROW(
        (void)ku::parseMclarenCommandLine(
            std::vector<std::string_view>{"--mclaren-replace-model"}),
        std::invalid_argument);
    EXPECT_THROW(
        (void)ku::parseMclarenCommandLine(
            std::vector<std::string_view>{
                "--mclaren-replace-after-updates",
                "not-a-number",
            }),
        std::invalid_argument);
}

TEST(MclarenModelFitTest, ProducesFiniteScaleForNormalAndDegenerateBounds)
{
    glm::vec3 center{};
    float scale = 0.0f;
    ASSERT_TRUE(ku::MclarenSceneAsset::calculateModelFit(
        glm::vec3{-2.0f, -1.0f, 0.0f},
        glm::vec3{2.0f, 3.0f, 4.0f},
        center,
        scale));
    EXPECT_EQ(center, (glm::vec3{0.0f, 1.0f, 2.0f}));
    EXPECT_TRUE(std::isfinite(scale));
    EXPECT_GT(scale, 0.0f);

    ASSERT_TRUE(ku::MclarenSceneAsset::calculateModelFit(
        glm::vec3{3.0f},
        glm::vec3{3.0f},
        center,
        scale));
    EXPECT_EQ(center, glm::vec3{3.0f});
    EXPECT_FLOAT_EQ(scale, 1.0f);
}

TEST(MclarenModelFitTest, RejectsNonFiniteOrReversedBoundsWithoutPublishing)
{
    glm::vec3 center{4.0f};
    float scale = 7.0f;
    EXPECT_FALSE(ku::MclarenSceneAsset::calculateModelFit(
        glm::vec3{0.0f},
        glm::vec3{
            std::numeric_limits<float>::infinity(),
            1.0f,
            1.0f},
        center,
        scale));
    EXPECT_EQ(center, glm::vec3{4.0f});
    EXPECT_FLOAT_EQ(scale, 7.0f);
    EXPECT_FALSE(ku::MclarenSceneAsset::calculateModelFit(
        glm::vec3{2.0f},
        glm::vec3{1.0f},
        center,
        scale));
    EXPECT_EQ(center, glm::vec3{4.0f});
    EXPECT_FLOAT_EQ(scale, 7.0f);
}
