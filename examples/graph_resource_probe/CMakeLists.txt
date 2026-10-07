add_executable(GraphResourceProbeApp
    main.cpp
    GraphResourceProbePass.cpp
)

target_include_directories(GraphResourceProbeApp PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(GraphResourceProbeApp PRIVATE KuEngine)

set_target_properties(GraphResourceProbeApp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

if(Vulkan_GLSLC_EXECUTABLE)
    add_custom_command(TARGET GraphResourceProbeApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:GraphResourceProbeApp>/shaders
        COMMAND ${Vulkan_GLSLC_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/graph_resource_probe.vert
            -o $<TARGET_FILE_DIR:GraphResourceProbeApp>/shaders/graph_resource_probe.vert.spv
        COMMAND ${Vulkan_GLSLC_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/graph_resource_probe.frag
            -o $<TARGET_FILE_DIR:GraphResourceProbeApp>/shaders/graph_resource_probe.frag.spv
        COMMAND ${Vulkan_GLSLC_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/graph_resource_probe.comp
            -o $<TARGET_FILE_DIR:GraphResourceProbeApp>/shaders/graph_resource_probe.comp.spv
        COMMENT "Compiling graph resource probe shaders with glslc")
else()
    message(WARNING "Vulkan_GLSLC_EXECUTABLE not found; graph resource probe shaders cannot be built")
endif()
