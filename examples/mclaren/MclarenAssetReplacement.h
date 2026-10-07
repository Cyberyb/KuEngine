// Mclaren replacement control: CPU-only request, path, status and CLI logic.
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ku {

enum class MclarenAssetKind {
    Model,
    Environment,
};

enum class MclarenReplacementStage {
    Input,
    CpuDecode,
    GpuUpload,
    Publish,
};

enum class MclarenReplacementCategory {
    None,
    InputPath,
    CpuDecodeCompatibility,
    OptionalTextureFallback,
    GpuUploadAllocation,
};

struct MclarenAssetRequest {
    MclarenAssetKind kind = MclarenAssetKind::Model;
    std::string path;
    uint64_t id = 0;
};

struct MclarenPublishedAssetState {
    std::filesystem::path modelPath;
    std::filesystem::path environmentPath;
    uint64_t modelGeneration = 0;
    uint64_t environmentGeneration = 0;
    uintptr_t modelIdentity = 0;
    uintptr_t environmentIdentity = 0;
    uint64_t expectedDraws = 0;
    uint64_t expectedVertices = 0;
    std::array<float, 4> modelGlobalBaseColorFactor{
        1.0f,
        1.0f,
        1.0f,
        1.0f,
    };

    bool operator==(const MclarenPublishedAssetState&) const = default;
};

class MclarenReplacementController {
public:
    void initialize(MclarenPublishedAssetState liveState);
    [[nodiscard]] bool queue(
        MclarenAssetKind kind,
        std::string path,
        std::string* errorMessage = nullptr);
    [[nodiscard]] std::optional<MclarenAssetRequest> takePending();

    void publishSuccess(
        const MclarenAssetRequest& request,
        const std::filesystem::path& activePath,
        uintptr_t identity,
        uint64_t expectedDraws,
        uint64_t expectedVertices,
        bool optionalTextureFallback,
        const std::array<float, 4>& modelGlobalBaseColorFactor);
    void publishFailure(
        const MclarenAssetRequest& request,
        MclarenReplacementStage stage,
        MclarenReplacementCategory category,
        std::string message);
    void swap(MclarenReplacementController& other) noexcept;

    [[nodiscard]] bool pending() const { return m_pending.has_value(); }
    [[nodiscard]] uint64_t nextRequestId() const { return m_nextRequestId; }
    [[nodiscard]] const MclarenPublishedAssetState& live() const { return m_live; }
    [[nodiscard]] std::array<float, 4>& modelGlobalBaseColorFactor()
    {
        return m_live.modelGlobalBaseColorFactor;
    }
    [[nodiscard]] std::string_view status() const { return m_status; }
    [[nodiscard]] MclarenReplacementCategory statusCategory() const
    {
        return m_statusCategory;
    }

private:
    MclarenPublishedAssetState m_live{};
    std::optional<MclarenAssetRequest> m_pending;
    uint64_t m_nextRequestId = 1;
    std::string m_status = "Ready";
    MclarenReplacementCategory m_statusCategory =
        MclarenReplacementCategory::None;
};

struct MclarenStartupReplacement {
    MclarenAssetKind kind = MclarenAssetKind::Model;
    std::string path;
};

struct MclarenCommandLineOptions {
    std::vector<MclarenStartupReplacement> replacements;
    uint64_t replaceAfterUpdates = 1;
    std::vector<std::string> forwardedArguments;
};

[[nodiscard]] MclarenCommandLineOptions parseMclarenCommandLine(
    std::span<const std::string_view> arguments);

[[nodiscard]] bool resolveMclarenAssetPath(
    MclarenAssetKind kind,
    std::string_view input,
    const std::filesystem::path& resourcesRoot,
    std::filesystem::path& outPath,
    std::string& errorMessage);

[[nodiscard]] const char* toString(MclarenAssetKind kind) noexcept;
[[nodiscard]] const char* toString(MclarenReplacementStage stage) noexcept;
[[nodiscard]] const char* toString(MclarenReplacementCategory category) noexcept;
[[nodiscard]] bool isDeviceLostError(std::string_view message) noexcept;

} // namespace ku
