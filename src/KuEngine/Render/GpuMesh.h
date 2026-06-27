// KuEngine GPU 网格模块：负责上传并持有顶点、索引缓冲，同时记录可绘制的子网格范围。
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <KuEngine/Asset/Model.h>

namespace ku {

class RHIBuffer;
class RHIDevice;
class ResourceUploader;

class GpuMesh {
public:
    GpuMesh(
        RHIDevice& device,
        ResourceUploader& uploader,
        const asset::MeshData& meshData);
    ~GpuMesh();

    GpuMesh(const GpuMesh&) = delete;
    GpuMesh& operator=(const GpuMesh&) = delete;

    [[nodiscard]] RHIBuffer& vertexBuffer() const { return *m_vertexBuffer; }
    [[nodiscard]] RHIBuffer& indexBuffer() const { return *m_indexBuffer; }
    [[nodiscard]] uint32_t vertexCount() const { return m_vertexCount; }
    [[nodiscard]] uint32_t indexCount() const { return m_indexCount; }
    [[nodiscard]] const std::vector<asset::SubMeshData>& subMeshes() const { return m_subMeshes; }

private:
    std::unique_ptr<RHIBuffer> m_vertexBuffer;
    std::unique_ptr<RHIBuffer> m_indexBuffer;
    std::vector<asset::SubMeshData> m_subMeshes;
    uint32_t m_vertexCount = 0;
    uint32_t m_indexCount = 0;
};

} // namespace ku
