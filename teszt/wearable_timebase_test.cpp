#include "wearable_timebase.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    bool check(bool condition, const char* name)
    {
        if (!condition)
            std::cerr << "FAILED: " << name << '\n';
        return condition;
    }

    bool same_stream(const signal_synth::wearable_stream_record& left, const signal_synth::wearable_stream_record& right)
    {
        if (left.fingerprint != right.fingerprint || left.samples.size() != right.samples.size() || left.packets.size() != right.packets.size() || left.channel_samples != right.channel_samples)
            return false;
        for (std::size_t i = 0; i < left.samples.size(); ++i)
        {
            const signal_synth::wearable_sample_mapping& a = left.samples[i];
            const signal_synth::wearable_sample_mapping& b = right.samples[i];
            if (a.sample_index != b.sample_index || a.packet_index != b.packet_index || a.latent_time_seconds != b.latent_time_seconds || a.ideal_device_time_seconds != b.ideal_device_time_seconds || a.reported_device_time_seconds != b.reported_device_time_seconds || a.received != b.received)
                return false;
        }
        return true;
    }
}

int main()
{
    bool ok = true;
    const unsigned int source_rate = 500u;
    const unsigned int source_count = 5001u;
    std::vector<double> source(source_count, 0.0);
    for (unsigned int i = 0; i < source_count; ++i)
        source[i] = 1.0 + 2.0 * i / source_rate;
    std::vector<signal_synth::wearable_source_channel> channels;
    channels.push_back(signal_synth::wearable_source_channel("linear", "a.u.", &source[0]));

    signal_synth::wearable_stream_config config;
    config.enabled = true;
    config.sample_rate_hz = 100u;
    config.clock_offset_ms = 12.5;
    config.clock_drift_ppm = 100.0;
    config.timestamp_jitter_ms = 1.0;
    config.packet_size_samples = 10u;
    config.packet_loss_probability = 0.25;
    config.packet_loss_burst_packets = 2u;
    config.seed = 77001u;

    signal_synth::wearable_stream_record one;
    signal_synth::wearable_stream_record chunked;
    ok &= check(signal_synth::render_wearable_stream(config, signal_synth::wearable_stream_ppg, 10.0, source_rate, source_count, channels, 1u, one), "render_single_sample_chunks");
    ok &= check(signal_synth::render_wearable_stream(config, signal_synth::wearable_stream_ppg, 10.0, source_rate, source_count, channels, 137u, chunked), "render_alternate_chunks");
    ok &= check(same_stream(one, chunked), "chunk_invariant_output");
    ok &= check(one.sample_count() == signal_synth::wearable_stream_sample_count(config, 10.0) && one.sample_count() == 1001u, "drift_adjusted_sample_count");

    bool mapping_ok = true;
    bool has_received = false;
    bool has_dropped = false;
    for (std::size_t i = 0; i < one.samples.size(); ++i)
    {
        const signal_synth::wearable_sample_mapping& sample = one.samples[i];
        mapping_ok = mapping_ok && sample.sample_index == i && sample.packet_index == i / config.packet_size_samples;
        mapping_ok = mapping_ok && std::fabs(one.channel_samples[0][i] - (1.0 + 2.0 * sample.latent_time_seconds)) < 1e-11;
        if (i) mapping_ok = mapping_ok && one.samples[i - 1u].reported_device_time_seconds < sample.reported_device_time_seconds;
        has_received = has_received || sample.received;
        has_dropped = has_dropped || !sample.received;
    }
    ok &= check(mapping_ok && has_received && has_dropped, "mapping_monotonicity_resampling_and_loss");
    bool packets_ok = true;
    for (std::size_t packet = 0; packet < one.packets.size(); ++packet)
    {
        const signal_synth::wearable_packet_annotation& item = one.packets[packet];
        for (unsigned int j = 0; j < item.sample_count; ++j)
            packets_ok = packets_ok && one.samples[static_cast<std::size_t>(item.first_sample_index + j)].received == !item.dropped;
    }
    ok &= check(packets_ok, "packet_sample_consistency");

    signal_synth::wearable_event_mapping event;
    ok &= check(signal_synth::map_wearable_event(one, 4.25, event) && event.present && std::fabs(event.latent_time_seconds - 4.25) < 1e-12 && event.sample_index < one.sample_count(), "event_mapping");

    signal_synth::wearable_timebase_config timebase;
    timebase.ecg = config;
    timebase.ppg = config;
    timebase.accelerometer = config;
    timebase.ecg.sample_rate_hz = 250u;
    timebase.ppg.sample_rate_hz = 100u;
    timebase.accelerometer.sample_rate_hz = 50u;
    ok &= check(signal_synth::validate_wearable_timebase_config(timebase, 10.0, source_rate, true, true), "valid_multirate_config");
    timebase.ppg.timestamp_jitter_ms = 5.0;
    ok &= check(!signal_synth::validate_wearable_timebase_config(timebase, 10.0, source_rate, true, true), "reject_nonmonotonic_jitter_bound");
    return ok ? 0 : 1;
}
