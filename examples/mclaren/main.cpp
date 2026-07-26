#include <iostream>
#include <utility>

#include <KuEngine/Core/Engine.h>

#include "MclarenPass.h"

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    try {
        ku::EngineConfig config{};
        config.title = "KuEngine Mclaren";
        config.width = 1280;
        config.height = 720;
        config.framesInFlight = 1;
        config.enableDepth = true;
        config.depthFormat = VK_FORMAT_UNDEFINED;
        config.clearColor = {{0.05f, 0.05f, 0.08f, 1.0f}};
        config.clearDepthStencil = {1.0f, 0};

        ku::Engine engine(std::move(config));
        engine.addPass<ku::MclarenPass>();

        engine.compile();
        engine.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}
