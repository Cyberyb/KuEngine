@echo off
REM Shader compile script for KuEngine
REM Requires Vulkan SDK installed (glslc in PATH)

set SRC_DIR=%~dp0
set GLSL_DIR=%SRC_DIR%
set OUT_DIR=%SRC_DIR%

echo Compiling shaders...

REM Triangle shaders
glslc %GLSL_DIR%\triangle.vert -o %OUT_DIR%\triangle.vert.spv
if %ERRORLEVEL% NEQ 0 (
    echo Failed to compile triangle.vert
    exit /b 1
)
echo   triangle.vert.spv

glslc %GLSL_DIR%\triangle.frag -o %OUT_DIR%\triangle.frag.spv
if %ERRORLEVEL% NEQ 0 (
    echo Failed to compile triangle.frag
    exit /b 1
)
echo   triangle.frag.spv

echo All shaders compiled successfully!
