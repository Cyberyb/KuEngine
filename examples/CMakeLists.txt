set(KUENGINE_FORWARD_SHADER_SOURCE_DIR
    ${CMAKE_SOURCE_DIR}/resources/shaders/forward)
set(KUENGINE_FORWARD_SHADER_OUTPUT_DIR
    ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/shaders/forward)

if(Vulkan_GLSLC_EXECUTABLE)
    add_custom_target(KuEngineForwardShaders
        COMMAND ${CMAKE_COMMAND} -E make_directory
            ${KUENGINE_FORWARD_SHADER_OUTPUT_DIR}
        COMMAND ${Vulkan_GLSLC_EXECUTABLE}
            ${KUENGINE_FORWARD_SHADER_SOURCE_DIR}/forward.vert
            -o ${KUENGINE_FORWARD_SHADER_OUTPUT_DIR}/forward.vert.spv
        COMMAND ${Vulkan_GLSLC_EXECUTABLE}
            ${KUENGINE_FORWARD_SHADER_SOURCE_DIR}/pbr.frag
            -o ${KUENGINE_FORWARD_SHADER_OUTPUT_DIR}/pbr.frag.spv
        COMMAND ${Vulkan_GLSLC_EXECUTABLE}
            ${KUENGINE_FORWARD_SHADER_SOURCE_DIR}/unlit.frag
            -o ${KUENGINE_FORWARD_SHADER_OUTPUT_DIR}/unlit.frag.spv
        COMMAND ${Vulkan_GLSLC_EXECUTABLE}
            ${KUENGINE_FORWARD_SHADER_SOURCE_DIR}/skybox.vert
            -o ${KUENGINE_FORWARD_SHADER_OUTPUT_DIR}/skybox.vert.spv
        COMMAND ${Vulkan_GLSLC_EXECUTABLE}
            ${KUENGINE_FORWARD_SHADER_SOURCE_DIR}/skybox.frag
            -o ${KUENGINE_FORWARD_SHADER_OUTPUT_DIR}/skybox.frag.spv
        COMMAND ${Vulkan_GLSLC_EXECUTABLE}
            ${KUENGINE_FORWARD_SHADER_SOURCE_DIR}/display.vert
            -o ${KUENGINE_FORWARD_SHADER_OUTPUT_DIR}/display.vert.spv
        COMMAND ${Vulkan_GLSLC_EXECUTABLE}
            ${KUENGINE_FORWARD_SHADER_SOURCE_DIR}/display.frag
            -o ${KUENGINE_FORWARD_SHADER_OUTPUT_DIR}/display.frag.spv
        SOURCES
            ${KUENGINE_FORWARD_SHADER_SOURCE_DIR}/forward.vert
            ${KUENGINE_FORWARD_SHADER_SOURCE_DIR}/pbr.frag
            ${KUENGINE_FORWARD_SHADER_SOURCE_DIR}/unlit.frag
            ${KUENGINE_FORWARD_SHADER_SOURCE_DIR}/skybox.vert
            ${KUENGINE_FORWARD_SHADER_SOURCE_DIR}/skybox.frag
            ${KUENGINE_FORWARD_SHADER_SOURCE_DIR}/display.vert
            ${KUENGINE_FORWARD_SHADER_SOURCE_DIR}/display.frag
        COMMENT "Compiling shared KuEngine forward shaders"
        VERBATIM)
else()
    message(WARNING "Vulkan_GLSLC_EXECUTABLE not found; shared forward shaders cannot be built")
endif()

function(kuengine_attach_forward_shaders target)
    if(TARGET KuEngineForwardShaders)
        add_dependencies(${target} KuEngineForwardShaders)
    endif()
endfunction()

add_subdirectory(triangle)
add_subdirectory(cube)
add_subdirectory(alpha3pass)
add_subdirectory(mclaren)
add_subdirectory(forward_reuse)
add_subdirectory(graph_resource_probe)
