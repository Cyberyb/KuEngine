add_executable(VolumetricIntegrationApp main.cpp VolumetricPass.cpp VolumetricReadback.cpp)
target_include_directories(VolumetricIntegrationApp PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(VolumetricIntegrationApp PRIVATE KuEngine imgui::imgui)
set_target_properties(VolumetricIntegrationApp PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
if(NOT Vulkan_GLSLC_EXECUTABLE)
    message(FATAL_ERROR "VolumetricIntegration requires glslc")
endif()
set(volumetric_shader_outputs)
foreach(shader IN ITEMS fullscreen.vert volumetric.frag display.frag)
    set(output ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/shaders/volumetric_integration/${shader}.spv)
    add_custom_command(OUTPUT ${output}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/shaders/volumetric_integration
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} "$<$<CONFIG:Debug>:-g>" "$<$<CONFIG:Debug>:-O0>"
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader} -o ${output}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}
        COMMAND_EXPAND_LISTS VERBATIM)
    list(APPEND volumetric_shader_outputs ${output})
endforeach()
add_custom_target(KuEngineVolumetricShaders DEPENDS ${volumetric_shader_outputs})
add_dependencies(VolumetricIntegrationApp KuEngineVolumetricShaders)
