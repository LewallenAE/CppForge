if(NOT DEFINED TASKFORGE_EXECUTABLE OR NOT DEFINED TASKFORGE_TEST_DIR)
    message(FATAL_ERROR "Integration test requires executable and test directory")
endif()

file(MAKE_DIRECTORY "${TASKFORGE_TEST_DIR}")

set(SUCCESS_FILE "${TASKFORGE_TEST_DIR}/success.jobs")
file(WRITE "${SUCCESS_FILE}"
    "1 1\n"
    "2 0\n"
    "3 1\n")

execute_process(
    COMMAND "${TASKFORGE_EXECUTABLE}" run "${SUCCESS_FILE}" --workers 2 --queue-capacity 1
    RESULT_VARIABLE success_result
    OUTPUT_VARIABLE success_output
    ERROR_VARIABLE success_error
)
if(NOT success_result EQUAL 0)
    message(FATAL_ERROR "Successful run exited ${success_result}: ${success_error}")
endif()
foreach(expected "task=1 status=completed" "task=2 status=completed"
                 "task=3 status=completed" "submitted=3" "completed=3" "failed=0")
    string(FIND "${success_output}" "${expected}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Missing '${expected}' in run output:\n${success_output}")
    endif()
endforeach()

set(FAILURE_FILE "${TASKFORGE_TEST_DIR}/failure.jobs")
file(WRITE "${FAILURE_FILE}" "10 0 fail\n11 0\n")
execute_process(
    COMMAND "${TASKFORGE_EXECUTABLE}" run "${FAILURE_FILE}" --workers 2 --queue-capacity 1
    RESULT_VARIABLE failure_result
    OUTPUT_VARIABLE failure_output
    ERROR_VARIABLE failure_error
)
if(NOT failure_result EQUAL 3)
    message(FATAL_ERROR "Failure run exited ${failure_result}, expected 3: ${failure_error}")
endif()
string(FIND "${failure_output}" "task=10 status=failed" failure_position)
if(failure_position EQUAL -1)
    message(FATAL_ERROR "Task failure was not propagated:\n${failure_output}")
endif()

set(MALFORMED_FILE "${TASKFORGE_TEST_DIR}/malformed.jobs")
file(WRITE "${MALFORMED_FILE}" "1 nope\n")
execute_process(
    COMMAND "${TASKFORGE_EXECUTABLE}" run "${MALFORMED_FILE}"
    RESULT_VARIABLE malformed_result
    OUTPUT_VARIABLE malformed_output
    ERROR_VARIABLE malformed_error
)
if(NOT malformed_result EQUAL 2)
    message(FATAL_ERROR "Malformed run exited ${malformed_result}, expected 2")
endif()
string(FIND "${malformed_error}" "malformed job at line 1" malformed_position)
if(malformed_position EQUAL -1)
    message(FATAL_ERROR "Missing malformed-file diagnostic: ${malformed_error}")
endif()

execute_process(
    COMMAND "${TASKFORGE_EXECUTABLE}" benchmark --workers 2 --queue-capacity 4 --tasks 100
    RESULT_VARIABLE benchmark_result
    OUTPUT_VARIABLE benchmark_output
    ERROR_VARIABLE benchmark_error
)
if(NOT benchmark_result EQUAL 0)
    message(FATAL_ERROR "Benchmark exited ${benchmark_result}: ${benchmark_error}")
endif()
foreach(expected "Benchmark" "workers=2" "queue_capacity=4" "tasks=100"
                 "tasks_per_second=" "submitted=100" "completed=100" "failed=0")
    string(FIND "${benchmark_output}" "${expected}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Missing '${expected}' in benchmark output:\n${benchmark_output}")
    endif()
endforeach()
