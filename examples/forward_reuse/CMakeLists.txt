add_executable(ForwardReuseApp
    main.cpp
    ForwardReusePass.cpp)
target_include_directories(ForwardReuseApp PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(ForwardReuseApp PRIVATE
    KUENGINE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(ForwardReuseApp PRIVATE KuEngine imgui::imgui)
set_target_properties(ForwardReuseApp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
kuengine_attach_forward_shaders(ForwardReuseApp)
add_custom_command(TARGET ForwardReuseApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:ForwardReuseApp>/resources/forward_reuse
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/shared-triangle.gltf
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/semantic-card.gltf
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/forward-reuse.scene.json
        $<TARGET_FILE_DIR:ForwardReuseApp>/resources/forward_reuse
    COMMENT "Copying ForwardReuse fixtures")
