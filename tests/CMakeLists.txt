find_package(GTest CONFIG REQUIRED)

add_executable(test_core core/test_engine.cpp)

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
