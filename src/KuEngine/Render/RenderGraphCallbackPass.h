#pragma once

#include "GraphCommandContext.h"
#include "RenderPass.h"

#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace ku {

template<typename Parameters>
struct CallbackPassDesc {
    std::string name;
    Parameters parameters{};
    NativeCommandContract nativeContract{};
    std::function<void(RenderGraphBuilder&, Parameters&)> declare;
    std::function<void(const RenderContext&, Parameters&)> prepare;
    std::function<void(
        GraphCommandContext&,
        const FrameData&,
        const Parameters&)> execute;
    std::optional<CommandListStatistics> expectedStatistics;
};

template<typename Parameters>
class CallbackRenderPass final : public RenderPass {
public:
    explicit CallbackRenderPass(CallbackPassDesc<Parameters> desc)
        : m_name(std::move(desc.name))
        , m_parameters(std::move(desc.parameters))
        , m_nativeContract(desc.nativeContract)
        , m_declare(std::move(desc.declare))
        , m_prepare(std::move(desc.prepare))
        , m_execute(std::move(desc.execute))
        , m_expectedStatistics(desc.expectedStatistics)
    {
        if (m_name.empty() || !m_declare || !m_execute) {
            throw std::invalid_argument(
                "Callback pass requires a name, declaration, and execute callback");
        }
        validateNativeCommandContractDeclaration(m_nativeContract);
    }

    [[nodiscard]] std::string_view name() const override { return m_name; }
    [[nodiscard]] PassExecutionModel executionModel() const noexcept override
    {
        return PassExecutionModel::RestrictedCallback;
    }
    [[nodiscard]] bool requiresOutsideRenderingScope() const noexcept override
    {
        const auto capabilities =
            static_cast<uint32_t>(m_nativeContract.capabilities);
        const auto outsideOnly = static_cast<uint32_t>(
            NativeCommandCapability::Transfer | NativeCommandCapability::Compute);
        return (capabilities & outsideOnly) != 0;
    }
    [[nodiscard]] const Parameters& parameters() const noexcept
    {
        return m_parameters;
    }

    void initialize(const RenderContext& context) override
    {
        m_device = &context.device;
    }

    void setup(RenderGraphBuilder& builder) override
    {
        m_declare(builder, m_parameters);
    }

    void prepare(const RenderContext& context) override
    {
        if (m_prepare) m_prepare(context, m_parameters);
    }

    void execute(
        CommandList& commands,
        const FrameData& frame,
        RenderGraphResourceResolver& resources) override
    {
        if (m_device == nullptr) {
            throw std::runtime_error("Callback pass was not initialized");
        }
        GraphCommandContext graphCommands(
            commands, resources, *m_device, m_nativeContract);
        m_execute(graphCommands, frame, std::as_const(m_parameters));
    }

    [[nodiscard]] std::optional<CommandListStatistics>
    expectedFrameStatistics() const override
    {
        return m_expectedStatistics;
    }

private:
    std::string m_name;
    Parameters m_parameters;
    NativeCommandContract m_nativeContract{};
    std::function<void(RenderGraphBuilder&, Parameters&)> m_declare;
    std::function<void(const RenderContext&, Parameters&)> m_prepare;
    std::function<void(
        GraphCommandContext&,
        const FrameData&,
        const Parameters&)> m_execute;
    std::optional<CommandListStatistics> m_expectedStatistics;
    RHIDevice* m_device = nullptr;
};

} // namespace ku
