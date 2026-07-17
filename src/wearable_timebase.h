#pragma once

#include <string>
#include <vector>

namespace signal_synth
{
    class clinical_ecg_record;
    class ppg_record;

    enum wearable_stream_kind
    {
        wearable_stream_ecg = 0,
        wearable_stream_ppg = 1,
        wearable_stream_accelerometer = 2
    };

    const char* wearable_stream_kind_name(wearable_stream_kind kind);

    struct wearable_stream_config
    {
        wearable_stream_config();

        bool enabled;
        unsigned int sample_rate_hz;
        double clock_offset_ms;
        double clock_drift_ppm;
        double timestamp_jitter_ms;
        unsigned int packet_size_samples;
        double packet_loss_probability;
        unsigned int packet_loss_burst_packets;
        unsigned long long seed;
    };

    struct wearable_timebase_config
    {
        wearable_timebase_config();

        wearable_stream_config ecg;
        wearable_stream_config ppg;
        wearable_stream_config accelerometer;
    };

    struct wearable_source_channel
    {
        wearable_source_channel();
        wearable_source_channel(const std::string& channel_name, const std::string& channel_unit, const double* channel_samples);

        std::string name;
        std::string unit;
        const double* samples;
    };

    struct wearable_sample_mapping
    {
        wearable_sample_mapping();

        unsigned long long sample_index;
        unsigned long long packet_index;
        double latent_time_seconds;
        double ideal_device_time_seconds;
        double reported_device_time_seconds;
        bool received;
    };

    struct wearable_packet_annotation
    {
        wearable_packet_annotation();

        unsigned long long packet_index;
        unsigned long long first_sample_index;
        unsigned int sample_count;
        double first_latent_time_seconds;
        double last_latent_time_seconds;
        double first_reported_device_time_seconds;
        double last_reported_device_time_seconds;
        bool dropped;
    };

    struct wearable_stream_record
    {
        wearable_stream_record();

        wearable_stream_kind kind;
        wearable_stream_config config;
        std::vector<std::string> channel_names;
        std::vector<std::string> channel_units;
        std::vector<std::vector<double> > channel_samples;
        std::vector<wearable_sample_mapping> samples;
        std::vector<wearable_packet_annotation> packets;
        std::string fingerprint;

        unsigned int channel_count() const;
        unsigned int sample_count() const;
        unsigned int received_sample_count() const;
        const double* channel_data(unsigned int channel) const;
    };

    struct wearable_event_mapping
    {
        wearable_event_mapping();

        bool present;
        double latent_time_seconds;
        unsigned long long sample_index;
        double reported_device_time_seconds;
        bool received;
    };

    struct wearable_alignment_annotation
    {
        wearable_alignment_annotation();

        unsigned long long ecg_beat_index;
        bool intentionally_missing;
        wearable_event_mapping ecg_r;
        wearable_event_mapping ppg_onset;
        wearable_event_mapping ppg_peak;
        bool has_physiological_onset_delay;
        double physiological_onset_delay_seconds;
        bool has_physiological_peak_delay;
        double physiological_peak_delay_seconds;
        bool has_observed_onset_device_delta;
        double observed_onset_device_delta_seconds;
        double onset_observed_minus_physiological_seconds;
        bool has_observed_peak_device_delta;
        double observed_peak_device_delta_seconds;
        double peak_observed_minus_physiological_seconds;
    };

    struct wearable_timebase_record
    {
        wearable_timebase_record();

        double duration_seconds;
        unsigned int latent_sample_rate_hz;
        std::vector<wearable_stream_record> streams;
        std::vector<wearable_alignment_annotation> alignments;
        std::string fingerprint;

        const wearable_stream_record* stream(wearable_stream_kind kind) const;
    };

    bool wearable_stream_config_is_default(const wearable_stream_config& config);
    unsigned int wearable_stream_sample_count(const wearable_stream_config& config, double duration_seconds);
    bool validate_wearable_timebase_config(const wearable_timebase_config& config, double duration_seconds, unsigned int latent_sample_rate_hz, bool ppg_available, bool accelerometer_available);
    bool render_wearable_stream(const wearable_stream_config& config, wearable_stream_kind kind, double duration_seconds, unsigned int source_sample_rate_hz, unsigned int source_sample_count, const std::vector<wearable_source_channel>& source_channels, unsigned int chunk_size_samples, wearable_stream_record& output);
    bool map_wearable_event(const wearable_stream_record& stream, double latent_time_seconds, wearable_event_mapping& output);
    bool build_wearable_alignment_truth(const clinical_ecg_record& ecg, const ppg_record& ppg, const wearable_stream_record& ecg_stream, const wearable_stream_record& ppg_stream, std::vector<wearable_alignment_annotation>& output);
    std::string wearable_timebase_record_fingerprint(const wearable_timebase_record& record);
}
