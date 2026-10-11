#pragma once
#include <KuEngine/Core/Engine.h>
#include <KuEngine/Render/ScenePasses.h>
#include <KuEngine/Render/DeferredLightingPass.h>
#include <KuEngine/Render/ForwardDisplayPass.h>
#include <KuEngine/Render/GBufferDisplayPass.h>
namespace ku {
// Called in the existing ApplicationRunner registration callback, after the
// Engine has selected device/depth. No parallel runtime or asset loader.
inline bool prepareScenePathCapability(Engine& engine, SceneDrawSource& source) {
    std::string reason;
    const bool available=deferredPathAvailable(engine.device(),engine.depthFormat(),reason);
    source.pathControl().setDeferredCapability(available,reason); return available;
}
struct SceneViewerBranches { RenderPass* geometry=nullptr; RenderPass* lighting=nullptr; RenderPass* debug=nullptr; };
inline SceneViewerBranches addSceneViewerBranches(Engine& engine, SceneDrawSource& source, std::function<void()> beforeBlend = {}) {
    auto& opaque=engine.addPass<SceneSurfacePass>(source,SceneDrawPhase::Opaque);
    SceneViewerBranches branches;
    const auto provider=[&source](const FrameData& frame){return source.checkedSceneSnapshot(frame).view;};
    if(source.pathControl().deferredAvailable()) {
        branches.geometry=&engine.addPass<SceneSurfacePass>(source,SceneDrawPhase::Opaque,true);
        branches.lighting=&engine.addPass<DeferredLightingPass>(provider,std::string(source.scenePrepareName()));
    }
    if (beforeBlend) beforeBlend();
    engine.addPass<SceneSurfacePass>(source,SceneDrawPhase::Blend);
    auto& display=engine.addPass<ForwardDisplayPass>(provider,std::string(source.scenePrepareName()));
    if(source.pathControl().deferredAvailable()) branches.debug=&engine.addPass<GBufferDisplayPass>(source);
    source.pathControl().bind(opaque,branches.geometry,branches.lighting,display,branches.debug);
    return branches;
}
}
