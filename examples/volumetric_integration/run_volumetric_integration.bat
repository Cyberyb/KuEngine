@echo off
setlocal
for %%I in ("%~dp0..\..") do set "ROOT_DIR=%%~fI"
set "BUILD_DIR=%ROOT_DIR%\build\cmake-tools\vs2022-vcpkg"
if defined KUENGINE_BUILD_DIR set "BUILD_DIR=%KUENGINE_BUILD_DIR%"
set "CONFIG=Debug"
if /i "%~1"=="Debug" set "CONFIG=Debug"
if /i "%~1"=="Release" set "CONFIG=Release"
cmake --preset vs2022-vcpkg -S "%ROOT_DIR%" -B "%BUILD_DIR%"
if errorlevel 1 exit /b 1
cmake --build "%BUILD_DIR%" --config "%CONFIG%" --target VolumetricIntegrationApp
if errorlevel 1 exit /b 1
pushd "%BUILD_DIR%\bin\%CONFIG%"
VolumetricIntegrationApp.exe
set "APP_EXIT=%ERRORLEVEL%"
popd
exit /b %APP_EXIT%
