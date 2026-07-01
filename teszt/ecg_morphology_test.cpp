#include "../src/ecg_morphology.h"
#include "../src/clinical_ecg.h"

#include <cmath>
#include <iostream>

namespace
{
    bool check(bool condition, const char* name)
    {
        if (condition)
        {
            std::cout << "PASS " << name << '\n';
            return true;
        }
        std::cerr << "FAIL " << name << '\n';
        return false;
    }
}

int main()
{
    bool ok = true;
    signal_synth::clinical_ecg_record record;
    signal_synth::clinical_ecg_generator generator;
    ok &= check(generator.generate(5000, record), "default_record_generation");

    signal_synth::ecg_morphology_report report;
    ok &= check(signal_synth::measure_ecg_morphology(record, report), "morphology_measurement");
    ok &= check(report.entry_count() == record.beat_count() * record.lead_count(), "one_entry_per_beat_and_lead");
    bool finite = true;
    for (unsigned int index = 0; index < report.entry_count(); ++index)
    {
        const signal_synth::ecg_lead_morphology& entry = report.entries()[index];
        finite &= entry.lead_index < record.lead_count() && std::isfinite(entry.p_amplitude_mv) && std::isfinite(entry.p_duration_seconds) && std::isfinite(entry.q_amplitude_mv) && std::isfinite(entry.q_duration_seconds) && std::isfinite(entry.r_amplitude_mv) && std::isfinite(entry.s_amplitude_mv) && std::isfinite(entry.qrs_peak_to_peak_mv) && std::isfinite(entry.qrs_rms_mv) && std::isfinite(entry.st_j_mv) && std::isfinite(entry.st_j60_mv) && std::isfinite(entry.t_amplitude_mv) && std::isfinite(entry.t_duration_seconds);
    }
    ok &= check(finite, "all_morphology_metrics_are_finite");
    ok &= check(report.find(0, signal_synth::clinical_lead_ii) && !report.find(record.beat_count(), 0) && !report.find(0, signal_synth::clinical_lead_count), "indexed_morphology_lookup");
    ok &= check(signal_synth::ecg_lead_is_in_region(signal_synth::clinical_lead_ii, signal_synth::ecg_region_inferior) && signal_synth::ecg_lead_is_in_region(signal_synth::clinical_lead_v2, signal_synth::ecg_region_anterior) && signal_synth::ecg_lead_is_in_region(signal_synth::clinical_lead_v6, signal_synth::ecg_region_lateral) && !signal_synth::ecg_lead_is_in_region(signal_synth::clinical_lead_i, signal_synth::ecg_region_inferior), "lead_region_membership");

    signal_synth::ecg_morphology_report copied = report;
    ok &= check(copied.entry_count() == report.entry_count() && copied.entries()[0].qrs_peak_to_peak_mv == report.entries()[0].qrs_peak_to_peak_mv, "morphology_report_copy");
    signal_synth::clinical_ecg_record empty;
    ok &= check(!signal_synth::measure_ecg_morphology(empty, copied) && copied.entry_count() == report.entry_count(), "failed_measurement_is_transactional");

    std::cout << (ok ? "All ECG morphology tests passed.\n" : "ECG morphology test failure.\n");
    return ok ? 0 : 1;
}
