#include "MclarenAssetReplacement.h"

#include <KuEngine/Asset/AssetPath.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace ku {
namespace {

uint64_t parseUnsigned(std::string_view value, std::string_view option)
{
    uint64_t parsed = 0;
    const auto result = std::from_chars(
        value.data(),
        value.data() + value.size(),
        parsed);
    if (value.empty() || result.ec != std::errc{}
        || result.ptr != value.data() + value.size()) {
        throw std::invalid_argument(
            std::string(option) + " requires a non-negative integer");
    }
    return parsed;
}

std::string lowerExtension(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(
        extension.begin(),
        extension.end(),
        extension.begin(),
        [](unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
    return extension;
}

void setError(std::string* destination, std::string message)
{
    if (destination != nullptr) {
        *destination = std::move(message);
    }
}

} // namespace

void MclarenReplacementController::initialize(
    MclarenPublishedAssetState liveState)
{
    m_live = std::move(liveState);
    m_pending.reset();
    m_nextRequestId = 1;
    m_status = "Ready";
    m_statusCategory = MclarenReplacementCategory::None;
}

bool MclarenReplacementController::queue(
    MclarenAssetKind kind,
    std::string path,
    std::string* errorMessage)
{
    if (m_pending.has_value()) {
        setError(errorMessage, "An asset replacement request is already pending");
        return false;
    }
    if (m_nextRequestId == std::numeric_limits<uint64_t>::max()) {
        setError(errorMessage, "Asset replacement request id capacity exhausted");
        return false;
    }

    m_pending = MclarenAssetRequest{kind, std::move(path), m_nextRequestId++};
    m_status = "Pending request " + std::to_string(m_pending->id);
    m_statusCategory = MclarenReplacementCategory::None;
    setError(errorMessage, {});
    return true;
}

std::optional<MclarenAssetRequest>
MclarenReplacementController::takePending()
{
    std::optional<MclarenAssetRequest> request = std::move(m_pending);
    m_pending.reset();
    if (request.has_value()) {
        m_status = "Processing request " + std::to_string(request->id);
    }
    return request;
}

void MclarenReplacementController::publishSuccess(
    const MclarenAssetRequest& request,
    const std::filesystem::path& activePath,
    uintptr_t identity,
    uint64_t expectedDraws,
    uint64_t expectedVertices,
    bool optionalTextureFallback,
    const std::array<float, 4>& modelGlobalBaseColorFactor)
{
    if (request.kind == MclarenAssetKind::Model) {
        m_live.modelPath = activePath;
        ++m_live.modelGeneration;
        m_live.modelIdentity = identity;
        m_live.expectedDraws = expectedDraws;
        m_live.expectedVertices = expectedVertices;
        m_live.modelGlobalBaseColorFactor = modelGlobalBaseColorFactor;
    } else {
        m_live.environmentPath = activePath;
        ++m_live.environmentGeneration;
        m_live.environmentIdentity = identity;
    }
    m_statusCategory = optionalTextureFallback
        ? MclarenReplacementCategory::OptionalTextureFallback
        : MclarenReplacementCategory::None;
    m_status = optionalTextureFallback
        ? "Loaded with optional texture fallback"
        : "Loaded successfully";
}

void MclarenReplacementController::publishFailure(
    const MclarenAssetRequest& request,
    MclarenReplacementStage stage,
    MclarenReplacementCategory category,
    std::string message)
{
    m_statusCategory = category;
    m_status = "Request " + std::to_string(request.id) + " failed at "
        + toString(stage) + ": " + std::move(message);
}

void MclarenReplacementController::swap(
    MclarenReplacementController& other) noexcept
{
    using std::swap;
    swap(m_live, other.m_live);
    swap(m_pending, other.m_pending);
    swap(m_nextRequestId, other.m_nextRequestId);
    swap(m_status, other.m_status);
    swap(m_statusCategory, other.m_statusCategory);
}

MclarenCommandLineOptions parseMclarenCommandLine(
    std::span<const std::string_view> arguments)
{
    MclarenCommandLineOptions options{};
    for (size_t index = 0; index < arguments.size(); ++index) {
        const std::string_view argument = arguments[index];
        if (argument == "--mclaren-replace-model"
            || argument == "--mclaren-replace-hdr") {
            if (++index >= arguments.size()) {
                throw std::invalid_argument(
                    std::string(argument) + " requires a path");
            }
            options.replacements.push_back(MclarenStartupReplacement{
                argument == "--mclaren-replace-model"
                    ? MclarenAssetKind::Model
                    : MclarenAssetKind::Environment,
                std::string(arguments[index]),
            });
        } else if (argument == "--mclaren-replace-after-updates") {
            if (++index >= arguments.size()) {
                throw std::invalid_argument(
                    "--mclaren-replace-after-updates requires a value");
            }
            options.replaceAfterUpdates = parseUnsigned(
                arguments[index],
                "--mclaren-replace-after-updates");
        } else {
            options.forwardedArguments.emplace_back(argument);
        }
    }
    return options;
}

bool resolveMclarenAssetPath(
    MclarenAssetKind kind,
    std::string_view input,
    const std::filesystem::path& resourcesRoot,
    std::filesystem::path& outPath,
    std::string& errorMessage)
{
    errorMessage.clear();
    if (input.empty()) {
        errorMessage = "Path is empty";
        return false;
    }
    if (resourcesRoot.empty()) {
        errorMessage = "Resolved resources root is empty";
        return false;
    }

    const std::filesystem::path inputPath{std::string(input)};
    const std::string extension = lowerExtension(inputPath);
    const bool supported = kind == MclarenAssetKind::Model
        ? extension == ".gltf" || extension == ".glb"
        : extension == ".hdr";
    if (!supported) {
        errorMessage = kind == MclarenAssetKind::Model
            ? "Model path must use .gltf or .glb"
            : "Environment path must use .hdr";
        return false;
    }

    const std::filesystem::path candidate = asset::resolveAssetPath(
        resourcesRoot,
        inputPath);
    std::error_code error;
    if (!std::filesystem::exists(candidate, error) || error) {
        errorMessage = "Asset path does not exist: " + candidate.string();
        return false;
    }
    if (!std::filesystem::is_regular_file(candidate, error) || error) {
        errorMessage = "Asset path is not a regular file: " + candidate.string();
        return false;
    }

    outPath = candidate;
    return true;
}

const char* toString(MclarenAssetKind kind) noexcept
{
    return kind == MclarenAssetKind::Model ? "model" : "hdr";
}

const char* toString(MclarenReplacementStage stage) noexcept
{
    switch (stage) {
        case MclarenReplacementStage::Input: return "Input";
        case MclarenReplacementStage::CpuDecode: return "CpuDecode";
        case MclarenReplacementStage::GpuUpload: return "GpuUpload";
        case MclarenReplacementStage::Publish: return "Publish";
        default: return "Unknown";
    }
}

const char* toString(MclarenReplacementCategory category) noexcept
{
    switch (category) {
        case MclarenReplacementCategory::None: return "None";
        case MclarenReplacementCategory::InputPath: return "Input/Path";
        case MclarenReplacementCategory::CpuDecodeCompatibility:
            return "CpuDecode/Compatibility";
        case MclarenReplacementCategory::OptionalTextureFallback:
            return "OptionalTextureFallback";
        case MclarenReplacementCategory::GpuUploadAllocation:
            return "GpuUpload/Allocation";
        default: return "Unknown";
    }
}

bool isDeviceLostError(std::string_view message) noexcept
{
    return message.find("VK_ERROR_DEVICE_LOST") != std::string_view::npos;
}

} // namespace ku
