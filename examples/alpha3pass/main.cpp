#include <array>
#include <iostream>
#include <utility>

#include <KuEngine/Core/Engine.h>

#include "AlphaPasses.h"

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    try {
        ku::EngineConfig config{};
        config.title = "KuEngine Alpha3Pass";
        config.width = 1280;
        config.height = 720;
        config.framesInFlight = 1;
        config.clearColor = {{0.05f, 0.06f, 0.08f, 1.0f}};

        ku::Engine engine(std::move(config));
        engine.addPass<ku::AlphaShapePass>(
            "Background",
            std::array<float, 4>{0.14f, 0.18f, 0.30f, 1.0f},
            std::array<float, 2>{0.0f, -0.15f},
            std::array<float, 2>{1.9f, 1.4f});
        engine.addPass<ku::AlphaShapePass>(
            "MainTriangle",
            std::array<float, 4>{0.95f, 0.45f, 0.18f, 0.95f},
            std::array<float, 2>{0.0f, 0.03f},
            std::array<float, 2>{0.78f, 0.78f},
            "Background");
        engine.addPass<ku::AlphaShapePass>(
            "Accent",
            std::array<float, 4>{0.15f, 0.85f, 0.70f, 0.92f},
            std::array<float, 2>{0.45f, -0.34f},
            std::array<float, 2>{0.35f, 0.35f},
            "MainTriangle");

        engine.compile();
        engine.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}
