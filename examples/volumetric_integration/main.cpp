#include "VolumetricPass.h"
#include "VolumetricReadback.h"
#include <KuEngine/Core/ApplicationRunner.h>
#include <iostream>

int main(int argc,char* argv[])
{
    try {
        std::vector<std::string_view> args;
        for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
        auto options = ku::volumetric::parseOptions(args);
        if (!options.readbackPrefix.empty()) {
            std::vector<std::string_view> runtimeArgs;
            for (const auto& arg : options.forwarded) runtimeArgs.emplace_back(arg);
            if (ku::parseApplicationRunOptions(runtimeArgs).engine.submittedFrameLimit == 0)
                throw std::invalid_argument("Volumetric readback requires a positive --smoke-frames limit");
        }
        std::vector<std::string> storage{argv[0]};
        storage.insert(storage.end(),options.forwarded.begin(),options.forwarded.end());
        std::vector<char*> forwarded;
        for (auto& arg : storage) forwarded.push_back(arg.data());
        auto report = std::make_shared<ku::volumetric::CaptureReport>();
        ku::EngineConfig config;
        config.title = "KuEngine - VolumetricIntegration (SebH / Shadertoy XlBSRz)";
        config.width = 1280; config.height = 720; config.framesInFlight = 2;
        config.clearColor = {{0,0,0,1}};
        const int result = ku::runApplication(int(forwarded.size()),forwarded.data(),std::move(config),
            [&options,report](ku::Engine& engine) {
                auto& scene = engine.addPass<ku::volumetric::ScenePass>(options.settings);
                engine.addPass<ku::volumetric::DisplayPass>();
                if (!options.readbackPrefix.empty())
                    engine.addPass<ku::volumetric::ReadbackPass>(engine.renderPipeline(),scene,options.readbackPrefix,report);
            });
        if (result == 0 && !options.readbackPrefix.empty() && (!report->error.empty() || report->completed == 0)) {
            std::cerr << "KUENGINE_VOLUMETRIC_READBACK_FAIL " << report->error << '\n';
            return 1;
        }
        return result;
    } catch (const std::exception& error) {
        std::cerr << "KUENGINE_ARGUMENT_ERROR: " << error.what() << '\n';
        return 2;
    }
}
