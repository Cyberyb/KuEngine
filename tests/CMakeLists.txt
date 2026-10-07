find_package(GTest CONFIG REQUIRED)

add_executable(test_core
    core/test_engine.cpp
    core/test_render_graph.cpp
    core/test_compute_graph.cpp
    core/test_asset_config.cpp
    core/test_scene_assets.cpp
    core/test_application_runner.cpp
    core/test_frame_statistics.cpp
    core/test_pbr.cpp
    core/test_forward.cpp
    core/test_forward_graph_targets.cpp
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
    core/test_mclaren_asset_replacement.cpp
    core/test_cube_viewer.cpp
    core/test_orbit_camera.cpp
    ${CMAKE_SOURCE_DIR}/examples/cube/CubePass.cpp
    ${CMAKE_SOURCE_DIR}/examples/mclaren/MclarenAssetReplacement.cpp
    ${CMAKE_SOURCE_DIR}/examples/mclaren/MclarenSceneAsset.cpp
    ${CMAKE_SOURCE_DIR}/examples/mclaren/OrbitCameraController.cpp
)

target_compile_definitions(test_core PRIVATE
    KUENGINE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

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

target_compile_definitions(test_mclaren_camera PRIVATE
    KUENGINE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
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
        "KUENGINE_COMPLETED_STATS.*draws=91 vertices=9202884 .*status=matched"
        --smoke-frames 3
        --smoke-require-validation)
    set_tests_properties(gpu_smoke_mclaren PROPERTIES TIMEOUT 180)

    kuengine_add_gpu_smoke(
        gpu_smoke_forward_reuse
        ForwardReuseApp
        "KUENGINE_COMPLETED_STATS.*draws=9 vertices=45 .*status=matched"
        --smoke-frames 3
        --smoke-require-validation)
    kuengine_add_gpu_smoke(
        gpu_smoke_forward_reuse_zero_lights
        ForwardReuseApp
        "KUENGINE_COMPLETED_STATS.*draws=9 vertices=45 .*status=matched"
        --smoke-frames 3
        --smoke-require-validation
        --forward-reuse-zero-lights)
    kuengine_add_gpu_smoke(
        gpu_smoke_forward_reuse_backface
        ForwardReuseApp
        "KUENGINE_COMPLETED_STATS.*draws=9 vertices=45 .*status=matched"
        --smoke-frames 3
        --smoke-require-validation
        --forward-reuse-backface)

    kuengine_add_gpu_smoke(
        gpu_smoke_graph_resource_probe
        GraphResourceProbeApp
        "KUENGINE_GRAPH_RESOURCE_PROBE_RESIZE_OK.*extent=800x600"
        --smoke-frames 5
        --smoke-require-validation
        --smoke-resize-after 2 800 600)

    kuengine_add_gpu_smoke(
        gpu_smoke_forward_reuse_recompile
        ForwardReuseApp
        "KUENGINE_SMOKE_RECOMPILE_OK"
        --smoke-frames 3
        --smoke-require-validation
        --smoke-recompile-after-setup)
    kuengine_add_gpu_smoke(
        gpu_smoke_graph_resource_probe_recompile
        GraphResourceProbeApp
        "KUENGINE_SMOKE_RECOMPILE_OK"
        --smoke-frames 3
        --smoke-require-validation
        --smoke-recompile-after-setup)

    function(kuengine_add_mclaren_replacement_smoke name scenario)
        add_test(
            NAME ${name}
            COMMAND ${CMAKE_COMMAND}
                -DKUENGINE_SMOKE_APP=$<TARGET_FILE:MclarenApp>
                -DKUENGINE_SMOKE_WORKING_DIRECTORY=$<TARGET_FILE_DIR:MclarenApp>
                -DKUENGINE_MCLAREN_SCENARIO=${scenario}
                -DKUENGINE_MCLAREN_MODEL=models/props/mclaren_765lt.glb
                -DKUENGINE_MCLAREN_HDR=environments/hdr/citrus_orchard_road_puresky_4k.hdr
                -DKUENGINE_MCLAREN_BAD_MODEL=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/invalid-replacement.glb
                -DKUENGINE_MCLAREN_BAD_HDR=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/invalid-replacement.hdr
                -P ${CMAKE_CURRENT_SOURCE_DIR}/run_mclaren_replacement_smoke.cmake)
        set_tests_properties(${name} PROPERTIES
            LABELS "gpu-smoke;mclaren-replacement"
            SKIP_REGULAR_EXPRESSION "KUENGINE_MCLAREN_REPLACEMENT_SKIP"
            TIMEOUT 210)
    endfunction()

    kuengine_add_mclaren_replacement_smoke(
        gpu_smoke_mclaren_replace_valid valid)
    kuengine_add_mclaren_replacement_smoke(
        gpu_smoke_mclaren_replace_bad_retains_live bad)
    kuengine_add_mclaren_replacement_smoke(
        gpu_smoke_mclaren_replace_continuous continuous)
    kuengine_add_mclaren_replacement_smoke(
        gpu_smoke_mclaren_replace_resize resize)

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
