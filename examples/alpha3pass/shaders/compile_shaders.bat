@echo off
REM Shader compile script for Alpha3PassApp
REM Requires Vulkan SDK installed (glslc in PATH)

set SRC_DIR=%~dp0
set GLSL_DIR=%SRC_DIR%
set OUT_DIR=%SRC_DIR%

echo Compiling alpha3pass shaders...

glslc %GLSL_DIR%\alpha.vert -o %OUT_DIR%\alpha.vert.spv
if %ERRORLEVEL% NEQ 0 (
    echo Failed to compile alpha.vert
    exit /b 1
)
echo   alpha.vert.spv

glslc %GLSL_DIR%\alpha.frag -o %OUT_DIR%\alpha.frag.spv
if %ERRORLEVEL% NEQ 0 (
    echo Failed to compile alpha.frag
    exit /b 1
)
echo   alpha.frag.spv

echo Alpha3Pass shaders compiled successfully!
