# KuEngine 架构设计文档

## 1. 项目概述

**项目名称**：KuEngine  
**项目类型**：实时渲染框架 / 算法实验平台  
**核心目标**：提供一个基于 Vulkan 1.3 的高性能实时渲染框架，支持快速实现和测试新图形学算法  
**目标用户**：图形学研究者、独立开发者、学习者

---

## 2. 技术栈

| 层次 | 技术选型 | 说明 |
|------|---------|------|
| 语言 | C++20 | Concepts, Modules, Ranges |
| 构建系统 | CMake 3.27+ | + vcpkg 管理依赖 |
| 图形 API | Vulkan 1.3 | Dynamic Rendering |
| 窗口/输入 | GLFW3 | 现代跨平台 |
| 数学库 | GLM | 通用矩阵/向量运算 |
| UI | Dear ImGui (Docking) | 调参面板 |
| 资产加载 | tinygltf | glTF 2.0 支持 |
| 文档 | Doxygen + Markdown |  |
| 测试 | GoogleTest | 单元测试 |
| 日志 | spdlog | 结构化日志 |

---

## 3. 架构分层

```
┌─────────────────────────────────────────────────┐
│                   Application                    │  ← examples/ (HelloTriangle, 等)
├─────────────────────────────────────────────────┤
│                     Engine                        │  ← src/KuEngine/Core/
│              (主循环, 场景管理, 生命周期)           │
├─────────────────────────────────────────────────┤
│                    Render                         │  ← src/KuEngine/Render/
│             (RenderGraph, FrameGraph)             │
├─────────────────────────────────────────────────┤
│                     RHI                           │  ← src/KuEngine/RHI/
│        (Vulkan 封装, Device, SwapChain)          │
├─────────────────────────────────────────────────┤
│                   Platform                        │  ← src/KuEngine/Core/
│              (GLFW3 封装, 窗口, 输入)             │
└─────────────────────────────────────────────────┘
```

---

## 4. RHI 层设计原则

RHI (Rendering Hardware Interface) 层是 Vulkan 的薄封装层，目标是：
- **消除样板代码**：不重复写 vkCreate* / vkDestroy* 的繁琐逻辑
- **保留 Vulkan 语义**：不隐藏 Pipeline、DescriptorSet 等概念
- **类型安全**：用 RAII + 智能指针管理资源生命周期
- **Vulkan 1.3 优先**：使用 Dynamic Rendering，减少 RenderPass 对象

### 核心类

| 类名 | 职责 |
|------|------|
| `RHIInstance` | Vulkan Instance 创建、Validation Layers |
| `RHIDevice` | PhysicalDevice 选择、LogicalDevice、VMA 内存分配器 |
| `SwapChain` | 交换链管理（重建、Resize） |
| `RHIBuffer` | GPU 缓冲区（Vertex, Index, Uniform） |
| `RHIImage` | GPU 图像资源 |
| `RHITexture` | 带 View + Sampler 的完整纹理资源 |
| `RHIPipeline` | Graphics/Compute Pipeline |
| `RHIShader` | Shader Module 加载 |
| `CommandList` | Command Buffer 录制 |

---

## 5. RenderGraph 设计

RenderGraph 是现代渲染框架的核心（Frostbite 2017 GDC）：

```
RenderGraph
├── addPass(name, func)           → 注册一个渲染 Pass
├── addRead/Write(resource)       → 声明资源依赖
├── compile()                     → 自动分析依赖、生成最优 barrier
└── execute(cmdBuffer, frame)     → 按拓扑顺序执行所有 Pass
```

每个 Pass 的标准接口：
```cpp
struct RenderPass {
    virtual void setup(RenderGraph& graph) = 0;   // 声明资源
    virtual void execute(CommandList& cmd) = 0;      // 实际绘制
    virtual void drawUI() = 0;                       // ImGui 面板
};
```

---

## 6. MVP 功能范围 (v0.1)

### 包含
- GLFW3 窗口初始化
- Vulkan 1.3 实例创建（带 Validation Layer）
- 物理设备选择（离散 GPU 优先）
- 交换链创建与重建
- Dynamic Rendering：单 Pass 渲染三角形
- ImGui 集成：FPS 显示、Clear Color 调节
- 最小渲染管线（Vertex + Fragment Shader）
- 编译脚本（glslc 或预编译 SPIR-V）

