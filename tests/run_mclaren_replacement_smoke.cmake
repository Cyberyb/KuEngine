if(NOT DEFINED KUENGINE_SMOKE_APP
    OR NOT DEFINED KUENGINE_SMOKE_WORKING_DIRECTORY
    OR NOT DEFINED KUENGINE_MCLAREN_SCENARIO
    OR NOT DEFINED KUENGINE_MCLAREN_MODEL
    OR NOT DEFINED KUENGINE_MCLAREN_HDR
    OR NOT DEFINED KUENGINE_MCLAREN_BAD_MODEL
    OR NOT DEFINED KUENGINE_MCLAREN_BAD_HDR)
    message(FATAL_ERROR "Missing Mclaren replacement smoke configuration")
endif()

set(arguments
    --smoke-frames 8
    --smoke-require-validation
    --mclaren-replace-after-updates 1)
set(expected_markers)

if(KUENGINE_MCLAREN_SCENARIO STREQUAL "valid")
    list(APPEND arguments
        --mclaren-replace-model "${KUENGINE_MCLAREN_MODEL}"
        --mclaren-replace-hdr "${KUENGINE_MCLAREN_HDR}")
    list(APPEND expected_markers
        "KUENGINE_ASSET_REPLACE_SUCCESS kind=model request=1"
        "KUENGINE_ASSET_REPLACE_SUCCESS kind=hdr request=2")
elseif(KUENGINE_MCLAREN_SCENARIO STREQUAL "bad")
    list(APPEND arguments
        --mclaren-replace-model "${KUENGINE_MCLAREN_BAD_MODEL}"
        --mclaren-replace-hdr "${KUENGINE_MCLAREN_BAD_HDR}")
    list(APPEND expected_markers
        "KUENGINE_ASSET_REPLACE_FAIL kind=model request=1 retained=1 stage=CpuDecode category=CpuDecode/Compatibility"
        "KUENGINE_ASSET_REPLACE_FAIL kind=hdr request=2 retained=1 stage=CpuDecode category=CpuDecode/Compatibility")
elseif(KUENGINE_MCLAREN_SCENARIO STREQUAL "continuous")
    list(APPEND arguments
        --mclaren-replace-model "${KUENGINE_MCLAREN_MODEL}"
        --mclaren-replace-hdr "${KUENGINE_MCLAREN_HDR}"
        --mclaren-replace-model "${KUENGINE_MCLAREN_MODEL}")
    list(APPEND expected_markers
        "KUENGINE_ASSET_REPLACE_SUCCESS kind=model request=1"
        "KUENGINE_ASSET_REPLACE_SUCCESS kind=hdr request=2"
        "KUENGINE_ASSET_REPLACE_SUCCESS kind=model request=3")
elseif(KUENGINE_MCLAREN_SCENARIO STREQUAL "resize")
    list(APPEND arguments
        --mclaren-replace-model "${KUENGINE_MCLAREN_MODEL}"
        --mclaren-replace-hdr "${KUENGINE_MCLAREN_HDR}"
        --smoke-resize-after 2 960 540)
    list(APPEND expected_markers
        "KUENGINE_ASSET_REPLACE_SUCCESS kind=model request=1"
        "KUENGINE_ASSET_REPLACE_SUCCESS kind=hdr request=2"
        "KUENGINE_VIEWER_LAYOUT sample=final")
else()
    message(FATAL_ERROR
        "Unknown Mclaren replacement scenario: ${KUENGINE_MCLAREN_SCENARIO}")
endif()

execute_process(
    COMMAND "${KUENGINE_SMOKE_APP}" ${arguments}
    WORKING_DIRECTORY "${KUENGINE_SMOKE_WORKING_DIRECTORY}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 180)

set(output "${stdout}${stderr}")
message("${output}")

if(result EQUAL 77)
    message(STATUS
        "KUENGINE_MCLAREN_REPLACEMENT_SKIP Validation layer unavailable")
    return()
endif()

if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "Mclaren replacement scenario ${KUENGINE_MCLAREN_SCENARIO} returned ${result}")
endif()
if(output MATCHES "KUENGINE_VALIDATION_ERROR")
    message(FATAL_ERROR
        "Mclaren replacement scenario emitted a Validation error")
endif()
if(output MATCHES "KUENGINE_SMOKE_FAIL")
    message(FATAL_ERROR
        "Mclaren replacement scenario emitted a smoke failure")
endif()
foreach(marker IN LISTS expected_markers)
    string(FIND "${output}" "${marker}" marker_position)
    if(marker_position EQUAL -1)
        message(FATAL_ERROR
            "Mclaren replacement scenario did not emit marker: ${marker}")
    endif()
endforeach()
if(NOT output MATCHES
    "KUENGINE_COMPLETED_STATS.*draws=[1-9][0-9]* vertices=[1-9][0-9]* .*status=matched")
    message(FATAL_ERROR
        "Mclaren replacement scenario did not finish with matched statistics")
endif()
if(NOT output MATCHES "KUENGINE_SMOKE_PASS submitted_frames=8")
    message(FATAL_ERROR
        "Mclaren replacement scenario did not continue through all frames")
endif()

message(STATUS
    "Mclaren replacement scenario ${KUENGINE_MCLAREN_SCENARIO} passed")
