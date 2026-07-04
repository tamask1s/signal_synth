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
signal_synth_add_test(ecg_infarction_test ecg_infarction_test.cpp TEST-ECG-INFARCTION-001)
signal_synth_add_test(ecg_repolarization_test ecg_repolarization_test.cpp TEST-ECG-REPOLARIZATION-001)
signal_synth_add_test(ecg_morphology_quality_test ecg_morphology_quality_test.cpp TEST-ECG-MORPH-QUALITY-001)
signal_synth_add_test(ecg_conduction_test ecg_conduction_test.cpp TEST-ECG-CONDUCTION-001)
signal_synth_add_test(ecg_episode_test ecg_episode_test.cpp TEST-ECG-EPISODE-001)
signal_synth_add_test(ecg_scenario_json_test ecg_scenario_json_test.cpp TEST-ECG-JSON-001)
signal_synth_add_test(ecg_export_test ecg_export_test.cpp TEST-ECG-EXPORT-001)
signal_synth_add_test(ecg_wfdb_export_test ecg_wfdb_export_test.cpp TEST-WFDB-EXPORT-001)
signal_synth_add_test(ecg_edf_bdf_export_test ecg_edf_bdf_export_test.cpp TEST-EDF-BDF-EXPORT-001)
signal_synth_add_test(hrv_metrics_test hrv_metrics_test.cpp TEST-HRV-METRICS-001)
signal_synth_add_test(hrv_scoring_test hrv_scoring_test.cpp TEST-HRV-SCORING-001)
signal_synth_add_test(ppg_model_test ppg_model_test.cpp TEST-PPG-001)
signal_synth_add_test(signal_quality_test signal_quality_test.cpp TEST-SIGNAL-QUALITY-001)
signal_synth_add_test(ecg_pack_test ecg_pack_test.cpp TEST-ECG-PACK-001)
signal_synth_add_test(ecg_pack_score_test ecg_pack_score_test.cpp TEST-PACK-SCORE-001)
signal_synth_add_test(ecg_compare_test ecg_compare_test.cpp TEST-COMPARE-001)
signal_synth_add_test(detection_io_test detection_io_test.cpp TEST-DETECTION-IO-001)
signal_synth_add_test(synsigra_api_test synsigra_api_test.cpp TEST-FACADE-001)
signal_synth_add_test(challenge_package_test challenge_package_test.cpp TEST-CHALLENGE-PACKAGE-001)

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
            -DSIGNAL_SYNTH_PACK_EXAMPLE=${CMAKE_CURRENT_SOURCE_DIR}/../examples/packs/r_peak_stress_v1.json
            -DSIGNAL_SYNTH_CLI_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_test_work
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_test.cmake)
endif()

find_program(SIGNAL_SYNTH_PYTHON_EXECUTABLE NAMES python3 python)
if(TARGET signal_synth_cli AND SIGNAL_SYNTH_PYTHON_EXECUTABLE)
    add_test(
        NAME TEST-PYTHON-SCORING-001
        COMMAND ${SIGNAL_SYNTH_PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/python_scoring_test.py)
    set_tests_properties(
        TEST-PYTHON-SCORING-001
        PROPERTIES
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/../python;SIGNAL_SYNTH_CLI=$<TARGET_FILE:signal_synth_cli>;SIGNAL_SYNTH_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/..;SIGNAL_SYNTH_PY_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/python_scoring_work")
endif()
