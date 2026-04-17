add_library(KuEngine STATIC)

target_compile_features(KuEngine PUBLIC cxx_std_20)

target_sources(KuEngine PRIVATE
    KuEngine/Asset/Model.cpp
    KuEngine/Core/Engine.cpp
    KuEngine/Core/Window.cpp
    KuEngine/Core/Input.cpp
    KuEngine/Core/Log.cpp
    KuEngine/RHI/RHIInstance.cpp
    KuEngine/RHI/RHIDevice.cpp
    KuEngine/RHI/SwapChain.cpp
    KuEngine/RHI/RHIBuffer.cpp
    KuEngine/RHI/RHITexture.cpp
    KuEngine/RHI/RHIShader.cpp
    KuEngine/RHI/RHIPipeline.cpp
    KuEngine/RHI/SyncManager.cpp
    KuEngine/RHI/CommandList.cpp
    KuEngine/RHI/vma.cpp
    KuEngine/Render/RenderGraph.cpp
    KuEngine/Render/RenderPipeline.cpp
    KuEngine/UI/UIOverlay.cpp
)

target_include_directories(KuEngine PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(KuEngine PUBLIC
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator
    glfw
    imgui::imgui
    spdlog::spdlog
    glm::glm
    fmt::fmt
)

if(MSVC)
    target_compile_definitions(KuEngine PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

set_target_properties(KuEngine PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
