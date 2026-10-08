cmake_minimum_required(VERSION 3.21)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(openvr_assets_staged C)
  enable_testing()
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "The staged OpenVR asset fixture requires Linux")
endif()

get_filename_component(lovr_source "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(staged_root "${CMAKE_CURRENT_BINARY_DIR}/artifacts/$<CONFIG>")
set(staged_library_directory "${staged_root}/library")
set(staged_executable_directory "${staged_root}/standalone")
set(staged_host_directory "${staged_root}/host")
set(staged_unrelated_directory "${staged_root}/unrelated")
set(asset_source "${lovr_source}/etc/openvr")
set(asset_files
  "${asset_source}/actions.json"
  "${asset_source}/bindings_vive_controller.json"
  "${asset_source}/bindings_knuckles.json"
  "${asset_source}/bindings_oculus_touch.json"
  "${asset_source}/bindings_frame_controller.json"
)

add_library(headset_openvr_assets_staged_library SHARED library.c
  "${lovr_source}/src/modules/headset/openvr_assets.c")
set_target_properties(headset_openvr_assets_staged_library PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${staged_library_directory}" C_VISIBILITY_PRESET hidden)
add_executable(headset_openvr_assets_staged_host host.c)
set_target_properties(headset_openvr_assets_staged_host PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${staged_host_directory}")
add_executable(headset_openvr_assets_staged_standalone host.c
  "${lovr_source}/src/modules/headset/openvr_assets.c")
set_target_properties(headset_openvr_assets_staged_standalone PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${staged_executable_directory}")

foreach(target IN ITEMS headset_openvr_assets_staged_library headset_openvr_assets_staged_host
    headset_openvr_assets_staged_standalone)
  target_include_directories(${target} PRIVATE "${lovr_source}/src/modules" "${CMAKE_CURRENT_SOURCE_DIR}/..")
  target_link_libraries(${target} PRIVATE ${CMAKE_DL_LIBS})
  if(COMMAND configure_native_test)
    configure_native_test(${target})
  else()
    set_target_properties(${target} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
  endif()
endforeach()

target_compile_definitions(headset_openvr_assets_staged_host PRIVATE STAGED_SHARED
  STAGED_LIBRARY="$<TARGET_FILE:headset_openvr_assets_staged_library>"
  STAGED_LIBRARY_NAME="$<TARGET_FILE_NAME:headset_openvr_assets_staged_library>"
  STAGED_LIBRARY_DIRECTORY="${staged_library_directory}"
  STAGED_ASSET_DIRECTORY="${staged_library_directory}/lovr-openvr"
  SOURCE_ASSET_DIRECTORY="${asset_source}"
  STAGED_UNRELATED_DIRECTORY="${staged_unrelated_directory}")
target_compile_definitions(headset_openvr_assets_staged_standalone PRIVATE
  STAGED_ASSET_DIRECTORY="${staged_executable_directory}/lovr-openvr"
  SOURCE_ASSET_DIRECTORY="${asset_source}"
  STAGED_UNRELATED_DIRECTORY="${staged_unrelated_directory}")

foreach(mode IN ITEMS library standalone)
  if(mode STREQUAL "library")
    set(destination "${staged_library_directory}")
  else()
    set(destination "${staged_executable_directory}")
  endif()
  add_custom_target(headset_openvr_assets_stage_${mode}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${destination}/lovr-openvr" "${staged_unrelated_directory}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${asset_files} "${destination}/lovr-openvr"
    DEPENDS ${asset_files}
    VERBATIM)
  add_dependencies(headset_openvr_assets_staged_${mode} headset_openvr_assets_stage_${mode})
endforeach()
add_dependencies(headset_openvr_assets_staged_host headset_openvr_assets_staged_library)

foreach(case IN ITEMS shared relative-rejected standalone)
  if(case STREQUAL "standalone")
    set(executable headset_openvr_assets_staged_standalone)
  else()
    set(executable headset_openvr_assets_staged_host)
  endif()
  add_test(NAME headset.openvr.assets.staged.${case}
    COMMAND ${executable} --case openvr.assets.staged.${case})
  set_tests_properties(headset.openvr.assets.staged.${case} PROPERTIES LABELS "headset;openvr-assets-staged")
endforeach()