### 不包含（后续迭代）
- 多渲染 Pass / RenderGraph
- 纹理加载 / glTF 场景
- Compute Shader
- 阴影、GI 等高级效果
- 帧率统计、截帧工具

---

## 7. 目录结构

```
KuEngine/
├── CMakeLists.txt
├── vcpkg.json
├── README.md
├── CHANGELOG.md
├── .gitignore
├── docs/
│   ├── design/
│   │   ├── 00-overview.md          ← 本文档
│   │   ├── 01-rhi-layer.md         ← RHI 层详细设计
│   │   ├── 02-render-pass.md       ← RenderPass 接口设计
│   │   ├── 03-logging.md           ← 日志规范
│   │   ├── 04-triangle-example-tech.md  ← Triangle 示例技术说明
│   │   └── 05-ui-layer.md          ← UI 层架构与自定义开发
│   ├── logs/
│   │   └── 2026-04-12-worklog.md   ← 工作日志示例
│   └── bugs/
│       └── template.md             ← Bug 模板
├── src/
│   ├── CMakeLists.txt
│   └── KuEngine/
│       ├── Core/
│       │   ├── Engine.h / .cpp
│       │   ├── Window.h / .cpp
│       │   ├── Input.h / .cpp
│       │   └── Log.h / .cpp
│       ├── RHI/
│       │   ├── RHICommon.h         ← 公共类型、错误检查宏
│       │   ├── RHIInstance.h / .cpp
│       │   ├── RHIDevice.h / .cpp
│       │   ├── SwapChain.h / .cpp
│       │   ├── RHIBuffer.h / .cpp
│       │   ├── RHITexture.h / .cpp
│       │   ├── RHIShader.h / .cpp
│       │   ├── RHIPipeline.h / .cpp
│       │   ├── CommandList.h / .cpp
│       │   ├── SyncManager.h / .cpp
│       │   └── vma.cpp
│       ├── Render/
│       │   ├── RenderPass.h
│       │   └── RenderPipeline.h / .cpp
│       ├── UI/
│       │   └── UIOverlay.h / .cpp
│       └── KuEngine.h
├── examples/
│   └── triangle/
│       ├── main.cpp
│       ├── TrianglePass.h / .cpp   ← 渲染 Pass 示例
│       └── shaders/
│           ├── triangle.vert
│           └── triangle.frag
├── shaders/                        ← 全局着色器源码
├── tests/
│   └── core/
│       └── test_engine.cpp
└── resources/                      ← 模型/纹理等资产
```

---

## 8. 开发流程规范

### 8.1 每次迭代步骤

1. **更新设计文档** → 在 `docs/design/` 下写或更新对应文档
2. **实现代码** → 严格按文档接口实现
3. **编译测试** → 本地编译运行
4. **记录 Bug** → 在 `docs/bugs/` 下创建 `YYYY-MM-DD-issue-N.md`
5. **Git 提交** → 提交信息遵循 Conventional Commits 规范
6. **Push** → 定期推送到远程仓库

### 8.2 Git 提交规范

```
<type>(<scope>): <subject>

Types: feat, fix, docs, refactor, test, chore
Examples:
  feat(rhi): add SwapChain resize support
  fix(core): resolve crash on window minimize
  docs(rhi): update RHI layer design doc
```

### 8.3 代码规范

- 头文件使用 `#pragma once`
- 命名：PascalCase（类/结构体），snake_case（变量/函数），kebab-case（文件名）
- 使用 `[[nodiscard]]`、`override`、`final` 等 C++20 关键字
- RAII 管理所有 GPU 资源
- `VK_CHECK` 宏处理所有 Vulkan 返回值

---

## 9. 版本计划

| 版本 | 里程碑 |
|------|--------|
| v0.1 | MVP：三角形 + ImGui + Vulkan 1.3 初始化 |
| v0.2 | RenderGraph 实现，多 Pass 支持 |
| v0.3 | 纹理加载 + glTF 场景 |
| v0.4 | Compute Shader 支持 |
| v0.5+ | 高级渲染效果（TAA, SSAO, PBR...）|
