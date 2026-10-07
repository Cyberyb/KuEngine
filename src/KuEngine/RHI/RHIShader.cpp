#include "RHIShader.h"
#include "RHIDevice.h"
#include "../Core/Log.h"

#include <fstream>
#include <stdexcept>

namespace ku {

void validateShaderDesc(const ShaderDesc& desc)
{
    if (desc.path.empty()) {
        throw std::invalid_argument("ShaderDesc path cannot be empty");
    }
    if (desc.entryPoint.empty()) {
        throw std::invalid_argument("ShaderDesc entry point cannot be empty");
    }
    if (desc.stage != VK_SHADER_STAGE_VERTEX_BIT
        && desc.stage != VK_SHADER_STAGE_FRAGMENT_BIT
        && desc.stage != VK_SHADER_STAGE_COMPUTE_BIT) {
        throw std::invalid_argument(
            "ShaderDesc stage must be vertex, fragment, or compute");
    }
}

RHIShader::RHIShader(const RHIDevice& device, ShaderDesc desc)
    : m_device(device.device())
    , m_path(std::move(desc.path))
    , m_stage(desc.stage)
    , m_entryPoint(std::move(desc.entryPoint))
{
    validateShaderDesc({m_path, m_stage, m_entryPoint});
    auto code = readFile(m_path);

    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code.size();
    info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VK_CHECK(vkCreateShaderModule(m_device, &info, nullptr, &m_module));
    KU_INFO("Shader loaded: {} (stage={}, entry={})",
        m_path.string(), static_cast<uint32_t>(m_stage), m_entryPoint);
}

RHIShader::~RHIShader()
{
    if (m_module) vkDestroyShaderModule(m_device, m_module, nullptr);
}

std::vector<char> RHIShader::readFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Failed to open shader: " + path.string());
    size_t size = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(size);
    file.seekg(0);
    file.read(buffer.data(), size);
    file.close();
    return buffer;
}

} // namespace ku
