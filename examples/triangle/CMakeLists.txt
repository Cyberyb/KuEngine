add_executable(TriangleApp
    main.cpp
    TrianglePass.cpp
)

target_include_directories(TriangleApp PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(TriangleApp PRIVATE
    KuEngine
    imgui::imgui
)

set_target_properties(TriangleApp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_custom_command(TARGET TriangleApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:TriangleApp>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/compile_shaders.bat
        $<TARGET_FILE_DIR:TriangleApp>/shaders/compile_shaders.bat
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert
        $<TARGET_FILE_DIR:TriangleApp>/shaders/triangle.vert
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag
        $<TARGET_FILE_DIR:TriangleApp>/shaders/triangle.frag
)

if(Vulkan_GLSLC_EXECUTABLE)
    add_custom_command(TARGET TriangleApp POST_BUILD
        COMMAND ${Vulkan_GLSLC_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert
            -o $<TARGET_FILE_DIR:TriangleApp>/shaders/triangle.vert.spv
        COMMAND ${Vulkan_GLSLC_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag
            -o $<TARGET_FILE_DIR:TriangleApp>/shaders/triangle.frag.spv
        COMMENT "Compiling triangle shaders with glslc"
    )
else()
    message(WARNING "Vulkan_GLSLC_EXECUTABLE not found. Compile shaders manually with shaders/compile_shaders.bat")
endif()

add_custom_command(TARGET TriangleApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:TriangleApp>/shaders
    COMMENT "Copying triangle shader sources"
)
