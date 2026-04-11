// KuEngine - Triangle App
// v0.1.0 MVP

#include <KuEngine/Core/Engine.h>
#include <KuEngine/Render/RenderPipeline.h>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    try {
        ku::Engine engine;
        engine.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
