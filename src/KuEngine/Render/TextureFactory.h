// KuEngine 纹理工厂模块：根据像素数据创建 GPU 纹理，并通过上传器完成初始化传输。
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

#include <vulkan/vulkan.h>

#include <KuEngine/Asset/Model.h>

namespace ku {

class RHIDevice;
class RHITexture;
class ResourceUploader;

class TextureFactory {
public:
    TextureFactory(RHIDevice& device, ResourceUploader& uploader);

    [[nodiscard]] std::unique_ptr<RHITexture> createTexture2D(
        const void* data,
        VkDeviceSize size,
        uint32_t width,
        uint32_t height,
        VkFormat format) const;

    [[nodiscard]] std::unique_ptr<RHITexture> createFromRgba8(
        const asset::TextureData& textureData,
        VkFormat format) const;

    [[nodiscard]] std::unique_ptr<RHITexture> createSolidColor(
        std::array<uint8_t, 4> rgba,
        VkFormat format) const;

private:
    RHIDevice* m_device = nullptr;
    ResourceUploader* m_uploader = nullptr;
};

} // namespace ku
