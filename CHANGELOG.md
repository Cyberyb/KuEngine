# Changelog

All notable changes to KuEngine will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/).

## [Unreleased]

### Changed
- Documentation sync for current engine status, roadmap, and module responsibilities.
- Clarified MVP scope as "v0.1.x baseline complete" and aligned progress wording across docs.

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
