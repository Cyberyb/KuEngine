# Triangle 示例运行指南

本文档说明如何在当前仓库中编译并运行 Triangle 示例。

## 1. 前置条件

- Windows 10/11
- Visual Studio 2022（含 C++ 工具链）
- CMake 3.27+
- Vulkan SDK（需要 `glslc`）

可用下面命令确认 `glslc` 是否可用：

```powershell
where.exe glslc
```

## 2. 配置与编译

在仓库根目录执行：

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="./vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Debug --target TriangleApp
```

如果你想编译全部目标（库、示例、测试）：

```powershell
cmake --build build --config Debug
```

## 2.1 一键脚本（推荐）

如果你希望一次性完成“配置 + 编译 + shader 编译 + 运行”，可直接执行：

```powershell
./examples/triangle/run_triangle.bat
```

可选参数：构建配置（默认 `Debug`）

```powershell
./examples/triangle/run_triangle.bat Release
```

## 3. 着色器处理方式

TrianglePass 会在当前工作目录下查找：

- `shaders/triangle.vert.spv`
- `shaders/triangle.frag.spv`

当前构建流程会在 `glslc` 可用时自动编译 `.spv` 到运行目录，并同时复制 shader 源文件与脚本。

如果自动编译失败，可手动执行：

```powershell
Set-Location build/bin/Debug/shaders
./compile_shaders.bat
```

## 4. 运行示例

仍在仓库根目录下执行：

```powershell
Set-Location build/bin/Debug
./TriangleApp.exe
```

## 5. 常见问题

### 5.1 提示找不到 glslc

现象：执行 `compile_shaders.bat` 报 `glslc` 不是内部或外部命令。

处理：

- 安装 Vulkan SDK
- 将 Vulkan SDK 的 `Bin` 目录加入 PATH（例如 `C:/VulkanSDK/1.3.275.0/Bin`）
- 重新打开终端后再执行 `where.exe glslc`

### 5.2 提示着色器文件不存在

现象：日志中出现 `Failed to open shader`。

处理：

- 确认你是在 `build/bin/Debug` 目录运行 `TriangleApp.exe`
- 确认 `build/bin/Debug/shaders` 中已经生成 `.spv` 文件

### 5.3 能启动窗口但画面不符合预期

请优先检查以下两点：

- `build/bin/Debug/shaders` 是否存在 `triangle.vert.spv` 和 `triangle.frag.spv`
- 运行路径是否为 `build/bin/Debug`（避免相对路径找不到 shader）

如仍有问题，可开启 Vulkan Validation Layer 进一步定位。
