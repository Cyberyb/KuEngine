#pragma once

#include <string_view>
#include <vector>
#include <memory>
#include <utility>
#include <typeindex>

namespace ku {

class RenderPass;
class RHIDevice;
class SwapChain;
class CommandList;

class RenderPipeline {
public:
    ~RenderPipeline();

    template<typename T, typename... Args>
    T& addPass(Args&&... args) {
        static_assert(std::is_base_of_v<RenderPass, T>, "T must derive from RenderPass");
        auto pass = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = pass.get();
        m_passes.emplace_back(std::move(pass));
        return *ptr;
    }

    void compile(RHIDevice& device);
    void execute(CommandList& cmd, const FrameData& frame);
    void drawUI();
    void onResize(uint32_t width, uint32_t height);

    [[nodiscard]] size_t passCount() const { return m_passes.size(); }

private:
    std::vector<std::unique_ptr<RenderPass>> m_passes;
};

} // namespace ku
