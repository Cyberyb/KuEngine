// KuEngine - Triangle App

#include <utility>

#include <KuEngine/Core/ApplicationRunner.h>

#include "TrianglePass.h"

int main(int argc, char* argv[])
{
    ku::EngineConfig config{};
    config.title = "KuEngine Triangle";
    config.width = 1280;
    config.height = 720;
    config.framesInFlight = 1;
    config.clearColor = {{0.08f, 0.09f, 0.12f, 1.0f}};

    return ku::runApplication(
        argc,
        argv,
        std::move(config),
        [](ku::Engine& engine) {
        engine.addPass<ku::TrianglePass>();
        });
}
