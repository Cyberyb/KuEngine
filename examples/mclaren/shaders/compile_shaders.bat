@echo off
REM Shader compile script for MclarenApp
REM Requires Vulkan SDK installed (glslc in PATH)

set SRC_DIR=%~dp0
set GLSL_DIR=%SRC_DIR%
set OUT_DIR=%SRC_DIR%

echo Compiling mclaren shaders...

glslc %GLSL_DIR%\mclaren.vert -o %OUT_DIR%\mclaren.vert.spv
if %ERRORLEVEL% NEQ 0 (
    echo Failed to compile mclaren.vert
    exit /b 1
)
echo   mclaren.vert.spv

glslc %GLSL_DIR%\mclaren.frag -o %OUT_DIR%\mclaren.frag.spv
if %ERRORLEVEL% NEQ 0 (
    echo Failed to compile mclaren.frag
    exit /b 1
)
echo   mclaren.frag.spv

echo Mclaren shaders compiled successfully!
