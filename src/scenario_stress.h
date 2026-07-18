#pragma once

#include "ecg_scenario_json.h"
#include "signal_quality.h"

#include <string>
#include <vector>

namespace signal_synth
{
    struct scenario_parameter_draw
    {
        std::string parameter;
        double minimum;
        double maximum;
        double unit_draw;
        double value;
    };

    bool resolve_scenario_controls(const ecg_scenario_document& input, ecg_scenario_document& resolved, std::vector<scenario_parameter_draw>& draws, std::vector<std::string>& messages);
    double physiology_respiration_phase_radians(const physiology_coupling_config& config);
    double physiology_respiration_value(const physiology_coupling_config& config, double time_seconds);
    bool apply_physiology_coupling(const physiology_coupling_config& config, const ppg_record& ppg, unsigned int sampling_rate_hz, signal_quality_waveforms& waveforms);
    std::string scenario_parameter_draws_json(const ecg_scenario_document& input, const ecg_scenario_document& resolved, const std::vector<scenario_parameter_draw>& draws);
}
