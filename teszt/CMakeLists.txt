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
    ecg_scenario_test
    ecg_scenario_test.cpp
    ../src/ecg_scenario.cpp
    ../src/clinical_ecg.cpp)

target_include_directories(ecg_scenario_test PRIVATE ../src)
target_compile_features(ecg_scenario_test PRIVATE cxx_std_11)

add_test(NAME signal_synth_test COMMAND signal_synth_test)
set_tests_properties(
    signal_synth_test
    PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_test(NAME ecg_model_test COMMAND ecg_model_test)
add_test(NAME clinical_ecg_test COMMAND clinical_ecg_test)
add_test(NAME ecg_scenario_test COMMAND ecg_scenario_test)

if(MSVC)
    target_compile_options(signal_synth_test PRIVATE /W4)
    target_compile_options(ecg_model_test PRIVATE /W4)
    target_compile_options(clinical_ecg_test PRIVATE /W4)
    target_compile_options(ecg_scenario_test PRIVATE /W4)
else()
    target_compile_options(signal_synth_test PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(ecg_model_test PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(clinical_ecg_test PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(ecg_scenario_test PRIVATE -Wall -Wextra -Wpedantic)
endif()
