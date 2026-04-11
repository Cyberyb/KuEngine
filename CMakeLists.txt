cmake_minimum_required(VERSION 3.27)
project(KuEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(KUENGINE_BUILD_EXAMPLES "Build example applications" ON)
option(KUENGINE_BUILD_TESTS "Build unit tests" ON)

find_package(glfw3 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)

add_subdirectory(src)

if(KUENGINE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(KUENGINE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
