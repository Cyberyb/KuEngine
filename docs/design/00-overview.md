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

## 9. 版本路线图（产品化优化版）

面向“基于 Vulkan 的快速算法验证框架”目标，版本规划按“验证效率优先”排序：

| 版本 | 产品目标 | 核心交付 | 验收标准 |
|------|----------|----------|----------|
| v0.1.x | 建立可运行基线 | Triangle + Dynamic Rendering + UI 调参 + 稳定 resize | 新机器可在 30 分钟内跑通示例；连续 resize 不崩溃 |
| v0.2 | 建立调度能力 | RenderGraph Alpha、多 Pass 拓扑执行、资源依赖编译 | 至少 3 个 Pass 正确执行；自动 barrier 基本可用 |
| v0.3 | 建立算法输入能力 | 纹理/材质输入、glTF 最小场景、相机与基础光照参数 | 新算法可在现有场景中 1 天内接入并对比效果 |
| v0.4 | 建立实验闭环能力 | Compute 管线、GPU 计时、帧时间曲线、实验参数预设 | 可量化比较两种算法性能和画质 |
| v0.5 | 建立研究工具化能力 | Capture/Replay、实验配置快照、批量回归脚本 | 同一实验可复现；结果可追踪、可回归 |

---

## 10. 版本进度评估（2026-04）

### 10.1 当前阶段判断

- 结论：当前处于 **v0.1.x 已完成、可进入 v0.2 设计实现** 阶段。
- 依据：渲染主链路、UI 调参、swapchain 稳定性已具备；但 RenderGraph 与实验评估工具链尚未落地。

### 10.2 产品视角评分（5 分制）

| 维度 | 评分 | 说明 |
|------|------|------|
| 可运行性 | 4.5 | 基础示例稳定，主链路完整 |
| 可扩展性 | 3.5 | 已有 Pass 生命周期，但缺 RenderGraph 资源编排 |
| 算法验证效率 | 2.5 | 目前主要是手工验证，缺统一实验流程 |
| 可观测性 | 2.0 | 仅有基础 FPS，缺 GPU 计时与结构化实验指标 |
| 工程一致性 | 3.0 | 文档与实现存在少量漂移，需要持续对齐 |

### 10.3 关键风险与对策

- 风险 1：若先做高级特效而非验证工具，迭代速度会下降。  
  对策：v0.2-v0.4 优先建设调度、观测、复现能力，再推进复杂效果。
- 风险 2：文档与实现持续漂移，团队沟通成本上升。  
  对策：每个版本增加“文档一致性检查”作为发布前置门禁。
- 风险 3：性能结论不可复现，算法对比缺说服力。  
  对策：v0.4 起引入统一 benchmark preset 与结果归档格式。
