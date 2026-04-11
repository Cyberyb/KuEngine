@echo off
setlocal

REM One-click build + shader compile + run for TriangleApp

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "ROOT_DIR=%%~fI"
set "BUILD_DIR=%ROOT_DIR%\build"
set "CONFIG=Debug"
if not "%~1"=="" set "CONFIG=%~1"
set "RUN_DIR=%BUILD_DIR%\bin\%CONFIG%"
set "SHADER_DIR=%RUN_DIR%\shaders"
set "TOOLCHAIN_FILE=%ROOT_DIR%\vcpkg\scripts\buildsystems\vcpkg.cmake"

echo [1/4] Configure project...
cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" -DCMAKE_TOOLCHAIN_FILE="%TOOLCHAIN_FILE%"
if errorlevel 1 (
    echo Configure failed.
    exit /b 1
)

echo [2/4] Build TriangleApp (%CONFIG%)...
cmake --build "%BUILD_DIR%" --config "%CONFIG%" --target TriangleApp
if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

echo [3/4] Compile shaders...
if exist "%SHADER_DIR%\compile_shaders.bat" (
    pushd "%SHADER_DIR%"
    call compile_shaders.bat
    if errorlevel 1 (
        popd
        echo Shader compile failed.
        exit /b 1
    )
    popd
) else (
    echo Shader script not found: "%SHADER_DIR%\compile_shaders.bat"
    exit /b 1
)

echo [4/4] Run TriangleApp...
pushd "%RUN_DIR%"
start "TriangleApp" "TriangleApp.exe"
popd

echo Done.
exit /b 0
