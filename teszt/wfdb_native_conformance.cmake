if(NOT DEFINED SIGNAL_SYNTH_CLI OR NOT DEFINED SIGNAL_SYNTH_ECG_EXAMPLE OR NOT DEFINED SIGNAL_SYNTH_PPG_EXAMPLE OR NOT DEFINED SIGNAL_SYNTH_WFDB_NATIVE_WORK_DIR)
    message(FATAL_ERROR "WFDB native conformance test is missing required inputs")
endif()

find_program(SIGNAL_SYNTH_RDSAMP_EXECUTABLE NAMES rdsamp)
find_program(SIGNAL_SYNTH_RDANN_EXECUTABLE NAMES rdann)
if(NOT SIGNAL_SYNTH_RDSAMP_EXECUTABLE OR NOT SIGNAL_SYNTH_RDANN_EXECUTABLE)
    message(STATUS "Skipping native WFDB conformance: rdsamp/rdann not found")
    return()
endif()

function(signal_synth_wfdb_case case_id scenario_path)
    set(case_dir "${SIGNAL_SYNTH_WFDB_NATIVE_WORK_DIR}/${case_id}")
    file(REMOVE_RECURSE "${case_dir}")
    execute_process(
        COMMAND "${SIGNAL_SYNTH_CLI}" render "${scenario_path}" --out "${case_dir}"
        RESULT_VARIABLE render_result
        OUTPUT_VARIABLE render_output
        ERROR_VARIABLE render_error)
    if(NOT render_result EQUAL 0 OR NOT render_error STREQUAL "" OR NOT EXISTS "${case_dir}/synsigra.hea" OR NOT EXISTS "${case_dir}/synsigra.dat" OR NOT EXISTS "${case_dir}/synsigra.atr")
        message(FATAL_ERROR "WFDB conformance render failed for ${case_id}: ${render_error}")
    endif()

    execute_process(
        COMMAND "${SIGNAL_SYNTH_RDSAMP_EXECUTABLE}" -r synsigra -n 8
        WORKING_DIRECTORY "${case_dir}"
        RESULT_VARIABLE rdsamp_result
        OUTPUT_VARIABLE rdsamp_output
        ERROR_VARIABLE rdsamp_error)
    if(NOT rdsamp_result EQUAL 0 OR rdsamp_output STREQUAL "")
        message(FATAL_ERROR "rdsamp failed for ${case_id}: ${rdsamp_error}")
    endif()

    execute_process(
        COMMAND "${SIGNAL_SYNTH_RDANN_EXECUTABLE}" -r synsigra -a atr
        WORKING_DIRECTORY "${case_dir}"
        RESULT_VARIABLE rdann_result
        OUTPUT_VARIABLE rdann_output
        ERROR_VARIABLE rdann_error)
    if(NOT rdann_result EQUAL 0 OR rdann_output STREQUAL "")
        message(FATAL_ERROR "rdann failed for ${case_id}: ${rdann_error}")
    endif()
endfunction()

signal_synth_wfdb_case(ecg_only "${SIGNAL_SYNTH_ECG_EXAMPLE}")
signal_synth_wfdb_case(ecg_ppg "${SIGNAL_SYNTH_PPG_EXAMPLE}")
