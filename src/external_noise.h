#pragma once

#include "clinical_ecg.h"

#include <string>
#include <vector>

namespace signal_synth
{
    enum external_noise_redistribution
    {
        external_noise_local_only = 0,
        external_noise_rendered_output = 1,
        external_noise_source_and_output = 2
    };

    const char* external_noise_redistribution_name(external_noise_redistribution value);
    bool external_noise_redistribution_from_name(const std::string& name, external_noise_redistribution& value);

    struct external_noise_asset_manifest
    {
        external_noise_asset_manifest();

        std::string id;
        std::string source_uri;
        std::string license;
        std::string content_sha256;
        unsigned int sample_rate_hz;
        std::vector<std::string> channels;
        external_noise_redistribution redistribution;
    };

    struct external_noise_interval_config
    {
        external_noise_interval_config();

        std::string asset_id;
        std::string asset_channel;
        double start_seconds;
        double duration_seconds;
        double asset_offset_seconds;
        double target_snr_db;
        double taper_seconds;
        double clip_limit_mv;
        bool ecg_leads[clinical_lead_count];
    };

    struct external_noise_config
    {
        std::vector<external_noise_asset_manifest> assets;
        std::vector<external_noise_interval_config> intervals;
    };

    struct external_noise_asset_input
    {
        std::string id;
        std::string csv_content;
    };

    struct external_noise_channel_truth
    {
        external_noise_channel_truth();

        unsigned int lead;
        double clean_rms_mv;
        double added_noise_rms_mv;
        double target_snr_db;
        double achieved_snr_db;
        unsigned long long clipping_count;
    };

    struct external_noise_interval_truth
    {
        external_noise_interval_truth();

        std::string asset_id;
        std::string asset_channel;
        std::string source_uri;
        std::string license;
        std::string content_sha256;
        external_noise_redistribution redistribution;
        double start_seconds;
        double end_seconds;
        double asset_offset_seconds;
        double taper_seconds;
        std::vector<external_noise_channel_truth> channels;
    };

    struct external_noise_result
    {
        external_noise_result();

        bool release_allowed;
        std::vector<external_noise_interval_truth> intervals;
    };

    bool validate_external_noise_config(const external_noise_config& config, double duration_seconds, unsigned int output_sample_rate_hz, std::vector<std::string>& messages);
    bool apply_external_noise(const external_noise_config& config, const std::vector<external_noise_asset_input>& assets, unsigned int sample_rate_hz, std::vector<std::vector<double> >& ecg_leads, external_noise_result& result, std::vector<std::string>& messages);
    std::string external_noise_truth_json(const external_noise_result& result);
}
