# 使用与回归说明

这里维护当前可执行的构建、运行和调试方法。设计见 [design](../design/README.md)，历史发布说明见 [archive](../structure/archive/README.md)。

- [Triangle](triangle-example.md)
- [Cube](cube-example.md)
- [Alpha3Pass](alpha3pass-example.md)
- [Mclaren](mclaren-example.md)
- [当前回归检查](regression-checks.md)
- [Shader 源码调试](shader-debugging.md)

两种构建入口输出不同：
- CMake preset：build/cmake-tools/vs2022-vcpkg/bin/Debug；
- 示例 run_*.bat：build/bin/Debug。

手动启动前进入对应 exe 所在目录，保证 shaders/resources 的相对路径正确。
