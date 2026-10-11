#include "ForwardReusePass.h"
#include "DeferredReadbackPass.h"
#include "../common/SceneViewerPasses.h"
#include "../common/ViewerPathSmoke.h"
#include <iostream>

#include <KuEngine/Core/ApplicationRunner.h>
#include <KuEngine/Render/ForwardDisplayPass.h>
#include <KuEngine/Render/DeferredLightingPass.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

int main(int argc, char* argv[])
{
    ku::asset::RenderPathCommandLine pathOptions;
    std::vector<std::string_view> pathArguments;
    for(int i=1;i<argc;++i) pathArguments.emplace_back(argv[i]);
    try { pathOptions=ku::asset::parseRenderPathCommandLine(pathArguments); }
    catch(const std::exception& e) { std::cerr << "KUENGINE_ARGUMENT_ERROR: " << e.what() << '\n'; return 2; }
    std::vector<char*> parsedArgv{argv[0]};
    for(auto& value:pathOptions.forwarded) parsedArgv.push_back(value.data());
    argc=static_cast<int>(parsedArgv.size()); argv=parsedArgv.data();
    bool zeroLights = false;
    bool backFacePreset = false;
    bool deferred = pathOptions.overridePath == ku::asset::RenderPath::Deferred;
    int lightPreset = 0;
    int fixture = 0;
    float normalScale = 1, exposure = 1;
    bool sky = false, environment = false;
    std::string readbackPrefix;
    std::string pathSmokePrefix;
    std::vector<std::string> storage;
    storage.emplace_back(argc > 0 ? argv[0] : "ForwardReuseApp");
    for (int index = 1; index < argc; ++index) {
        if (std::string_view(argv[index]) == "--viewer-path-smoke-prefix" && index+1<argc) {
            pathSmokePrefix = argv[++index];
        } else if (std::string_view(argv[index]) == "--gbuffer-clamp-fixture") {
            fixture = 3;
        } else if (std::string_view(argv[index]) == "--hdr-fixture") {
            fixture = 1;
        } else if (std::string_view(argv[index]) == "--normal-fixture") {
            fixture = 2;
        } else if (std::string_view(argv[index]) == "--normal-scale" && index+1<argc) {
            normalScale = std::stof(argv[++index]);
        } else if (std::string_view(argv[index]) == "--display-exposure" && index+1<argc) {
            exposure = std::stof(argv[++index]);
        } else if (std::string_view(argv[index]) == "--scene-sky") {
            sky = true;
        } else if (std::string_view(argv[index]) == "--scene-environment") {
            environment = true;
        } else if (std::string_view(argv[index]) == "--deferred-readback-prefix" && index + 1 < argc) {
            readbackPrefix = argv[++index];
        } else if (std::string_view(argv[index]) == "--forward-reuse-directional-only") {
            lightPreset = 1;
        } else if (std::string_view(argv[index]) == "--forward-reuse-point-only") {
            lightPreset = 2;
        } else if (std::string_view(argv[index]) == "--forward-reuse-zero-lights") {
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
        [=](ku::Engine& engine) {
            auto& geometry = engine.addPass<ku::ForwardReusePass>(
                zeroLights, backFacePreset, deferred, lightPreset, fixture, normalScale, exposure, sky, environment,pathOptions.overridePath);
            ku::prepareScenePathCapability(engine,geometry);
            engine.addPass<ku::SceneBackgroundPass>(geometry);
            ku::HdrDepthCapturePass* depthCapture=nullptr;
            ku::addSceneViewerBranches(engine,geometry,[&] { if(!readbackPrefix.empty()) depthCapture=&engine.addPass<ku::HdrDepthCapturePass>(); });
            if (!readbackPrefix.empty()) {
                engine.addPass<ku::HdrDisplayProbePass>(geometry,false);
                engine.addPass<ku::HdrDisplayProbePass>(geometry,true);
                auto& readback = engine.addPass<ku::DeferredReadbackPass>(engine.renderPipeline(), readbackPrefix, backFacePreset, deferred, fixture==0);
                depthCapture->setOwner(readback);
            }
            if (!pathSmokePrefix.empty()) ku::addViewerPathSmoke(engine,geometry,pathSmokePrefix,[&geometry](bool variant){geometry.setPathSmokeVariant(variant);},4);
        });
}
