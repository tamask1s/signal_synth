#include "scenario_stress.h"

#include "clinical_ecg.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace
{
    const double pi = 3.141592653589793238462643383279502884;

    unsigned long long mix64(unsigned long long value)
    {
        value ^= value >> 33;
        value *= 0xff51afd7ed558ccdULL;
        value ^= value >> 33;
        value *= 0xc4ceb9fe1a85ec53ULL;
        value ^= value >> 33;
        return value;
    }

    unsigned long long string_hash(const std::string& value)
    {
        unsigned long long hash = 1469598103934665603ULL;
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            hash ^= static_cast<unsigned char>(value[i]);
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    double unit(unsigned long long seed, const std::string& parameter)
    {
        return static_cast<double>(mix64(seed ^ string_hash(parameter)) >> 11) * (1.0 / 9007199254740992.0);
    }

    bool morphology_parameter(const std::string& parameter, signal_synth::ecg_morphology_control& control)
    {
        const std::string prefix = "ecg.morphology.";
        return parameter.size() > prefix.size()
            && parameter.compare(0, prefix.size(), prefix) == 0
            && signal_synth::ecg_morphology_control_from_name(parameter.c_str() + prefix.size(), control);
    }

    double signed_unit(unsigned long long seed, unsigned long long index)
    {
        return 2.0 * static_cast<double>(mix64(seed ^ mix64(index)) >> 11) * (1.0 / 9007199254740992.0) - 1.0;
    }

    std::string json_string(const std::string& value)
    {
        std::ostringstream output;
        output << '"';
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            if (value[i] == '"' || value[i] == '\\')
                output << '\\';
            output << value[i];
        }
        output << '"';
        return output.str();
    }

    double activity_envelope(const signal_synth::physiology_coupling_config& config, double time)
    {
        if (config.activity_intensity <= 0.0 || time < config.activity_start_seconds || time >= config.activity_start_seconds + config.activity_duration_seconds)
            return 0.0;
        const double position = time - config.activity_start_seconds;
        const double remaining = config.activity_duration_seconds - position;
        const double taper = std::min(2.0, 0.2 * config.activity_duration_seconds);
        const double edge = taper > 0.0 ? std::min(1.0, std::min(position, remaining) / taper) : 1.0;
        return config.activity_intensity * (0.5 - 0.5 * std::cos(pi * edge));
    }

    double respiration_lead_gain(unsigned int lead)
    {
        const double lead_i = 0.30;
        const double lead_ii = 1.0;
        const double limb[6] = {lead_i, lead_ii, lead_ii - lead_i, -(lead_i + lead_ii) / 2.0, lead_i - lead_ii / 2.0, lead_ii - lead_i / 2.0};
        if (lead < 6)
            return limb[lead];
        const double chest[6] = {-0.45, -0.25, 0.05, 0.40, 0.70, 0.85};
        return chest[lead - 6];
    }
}

namespace signal_synth
{
    bool resolve_scenario_controls(const ecg_scenario_document& input, ecg_scenario_document& resolved, std::vector<scenario_parameter_draw>& draws, std::vector<std::string>& messages)
    {
        ecg_scenario_document fresh = input;
        std::vector<scenario_parameter_draw> fresh_draws;
        std::vector<scenario_randomization_envelope> envelopes = input.randomization.envelopes;
        std::sort(envelopes.begin(), envelopes.end(), [](const scenario_randomization_envelope& left, const scenario_randomization_envelope& right) { return left.parameter < right.parameter; });
        if (input.randomization.enabled)
        {
            for (std::size_t i = 0; i < envelopes.size(); ++i)
            {
                scenario_parameter_draw draw;
                draw.parameter = envelopes[i].parameter;
                draw.minimum = envelopes[i].minimum;
                draw.maximum = envelopes[i].maximum;
                draw.unit_draw = unit(input.randomization.seed, draw.parameter);
                draw.value = draw.minimum + (draw.maximum - draw.minimum) * draw.unit_draw;
                if (draw.parameter == "ecg.heart_rate_bpm")
                    fresh.ecg.set_heart_rate_bpm(draw.value);
                else if (draw.parameter == "ecg.rr_variability_seconds")
                    fresh.ecg.set_rr_variability_seconds(draw.value);
                else if (draw.parameter == "ppg.pulse_delay_ms")
                    fresh.ppg.pulse_delay_ms = draw.value;
                else if (draw.parameter == "ppg.amplitude_au")
                    fresh.ppg.amplitude_au = draw.value;
                else if (draw.parameter == "hrv.target_sdnn_seconds")
                    fresh.hrv.target_sdnn_seconds = draw.value;
                else if (draw.parameter == "hrv.lf_hf_ratio")
                    fresh.hrv.lf_hf_ratio = draw.value;
                else if (draw.parameter == "physiology.activity_intensity")
                    fresh.physiology.activity_intensity = draw.value;
                else
                {
                    ecg_morphology_control control = ecg_morphology_control_count;
                    if (!morphology_parameter(draw.parameter, control) || !fresh.ecg.set_morphology_control(control, draw.value))
                    {
                        messages.assign(1, "unsupported randomization parameter: " + draw.parameter);
                        return false;
                    }
                }
                fresh_draws.push_back(draw);
            }
        }
        if (fresh.physiology.respiratory_rr_amplitude_seconds > 0.0)
        {
            const double minimum_sdnn = fresh.physiology.respiratory_rr_amplitude_seconds / std::sqrt(2.0);
            if (fresh.hrv.enabled)
            {
                fresh.hrv.target_sdnn_seconds = std::max(fresh.hrv.target_sdnn_seconds, minimum_sdnn);
                fresh.hrv.respiratory_frequency_hz = fresh.physiology.respiration_frequency_hz;
                fresh.hrv.respiratory_amplitude_seconds = fresh.physiology.respiratory_rr_amplitude_seconds;
                fresh.ecg.set_heart_rate_bpm(fresh.hrv.target_mean_hr_bpm);
                fresh.ecg.set_rr_variability_seconds(fresh.hrv.target_sdnn_seconds);
                fresh.ecg.set_minimum_rr_seconds(fresh.hrv.minimum_rr_seconds);
                fresh.ecg.set_maximum_rr_seconds(fresh.hrv.maximum_rr_seconds);
                fresh.ecg.set_hrv_modulation(fresh.hrv.lf_hf_ratio, fresh.hrv.lf_center_hz, fresh.hrv.lf_bandwidth_hz, fresh.hrv.hf_center_hz, fresh.hrv.hf_bandwidth_hz, fresh.hrv.respiratory_frequency_hz, fresh.hrv.respiratory_amplitude_seconds);
            }
            else
            {
                fresh.ecg.set_rr_variability_seconds(std::max(fresh.ecg.rr_variability_seconds(), minimum_sdnn));
                fresh.ecg.set_hrv_modulation(1.0, 0.10, 0.04, 0.25, 0.12, fresh.physiology.respiration_frequency_hz, fresh.physiology.respiratory_rr_amplitude_seconds);
            }
        }
        else if (fresh.hrv.enabled)
        {
            fresh.ecg.set_heart_rate_bpm(fresh.hrv.target_mean_hr_bpm);
            fresh.ecg.set_rr_variability_seconds(fresh.hrv.target_sdnn_seconds);
            fresh.ecg.set_hrv_modulation(fresh.hrv.lf_hf_ratio, fresh.hrv.lf_center_hz, fresh.hrv.lf_bandwidth_hz, fresh.hrv.hf_center_hz, fresh.hrv.hf_bandwidth_hz, fresh.hrv.respiratory_frequency_hz, fresh.hrv.respiratory_amplitude_seconds);
        }
        if (!fresh.ecg.set_activity_modulation(fresh.physiology.activity_start_seconds, fresh.physiology.activity_duration_seconds, fresh.physiology.activity_intensity))
        {
            messages.assign(1, "invalid activity modulation");
            return false;
        }
        fresh.ecg.set_retain_source_channels(fresh.output.retain_source_channels);
        ecg_scenario_json_result validation;
        if (!write_ecg_scenario_json(fresh, validation))
        {
            messages.clear();
            for (std::size_t i = 0; i < validation.messages.size(); ++i)
                messages.push_back(validation.messages[i].path + ": " + validation.messages[i].message);
            return false;
        }
        resolved = fresh;
        draws = fresh_draws;
        messages.clear();
        return true;
    }

