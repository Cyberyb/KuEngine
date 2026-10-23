#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <KuEngine/Core/ApplicationRunner.h>
#include <KuEngine/Render/ForwardDisplayPass.h>
#include <KuEngine/Render/DeferredLightingPass.h>
#include <algorithm>

#include "MclarenAssetReplacement.h"
#include "MclarenPass.h"
#include "MclarenEditSmoke.h"
#include "../common/SceneViewerPasses.h"
#include "../common/ViewerPathSmoke.h"

int main(int argc, char* argv[])
{
    ku::asset::RenderPathCommandLine pathOptions;
    ku::MclarenCommandLineOptions mclarenOptions;
    std::string pathSmokePrefix;
    std::string editSmokePrefix;
    try {
        std::vector<std::string_view> arguments;
        arguments.reserve(argc > 1 ? static_cast<size_t>(argc - 1) : 0);
        for (int index = 1; index < argc; ++index) {
            arguments.emplace_back(argv[index]);
        }
        pathOptions = ku::asset::parseRenderPathCommandLine(arguments);
        arguments.clear();
        for(size_t i=0;i<pathOptions.forwarded.size();++i) {
            if(pathOptions.forwarded[i]=="--mclaren-edit-smoke-prefix") {
                if(++i==pathOptions.forwarded.size())throw std::invalid_argument("--mclaren-edit-smoke-prefix requires a prefix");
                editSmokePrefix=pathOptions.forwarded[i];
            } else if(pathOptions.forwarded[i]=="--viewer-path-smoke-prefix") {
                if(++i==pathOptions.forwarded.size())throw std::invalid_argument("--viewer-path-smoke-prefix requires a prefix");
                pathSmokePrefix=pathOptions.forwarded[i];
            } else arguments.emplace_back(pathOptions.forwarded[i]);
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
    config.framesInFlight = 2;
    config.enableDepth = true;
    config.depthFormat = VK_FORMAT_UNDEFINED;
    config.clearColor = {{0.05f, 0.05f, 0.08f, 1.0f}};
    config.clearDepthStencil = {1.0f, 0};

    return ku::runApplication(
        static_cast<int>(forwardedArgv.size()),
        forwardedArgv.data(),
        std::move(config),
        [mclarenOptions, pathOptions, pathSmokePrefix, editSmokePrefix](ku::Engine& engine) {
        auto& source = engine.addPass<ku::MclarenPass>(
            mclarenOptions.replacements,
            mclarenOptions.replaceAfterUpdates, false,pathOptions.overridePath,mclarenOptions.scenePath);
        ku::prepareScenePathCapability(engine,source);
        if (!pathSmokePrefix.empty()) source.seedPathSmokeState();
        engine.addPass<ku::SceneBackgroundPass>(source);
        ku::addSceneViewerBranches(engine,source);
        if(!editSmokePrefix.empty()) ku::addMclarenEditSmoke(engine,source,editSmokePrefix);
        if (!pathSmokePrefix.empty()) ku::addViewerPathSmoke(engine,source,pathSmokePrefix,[&source](bool variant){source.setPathSmokeVariant(variant);},4,[&source]{return source.pathSmokeState();});
        });
}
