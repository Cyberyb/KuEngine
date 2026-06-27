#include "GpuMesh.h"

#include <KuEngine/RHI/RHIBuffer.h>
#include <KuEngine/RHI/RHIDevice.h>
#include <KuEngine/RHI/ResourceUploader.h>

#include <stdexcept>

namespace ku {

GpuMesh::GpuMesh(
    RHIDevice& device,
    ResourceUploader& uploader,
    const asset::MeshData& meshData)
    : m_subMeshes(meshData.subMeshes)
    , m_vertexCount(static_cast<uint32_t>(meshData.vertices.size()))
    , m_indexCount(static_cast<uint32_t>(meshData.indices.size()))
{
    if (meshData.vertices.empty() || meshData.indices.empty()) {
        throw std::invalid_argument("GpuMesh requires non-empty vertex and index data");
    }

    if (m_subMeshes.empty()) {
        m_subMeshes.push_back(asset::SubMeshData{0, m_indexCount, 0});
    }

    const VkDeviceSize vertexBytes =
        static_cast<VkDeviceSize>(meshData.vertices.size() * sizeof(asset::MeshVertex));
    const VkDeviceSize indexBytes =
        static_cast<VkDeviceSize>(meshData.indices.size() * sizeof(uint32_t));

    RHIBuffer::CreateInfo vertexInfo{};
    vertexInfo.size = vertexBytes;
    vertexInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vertexInfo.memoryUsage = VMA_MEMORY_USAGE_AUTO;

    RHIBuffer::CreateInfo indexInfo{};
    indexInfo.size = indexBytes;
    indexInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    indexInfo.memoryUsage = VMA_MEMORY_USAGE_AUTO;

    m_vertexBuffer = std::make_unique<RHIBuffer>(device, vertexInfo);
    m_indexBuffer = std::make_unique<RHIBuffer>(device, indexInfo);

    uploader.uploadBuffer(meshData.vertices.data(), vertexBytes, *m_vertexBuffer);
    uploader.uploadBuffer(meshData.indices.data(), indexBytes, *m_indexBuffer);
}

GpuMesh::~GpuMesh() = default;

} // namespace ku
