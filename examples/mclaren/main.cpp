#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <KuEngine/Core/ApplicationRunner.h>
#include <KuEngine/Render/ForwardDisplayPass.h>

#include "MclarenAssetReplacement.h"
#include "MclarenPass.h"

int main(int argc, char* argv[])
{
    ku::MclarenCommandLineOptions mclarenOptions;
    try {
        std::vector<std::string_view> arguments;
        arguments.reserve(argc > 1 ? static_cast<size_t>(argc - 1) : 0);
        for (int index = 1; index < argc; ++index) {
            arguments.emplace_back(argv[index]);
        }
        mclarenOptions = ku::parseMclarenCommandLine(arguments);
    } catch (const std::invalid_argument& error) {
        std::cerr << "KUENGINE_ARGUMENT_ERROR: " << error.what() << '\n';
        return static_cast<int>(ku::ApplicationExitCode::invalidArguments);
    }

    std::vector<std::string> forwardedStorage;
    forwardedStorage.reserve(mclarenOptions.forwardedArguments.size() + 1);
    forwardedStorage.emplace_back(argc > 0 ? argv[0] : "MclarenApp");
    for (const std::string& argument : mclarenOptions.forwardedArguments) {
        forwardedStorage.push_back(argument);
    }
    std::vector<char*> forwardedArgv;
    forwardedArgv.reserve(forwardedStorage.size());
    for (std::string& argument : forwardedStorage) {
        forwardedArgv.push_back(argument.data());
    }

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
        static_cast<int>(forwardedArgv.size()),
        forwardedArgv.data(),
        std::move(config),
        [mclarenOptions](ku::Engine& engine) {
        engine.addPass<ku::MclarenPass>(
            mclarenOptions.replacements,
            mclarenOptions.replaceAfterUpdates);
        engine.addPass<ku::ForwardDisplayPass>();
        });
}
