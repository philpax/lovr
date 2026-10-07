if(NOT DEFINED TEST_EXECUTABLE)
  message(FATAL_ERROR "TEST_EXECUTABLE is required")
endif()

if(NOT DEFINED CTEST_EXECUTABLE OR NOT DEFINED TEST_DIRECTORY OR NOT DEFINED OPENVR_ENABLED)
  message(FATAL_ERROR "CTEST_EXECUTABLE, TEST_DIRECTORY, and OPENVR_ENABLED are required")
endif()

set(expected_names headset.dispatch.lifecycle headset.dispatch.features-and-layers
  headset.dispatch.false-and-null-forwarding headset.dispatch.vulkan-forwarding
  headset.dispatch.creator-and-generation headset.dispatch.unchecked.creator-and-generation
  headset.harness headset.harness.discovery headset.harness.failure-propagation)
foreach(mode IN ITEMS native disabled emscripten)
  foreach(case IN ITEMS yield restart actual-exit discovery)
    list(APPEND expected_names headset.standalone.${mode}.${case})
  endforeach()
endforeach()
if(GPU_TESTS_ENABLED)
  list(APPEND expected_names headset.config)
  foreach(mode IN ITEMS enabled disabled)
    foreach(case IN ITEMS matrix connect-false legacy-connect-default cleanup-pending freeze gpu-lifetime ownership deferred-acquire failed-init-acquire actual-exit discovery)
      list(APPEND expected_names headset.selection.${mode}.${case})
    endforeach()
  endforeach()
  list(APPEND expected_names headset.openxr.connect.no-overlay headset.openxr.connect.probe
    headset.openxr.connect.cleanup-retry headset.openxr.connect.failures
    headset.openxr.connect.bindings-retry headset.openxr.connect.discovery)
  if(NOT DEFINED JOINT_EXECUTABLE OR NOT DEFINED JOINT_CASES)
    message(FATAL_ERROR "JOINT_EXECUTABLE and JOINT_CASES are required")
  endif()
  string(REPLACE "|" ";" joint_cases "${JOINT_CASES}")
  foreach(case IN LISTS joint_cases)
    list(APPEND expected_names headset.${case})
  endforeach()
  list(JOIN joint_cases "\n" joint_expected_output)
  string(APPEND joint_expected_output "\n")
  execute_process(COMMAND "${JOINT_EXECUTABLE}" --list
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE errors)
  if(NOT "${status}" STREQUAL "0" OR NOT "${output}" STREQUAL "${joint_expected_output}" OR NOT "${errors}" STREQUAL "")
    message(FATAL_ERROR "joint layer graphics discovery failed: status=${status}, stdout=${output}, stderr=${errors}")
  endif()
  if(NOT DEFINED LAYER_EXECUTABLE OR NOT DEFINED LAYER_CASES)
    message(FATAL_ERROR "LAYER_EXECUTABLE and LAYER_CASES are required")
  endif()
  string(REPLACE "|" ";" layer_cases "${LAYER_CASES}")
  foreach(case IN LISTS layer_cases)
    list(APPEND expected_names headset.${case})
  endforeach()
  list(JOIN layer_cases "\n" layer_expected_output)
  string(APPEND layer_expected_output "\n")
  execute_process(COMMAND "${LAYER_EXECUTABLE}" --list
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE errors)
  if(NOT "${status}" STREQUAL "0" OR NOT "${output}" STREQUAL "${layer_expected_output}" OR NOT "${errors}" STREQUAL "")
    message(FATAL_ERROR "layer discovery failed: status=${status}, stdout=${output}, stderr=${errors}")
  endif()
  list(APPEND expected_names headset.gpu.normal headset.gpu.partial headset.gpu.absent headset.gpu.init-failure
    headset.gpu.runtime-retirement headset.gpu.wait-errors headset.gpu.mutex-failure headset.gpu.prepare-teardown
    headset.gpu.external.descriptor headset.gpu.external.rejection headset.gpu.external.ownership headset.gpu.external.transitions)
  if(NOT DEFINED CLEANUP_EXECUTABLE OR NOT DEFINED CLEANUP_CASES)
    message(FATAL_ERROR "CLEANUP_EXECUTABLE and CLEANUP_CASES are required")
  endif()
  string(REPLACE "|" ";" cleanup_cases "${CLEANUP_CASES}")
  foreach(case IN LISTS cleanup_cases)
    list(APPEND expected_names headset.${case})
  endforeach()
  list(JOIN cleanup_cases "\n" cleanup_expected_output)
  string(APPEND cleanup_expected_output "\n")
  execute_process(COMMAND "${CLEANUP_EXECUTABLE}" --list
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE errors)
  if(NOT "${status}" STREQUAL "0" OR NOT "${output}" STREQUAL "${cleanup_expected_output}" OR NOT "${errors}" STREQUAL "")
    message(FATAL_ERROR "graphics cleanup discovery failed: status=${status}, stdout=${output}, stderr=${errors}")
  endif()
