if(NOT DEFINED TEST_EXECUTABLE)
  message(FATAL_ERROR "TEST_EXECUTABLE is required")
endif()

if(NOT DEFINED CTEST_EXECUTABLE OR NOT DEFINED TEST_DIRECTORY OR NOT DEFINED OPENVR_ENABLED)
  message(FATAL_ERROR "CTEST_EXECUTABLE, TEST_DIRECTORY, and OPENVR_ENABLED are required")
endif()

set(expected_names headset.dispatch-regression headset.harness headset.harness.discovery headset.harness.failure-propagation)
if(OPENVR_ENABLED)
  list(APPEND expected_names
    headset.openvr.overlay-sdk-fntables headset.openvr.idempotent-lifecycle headset.openvr.exclusive-owner
    headset.openvr.init-failure-cleanup-retry headset.openvr.version-failure-cleanup
    headset.openvr.interface-failure-cleanup headset.openvr.sdk-failure-cleanup headset.openvr.incomplete-loader
    headset.openvr.discovery headset.openvr.without-runtime)
endif()
execute_process(COMMAND "${CTEST_EXECUTABLE}" -N -L headset WORKING_DIRECTORY "${TEST_DIRECTORY}"
  RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT "${status}" STREQUAL "0" OR NOT "${errors}" STREQUAL "")
  message(FATAL_ERROR "CTest discovery failed: status=${status}, stdout=${output}, stderr=${errors}")
endif()
string(REGEX MATCHALL "Test +#[0-9]+: [^\n\r]+" registered "${output}")
set(actual_names)
foreach(entry IN LISTS registered)
  string(REGEX REPLACE "^Test +#[0-9]+: " "" name "${entry}")
  list(APPEND actual_names "${name}")
endforeach()
list(SORT expected_names)
list(SORT actual_names)
if(NOT "${actual_names}" STREQUAL "${expected_names}")
  message(FATAL_ERROR "CTest registration differs: expected=${expected_names}, actual=${actual_names}")
endif()
list(LENGTH expected_names expected_count)
if(NOT output MATCHES "Total Tests: ${expected_count}([^0-9]|$)")
  message(FATAL_ERROR "CTest total differs: ${output}")
endif()

execute_process(COMMAND "${TEST_EXECUTABLE}" --list RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT "${status}" STREQUAL "0" OR NOT "${output}" STREQUAL "harness.passing\n" OR NOT "${errors}" STREQUAL "")
  message(FATAL_ERROR "native discovery failed: status=${status}, stdout=${output}, stderr=${errors}")
endif()

execute_process(COMMAND "${TEST_EXECUTABLE}" RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT "${status}" STREQUAL "0" OR NOT output MATCHES "1 tests, 0 failures" OR NOT "${errors}" STREQUAL "")
  message(FATAL_ERROR "native passing child failed: status=${status}, stdout=${output}, stderr=${errors}")
endif()

execute_process(COMMAND "${TEST_EXECUTABLE}" --fail-child RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT "${status}" STREQUAL "1" OR NOT output MATCHES "FAIL harness.intentional-failure" OR NOT output MATCHES "after-failure-ran" OR NOT output MATCHES "2 tests, 1 failures" OR NOT errors MATCHES "check failed: false")
  message(FATAL_ERROR "native failure propagation failed: status=${status}, stdout=${output}, stderr=${errors}")
endif()

execute_process(COMMAND "${TEST_EXECUTABLE}" --unknown RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT "${status}" STREQUAL "2" OR NOT "${output}" STREQUAL "" OR NOT errors MATCHES "unknown test argument")
  message(FATAL_ERROR "native argument rejection failed: status=${status}, stdout=${output}, stderr=${errors}")
endif()
