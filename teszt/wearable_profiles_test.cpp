#include "clinical_ecg.h"
#include "ppg_model.h"
#include "wearable_profiles.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    bool check(bool condition, const char* name)
    {
        if (!condition) std::cerr << "FAILED: " << name << '\n';
        return condition;
    }
}

int main()
{
    bool ok = true;
    const unsigned int source_rate = 500u;
    const unsigned int source_count = 2000u;
    std::vector<std::vector<double> > lead_samples(signal_synth::clinical_lead_count, std::vector<double>(source_count, 0.0));
    std::vector<signal_synth::wearable_source_channel> leads;
    for (unsigned int lead = 0; lead < signal_synth::clinical_lead_count; ++lead)
    {
        for (unsigned int sample = 0; sample < source_count; ++sample) lead_samples[lead][sample] = (lead + 1u) * 0.05 * std::sin(2.0 * 3.14159265358979323846 * sample / 100.0);
        leads.push_back(signal_synth::wearable_source_channel("lead", "mV", &lead_samples[lead][0]));
    }

    signal_synth::wearable_stream_config stream_config;
    stream_config.enabled = true;
    stream_config.sample_rate_hz = 200u;
    stream_config.packet_size_samples = 20u;
    stream_config.seed = 84001u;
    signal_synth::wearable_stream_record clinical, holter, replay, patch, watch;
    ok &= check(signal_synth::wearable_ecg_profile_count() == 4u
        && signal_synth::render_wearable_ecg_profile("clinical_12lead_reference_v1", stream_config, 4.0, source_rate, source_count, leads, 128u, clinical)
        && clinical.channel_count() == 12u && clinical.profile_id == "clinical_12lead_reference_v1", "clinical_profile");
    ok &= check(signal_synth::render_wearable_ecg_profile("holter_lead_ii_v1", stream_config, 4.0, source_rate, source_count, leads, 128u, holter)
        && signal_synth::render_wearable_ecg_profile("holter_lead_ii_v1", stream_config, 4.0, source_rate, source_count, leads, 128u, replay)
        && holter.channel_count() == 1u && holter.channel_names[0] == "ecg_holter_ii"
        && holter.channel_samples == replay.channel_samples && holter.fingerprint == replay.fingerprint
        && holter.channel_clipping_counts.size() == 1u, "deterministic_holter_profile");
    ok &= check(signal_synth::render_wearable_ecg_profile("patch_left_chest_vector_v1", stream_config, 4.0, source_rate, source_count, leads, 128u, patch)
        && signal_synth::render_wearable_ecg_profile("watch_lead_i_v1", stream_config, 4.0, source_rate, source_count, leads, 128u, watch)
        && patch.channel_samples != watch.channel_samples && patch.fingerprint != watch.fingerprint, "distinct_device_vectors");
    ok &= check(!signal_synth::validate_wearable_ecg_profile("unknown", 200u)
        && !signal_synth::validate_wearable_ecg_profile("watch_lead_i_v1", 50u), "profile_validation");

    signal_synth::ppg_config finger, wrist, ear;
    ok &= check(signal_synth::ppg_site_profile_count() == 4u
        && signal_synth::configure_ppg_site_profile("finger_transmissive_v1", finger)
        && signal_synth::configure_ppg_site_profile("wrist_reflectance_v1", wrist)
        && signal_synth::configure_ppg_site_profile("ear_reflectance_v1", ear)
        && finger.optical.profile_id == "finger_transmissive_v1"
        && wrist.pulse_delay_ms > finger.pulse_delay_ms && ear.pulse_delay_ms < finger.pulse_delay_ms
        && wrist.rise_time_ms > ear.rise_time_ms && wrist.optical.red.motion_sensitivity > ear.optical.red.motion_sensitivity
        && !signal_synth::configure_ppg_site_profile("unknown", ear), "ppg_site_profiles");

    if (ok) std::cout << "wearable profile tests passed\n";
    return ok ? 0 : 1;
}