endif()
if(GPU_TESTS_ENABLED)
  if(NOT DEFINED GRAPHICS_EXECUTABLE OR NOT DEFINED GRAPHICS_CASES)
    message(FATAL_ERROR "GRAPHICS_EXECUTABLE and GRAPHICS_CASES are required")
  endif()
  string(REPLACE "|" ";" graphics_cases "${GRAPHICS_CASES}")
  foreach(case IN LISTS graphics_cases)
    list(APPEND expected_names headset.${case})
  endforeach()
  list(APPEND expected_names headset.graphics.external.ordering headset.graphics.external.callback-failure
    headset.graphics.external.invalid headset.graphics.external.failures
    headset.graphics.external.callback-last-reference-release
    headset.graphics.external.arbitrary-texture-api-reentry-rejected headset.graphics.external.selective-quiescence
    headset.graphics.external.callback-material-last-reference-release)
  list(APPEND expected_names headset.graphics.session.discovery
    headset.graphics.session.lua-boundary.checked headset.graphics.session.lua-boundary.unchecked)
  list(JOIN graphics_cases "\n" graphics_expected_output)
  string(APPEND graphics_expected_output "\n")
  execute_process(COMMAND "${GRAPHICS_EXECUTABLE}" --list
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE errors)
  if(NOT "${status}" STREQUAL "0" OR NOT "${output}" STREQUAL "${graphics_expected_output}" OR NOT "${errors}" STREQUAL "")
    message(FATAL_ERROR "graphics discovery failed: status=${status}, stdout=${output}, stderr=${errors}")
  endif()
  execute_process(COMMAND "${GRAPHICS_EXECUTABLE}" --case graphics.session.unknown
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE errors)
  if(NOT "${status}" STREQUAL "2" OR NOT "${output}" STREQUAL "" OR NOT errors MATCHES "unknown test case")
    message(FATAL_ERROR "graphics case rejection failed: status=${status}, stdout=${output}, stderr=${errors}")
  endif()
endif()
if(OPENVR_ENABLED)
  foreach(helper IN ITEMS frame projection events haptics)
    file(READ "${CMAKE_CURRENT_LIST_DIR}/openvr_${helper}.c" helper_source)
    string(REGEX MATCH "NativeTest[ \t\r\n]+tests\\[\\][ \t\r\n]*=[ \t\r\n]*\\{[^;]*\\};" helper_test_list "${helper_source}")
    string(REGEX MATCHALL "\\{[ \t\r\n]*\"[^\"]+\"[ \t\r\n]*," helper_case_entries "${helper_test_list}")
    if(NOT helper_case_entries)
      message(FATAL_ERROR "No OpenVR ${helper} regression cases found")
    endif()
    foreach(entry IN LISTS helper_case_entries)
      string(REGEX MATCH "\"([^\"]+)\"" helper_case_match "${entry}")
      list(APPEND expected_names headset.openvr.${helper}.${CMAKE_MATCH_1})
    endforeach()
    list(APPEND expected_names headset.openvr.${helper}.discovery)
  endforeach()
  foreach(suite IN ITEMS backend backend_graphics)
    file(READ "${CMAKE_CURRENT_LIST_DIR}/openvr_${suite}.c" backend_source)
    string(REGEX MATCH "NativeTest[ \t\r\n]+tests\\[\\][ \t\r\n]*=[ \t\r\n]*\\{[^;]*\\};" backend_test_list "${backend_source}")
    string(REGEX MATCHALL "\\{[ \t\r\n]*\"openvr\\.backend\\.[^\"]+\"[ \t\r\n]*," backend_case_entries "${backend_test_list}")
    if(NOT backend_case_entries)
      message(FATAL_ERROR "No OpenVR ${suite} regression cases found")
    endif()
    foreach(entry IN LISTS backend_case_entries)
      string(REGEX MATCH "\"([^\"]+)\"" backend_case_match "${entry}")
      list(APPEND expected_names headset.${CMAKE_MATCH_1})
    endforeach()
  endforeach()
  list(APPEND expected_names
    headset.openvr.overlay-sdk-fntables headset.openvr.idempotent-lifecycle headset.openvr.exclusive-owner
    headset.openvr.init-failure-cleanup-retry headset.openvr.version-failure-cleanup
    headset.openvr.interface-failure-cleanup headset.openvr.sdk-failure-cleanup headset.openvr.incomplete-loader
    headset.openvr.discovery headset.openvr.without-runtime
    headset.openvr.vulkan.negotiation headset.openvr.vulkan.query-errors
    headset.openvr.vulkan.empty-and-invalid headset.openvr.vulkan.allocation-errors
    headset.openvr.vulkan.adapter-errors headset.openvr.vulkan.vulkan-errors headset.openvr.vulkan.discovery
    headset.openvr.panel_property_matrix headset.openvr.panel_failure_cleanup_retry
    headset.openvr.panel_capability_contract headset.openvr.panel_stereo_curve_mapping headset.openvr.panel.discovery
    headset.openvr.input.initialization headset.openvr.input.action-snapshot-transitions
    headset.openvr.input.haptic-action-routing headset.openvr.input.scalar-axis-canary
    headset.openvr.input.malformed-tracking headset.openvr.input.discovery
    headset.openvr.assets.installed_action_asset_resolution headset.openvr.assets.artifact_path_failures
    headset.openvr.assets.asset_file_failures headset.openvr.assets.asset_directory_failures
    headset.openvr.assets.relative_artifact_rejected_after_cwd_change headset.openvr.assets.asset_error_context
    headset.openvr.assets.discovery
    headset.openvr.backend.discovery headset.openvr.backend.graphics-discovery
    headset.openvr.diagnostic.connected-oracle headset.openvr.diagnostic.identity
    headset.openvr.diagnostic.exhaustion headset.openvr.diagnostic.texture-counters
    headset.openvr.diagnostic.discovery
    headset.openvr.assets.staged.shared headset.openvr.assets.staged.relative-rejected
    headset.openvr.assets.staged.standalone)
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
