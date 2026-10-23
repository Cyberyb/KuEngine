#include "CornellBoxPass.h"
#include "CornellBoxReadback.h"
#include "../common/SceneViewerPasses.h"
#include <KuEngine/Core/ApplicationRunner.h>

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

int main(int argc,char* argv[])
{
    try {
        std::vector<std::string_view> arguments;
        for (int i=1;i<argc;++i) arguments.emplace_back(argv[i]);
        auto parsed=ku::asset::parseRenderPathCommandLine(arguments);
        const auto path=parsed.overridePath.value_or(ku::asset::RenderPath::Deferred);
        std::string prefix;
        bool verifyFrameIsolation=false;
        std::vector<std::string> storage{argv[0]};
        for (size_t i=0;i<parsed.forwarded.size();++i) {
            if (parsed.forwarded[i]=="--cornell-readback-prefix") {
                if (++i==parsed.forwarded.size()) throw std::invalid_argument("--cornell-readback-prefix requires a path");
                prefix=parsed.forwarded[i];
            } else if (parsed.forwarded[i]=="--cornell-verify-frame-isolation") verifyFrameIsolation=true;
            else storage.push_back(parsed.forwarded[i]);
        }
        if (!prefix.empty() && path!=ku::asset::RenderPath::Deferred)
            throw std::invalid_argument("Cornell MRT readback requires Deferred");
        if (verifyFrameIsolation && prefix.empty())
            throw std::invalid_argument("Frame isolation verification requires Cornell readback");
        std::vector<char*> forwarded;
        for (auto& value : storage) forwarded.push_back(value.data());
        ku::EngineConfig config{};
        config.title="KuEngine Cornell Box - Deferred PBR";
        config.width=1280; config.height=960; config.framesInFlight=2; config.enableDepth=true;
        config.clearColor={{.012f,.012f,.015f,1}};
        return ku::runApplication(static_cast<int>(forwarded.size()),forwarded.data(),std::move(config),
            [path,prefix,verifyFrameIsolation](ku::Engine& engine) {
                auto& scene=engine.addPass<ku::CornellBoxPass>(path);
                scene.verifyFrameIsolation(verifyFrameIsolation);
                ku::prepareScenePathCapability(engine,scene);
                engine.addPass<ku::SceneBackgroundPass>(scene);
                ku::addSceneViewerBranches(engine,scene);
                if (!prefix.empty()) {
                    auto& readback=engine.addPass<ku::CornellBoxReadback>(engine.renderPipeline(),scene,prefix);
                    readback.verifyFrameIsolation(verifyFrameIsolation);
                }
            });
    } catch (const std::exception& error) {
        std::cerr<<"KUENGINE_ARGUMENT_ERROR: "<<error.what()<<'\n';
        return 2;
    }
}
