#pragma once

#include <string>
#include <utility>

#include <KuEngine/Asset/AssetConfig.h>

namespace ku {

class Material {
public:
    Material() = default;
    explicit Material(std::string name)
        : m_name(std::move(name))
    {
    }

    [[nodiscard]] const std::string& name() const { return m_name; }
    void setName(std::string name) { m_name = std::move(name); }

    [[nodiscard]] const asset::MaterialConfig& config() const { return m_config; }
    void setConfig(const asset::MaterialConfig& config) { m_config = config; }

    [[nodiscard]] bool isTransparent() const
    {
        return m_config.alphaMode == "MASK" || m_config.alphaMode == "BLEND";
    }

private:
    std::string m_name;
    asset::MaterialConfig m_config{};
};

} // namespace ku
