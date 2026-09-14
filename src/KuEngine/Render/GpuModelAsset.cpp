#include "GpuModelAsset.h"

#include <KuEngine/Render/GpuMesh.h>

#include <exception>
#include <stdexcept>

namespace ku {

GpuModelAsset::GpuModelAsset() = default;
GpuModelAsset::~GpuModelAsset() = default;

bool GpuModelAsset::initialize(
    RHIDevice& device,
    ResourceUploader& uploader,
    const asset::MeshData& meshData,
    std::string& errorMessage)
{
    reset();
    errorMessage.clear();
    try {
        m_mesh = std::make_unique<GpuMesh>(device, uploader, meshData);
        return true;
    } catch (const std::exception& error) {
        errorMessage = error.what();
        reset();
        return false;
    }
}

void GpuModelAsset::reset()
{
    m_mesh.reset();
}

GpuMesh& GpuModelAsset::mesh() const
{
    if (!m_mesh) {
        throw std::runtime_error("GpuModelAsset is not initialized");
    }
    return *m_mesh;
}

} // namespace ku