    bool apply_physiology_coupling(const physiology_coupling_config& config, double ppg_baseline_au, unsigned int sampling_rate_hz, signal_quality_waveforms& waveforms)
    {
        if (!sampling_rate_hz || waveforms.ecg_leads.size() != clinical_lead_count)
            return false;
        const std::size_t sample_count = waveforms.ecg_leads[0].size();
        for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
            if (waveforms.ecg_leads[lead].size() != sample_count)
                return false;
        if (!waveforms.ppg.empty() && waveforms.ppg.size() != sample_count)
            return false;
        const double phase = 2.0 * pi * unit(config.seed, "respiration_phase");
        for (std::size_t sample = 0; sample < sample_count; ++sample)
        {
            const double time = static_cast<double>(sample) / sampling_rate_hz;
            const double respiration = std::sin(2.0 * pi * config.respiration_frequency_hz * time + phase);
            const double activity = activity_envelope(config, time);
            for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
            {
                const double activity_noise = activity * (0.08 * signed_unit(config.seed + lead, sample) + 0.06 * std::sin(2.0 * pi * (1.7 + 0.11 * lead) * time));
                waveforms.ecg_leads[lead][sample] += respiration_lead_gain(lead) * config.ecg_baseline_amplitude_mv * respiration + activity_noise;
            }
            if (!waveforms.ppg.empty())
            {
                const double centered = waveforms.ppg[sample] - ppg_baseline_au;
                const double modulation = std::max(0.0, 1.0 + config.ppg_amplitude_modulation_ratio * respiration - 0.55 * activity);
                waveforms.ppg[sample] = ppg_baseline_au + centered * modulation + activity * 0.05 * signed_unit(config.seed ^ 0x5050474143544956ULL, sample);
            }
        }
        return true;
    }

    std::string scenario_parameter_draws_json(const ecg_scenario_document& input, const ecg_scenario_document& resolved, const std::vector<scenario_parameter_draw>& draws)
    {
        ecg_scenario_json_result input_identity;
        ecg_scenario_json_result resolved_identity;
        write_ecg_scenario_json(input, input_identity);
        write_ecg_scenario_json(resolved, resolved_identity);
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10)
               << "{\"schema_version\":1,\"randomization_version\":\"synsigra_randomization_v1\",\"enabled\":" << (input.randomization.enabled ? "true" : "false")
               << ",\"seed\":" << input.randomization.seed
               << ",\"input_document_fingerprint\":" << json_string(input_identity.document_fingerprint)
               << ",\"resolved_document_fingerprint\":" << json_string(resolved_identity.document_fingerprint)
               << ",\"draws\":[";
        for (std::size_t i = 0; i < draws.size(); ++i)
            output << (i ? "," : "") << "{\"parameter\":" << json_string(draws[i].parameter)
                   << ",\"minimum\":" << draws[i].minimum << ",\"maximum\":" << draws[i].maximum
                   << ",\"unit_draw\":" << draws[i].unit_draw << ",\"value\":" << draws[i].value << '}';
        output << "]}";
        return output.str();
    }
}
