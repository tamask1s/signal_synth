#pragma once

#include "wearable_timebase.h"

#include <string>
#include <vector>

namespace signal_synth
{
    struct wearable_ecg_profile_info
    {
        wearable_ecg_profile_info();

        std::string profile_id;
        std::string channel_name;
        std::string placement;
        bool preserve_standard_12_lead;
        double lead_weights[12];
        double highpass_hz;
        double lowpass_hz;
        double gain;
        double minimum_output_mv;
        double maximum_output_mv;
        unsigned int quantization_bits;
    };

    unsigned int wearable_ecg_profile_count();
    const wearable_ecg_profile_info* wearable_ecg_profile(unsigned int index);
    const wearable_ecg_profile_info* find_wearable_ecg_profile(const char* profile_id);
    bool validate_wearable_ecg_profile(const char* profile_id, unsigned int sample_rate_hz);
    bool render_wearable_ecg_profile(const char* profile_id, const wearable_stream_config& config, double duration_seconds, unsigned int source_sample_rate_hz, unsigned int source_sample_count, const std::vector<wearable_source_channel>& standard_leads, unsigned int chunk_size_samples, wearable_stream_record& output);
}
