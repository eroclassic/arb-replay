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

foreach(expected IN ITEMS
    "events: 6"
    "detections: 2"
    "estimated_net_profits (per detection; not summed):"
    "1: 0.255000"
    "2: 0.170000")
  string(FIND "${output}" "${expected}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR
      "CLI output did not contain '${expected}'\nstdout:\n${output}")
  endif()
endforeach()

string(FIND "${output}" "opportunity 1:" verbose_only_position)
if(NOT verbose_only_position EQUAL -1)
  message(FATAL_ERROR
    "Default CLI output unexpectedly included verbose details\n"
    "stdout:\n${output}")
endif()

execute_process(
  COMMAND
    "${ARBREPLAY_CLI}" replay "${SAMPLE_EVENTS}"
    --payout 1.000000 --verbose
  RESULT_VARIABLE verbose_result
  OUTPUT_VARIABLE verbose_output
  ERROR_VARIABLE verbose_error
)

if(NOT verbose_result EQUAL 0)
  message(FATAL_ERROR
    "Verbose CLI exited with ${verbose_result}\nstdout:\n${verbose_output}"
    "\nstderr:\n${verbose_error}")
endif()

foreach(expected IN ITEMS
    "opportunity 1:"
    "available_sets: 8.500000"
    "payout_per_set: 1.000000"
    "total_cost: 8.245000"
    "estimated_fees: 0.000000"
    "gross_profit: 0.255000"
    "estimated_net_profit: 0.255000"
    "cost_per_set: 0.970000, quantity: 8.500000")
  string(FIND "${verbose_output}" "${expected}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR
      "Verbose CLI output did not contain '${expected}'\n"
      "stdout:\n${verbose_output}")
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

execute_process(
  COMMAND
    "${ARBREPLAY_CLI}" replay "${SAMPLE_EVENTS}"
    --payout 1.000000 --fee-bps 100
  RESULT_VARIABLE net_fee_result
  OUTPUT_VARIABLE net_fee_output
  ERROR_VARIABLE net_fee_error
)

if(NOT net_fee_result EQUAL 0)
  message(FATAL_ERROR
    "Net-fee CLI exited with ${net_fee_result}\nstdout:\n${net_fee_output}"
    "\nstderr:\n${net_fee_error}")
endif()

foreach(expected IN ITEMS "estimated_net_profits" "1: 0.172550")
  string(FIND "${net_fee_output}" "${expected}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR
      "CLI output did not contain '${expected}'\nstdout:\n${net_fee_output}")
  endif()
endforeach()
