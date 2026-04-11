cmake_minimum_required(VERSION 3.27)
project(KuEngine VERSION 0.1.0 LANGUAGES CXX)

# ===== C++ 标准 =====
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ===== 输出目录 =====
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ===== 编译选项 =====
if(MSVC)
    add_compile_options(/W4 /permissive-)
    add_compile_options(/source-charset:utf-8 /execution-charset:utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ===== 生成 compile_commands.json（用于 IDE） =====
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ===== 第三方依赖（vcpkg） =====
find_package(Vulkan REQUIRED CONFIG)
find_package(SDL3 REQUIRED CONFIG)
find_package(spdlog REQUIRED)
find_package(glm REQUIRED)
find_package(fmt REQUIRED)

# ===== KuEngine 核心库 =====
add_library(KuEngine STATIC)
target_sources(KuEngine PRIVATE
    src/KuEngine/Core/Engine.cpp
    src/KuEngine/Core/Window.cpp
    src/KuEngine/Core/Input.cpp
    src/KuEngine/Core/Log.cpp
    src/KuEngine/RHI/RHIInstance.cpp
    src/KuEngine/RHI/RHIDevice.cpp
    src/KuEngine/RHI/SwapChain.cpp
    src/KuEngine/RHI/RHIBuffer.cpp
    src/KuEngine/RHI/RHITexture.cpp
    src/KuEngine/RHI/RHIShader.cpp
    src/KuEngine/RHI/RHIPipeline.cpp
    src/KuEngine/RHI/SyncManager.cpp
    src/KuEngine/UI/UIOverlay.cpp
)
target_include_directories(KuEngine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(KuEngine PUBLIC
    Vulkan::Vulkan
    SDL3::SDL3
    spdlog::spdlog
    glm::glm
    fmt::fmt
)

# ===== 示例程序：三角形 =====
add_executable(TriangleApp examples/triangle/main.cpp examples/triangle/TrianglePass.cpp)
target_link_libraries(TriangleApp PRIVATE KuEngine)
target_include_directories(TriangleApp PRIVATE examples/triangle)
set_target_properties(TriangleApp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# ===== 复制着色器到输出目录 =====
add_custom_command(TARGET TriangleApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/examples/triangle/shaders/
    $<TARGET_FILE_DIR:TriangleApp>/shaders/
    COMMENT "Copying shaders to output directory"
)

# ===== 单元测试 =====
enable_testing()
find_package(GTest REQUIRED)
add_executable(test_core tests/core/test_engine.cpp)
target_link_libraries(test_core PRIVATE KuEngine GTest::gtest GTest::gtest_main)
add_test(NAME core.test_engine COMMAND test_core)
