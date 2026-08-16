if(NOT DEFINED LOGFORGE_EXECUTABLE OR NOT DEFINED LOGFORGE_TEST_DIR)
    message(FATAL_ERROR "Integration test requires executable and test directory")
endif()

file(MAKE_DIRECTORY "${LOGFORGE_TEST_DIR}")
set(LOG_FILE "${LOGFORGE_TEST_DIR}/integration.log")
file(WRITE "${LOG_FILE}"
    "2026-08-15T10:15:01 INFO auth Login succeeded\n"
    "malformed\n"
    "2026-08-15T10:15:03 ERROR payments Payment failed\n"
    "2026-08-15T10:15:04 ERROR auth Login failed\n")

execute_process(
    COMMAND "${LOGFORGE_EXECUTABLE}" "${LOG_FILE}" --level ERROR --service payments --summary
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE diagnostics
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "LogForge exited ${result}: ${diagnostics}")
endif()

foreach(expected
        "Lines examined:    4"
        "Valid records:     3"
        "Malformed records: 1"
        "Matched records:   1"
        "ERROR  1"
        "payments  1")
    string(FIND "${output}" "${expected}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Missing '${expected}' in output:\n${output}")
    endif()
endforeach()

string(FIND "${diagnostics}" "malformed record at line 2" warning_position)
if(warning_position EQUAL -1)
    message(FATAL_ERROR "Missing malformed-record diagnostic: ${diagnostics}")
endif()
