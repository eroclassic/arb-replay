if(NOT DEFINED ARBREPLAY_CLI)
  message(FATAL_ERROR "ARBREPLAY_CLI is required")
endif()

if(NOT DEFINED SAMPLE_EVENTS)
  message(FATAL_ERROR "SAMPLE_EVENTS is required")
endif()

execute_process(
  COMMAND
    "${ARBREPLAY_CLI}" replay "${SAMPLE_EVENTS}"
    --payout 1.000000
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error_output
)

if(NOT result EQUAL 0)
  message(FATAL_ERROR
    "CLI exited with ${result}\nstdout:\n${output}\nstderr:\n${error_output}")
endif()

foreach(expected IN ITEMS "events: 6" "detections: 2")
  string(FIND "${output}" "${expected}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR
      "CLI output did not contain '${expected}'\nstdout:\n${output}")
  endif()
endforeach()

execute_process(
  COMMAND
    "${ARBREPLAY_CLI}" replay "${SAMPLE_EVENTS}"
    --payout 1.000000 --latency-ns 500000000
  RESULT_VARIABLE latency_result
  OUTPUT_VARIABLE latency_output
  ERROR_VARIABLE latency_error
)

if(NOT latency_result EQUAL 0)
  message(FATAL_ERROR
    "Latency CLI exited with ${latency_result}\nstdout:\n${latency_output}"
    "\nstderr:\n${latency_error}")
endif()

string(FIND "${latency_output}" "detections: 1" latency_position)
if(latency_position EQUAL -1)
  message(FATAL_ERROR
    "Latency CLI output did not contain 'detections: 1'\n"
    "stdout:\n${latency_output}")
endif()

execute_process(
  COMMAND
    "${ARBREPLAY_CLI}" replay "${SAMPLE_EVENTS}"
    --payout 1.000000 --fee-bps 400
  RESULT_VARIABLE fee_result
  OUTPUT_VARIABLE fee_output
  ERROR_VARIABLE fee_error
)

if(NOT fee_result EQUAL 0)
  message(FATAL_ERROR
    "Fee CLI exited with ${fee_result}\nstdout:\n${fee_output}"
    "\nstderr:\n${fee_error}")
endif()

string(FIND "${fee_output}" "detections: 0" fee_position)
if(fee_position EQUAL -1)
  message(FATAL_ERROR
    "Fee CLI output did not contain 'detections: 0'\n"
    "stdout:\n${fee_output}")
endif()
