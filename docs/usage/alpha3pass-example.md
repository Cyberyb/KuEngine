# Alpha3Pass 示例运行指南

本文档说明如何编译并运行 `Alpha3PassApp`（v0.2 阶段4三 Pass Alpha 示例）。

## 1. 前置条件

- Windows 10/11
- Visual Studio 2022（含 C++ 工具链）
- CMake 3.27+
- Vulkan SDK（需要 `glslc`）

## 2. 使用 CMake Preset（推荐）

在仓库根目录执行：

```powershell
cmake --preset vs2022-vcpkg
cmake --build --preset debug --target Alpha3PassApp
```

如需运行测试：

```powershell
ctest --preset debug
```

## 3. 一键脚本（可选）

```powershell
./examples/alpha3pass/run_alpha3pass.bat
```

可选参数：

```powershell
./examples/alpha3pass/run_alpha3pass.bat Release
```

## 4. 示例内容

`Alpha3PassApp` 在同一帧中调度 3 个 Pass：

1. `Background`：绘制背景三角形
2. `MainTriangle`：主三角形，显式依赖 `Background`
3. `Accent`：强调三角形，显式依赖 `MainTriangle`

每个 Pass 都在 UI 中暴露颜色、位置和缩放参数。

## 5. 常见问题

### 5.1 找不到 glslc

现象：`compile_shaders.bat` 报错 glslc 不存在。

处理：

- 安装 Vulkan SDK
- 将 Vulkan SDK `Bin` 目录加入 PATH
- 重开终端后执行 `where.exe glslc`

### 5.2 启动后没有看到期望画面

请检查：

- 运行目录是否为 `build/.../bin/Debug`
- `shaders` 目录下是否存在 `alpha.vert.spv` 和 `alpha.frag.spv`
- CMake 构建输出中是否包含 `Alpha3PassApp`

## 6. 进一步回归检查

如需执行 v0.2 最小回归流程，请参考：`docs/usage/v0.2-regression-checks.md`。
