@echo off
REM Shader compile script for MclarenApp
REM Requires Vulkan SDK installed (glslc in PATH)

set SRC_DIR=%~dp0
set GLSL_DIR=%SRC_DIR%
set OUT_DIR=%SRC_DIR%
set LOCAL_COMMON_DIR=%GLSL_DIR%\common
set REPO_COMMON_DIR=%GLSL_DIR%\..\..\..\resources\shaders\common
set REPO_COMMON_DIR_ALT=%GLSL_DIR%\..\..\..\..\resources\shaders\common
set COMMON_DIR=
set COMMON_INCLUDE_GLSLC=
set COMMON_INCLUDE_GLSLANG=
set GLSLC_FLAGS=
set SHADER_MODE=optimized-no-debug
set USE_GLSLANG_SOURCE_DEBUG=0

if defined KU_SHADER_SOURCE_DEBUG (
    if "%KU_SHADER_SOURCE_DEBUG%"=="1" (
        set USE_GLSLANG_SOURCE_DEBUG=1
    )
)

if /I "%CONFIG%"=="Debug" (
    set GLSLC_FLAGS=-g -O0
    set SHADER_MODE=debug-symbols
)

if "%GLSLC_FLAGS%"=="" (
    echo %OUT_DIR% | findstr /I /C:"\Debug\" >nul
    if %ERRORLEVEL% EQU 0 (
        set GLSLC_FLAGS=-g -O0
        set SHADER_MODE=debug-symbols
    )
)

if defined KU_SHADER_DEBUG (
    if "%KU_SHADER_DEBUG%"=="1" (
        set GLSLC_FLAGS=-g -O0
        set SHADER_MODE=debug-symbols
    )
)

if "%USE_GLSLANG_SOURCE_DEBUG%"=="1" (
    set SHADER_MODE=source-debug-gVS
)

echo Shader compile mode: %SHADER_MODE%
if not "%GLSLC_FLAGS%"=="" echo Shader debug flags: %GLSLC_FLAGS%
if "%USE_GLSLANG_SOURCE_DEBUG%"=="1" echo Shader source debug compiler: glslangValidator -gVS -Od

if exist "%LOCAL_COMMON_DIR%\lighting.glsl" (
    set COMMON_DIR=%LOCAL_COMMON_DIR%
) else if exist "%REPO_COMMON_DIR%\lighting.glsl" (
    set COMMON_DIR=%REPO_COMMON_DIR%
) else if exist "%REPO_COMMON_DIR_ALT%\lighting.glsl" (
    set COMMON_DIR=%REPO_COMMON_DIR_ALT%
) else (
    echo ERROR: common lighting include not found. Expected at:
    echo   %LOCAL_COMMON_DIR%\lighting.glsl
    echo   %REPO_COMMON_DIR%\lighting.glsl
    echo   %REPO_COMMON_DIR_ALT%\lighting.glsl
    exit /b 1
)

set COMMON_INCLUDE_GLSLC=-I "%COMMON_DIR%"
set COMMON_INCLUDE_GLSLANG=-I"%COMMON_DIR%"

echo Compiling mclaren shaders...

if "%USE_GLSLANG_SOURCE_DEBUG%"=="1" (
    where glslangValidator >nul 2>nul
    if %ERRORLEVEL% NEQ 0 (
        echo Failed to find glslangValidator in PATH. Install Vulkan SDK and reopen terminal.
        exit /b 1
    )

    glslangValidator -V -S vert -gVS -Od -o "%OUT_DIR%\mclaren.vert.spv" "%GLSL_DIR%\mclaren.vert"
    if %ERRORLEVEL% NEQ 0 (
        echo Failed to compile mclaren.vert with glslangValidator
        exit /b 1
    )
    echo   mclaren.vert.spv

    glslangValidator -V -S frag %COMMON_INCLUDE_GLSLANG% -gVS -Od -o "%OUT_DIR%\mclaren.frag.spv" "%GLSL_DIR%\mclaren.frag"
    if %ERRORLEVEL% NEQ 0 (
        echo Failed to compile mclaren.frag with glslangValidator
        exit /b 1
    )
    echo   mclaren.frag.spv

    glslangValidator -V -S vert -gVS -Od -o "%OUT_DIR%\skybox.vert.spv" "%GLSL_DIR%\skybox.vert"
    if %ERRORLEVEL% NEQ 0 (
        echo Failed to compile skybox.vert with glslangValidator
        exit /b 1
    )
    echo   skybox.vert.spv

    glslangValidator -V -S frag -gVS -Od -o "%OUT_DIR%\skybox.frag.spv" "%GLSL_DIR%\skybox.frag"
    if %ERRORLEVEL% NEQ 0 (
        echo Failed to compile skybox.frag with glslangValidator
        exit /b 1
    )
    echo   skybox.frag.spv
) else (
    glslc "%GLSL_DIR%\mclaren.vert" %GLSLC_FLAGS% -o "%OUT_DIR%\mclaren.vert.spv"
    if %ERRORLEVEL% NEQ 0 (
        echo Failed to compile mclaren.vert
        exit /b 1
    )
    echo   mclaren.vert.spv

    glslc "%GLSL_DIR%\mclaren.frag" %COMMON_INCLUDE_GLSLC% %GLSLC_FLAGS% -o "%OUT_DIR%\mclaren.frag.spv"
    if %ERRORLEVEL% NEQ 0 (
        echo Failed to compile mclaren.frag
        exit /b 1
    )
    echo   mclaren.frag.spv

    glslc "%GLSL_DIR%\skybox.vert" %GLSLC_FLAGS% -o "%OUT_DIR%\skybox.vert.spv"
    if %ERRORLEVEL% NEQ 0 (
        echo Failed to compile skybox.vert
        exit /b 1
    )
    echo   skybox.vert.spv

    glslc "%GLSL_DIR%\skybox.frag" %GLSLC_FLAGS% -o "%OUT_DIR%\skybox.frag.spv"
    if %ERRORLEVEL% NEQ 0 (
        echo Failed to compile skybox.frag
        exit /b 1
    )
    echo   skybox.frag.spv
)

echo Mclaren shaders compiled successfully!
