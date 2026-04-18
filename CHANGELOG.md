# Changelog

All notable changes to KuEngine will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/).

## [Unreleased]

### Added
- v0.2 execution plan document: scope freeze, stage milestones, acceptance gates, and risk handling (`docs/design/06-v0.2-execution-plan.md`).
- RenderGraph stage-1 foundation (`RenderGraph`, `RenderGraphBuilder`, `ResourceHandle`, pass resource access declarations).
- RenderGraph stage-2 compiler features: dependency graph building, topological ordering, cycle detection, and barrier plan data (RAW/WAR/WAW).
- New RenderGraph unit tests (`tests/core/test_render_graph.cpp`) covering ordering, missing dependency, cycle detection, and resource hazards.
- CMake preset workflow for VS Code CMake Tools (`CMakePresets.json`, `.vscode/settings.json`).
- New stage-4 sample `Alpha3PassApp` with three-pass alpha pipeline and runtime UI tuning.
- Usage guide for the new sample (`docs/usage/alpha3pass-example.md`).
- Stage-5 regression checklist (`docs/usage/v0.2-regression-checks.md`).
- v0.3 asset config parser module (`src/KuEngine/Asset/AssetConfig.h`, `src/KuEngine/Asset/AssetConfig.cpp`) for scene/material JSON loading with default fallback.
- New regression tests for asset config parsing (`tests/core/test_asset_config.cpp`).
- v0.3 sample asset descriptors (`resources/scenes/sandbox/mclaren-sandbox.scene.json`, `resources/materials/pbr/mclaren-765lt.material.json`, `resources/manifests/asset-registry.json`).

### Changed
- Documentation sync for current engine status, roadmap, and module responsibilities.
- Clarified MVP scope as "v0.1.x baseline complete" and aligned progress wording across docs.
- Synced overview document with v0.2 stage-0 freeze conclusions and release gates.
- RenderPipeline compile path now bridges to `setup(RenderGraphBuilder&)` while keeping backward compatibility with legacy `setup()`.
- RenderPipeline compile logging now reports execution order, dependency edges, and barrier-plan summary.
- RenderPipeline execute path now consumes barrier-plan entries and emits image barriers for bound external resources.
- Swapchain layout transitions in Triangle/Cube examples now use `CommandList::imageBarrier` unified path.
- Examples tree now includes `examples/alpha3pass` and builds target `Alpha3PassApp`.
- RenderPipeline UI now exposes a `RenderGraph Debug` panel with compile summary, dependency list, barrier plan, and last execute stats.
- RenderGraph tests now include explicit hazard-type assertions (RAW/WAR/WAW) and external resource flag validation.
- RenderPipeline execute path now supports `setExecuteInsideRendering(bool)` to guard dynamic-rendering scope and avoid invalid image barriers (VUID-08719).
- Stage-6 release wrap-up completed: gate checklist verification, release notes, and final documentation sync.
- VS Code workspace settings now pin C/C++ IntelliSense to CMake Tools + C++20 for better diagnostics consistency.
- Mclaren sample now reads scene/material JSON config first (with fallback), and drives camera FOV/near/far + basic lighting params through runtime UI and shader push constants.

## [0.1.0] - 2026-04-11

### Added
- Initial project setup
- RHI layer: RHIInstance, RHIDevice, SwapChain, RHIBuffer, RHITexture, RHIShader, RHIPipeline
- Core: Engine, Window (GLFW3), Input, Log (spdlog)
- Render: RenderPass interface, RenderPipeline
- UI: UIOverlay (Dear ImGui)
- TrianglePass example
- Design documents (overview, rhi-layer, render-pass, logging)
- CMake build system with vcpkg integration
- Unit tests (GoogleTest)
- Shader compilation script
