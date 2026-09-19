add_executable(arbreplay_smoke_test smoke_test.cpp)
target_link_libraries(arbreplay_smoke_test PRIVATE arbreplay_project_options)
add_test(NAME arbreplay_smoke_test COMMAND arbreplay_smoke_test)

add_executable(arbreplay_price_test price_test.cpp)
target_link_libraries(arbreplay_price_test PRIVATE arbreplay_project_options)
add_test(NAME arbreplay_price_test COMMAND arbreplay_price_test)

add_executable(arbreplay_quantity_test quantity_test.cpp)
target_link_libraries(arbreplay_quantity_test PRIVATE arbreplay_project_options)
add_test(NAME arbreplay_quantity_test COMMAND arbreplay_quantity_test)

add_executable(arbreplay_book_level_test book_level_test.cpp)
target_link_libraries(arbreplay_book_level_test PRIVATE arbreplay_project_options)
add_test(NAME arbreplay_book_level_test COMMAND arbreplay_book_level_test)

add_executable(arbreplay_book_side_test book_side_test.cpp)
target_link_libraries(arbreplay_book_side_test PRIVATE arbreplay)
add_test(NAME arbreplay_book_side_test COMMAND arbreplay_book_side_test)

add_executable(arbreplay_outcome_book_test outcome_book_test.cpp)
target_link_libraries(arbreplay_outcome_book_test PRIVATE arbreplay)
add_test(NAME arbreplay_outcome_book_test COMMAND arbreplay_outcome_book_test)

add_executable(arbreplay_outcome_id_test outcome_id_test.cpp)
target_link_libraries(arbreplay_outcome_id_test PRIVATE arbreplay)
add_test(NAME arbreplay_outcome_id_test COMMAND arbreplay_outcome_id_test)

add_executable(arbreplay_market_test market_test.cpp)
target_link_libraries(arbreplay_market_test PRIVATE arbreplay)
add_test(NAME arbreplay_market_test COMMAND arbreplay_market_test)

add_executable(arbreplay_money_test money_test.cpp)
target_link_libraries(arbreplay_money_test PRIVATE arbreplay)
add_test(NAME arbreplay_money_test COMMAND arbreplay_money_test)

add_executable(arbreplay_complete_set_opportunity_test
  complete_set_opportunity_test.cpp)
target_link_libraries(arbreplay_complete_set_opportunity_test
  PRIVATE arbreplay)
add_test(NAME arbreplay_complete_set_opportunity_test
  COMMAND arbreplay_complete_set_opportunity_test)

add_executable(arbreplay_complete_set_opportunity_level_test
  complete_set_opportunity_level_test.cpp)
target_link_libraries(arbreplay_complete_set_opportunity_level_test
  PRIVATE arbreplay)
add_test(NAME arbreplay_complete_set_opportunity_level_test
  COMMAND arbreplay_complete_set_opportunity_level_test)

add_executable(arbreplay_complete_set_detector_test
  complete_set_detector_test.cpp)
target_link_libraries(arbreplay_complete_set_detector_test PRIVATE arbreplay)
add_test(NAME arbreplay_complete_set_detector_test
  COMMAND arbreplay_complete_set_detector_test)

add_executable(arbreplay_market_event_test market_event_test.cpp)
target_link_libraries(arbreplay_market_event_test PRIVATE arbreplay)
add_test(NAME arbreplay_market_event_test COMMAND arbreplay_market_event_test)

add_executable(arbreplay_market_event_csv_test market_event_csv_test.cpp)
target_link_libraries(arbreplay_market_event_csv_test PRIVATE arbreplay)
add_test(NAME arbreplay_market_event_csv_test
  COMMAND arbreplay_market_event_csv_test)

add_executable(arbreplay_replay_engine_test replay_engine_test.cpp)
target_link_libraries(arbreplay_replay_engine_test PRIVATE arbreplay)
add_test(NAME arbreplay_replay_engine_test COMMAND arbreplay_replay_engine_test)

add_executable(arbreplay_replay_determinism_test replay_determinism_test.cpp)
target_link_libraries(arbreplay_replay_determinism_test PRIVATE arbreplay)
add_test(NAME arbreplay_replay_determinism_test
  COMMAND arbreplay_replay_determinism_test)

add_executable(arbreplay_replay_snapshot_test replay_snapshot_test.cpp)
target_link_libraries(arbreplay_replay_snapshot_test PRIVATE arbreplay)
add_test(NAME arbreplay_replay_snapshot_test
  COMMAND arbreplay_replay_snapshot_test)

add_test(NAME arbreplay_cli_replay_test
  COMMAND ${CMAKE_COMMAND}
    -DARBREPLAY_CLI=$<TARGET_FILE:arbreplay_cli>
    -DSAMPLE_EVENTS=${CMAKE_SOURCE_DIR}/data/sample_events.csv
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_replay_test.cmake)

foreach(test_case IN ITEMS invalid_path invalid_payout incorrect_arguments)
  add_test(NAME arbreplay_cli_${test_case}_test
    COMMAND ${CMAKE_COMMAND}
      -DARBREPLAY_CLI=$<TARGET_FILE:arbreplay_cli>
      -DSAMPLE_EVENTS=${CMAKE_SOURCE_DIR}/data/sample_events.csv
      -DMISSING_EVENTS=${CMAKE_BINARY_DIR}/missing-events.csv
      -DTEST_CASE=${test_case}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_failure_test.cmake)
endforeach()

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME arbreplay_polymarket_snapshot_test
    COMMAND ${CMAKE_COMMAND} -E env
      ARBREPLAY_CLI=$<TARGET_FILE:arbreplay_cli>
      PYTHONDONTWRITEBYTECODE=1
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/polymarket_snapshot_test.py)
  set_tests_properties(arbreplay_polymarket_snapshot_test PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
