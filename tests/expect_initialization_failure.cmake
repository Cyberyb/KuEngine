if(NOT DEFINED KUENGINE_SMOKE_APP OR NOT DEFINED KUENGINE_SMOKE_WORKING_DIRECTORY)
    message(FATAL_ERROR "Expected KUENGINE_SMOKE_APP and KUENGINE_SMOKE_WORKING_DIRECTORY")
endif()

execute_process(
    COMMAND "${KUENGINE_SMOKE_APP}" --smoke-frames 1
    WORKING_DIRECTORY "${KUENGINE_SMOKE_WORKING_DIRECTORY}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 60)

set(output "${stdout}${stderr}")
message("${output}")

if(result EQUAL 77)
    message(STATUS "KUENGINE_EXPECTED_INITIALIZATION_SKIP Vulkan runtime unavailable")
    return()
endif()

if(NOT result EQUAL 1)
    message(FATAL_ERROR
        "Missing-shader initialization returned ${result}, expected exit code 1")
endif()

if(NOT output MATCHES "KUENGINE_SMOKE_FAIL reason=initialization")
    message(FATAL_ERROR "Missing-shader run did not emit the initialization failure marker")
endif()

if(output MATCHES "KUENGINE_SMOKE_PASS")
    message(FATAL_ERROR "Missing-shader run incorrectly emitted the success marker")
endif()

if(output MATCHES "KUENGINE_SMOKE_SKIP")
    message(FATAL_ERROR "Missing-shader business failure was incorrectly reported as an environment skip")
endif()

message(STATUS "Missing-shader initialization produced the expected business failure")
