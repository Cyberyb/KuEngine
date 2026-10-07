// KuEngine RHI 着色器模块：加载 SPIR-V 字节码并管理 Vulkan 着色器模块生命周期。
#pragma once

#include <vulkan/vulkan.h>
#include <filesystem>
#include <string>
#include <vector>

namespace ku {

class RHIDevice;

struct ShaderDesc {
    std::filesystem::path path;
    VkShaderStageFlagBits stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
    std::string entryPoint = "main";
};

void validateShaderDesc(const ShaderDesc& desc);

class RHIShader {
public:
    RHIShader() = default;
    explicit RHIShader(const RHIDevice& device, ShaderDesc desc);
    ~RHIShader();

    RHIShader(const RHIShader&) = delete;
    RHIShader& operator=(const RHIShader&) = delete;

    [[nodiscard]] VkShaderModule module() const { return m_module; }
    [[nodiscard]] bool isValid() const { return m_module != VK_NULL_HANDLE; }
    [[nodiscard]] VkShaderStageFlagBits stage() const noexcept { return m_stage; }
    [[nodiscard]] const std::string& entryPoint() const noexcept { return m_entryPoint; }
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return m_path; }

private:
    static std::vector<char> readFile(const std::filesystem::path& path);

    VkShaderModule m_module = VK_NULL_HANDLE;
    VkDevice       m_device = VK_NULL_HANDLE;
    std::filesystem::path m_path;
    VkShaderStageFlagBits m_stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
    std::string m_entryPoint;
};

} // namespace ku
