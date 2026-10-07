#include "ForwardReusePass.h"

#include <KuEngine/Core/ApplicationRunner.h>
#include <KuEngine/Render/ForwardDisplayPass.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

int main(int argc, char* argv[])
{
    bool zeroLights = false;
    bool backFacePreset = false;
    std::vector<std::string> storage;
    storage.emplace_back(argc > 0 ? argv[0] : "ForwardReuseApp");
    for (int index = 1; index < argc; ++index) {
        if (std::string_view(argv[index]) == "--forward-reuse-zero-lights") {
            zeroLights = true;
        } else if (std::string_view(argv[index])
            == "--forward-reuse-backface") {
            backFacePreset = true;
        } else {
            storage.emplace_back(argv[index]);
        }
    }
    std::vector<char*> forwarded;
    for (std::string& argument : storage) forwarded.push_back(argument.data());
    ku::EngineConfig config{};
    config.title = "KuEngine Forward Reuse";
    config.width = 960;
    config.height = 540;
    config.framesInFlight = 1;
    config.enableDepth = true;
    config.clearColor = {{0.025f,0.03f,0.045f,1.0f}};
    return ku::runApplication(static_cast<int>(forwarded.size()),
        forwarded.data(), std::move(config),
        [zeroLights, backFacePreset](ku::Engine& engine) {
            engine.addPass<ku::ForwardReusePass>(
                zeroLights, backFacePreset);
            engine.addPass<ku::ForwardDisplayPass>();
        });
}
