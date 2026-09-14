find_package(GTest CONFIG REQUIRED)

add_executable(test_core
    core/test_engine.cpp
    core/test_render_graph.cpp
    core/test_asset_config.cpp
    core/test_scene_assets.cpp
    core/test_application_runner.cpp
    core/test_frame_statistics.cpp
    core/test_pbr.cpp
    core/test_log.cpp
    core/test_sidebar.cpp
    core/test_viewer_layout.cpp
    core/test_vulkan_validation.cpp
)

target_include_directories(test_core PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_core PRIVATE
    KuEngine
    GTest::gtest
    GTest::gtest_main
)

set_target_properties(test_core PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(NAME core_tests COMMAND test_core)

add_executable(test_mclaren_camera
    core/test_cube_viewer.cpp
    core/test_orbit_camera.cpp
    ${CMAKE_SOURCE_DIR}/examples/cube/CubePass.cpp
    ${CMAKE_SOURCE_DIR}/examples/mclaren/OrbitCameraController.cpp
)

target_include_directories(test_mclaren_camera PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/examples/cube
    ${CMAKE_SOURCE_DIR}/examples/mclaren
)

target_link_libraries(test_mclaren_camera PRIVATE
    KuEngine
    GTest::gtest
    GTest::gtest_main
)

set_target_properties(test_mclaren_camera PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(NAME mclaren_camera_tests COMMAND test_mclaren_camera)

if(KUENGINE_ENABLE_GPU_SMOKE_TESTS)
    if(NOT KUENGINE_BUILD_EXAMPLES)
        message(FATAL_ERROR
            "KUENGINE_ENABLE_GPU_SMOKE_TESTS requires KUENGINE_BUILD_EXAMPLES=ON")
    endif()

    function(kuengine_add_gpu_smoke name target stats_pattern)
        add_test(
            NAME ${name}
            COMMAND ${target} ${ARGN})
        set_tests_properties(${name} PROPERTIES
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:${target}>"
            LABELS "gpu-smoke"
            SKIP_RETURN_CODE 77
            PASS_REGULAR_EXPRESSION "${stats_pattern}"
            FAIL_REGULAR_EXPRESSION "KUENGINE_VALIDATION_ERROR;KUENGINE_SMOKE_FAIL"
            TIMEOUT 120)
    endfunction()

    kuengine_add_gpu_smoke(
        gpu_smoke_triangle
        TriangleApp
        "KUENGINE_COMPLETED_STATS.*draws=1 vertices=3 .*status=matched"
        --smoke-frames 5
        --smoke-require-validation
        --smoke-resize-after 2 960 540)
    kuengine_add_gpu_smoke(
        gpu_smoke_cube
        CubeApp
        "KUENGINE_COMPLETED_STATS.*draws=1 vertices=36 .*status=matched"
        --smoke-frames 3
        --smoke-require-validation)
    kuengine_add_gpu_smoke(
        gpu_smoke_alpha3pass
        Alpha3PassApp
        "KUENGINE_COMPLETED_STATS.*draws=3 vertices=9 .*status=matched"
        --smoke-frames 3
        --smoke-require-validation)
    kuengine_add_gpu_smoke(
        gpu_smoke_mclaren
        MclarenApp
        "KUENGINE_COMPLETED_STATS.*draws=[1-9][0-9]* vertices=[1-9][0-9]* .*status=matched"
        --smoke-frames 3
        --smoke-require-validation)
    set_tests_properties(gpu_smoke_mclaren PROPERTIES TIMEOUT 180)

    kuengine_add_gpu_smoke(
        gpu_smoke_viewer_layout_collapsed
        TriangleApp
        "KUENGINE_COMPLETED_STATS.*draws=1 vertices=3 .*status=matched"
        --smoke-frames 3
        --smoke-require-validation
        --smoke-sidebar-collapsed)

    add_test(
        NAME gpu_smoke_validation_injection
        COMMAND ${CMAKE_COMMAND}
            -DKUENGINE_SMOKE_APP=$<TARGET_FILE:TriangleApp>
            -DKUENGINE_SMOKE_WORKING_DIRECTORY=$<TARGET_FILE_DIR:TriangleApp>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/expect_validation_failure.cmake)
    set_tests_properties(gpu_smoke_validation_injection PROPERTIES
        LABELS "gpu-smoke;negative"
        SKIP_REGULAR_EXPRESSION "KUENGINE_EXPECTED_VALIDATION_SKIP"
        TIMEOUT 90)

    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/missing_runtime_assets")
    add_test(
        NAME gpu_missing_shader_initialization_failure
        COMMAND ${CMAKE_COMMAND}
            -DKUENGINE_SMOKE_APP=$<TARGET_FILE:TriangleApp>
            -DKUENGINE_SMOKE_WORKING_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/missing_runtime_assets
            -P ${CMAKE_CURRENT_SOURCE_DIR}/expect_initialization_failure.cmake)
    set_tests_properties(gpu_missing_shader_initialization_failure PROPERTIES
        LABELS "gpu-negative;negative"
        SKIP_REGULAR_EXPRESSION "KUENGINE_EXPECTED_INITIALIZATION_SKIP"
        TIMEOUT 90)
endif()
