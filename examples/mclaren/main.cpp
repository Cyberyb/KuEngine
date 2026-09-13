#include <utility>

#include <KuEngine/Core/ApplicationRunner.h>

#include "MclarenPass.h"

int main(int argc, char* argv[])
{
    ku::EngineConfig config{};
    config.title = "KuEngine Mclaren";
    config.width = 1280;
    config.height = 720;
    config.framesInFlight = 1;
    config.enableDepth = true;
    config.depthFormat = VK_FORMAT_UNDEFINED;
    config.clearColor = {{0.05f, 0.05f, 0.08f, 1.0f}};
    config.clearDepthStencil = {1.0f, 0};

    return ku::runApplication(
        argc,
        argv,
        std::move(config),
        [](ku::Engine& engine) {
        engine.addPass<ku::MclarenPass>();
        });
}
