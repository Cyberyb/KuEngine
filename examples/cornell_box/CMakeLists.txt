add_executable(CornellBoxApp
    main.cpp
    CornellBoxScene.cpp
    CornellBoxPass.cpp
    CornellBoxReadback.cpp)
target_include_directories(CornellBoxApp PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(CornellBoxApp PRIVATE KuEngine imgui::imgui)
set_target_properties(CornellBoxApp PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
kuengine_attach_forward_shaders(CornellBoxApp)
