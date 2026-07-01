cmake_minimum_required(VERSION 3.10)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(signal_synth_test LANGUAGES CXX)
    set(SIGNAL_SYNTH_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SIGNAL_SYNTH_INSTALL ON CACHE BOOL "" FORCE)
    add_subdirectory(.. signal_synth_core)
    enable_testing()
endif()

function(signal_synth_add_test target source test_id)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE signal_synth::signal_synth)
    target_compile_features(${target} PRIVATE cxx_std_11)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME ${test_id} COMMAND ${target})
endfunction()

signal_synth_add_test(signal_synth_test signal_synth_test.cpp TEST-SYNTH-001)
set_tests_properties(
    TEST-SYNTH-001
    PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")
signal_synth_add_test(ecg_model_test ecg_model_test.cpp TEST-ECG-MODEL-001)
signal_synth_add_test(clinical_ecg_test clinical_ecg_test.cpp TEST-ECG-PHANTOM-001)
signal_synth_add_test(ecg_morphology_test ecg_morphology_test.cpp TEST-ECG-MORPH-001)
signal_synth_add_test(ecg_scenario_test ecg_scenario_test.cpp TEST-ECG-SCENARIO-001)
signal_synth_add_test(ecg_scenario_json_test ecg_scenario_json_test.cpp TEST-ECG-JSON-001)
signal_synth_add_test(ecg_export_test ecg_export_test.cpp TEST-ECG-EXPORT-001)
signal_synth_add_test(ppg_model_test ppg_model_test.cpp TEST-PPG-001)

add_test(
    NAME TEST-BUILD-001
    COMMAND
        ${CMAKE_COMMAND}
        -DSIGNAL_SYNTH_BUILD_DIR=${CMAKE_BINARY_DIR}
        -DSIGNAL_SYNTH_SMOKE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/package_smoke
        -DSIGNAL_SYNTH_SMOKE_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/package_smoke_work
        -DSIGNAL_SYNTH_BUILD_CONFIG=$<CONFIGURATION>
        -DSIGNAL_SYNTH_EXPECT_CLI=${SIGNAL_SYNTH_BUILD_CLI}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/package_smoke/run.cmake)

if(TARGET signal_synth_cli)
    add_test(
        NAME TEST-CLI-001
        COMMAND
            ${CMAKE_COMMAND}
            -DSIGNAL_SYNTH_CLI=$<TARGET_FILE:signal_synth_cli>
            -DSIGNAL_SYNTH_EXAMPLE=${CMAKE_CURRENT_SOURCE_DIR}/../examples/scenarios/ecg_clean.json
            -DSIGNAL_SYNTH_PPG_EXAMPLE=${CMAKE_CURRENT_SOURCE_DIR}/../examples/scenarios/ecg_ppg_clean.json
            -DSIGNAL_SYNTH_CLI_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_test_work
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_test.cmake)
endif()
