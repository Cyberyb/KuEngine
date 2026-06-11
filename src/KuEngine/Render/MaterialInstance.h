#pragma once

#include <string>
#include <utility>

#include <KuEngine/Render/Material.h>
#include <KuEngine/Render/PBRCommon.h>

namespace ku {

class MaterialInstance {
public:
    MaterialInstance() = default;
    explicit MaterialInstance(const Material* material)
        : m_material(material)
    {
    }

    [[nodiscard]] const Material* material() const { return m_material; }
    void setMaterial(const Material* material) { m_material = material; }

    [[nodiscard]] const PBRMaterialBinding& binding() const { return m_binding; }
    PBRMaterialBinding& binding() { return m_binding; }

    [[nodiscard]] const std::string& name() const { return m_name; }
    void setName(std::string name) { m_name = std::move(name); }

private:
    std::string m_name;
    const Material* m_material = nullptr;
    PBRMaterialBinding m_binding{};
};

} // namespace ku
