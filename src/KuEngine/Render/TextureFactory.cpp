#include "TextureFactory.h"

#include <KuEngine/RHI/RHIDevice.h>
#include <KuEngine/RHI/RHITexture.h>
#include <KuEngine/RHI/ResourceUploader.h>

#include <stdexcept>

namespace ku {

TextureFactory::TextureFactory(RHIDevice& device, ResourceUploader& uploader)
    : m_device(&device)
    , m_uploader(&uploader)
{
}

std::unique_ptr<RHITexture> TextureFactory::createTexture2D(
    const void* data,
    VkDeviceSize size,
    uint32_t width,
    uint32_t height,
    VkFormat format) const
{
    if (data == nullptr || size == 0 || width == 0 || height == 0) {
        throw std::invalid_argument("TextureFactory requires non-empty pixel data");
    }

    RHITexture::CreateInfo info{};
    info.width = width;
    info.height = height;
    info.format = format;
    info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    info.aspect = VK_IMAGE_ASPECT_COLOR_BIT;

    auto texture = std::make_unique<RHITexture>(*m_device, info);
    m_uploader->uploadTexture2D(data, size, width, height, *texture);
    return texture;
}

std::unique_ptr<RHITexture> TextureFactory::createFromRgba8(
    const asset::TextureData& textureData,
    VkFormat format) const
{
    if (!textureData.valid()) {
        throw std::invalid_argument("TextureFactory received invalid RGBA8 texture data");
    }

    return createTexture2D(
        textureData.rgba8.data(),
        static_cast<VkDeviceSize>(textureData.rgba8.size()),
        textureData.width,
        textureData.height,
        format);
}

std::unique_ptr<RHITexture> TextureFactory::createSolidColor(
    std::array<uint8_t, 4> rgba,
    VkFormat format) const
{
    return createTexture2D(
        rgba.data(),
        static_cast<VkDeviceSize>(rgba.size()),
        1,
        1,
        format);
}

} // namespace ku
