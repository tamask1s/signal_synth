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
signal_synth_add_test(hrv_pack_test hrv_pack_test.cpp TEST-HRV-PACK-001)
set_tests_properties(TEST-HRV-PACK-001 PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")
signal_synth_add_test(ecg_beat_classification_test ecg_beat_classification_test.cpp TEST-ECG-BEAT-CLASS-001)
signal_synth_add_test(ppg_model_test ppg_model_test.cpp TEST-PPG-001)
signal_synth_add_test(ppg_physiology_test ppg_physiology_test.cpp TEST-PPG-PHYSIOLOGY-001)
signal_synth_add_test(signal_quality_test signal_quality_test.cpp TEST-SIGNAL-QUALITY-001)
signal_synth_add_test(ecg_pack_test ecg_pack_test.cpp TEST-ECG-PACK-001)
signal_synth_add_test(ecg_pack_score_test ecg_pack_score_test.cpp TEST-PACK-SCORE-001)
signal_synth_add_test(ecg_compare_test ecg_compare_test.cpp TEST-COMPARE-001)
signal_synth_add_test(detection_io_test detection_io_test.cpp TEST-DETECTION-IO-001)
signal_synth_add_test(synsigra_api_test synsigra_api_test.cpp TEST-FACADE-001)
signal_synth_add_test(challenge_package_test challenge_package_test.cpp TEST-CHALLENGE-PACKAGE-001)
signal_synth_add_test(scenario_authoring_test scenario_authoring_test.cpp TEST-AUTHORING-001)
signal_synth_add_test(scenario_stress_test scenario_stress_test.cpp TEST-SCENARIO-STRESS-001)

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
    add_test(
        NAME TEST-WFDB-NATIVE-001
        COMMAND
            ${CMAKE_COMMAND}
            -DSIGNAL_SYNTH_CLI=$<TARGET_FILE:signal_synth_cli>
            -DSIGNAL_SYNTH_ECG_EXAMPLE=${CMAKE_CURRENT_SOURCE_DIR}/../examples/scenarios/ecg_clean.json
            -DSIGNAL_SYNTH_PPG_EXAMPLE=${CMAKE_CURRENT_SOURCE_DIR}/../examples/scenarios/ecg_ppg_clean.json
            -DSIGNAL_SYNTH_WFDB_NATIVE_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/wfdb_native_work
            -P ${CMAKE_CURRENT_SOURCE_DIR}/wfdb_native_conformance.cmake)
endif()

find_program(SIGNAL_SYNTH_PYTHON_EXECUTABLE NAMES python3 python)
if(TARGET signal_synth_cli AND SIGNAL_SYNTH_PYTHON_EXECUTABLE)
    add_test(
        NAME TEST-AUTHORING-CLI-001
        COMMAND ${SIGNAL_SYNTH_PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/authoring_cli_test.py)
    set_tests_properties(
        TEST-AUTHORING-CLI-001
        PROPERTIES
            ENVIRONMENT "SIGNAL_SYNTH_CLI=$<TARGET_FILE:signal_synth_cli>;SIGNAL_SYNTH_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/..")
    add_test(
        NAME TEST-VERIFICATION-CATALOG-001
        COMMAND ${SIGNAL_SYNTH_PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/verification_catalog_test.py)
    set_tests_properties(
        TEST-VERIFICATION-CATALOG-001
        PROPERTIES
            ENVIRONMENT "SIGNAL_SYNTH_CLI=$<TARGET_FILE:signal_synth_cli>;SIGNAL_SYNTH_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/..")
    add_test(
        NAME TEST-PYTHON-SCORING-001
        COMMAND ${SIGNAL_SYNTH_PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/python_scoring_test.py)
    set_tests_properties(
        TEST-PYTHON-SCORING-001
        PROPERTIES
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/../python;SIGNAL_SYNTH_CLI=$<TARGET_FILE:signal_synth_cli>;SIGNAL_SYNTH_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/..;SIGNAL_SYNTH_PY_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/python_scoring_work")
    add_test(
        NAME TEST-EDF-BDF-NATIVE-001
        COMMAND
            ${SIGNAL_SYNTH_PYTHON_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/edf_bdf_native_conformance.py
            --cli $<TARGET_FILE:signal_synth_cli>
            --ecg-example ${CMAKE_CURRENT_SOURCE_DIR}/../examples/scenarios/ecg_clean.json
            --ppg-example ${CMAKE_CURRENT_SOURCE_DIR}/../examples/scenarios/ecg_ppg_clean.json
            --work-dir ${CMAKE_CURRENT_BINARY_DIR}/edf_bdf_native_work)
endif()
