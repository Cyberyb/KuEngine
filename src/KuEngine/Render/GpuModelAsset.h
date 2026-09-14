// KuEngine GPU model asset: owns one uploaded mesh and borrows its uploader.
#pragma once

#include <memory>
#include <string>

namespace ku {

namespace asset {
struct MeshData;
}

class GpuMesh;
class RHIDevice;
class ResourceUploader;

class GpuModelAsset {
public:
    GpuModelAsset();
    ~GpuModelAsset();

    GpuModelAsset(const GpuModelAsset&) = delete;
    GpuModelAsset& operator=(const GpuModelAsset&) = delete;

    [[nodiscard]] bool initialize(
        RHIDevice& device,
        ResourceUploader& uploader,
        const asset::MeshData& meshData,
        std::string& errorMessage);
    void reset();

    [[nodiscard]] bool ready() const { return m_mesh != nullptr; }
    [[nodiscard]] GpuMesh& mesh() const;

private:
    std::unique_ptr<GpuMesh> m_mesh;
};

} // namespace ku
