// KuEngine 资源上传模块：通过暂存缓冲与即时提交，将 CPU 数据传输至 GPU 缓冲和纹理。
#pragma once

#include <cstddef>
#include <cstdint>

#include <vulkan/vulkan.h>

namespace ku {

class RHIDevice;
class RHIBuffer;
class RHITexture;

class ResourceUploader {
public:
    explicit ResourceUploader(RHIDevice& device);
    ~ResourceUploader();

    ResourceUploader(const ResourceUploader&) = delete;
    ResourceUploader& operator=(const ResourceUploader&) = delete;

    void uploadBuffer(
        const void* data,
        VkDeviceSize size,
        RHIBuffer& destination);

    void uploadTexture2D(
        const void* data,
        VkDeviceSize size,
        uint32_t width,
        uint32_t height,
        RHITexture& destination,
        VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

private:
    RHIDevice* m_device = nullptr;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
};

} // namespace ku
