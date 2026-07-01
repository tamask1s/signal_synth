if(NOT DEFINED SIGNAL_SYNTH_BUILD_DIR OR NOT DEFINED SIGNAL_SYNTH_SMOKE_SOURCE_DIR OR NOT DEFINED SIGNAL_SYNTH_SMOKE_WORK_DIR)
    message(FATAL_ERROR "Package smoke test paths are not configured")
endif()

file(REMOVE_RECURSE "${SIGNAL_SYNTH_SMOKE_WORK_DIR}")
file(MAKE_DIRECTORY "${SIGNAL_SYNTH_SMOKE_WORK_DIR}")
set(prefix "${SIGNAL_SYNTH_SMOKE_WORK_DIR}/prefix")
set(consumer_build "${SIGNAL_SYNTH_SMOKE_WORK_DIR}/build")

set(config_args)
if(DEFINED SIGNAL_SYNTH_BUILD_CONFIG AND NOT "${SIGNAL_SYNTH_BUILD_CONFIG}" STREQUAL "")
    list(APPEND config_args -DCMAKE_INSTALL_CONFIG_NAME=${SIGNAL_SYNTH_BUILD_CONFIG})
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -DCMAKE_INSTALL_PREFIX=${prefix} ${config_args} -P "${SIGNAL_SYNTH_BUILD_DIR}/cmake_install.cmake"
    RESULT_VARIABLE install_result)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "signal_synth package installation failed")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -H${SIGNAL_SYNTH_SMOKE_SOURCE_DIR} -B${consumer_build} -DCMAKE_PREFIX_PATH=${prefix}
    RESULT_VARIABLE configure_result)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR "Installed-package consumer configuration failed")
endif()

set(build_args --build "${consumer_build}")
if(DEFINED SIGNAL_SYNTH_BUILD_CONFIG AND NOT "${SIGNAL_SYNTH_BUILD_CONFIG}" STREQUAL "")
    list(APPEND build_args --config "${SIGNAL_SYNTH_BUILD_CONFIG}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" ${build_args} RESULT_VARIABLE build_result)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "Installed-package consumer build failed")
endif()

set(smoke_executable "${consumer_build}/signal_synth_package_smoke")
if(WIN32 AND DEFINED SIGNAL_SYNTH_BUILD_CONFIG AND NOT "${SIGNAL_SYNTH_BUILD_CONFIG}" STREQUAL "")
    set(smoke_executable "${consumer_build}/${SIGNAL_SYNTH_BUILD_CONFIG}/signal_synth_package_smoke.exe")
endif()
execute_process(COMMAND "${smoke_executable}" RESULT_VARIABLE run_result)
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "Installed-package consumer returned ${run_result}")
endif()

if(SIGNAL_SYNTH_EXPECT_CLI)
    set(installed_cli "${prefix}/bin/signal-synth")
    if(WIN32)
        set(installed_cli "${prefix}/bin/signal-synth.exe")
    endif()
    if(NOT EXISTS "${installed_cli}")
        message(FATAL_ERROR "Installed signal-synth executable is missing")
    endif()
endif()
