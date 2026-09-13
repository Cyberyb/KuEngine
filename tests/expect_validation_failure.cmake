if(NOT DEFINED KUENGINE_SMOKE_APP OR NOT DEFINED KUENGINE_SMOKE_WORKING_DIRECTORY)
    message(FATAL_ERROR "Expected KUENGINE_SMOKE_APP and KUENGINE_SMOKE_WORKING_DIRECTORY")
endif()

execute_process(
    COMMAND "${KUENGINE_SMOKE_APP}"
        --smoke-frames 1
        --smoke-require-validation
        --smoke-inject-validation-error
    WORKING_DIRECTORY "${KUENGINE_SMOKE_WORKING_DIRECTORY}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 60)

set(output "${stdout}${stderr}")
message("${output}")

if(result EQUAL 77)
    # CMake 3.27 has no portable way for script mode to select exit code 77.
    # Emit a wrapper-only marker and let CTest's SKIP_REGULAR_EXPRESSION map the
    # otherwise-successful script result to Skipped.
    message(STATUS "KUENGINE_EXPECTED_VALIDATION_SKIP controlled Validation injection unavailable")
    return()
endif()

if(NOT result EQUAL 3)
    message(FATAL_ERROR
        "Controlled Validation injection returned ${result}, expected exit code 3")
endif()

if(NOT output MATCHES "KUENGINE_VALIDATION_ERROR")
    message(FATAL_ERROR "Controlled Validation injection did not emit the error marker")
endif()

if(NOT output MATCHES "KUENGINE_SMOKE_FAIL reason=validation")
    message(FATAL_ERROR "Controlled Validation injection did not reach runner failure handling")
endif()

message(STATUS "Controlled Validation ERROR produced the expected non-success result")
