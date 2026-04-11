add_executable(TriangleApp
    main.cpp
    TrianglePass.cpp
)

target_include_directories(TriangleApp PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(TriangleApp PRIVATE KuEngine)

set_target_properties(TriangleApp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_custom_command(TARGET TriangleApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:TriangleApp>/shaders
    COMMENT "Copying triangle shaders"
)
