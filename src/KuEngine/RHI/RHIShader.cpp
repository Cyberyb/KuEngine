#include "RHIShader.h"
#include "RHIDevice.h"

#include <spdlog/spdlog.h>
#include <fstream>

namespace ku {

RHIShader::RHIShader(const RHIDevice& device, const std::filesystem::path& path)
    : m_device(device.device()) {
    auto code = readFile(path);

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VK_CHECK(vkCreateShaderModule(m_device, &createInfo, nullptr, &m_module));
    KU_DEBUG("Shader loaded: {}", path.string());
}

RHIShader::~RHIShader() {
    if (m_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_device, m_module, nullptr);
    }
}

std::vector<char> RHIShader::readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + path.string());
    }
    size_t size = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(size);
    file.seekg(0);
    file.read(buffer.data(), size);
    file.close();
    return buffer;
}

} // namespace ku
