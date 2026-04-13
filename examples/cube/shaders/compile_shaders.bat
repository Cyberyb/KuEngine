@echo off
REM Shader compile script for CubeApp
REM Requires Vulkan SDK installed (glslc in PATH)

set SRC_DIR=%~dp0
set GLSL_DIR=%SRC_DIR%
set OUT_DIR=%SRC_DIR%

echo Compiling cube shaders...

glslc %GLSL_DIR%\cube.vert -o %OUT_DIR%\cube.vert.spv
if %ERRORLEVEL% NEQ 0 (
    echo Failed to compile cube.vert
    exit /b 1
)
echo   cube.vert.spv

glslc %GLSL_DIR%\cube.frag -o %OUT_DIR%\cube.frag.spv
if %ERRORLEVEL% NEQ 0 (
    echo Failed to compile cube.frag
    exit /b 1
)
echo   cube.frag.spv

echo Cube shaders compiled successfully!
