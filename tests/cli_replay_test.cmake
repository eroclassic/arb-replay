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
