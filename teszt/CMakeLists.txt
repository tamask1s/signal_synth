cmake_minimum_required(VERSION 3.10)
project(signal_synth_test LANGUAGES CXX)

enable_testing()

add_executable(
    signal_synth_test
    signal_synth_test.cpp
    ../src/signal_synth.cpp)

target_include_directories(signal_synth_test PRIVATE ../src)
target_compile_features(signal_synth_test PRIVATE cxx_std_11)

add_executable(
    ecg_model_test
    ecg_model_test.cpp
    ../src/ecg_model.cpp)

target_include_directories(ecg_model_test PRIVATE ../src)
target_compile_features(ecg_model_test PRIVATE cxx_std_11)

add_executable(
    clinical_ecg_test
    clinical_ecg_test.cpp
    ../src/clinical_ecg.cpp)

target_include_directories(clinical_ecg_test PRIVATE ../src)
target_compile_features(clinical_ecg_test PRIVATE cxx_std_11)

add_executable(
    ecg_morphology_test
    ecg_morphology_test.cpp
    ../src/ecg_morphology.cpp
    ../src/clinical_ecg.cpp)

target_include_directories(ecg_morphology_test PRIVATE ../src)
target_compile_features(ecg_morphology_test PRIVATE cxx_std_11)

add_executable(
    ecg_scenario_test
    ecg_scenario_test.cpp
    ../src/ecg_scenario.cpp
    ../src/ecg_morphology.cpp
    ../src/clinical_ecg.cpp)

target_include_directories(ecg_scenario_test PRIVATE ../src)
target_compile_features(ecg_scenario_test PRIVATE cxx_std_11)

add_test(NAME TEST-SYNTH-001 COMMAND signal_synth_test)
set_tests_properties(
    TEST-SYNTH-001
    PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_test(NAME TEST-ECG-MODEL-001 COMMAND ecg_model_test)
add_test(NAME TEST-ECG-PHANTOM-001 COMMAND clinical_ecg_test)
add_test(NAME TEST-ECG-MORPH-001 COMMAND ecg_morphology_test)
add_test(NAME TEST-ECG-SCENARIO-001 COMMAND ecg_scenario_test)

if(MSVC)
    target_compile_options(signal_synth_test PRIVATE /W4)
    target_compile_options(ecg_model_test PRIVATE /W4)
    target_compile_options(clinical_ecg_test PRIVATE /W4)
    target_compile_options(ecg_morphology_test PRIVATE /W4)
    target_compile_options(ecg_scenario_test PRIVATE /W4)
else()
    target_compile_options(signal_synth_test PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(ecg_model_test PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(clinical_ecg_test PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(ecg_morphology_test PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(ecg_scenario_test PRIVATE -Wall -Wextra -Wpedantic)
endif()
