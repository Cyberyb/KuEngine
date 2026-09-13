#include <utility>

#include <KuEngine/Core/ApplicationRunner.h>

#include "CubePass.h"

int main(int argc, char* argv[])
{
    ku::EngineConfig config{};
    config.title = "KuEngine Cube";
    config.width = 1280;
    config.height = 720;
    config.framesInFlight = 1;
    config.clearColor = {{0.06f, 0.07f, 0.10f, 1.0f}};

    return ku::runApplication(
        argc,
        argv,
        std::move(config),
        [](ku::Engine& engine) {
        engine.addPass<ku::CubePass>();
        });
}
