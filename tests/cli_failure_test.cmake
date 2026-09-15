if(NOT DEFINED ARBREPLAY_CLI)
  message(FATAL_ERROR "ARBREPLAY_CLI is required")
endif()

if(NOT DEFINED TEST_CASE)
  message(FATAL_ERROR "TEST_CASE is required")
endif()

if(TEST_CASE STREQUAL "invalid_path")
  set(arguments replay "${MISSING_EVENTS}" --payout 1.000000)
  set(expected_error "unable to open event CSV")
elseif(TEST_CASE STREQUAL "invalid_payout")
  set(arguments replay "${SAMPLE_EVENTS}" --payout 0)
  set(expected_error "payout must be positive")
elseif(TEST_CASE STREQUAL "incorrect_arguments")
  set(arguments replay)
  set(expected_error "usage: arbreplay replay")
else()
  message(FATAL_ERROR "unknown TEST_CASE: ${TEST_CASE}")
endif()

execute_process(
  COMMAND "${ARBREPLAY_CLI}" ${arguments}
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error_output
)

if(result EQUAL 0)
  message(FATAL_ERROR
    "CLI unexpectedly succeeded\nstdout:\n${output}\nstderr:\n${error_output}")
endif()

string(FIND "${error_output}" "${expected_error}" position)
if(position EQUAL -1)
  message(FATAL_ERROR
    "CLI error did not contain '${expected_error}'\nstderr:\n${error_output}")
endif()
