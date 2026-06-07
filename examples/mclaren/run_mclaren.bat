@echo off
setlocal

REM One-click build + shader compile + run for MclarenApp

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "ROOT_DIR=%%~fI"
set "BUILD_DIR=%ROOT_DIR%\build"
set "CONFIG=Debug"
set "RUN_MODE=foreground"
if /I "%~1"=="bg" (
    set "RUN_MODE=background"
) else if not "%~1"=="" (
    set "CONFIG=%~1"
)
if /I "%~2"=="bg" set "RUN_MODE=background"
set "RUN_DIR=%BUILD_DIR%\bin\%CONFIG%"
set "SHADER_DIR=%RUN_DIR%\shaders"
set "TOOLCHAIN_FILE=%ROOT_DIR%\vcpkg\scripts\buildsystems\vcpkg.cmake"

echo [1/4] Configure project...
cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" -DCMAKE_TOOLCHAIN_FILE="%TOOLCHAIN_FILE%"
if errorlevel 1 (
    echo Configure failed.
    exit /b 1
)

echo [2/4] Build MclarenApp (%CONFIG%)...
cmake --build "%BUILD_DIR%" --config "%CONFIG%" --target MclarenApp
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

echo [4/4] Run MclarenApp...
pushd "%RUN_DIR%"
if /I "%RUN_MODE%"=="background" (
    start "MclarenApp" "MclarenApp.exe"
    set "APP_EXIT=0"
) else (
    echo Running in foreground mode. Window close will return exit code.
    MclarenApp.exe
    set "APP_EXIT=%ERRORLEVEL%"
)
popd

if not "%APP_EXIT%"=="0" (
    echo MclarenApp exited with code %APP_EXIT%.
    exit /b %APP_EXIT%
)

echo Done.
exit /b 0
