#include <KuEngine/Core/ApplicationRunner.h>

#include "GraphResourceProbePass.h"

#include <utility>

int main(int argc, char* argv[])
{
    ku::EngineConfig config{};
    config.title = "KuEngine Graph Resource Probe";
    config.width = 640;
    config.height = 480;
    config.framesInFlight = 1;
    config.enableDepth = false;
    config.clearColor = {{0.04f, 0.05f, 0.07f, 1.0f}};

    return ku::runApplication(
        argc,
        argv,
        std::move(config),
        [](ku::Engine& engine) {
            engine.addPass<ku::GraphResourceImageClearPass>();
            engine.addPass<ku::GraphResourceClearPass>();
            engine.addCallbackPass(ku::makeGraphResourceCopyCallback());
            engine.addCallbackPass(ku::makeGraphResourceComputeCallback());
            engine.addPass<ku::GraphResourceDrawPass>();
            engine.addPass<ku::GraphResourcePostDrawCopyPass>();
        });
}
