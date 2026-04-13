# KuEngine

A modern real-time rendering framework built on Vulkan 1.3, designed for fast algorithm experimentation and research.

**Status**: v0.1.x MVP baseline complete, entering v0.2 planning

## Features

- **Vulkan 1.3** with Dynamic Rendering (no traditional RenderPass objects)
- **GLFW3** window and input management
- **Dear ImGui** integrated via `UIOverlay` (FPS panel + pass parameter panels)
- **Pass-based RenderPipeline** with `initialize/setup/execute/drawUI/onResize` lifecycle
- **Stable swapchain flow** with per-image layout tracking and resize recreation
- **TriangleApp baseline** with runtime color tuning through push constants
- **RAII** resource management with VMA
- Modern **C++20** codebase

## Current Progress

- Completed in v0.1.x
	- Vulkan instance/device/swapchain/command/sync pipeline is runnable.
	- Triangle sample renders with Dynamic Rendering and ImGui controls.
	- UI responsibilities are separated between `UIOverlay` and `RenderPass::drawUI`.
- In progress
	- Documentation synchronization and milestone refinement.
- Planned next
	- RenderGraph introduction, multi-pass scheduling, and resource dependency compilation.

## Build

### Prerequisites

- Windows 10/11
- [Vulkan SDK 1.3+](https://vulkan.lunarg.com/)
- [CMake 3.27+](https://cmake.org/download/)
- [vcpkg](https://github.com/microsoft/vcpkg)
- Visual Studio 2022 (or compatible compiler)

### Quick Start

```bash
# Clone
git clone https://github.com/Cyberyb/KuEngine.git
cd KuEngine

# Install dependencies
vcpkg install

# Configure
cmake -B build -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Debug

# Compile shaders (requires glslc in PATH)
cd build/bin/Debug/shaders
compile_shaders.bat

# Run
cd build/bin/Debug
TriangleApp.exe
```

## Project Structure

```
KuEngine/
├── src/
│   ├── CMakeLists.txt
│   └── KuEngine/          # Core engine library
│       ├── Core/          # Engine, Window (GLFW3), Input, Log
│       ├── RHI/           # Vulkan abstraction layer
│       ├── Render/        # RenderPass, RenderPipeline
│       ├── UI/            # UI overlay abstraction
│       └── KuEngine.h     # Aggregated public include
├── examples/
│   ├── CMakeLists.txt
│   └── triangle/          # MVP: visible triangle + runtime UI control
├── docs/                  # Design documents
│   ├── design/            # Architecture specs
│   └── bugs/             # Bug reports
└── tests/                 # Unit tests
```

## Documentation

- [Overview](docs/design/00-overview.md)
- [RHI Layer Design](docs/design/01-rhi-layer.md)
- [RenderPass Interface](docs/design/02-render-pass.md)
- [Logging & Debugging](docs/design/03-logging.md)
- [Triangle 示例技术说明](docs/design/04-triangle-example-tech.md)
- [UI 层架构与自定义开发](docs/design/05-ui-layer.md)
- [工作日志（2026-04-12）](docs/logs/2026-04-12-worklog.md)
- [Triangle 示例运行指南](docs/usage/triangle-example.md)

## Version History

| Version | Description |
|---------|-------------|
| v0.1.0 | MVP: Vulkan init + Triangle render + GLFW3 + ImGui |

## Roadmap Summary

| Version | Product Goal |
|---------|--------------|
| v0.2 | RenderGraph alpha, multi-pass orchestration, deterministic resize recovery |
| v0.3 | Algorithm-validation workflow: texture/material inputs + glTF scene baseline |
| v0.4 | Compute-enabled validation loop and GPU timing instrumentation |
| v0.5 | Research-ready toolkit: capture/replay, reproducible benchmark presets |

## License

MIT
