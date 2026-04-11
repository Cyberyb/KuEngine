#pragma once

#include <vulkan/vulkan.h>
#include <filesystem>
#include <string>
#include <vector>

namespace ku {

class RHIDevice;

class RHIShader {
public:
    RHIShader() = default;
    explicit RHIShader(const RHIDevice& device, const std::filesystem::path& path);
    ~RHIShader();

    [[nodiscard]] VkShaderModule module() const { return m_module; }
    [[nodiscard]] bool isValid() const { return m_module != VK_NULL_HANDLE; }

private:
    static std::vector<char> readFile(const std::filesystem::path& path);

    VkShaderModule m_module = VK_NULL_HANDLE;
    VkDevice       m_device = VK_NULL_HANDLE;
};

} // namespace ku
