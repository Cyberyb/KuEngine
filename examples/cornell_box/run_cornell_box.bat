@echo off
setlocal
for %%I in ("%~dp0..\..") do set "ROOT_DIR=%%~fI"
set "BUILD_DIR=%ROOT_DIR%\build"
set "CONFIG=Debug"
if not "%~1"=="" set "CONFIG=%~1"
cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" -DCMAKE_TOOLCHAIN_FILE="%ROOT_DIR%\vcpkg\scripts\buildsystems\vcpkg.cmake"
if errorlevel 1 exit /b 1
cmake --build "%BUILD_DIR%" --config "%CONFIG%" --target CornellBoxApp
if errorlevel 1 exit /b 1
pushd "%BUILD_DIR%\bin\%CONFIG%"
CornellBoxApp.exe
set "APP_EXIT=%ERRORLEVEL%"
popd
exit /b %APP_EXIT%
