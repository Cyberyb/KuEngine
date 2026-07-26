// KuEngine - Triangle App

#include <iostream>
#include <utility>

#include <KuEngine/Core/Engine.h>

#include "TrianglePass.h"

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    try {
        ku::EngineConfig config{};
        config.title = "KuEngine Triangle";
        config.width = 1280;
        config.height = 720;
        config.framesInFlight = 1;
        config.clearColor = {{0.08f, 0.09f, 0.12f, 1.0f}};

        ku::Engine engine(std::move(config));
        engine.addPass<ku::TrianglePass>();
        engine.compile();
        engine.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}
