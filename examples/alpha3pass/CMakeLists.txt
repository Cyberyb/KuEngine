add_executable(Alpha3PassApp
    main.cpp
    AlphaPasses.cpp
)

target_include_directories(Alpha3PassApp PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(Alpha3PassApp PRIVATE
    KuEngine
    imgui::imgui
)

set_target_properties(Alpha3PassApp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_custom_command(TARGET Alpha3PassApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:Alpha3PassApp>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/compile_shaders.bat
        $<TARGET_FILE_DIR:Alpha3PassApp>/shaders/compile_shaders.bat
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/alpha.vert
        $<TARGET_FILE_DIR:Alpha3PassApp>/shaders/alpha.vert
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/alpha.frag
        $<TARGET_FILE_DIR:Alpha3PassApp>/shaders/alpha.frag
)

if(Vulkan_GLSLC_EXECUTABLE)
    add_custom_command(TARGET Alpha3PassApp POST_BUILD
        COMMAND ${Vulkan_GLSLC_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/alpha.vert
            -o $<TARGET_FILE_DIR:Alpha3PassApp>/shaders/alpha.vert.spv
        COMMAND ${Vulkan_GLSLC_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/alpha.frag
            -o $<TARGET_FILE_DIR:Alpha3PassApp>/shaders/alpha.frag.spv
        COMMENT "Compiling alpha3pass shaders with glslc"
    )
else()
    message(WARNING "Vulkan_GLSLC_EXECUTABLE not found. Compile shaders manually with shaders/compile_shaders.bat")
endif()

add_custom_command(TARGET Alpha3PassApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:Alpha3PassApp>/shaders
    COMMENT "Copying alpha3pass shader sources"
)
