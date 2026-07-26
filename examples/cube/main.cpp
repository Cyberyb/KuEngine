#include <iostream>
#include <utility>

#include <KuEngine/Core/Engine.h>

#include "CubePass.h"

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    try {
        ku::EngineConfig config{};
        config.title = "KuEngine Cube";
        config.width = 1280;
        config.height = 720;
        config.framesInFlight = 1;
        config.clearColor = {{0.06f, 0.07f, 0.10f, 1.0f}};

        ku::Engine engine(std::move(config));
        engine.addPass<ku::CubePass>();
        engine.compile();
        engine.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}
