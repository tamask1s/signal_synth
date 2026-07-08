cmake_minimum_required(VERSION 3.10)
project(signal_synth LANGUAGES CXX)

option(SIGNAL_SYNTH_BUILD_TESTS "Build signal_synth verification tests" ON)
option(SIGNAL_SYNTH_BUILD_CLI "Build the signal-synth command-line tool" ON)
option(SIGNAL_SYNTH_INSTALL "Install the signal_synth library and CMake package" ON)

set(SIGNAL_SYNTH_PUBLIC_HEADERS
    src/signal_synth.h
    src/ecg_model.h
    src/clinical_ecg.h
    src/ecg_morphology.h
    src/ecg_scenario.h
    src/ecg_scenario_json.h
    src/ecg_export.h
    src/ecg_wfdb_export.h
    src/ecg_edf_bdf_export.h
    src/hrv_metrics.h
    src/hrv_scoring.h
    src/ecg_beat_classification.h
    src/ecg_compare.h
    src/detection_io.h
    src/synsigra_api.h
    src/challenge_package.h
    src/challenge_assembly.h
    src/scenario_authoring.h
    src/scenario_stress.h
    src/signal_quality.h
    src/ecg_pack.h
    src/ecg_pack_score.h)
list(APPEND SIGNAL_SYNTH_PUBLIC_HEADERS src/ppg_model.h)

add_library(
    signal_synth
    STATIC
    src/signal_synth.cpp
    src/ecg_model.cpp
    src/clinical_ecg.cpp
    src/ecg_morphology.cpp
    src/ecg_scenario.cpp
    src/ecg_scenario_json.cpp
    src/ecg_export.cpp
    src/ecg_wfdb_export.cpp
    src/ecg_edf_bdf_export.cpp
    src/hrv_metrics.cpp
    src/hrv_scoring.cpp
    src/ecg_beat_classification.cpp
    src/ecg_compare.cpp
    src/detection_io.cpp
    src/synsigra_api.cpp
    src/challenge_package.cpp
    src/challenge_assembly.cpp
    src/scenario_authoring.cpp
    src/scenario_stress.cpp
    src/signal_quality.cpp
    src/ecg_pack.cpp
    src/ecg_pack_score.cpp)
target_sources(signal_synth PRIVATE src/ppg_model.cpp)

add_library(signal_synth::signal_synth ALIAS signal_synth)

target_compile_features(signal_synth PUBLIC cxx_std_11)
set_target_properties(signal_synth PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(
    signal_synth
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>)

find_package(Git QUIET)
set(SIGNAL_SYNTH_GIT_COMMIT "unknown")
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --verify HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE SIGNAL_SYNTH_GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
endif()
if(NOT SIGNAL_SYNTH_GIT_COMMIT)
    set(SIGNAL_SYNTH_GIT_COMMIT "unknown")
endif()
target_compile_definitions(signal_synth PRIVATE SIGNAL_SYNTH_GIT_COMMIT="${SIGNAL_SYNTH_GIT_COMMIT}")

if(MSVC)
    target_compile_options(signal_synth PRIVATE /W4)
else()
    target_compile_options(signal_synth PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(SIGNAL_SYNTH_BUILD_CLI)
    add_executable(signal_synth_cli apps/signal_synth_cli/main.cpp)
    target_link_libraries(signal_synth_cli PRIVATE signal_synth::signal_synth)
    target_compile_features(signal_synth_cli PRIVATE cxx_std_11)
    set_target_properties(signal_synth_cli PROPERTIES CXX_EXTENSIONS OFF OUTPUT_NAME signal-synth)
    if(MSVC)
        target_compile_options(signal_synth_cli PRIVATE /W4)
    else()
        target_compile_options(signal_synth_cli PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if(SIGNAL_SYNTH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(teszt)
endif()

if(SIGNAL_SYNTH_INSTALL)
    include(CMakePackageConfigHelpers)
    set(SIGNAL_SYNTH_INSTALL_CMAKEDIR "lib/cmake/signal_synth")

    install(
        TARGETS signal_synth
        EXPORT signal_synth-targets
        ARCHIVE DESTINATION lib)
    if(SIGNAL_SYNTH_BUILD_CLI)
        install(TARGETS signal_synth_cli RUNTIME DESTINATION bin)
    endif()
    install(FILES ${SIGNAL_SYNTH_PUBLIC_HEADERS} DESTINATION include/signal_synth)
    install(
        EXPORT signal_synth-targets
        FILE signal_synth-targets.cmake
        NAMESPACE signal_synth::
        DESTINATION ${SIGNAL_SYNTH_INSTALL_CMAKEDIR})

    configure_package_config_file(
        cmake/signal_synth-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/signal_synth-config.cmake
        INSTALL_DESTINATION ${SIGNAL_SYNTH_INSTALL_CMAKEDIR}
        NO_SET_AND_CHECK_MACRO
        NO_CHECK_REQUIRED_COMPONENTS_MACRO)
    install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/signal_synth-config.cmake
        DESTINATION ${SIGNAL_SYNTH_INSTALL_CMAKEDIR})
endif()
