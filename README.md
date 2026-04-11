# KuEngine

A modern real-time rendering framework built on Vulkan 1.3, designed for fast algorithm experimentation and research.

**Status**: v0.1.0 MVP — under development

## Features

- **Vulkan 1.3** with Dynamic Rendering (no traditional RenderPass objects)
- **SDL3** window and input management
- **Dear ImGui** integration for real-time parameter tuning
- **RenderGraph** architecture for flexible pass management
- **RAII** resource management with VMA
- Modern **C++20** codebase

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

# Install dependencies (vcpkg)
vcpkg install

# Configure
cmake -B build -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Debug

# Compile shaders
cd build/bin/shaders && ..\compile_shaders.bat

# Run
./build/bin/TriangleApp.exe
```

## Project Structure

```
KuEngine/
├── src/KuEngine/          # Core engine library
│   ├── Core/              # Engine, Window, Input, Log
│   ├── RHI/               # Vulkan abstraction layer
│   └── UI/                # ImGui integration
├── examples/              # Example applications
│   └── triangle/          # MVP: fullscreen triangle
├── docs/                  # Design documents
│   ├── design/            # Architecture specs
│   └── bugs/              # Bug reports
├── tests/                 # Unit tests
└── shaders/               # Global shader sources
```

## Documentation

- [Overview](docs/design/00-overview.md)
- [RHI Layer Design](docs/design/01-rhi-layer.md)
- [RenderPass Interface](docs/design/02-render-pass.md)
- [Logging & Debugging](docs/design/03-logging.md)

## Version History

| Version | Description |
|---------|-------------|
| v0.1.0 | MVP: Vulkan init + Triangle render + ImGui |

## License

MIT
