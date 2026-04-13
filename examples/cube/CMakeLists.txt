add_executable(CubeApp
    main.cpp
    CubePass.cpp
)

target_include_directories(CubeApp PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(CubeApp PRIVATE
    KuEngine
    imgui::imgui
)

set_target_properties(CubeApp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_custom_command(TARGET CubeApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:CubeApp>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/compile_shaders.bat
        $<TARGET_FILE_DIR:CubeApp>/shaders/compile_shaders.bat
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/cube.vert
        $<TARGET_FILE_DIR:CubeApp>/shaders/cube.vert
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/cube.frag
        $<TARGET_FILE_DIR:CubeApp>/shaders/cube.frag
)

if(Vulkan_GLSLC_EXECUTABLE)
    add_custom_command(TARGET CubeApp POST_BUILD
        COMMAND ${Vulkan_GLSLC_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/cube.vert
            -o $<TARGET_FILE_DIR:CubeApp>/shaders/cube.vert.spv
        COMMAND ${Vulkan_GLSLC_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/cube.frag
            -o $<TARGET_FILE_DIR:CubeApp>/shaders/cube.frag.spv
        COMMENT "Compiling cube shaders with glslc"
    )
else()
    message(WARNING "Vulkan_GLSLC_EXECUTABLE not found. Compile shaders manually with shaders/compile_shaders.bat")
endif()

add_custom_command(TARGET CubeApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:CubeApp>/shaders
    COMMENT "Copying cube shader sources"
)
