# Cube 示例使用说明

在仓库根目录运行：

```powershell
./examples/cube/run_cube.bat Debug
```

脚本构建到 build/bin/Debug 并处理 Shader。手动 preset 构建：

```powershell
cmake --preset vs2022-vcpkg
cmake --build --preset debug --target CubeApp
Push-Location build/cmake-tools/vs2022-vcpkg/bin/Debug
try { ./CubeApp.exe } finally { Pop-Location }
```

左键拖动旋转；面板可调颜色、相机距离，切换 Wireframe Mode 和重置旋转。实心模式预期业务 Draw Calls=1、Vertices=36，线框模式为 1/24。

设计见 [示例模块](../design/04-triangle-example-tech.md)，完整检查见 [回归指南](regression-checks.md)。
