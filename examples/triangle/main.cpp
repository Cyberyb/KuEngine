// KuEngine - Main Entry Point
// v0.1.0 MVP

#include "Core/Engine.h"
#include "Core/Log.h"
#include "RenderPipeline.h"
#include "examples/triangle/TrianglePass.h"

using namespace ku;

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    try {
        Engine engine;

        // 注册 TrianglePass
        auto& pipeline = engine.renderPipeline();
        pipeline.addPass<TrianglePass>();

        engine.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
