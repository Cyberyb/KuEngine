# KuEngine

A modern real-time rendering framework built on Vulkan 1.3, designed for fast algorithm experimentation and research.

**Status**: v0.2 RenderGraph Alpha complete; v0.3 asset/material/PBR workflow in progress

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

- Completed
	- Vulkan instance/device/swapchain/command/sync baseline.
	- Dynamic Rendering examples with ImGui parameter controls.
	- RenderGraph Alpha: dependencies, topological ordering, barrier planning and debug UI.
	- scene/material JSON and glTF/GLB loading baseline.
- In progress
	- Moving reusable asset, material and PBR responsibilities out of `MclarenPass`.
	- Consolidating runtime material and renderer abstractions.
- Planned next
	- Unified frame runtime, deeper RenderGraph resource ownership and GPU observability.

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
│       ├── Render/        # Pass, Graph execution, Mesh, Texture, PBR
│       ├── Asset/         # Scene/material config and glTF loading
│       ├── UI/            # UI overlay abstraction
│       └── KuEngine.h     # Aggregated public include
├── examples/
│   ├── CMakeLists.txt
│   ├── triangle/          # Minimal draw and runtime UI control
│   ├── cube/              # Solid/wireframe and input
│   ├── alpha3pass/        # Three-pass graph execution
│   └── mclaren/           # Model, materials, depth and PBR
├── docs/                  # Documentation center
│   ├── design/            # Current architecture and design contracts
│   ├── logs/              # Concise daily iteration logs
│   ├── structure/         # Architecture, roadmap and historical archives
│   ├── bugs/              # Bug reports and fix verification
│   └── usage/             # Examples, debugging and regression guides
└── tests/                 # Unit tests
```

## Documentation

- [Documentation Center](docs/README.md)
- [当前宏观架构](docs/structure/current-architecture.md)
- [开发目标与计划](docs/structure/roadmap.md)
- [模块设计索引](docs/design/README.md)
- [RHI Layer Design](docs/design/01-rhi-layer.md)
- [RenderPass Interface](docs/design/02-render-pass.md)
- [Logging & Debugging](docs/design/03-logging.md)
- [四个示例的设计](docs/design/04-triangle-example-tech.md)
- [UI 与性能统计](docs/design/05-ui-layer.md)
- [每日迭代日志](docs/logs/README.md)
- [Bug 维护规则](docs/bugs/README.md)
- [Triangle 示例运行指南](docs/usage/triangle-example.md)

## Version History

| Version | Description |
|---------|-------------|
| v0.1.0 | MVP: Vulkan init + Triangle render + GLFW3 + ImGui |

## Roadmap Summary

Current capabilities and future priorities are maintained in [Architecture](docs/structure/current-architecture.md) and [Roadmap](docs/structure/roadmap.md).

## License

MIT
