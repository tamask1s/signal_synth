#include "wearable_timebase.h"

#include "clinical_ecg.h"
#include "ppg_model.h"
#include "wearable_profiles.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>

namespace
{
    bool finite_value(double value)
    {
        return std::isfinite(value);
    }

    unsigned long long mix64(unsigned long long value)
    {
        value ^= value >> 30;
        value *= 0xbf58476d1ce4e5b9ULL;
        value ^= value >> 27;
        value *= 0x94d049bb133111ebULL;
        value ^= value >> 31;
        return value;
    }

    double unit_from_index(unsigned long long seed, unsigned long long index, unsigned long long stream)
    {
        const unsigned long long bits = mix64(seed ^ (0x9e3779b97f4a7c15ULL * (index + 1u)) ^ stream);
        return static_cast<double>(bits >> 11) * (1.0 / 9007199254740992.0);
    }

    unsigned long long stream_tag(signal_synth::wearable_stream_kind kind)
    {
        switch (kind)
        {
        case signal_synth::wearable_stream_ecg: return 0x4543475f54494d45ULL;
        case signal_synth::wearable_stream_ppg: return 0x5050475f54494d45ULL;
        case signal_synth::wearable_stream_accelerometer: return 0x4143435f54494d45ULL;
        }
        return 0;
    }

    bool loss_burst_starts(const signal_synth::wearable_stream_config& config, signal_synth::wearable_stream_kind kind, unsigned long long packet_index)
    {
        return config.packet_loss_probability > 0.0
            && unit_from_index(config.seed, packet_index, stream_tag(kind) ^ 0x4c4f53535f535452ULL) < config.packet_loss_probability;
    }

    bool packet_is_dropped(const signal_synth::wearable_stream_config& config, signal_synth::wearable_stream_kind kind, unsigned long long packet_index)
    {
        const unsigned long long first = packet_index + 1u > config.packet_loss_burst_packets ? packet_index + 1u - config.packet_loss_burst_packets : 0u;
        for (unsigned long long start = first; start <= packet_index; ++start)
            if (loss_burst_starts(config, kind, start))
                return true;
        return false;
    }

    double interpolate(const double* samples, unsigned int sample_count, unsigned int sample_rate_hz, double time_seconds)
    {
        if (!samples || sample_count == 0 || sample_rate_hz == 0)
            return 0.0;
        const double position = time_seconds * sample_rate_hz;
        if (position <= 0.0)
            return samples[0];
        const unsigned long long first = static_cast<unsigned long long>(std::floor(position));
        if (first >= sample_count - 1u)
            return samples[sample_count - 1u];
        const double fraction = position - static_cast<double>(first);
        return samples[first] + fraction * (samples[first + 1u] - samples[first]);
    }

    class fnv64
    {
    public:
        fnv64() : value_(14695981039346656037ULL) {}

        void add_byte(unsigned char value)
        {
            value_ ^= value;
            value_ *= 1099511628211ULL;
        }

        void add_u64(unsigned long long value)
        {
            for (unsigned int i = 0; i < 8; ++i)
                add_byte(static_cast<unsigned char>((value >> (i * 8u)) & 0xffu));
        }

        void add_bool(bool value)
        {
            add_byte(value ? 1u : 0u);
        }

        void add_double(double value)
        {
            unsigned long long bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            add_u64(bits);
        }

        void add_string(const std::string& value)
        {
            add_u64(static_cast<unsigned long long>(value.size()));
            for (std::size_t i = 0; i < value.size(); ++i)
                add_byte(static_cast<unsigned char>(value[i]));
        }

        std::string text() const
        {
            std::ostringstream output;
            output << "fnv1a64:" << std::hex << std::setw(16) << std::setfill('0') << value_;
            return output.str();
        }

    private:
        unsigned long long value_;
    };

    void add_config(fnv64& hash, const signal_synth::wearable_stream_config& config)
    {
        hash.add_bool(config.enabled);
        hash.add_u64(config.sample_rate_hz);
        hash.add_double(config.clock_offset_ms);
        hash.add_double(config.clock_drift_ppm);
        hash.add_double(config.timestamp_jitter_ms);
        hash.add_u64(config.packet_size_samples);
        hash.add_double(config.packet_loss_probability);
        hash.add_u64(config.packet_loss_burst_packets);
        hash.add_u64(config.seed);
    }

    std::string compute_stream_fingerprint(const signal_synth::wearable_stream_record& stream)
    {
        fnv64 hash;
        hash.add_string("synsigra_wearable_stream_v3");
        hash.add_u64(static_cast<unsigned long long>(stream.kind));
        add_config(hash, stream.config);
        hash.add_string(stream.profile_id);
        for (std::size_t channel = 0; channel < stream.channel_names.size(); ++channel)
        {
            hash.add_string(stream.channel_names[channel]);
            hash.add_string(stream.channel_units[channel]);
            hash.add_u64(channel < stream.channel_clipping_counts.size() ? stream.channel_clipping_counts[channel] : 0u);
        }
        for (std::size_t sample = 0; sample < stream.samples.size(); ++sample)
        {
            const signal_synth::wearable_sample_mapping& mapping = stream.samples[sample];
            hash.add_u64(mapping.sample_index);
            hash.add_u64(mapping.packet_index);
            hash.add_double(mapping.latent_time_seconds);
            hash.add_double(mapping.ideal_device_time_seconds);
            hash.add_double(mapping.reported_device_time_seconds);
            hash.add_bool(mapping.received);
            for (std::size_t channel = 0; channel < stream.channel_samples.size(); ++channel)
                hash.add_double(stream.channel_samples[channel][sample]);
        }
        return hash.text();
    }

    const signal_synth::ppg_annotation* measured_peak(const signal_synth::ppg_record& ppg, unsigned long long beat_index)
    {
        const signal_synth::ppg_annotation* annotations = ppg.annotations();
        for (unsigned int i = 0; annotations && i < ppg.annotation_count(); ++i)
            if (annotations[i].ecg_beat_index == beat_index
                && annotations[i].kind == signal_synth::ppg_systolic_peak
                && annotations[i].source == signal_synth::ppg_fiducial_measurement)
                return annotations + i;
        return 0;
    }
}

namespace signal_synth
{
    const char* wearable_stream_kind_name(wearable_stream_kind kind)
    {
        switch (kind)
        {
        case wearable_stream_ecg: return "ecg";
        case wearable_stream_ppg: return "ppg";
        case wearable_stream_accelerometer: return "accelerometer";
        }
        return "unknown";
    }

    wearable_stream_config::wearable_stream_config()
        : enabled(false), sample_rate_hz(0), clock_offset_ms(0.0), clock_drift_ppm(0.0), timestamp_jitter_ms(0.0), packet_size_samples(1), packet_loss_probability(0.0), packet_loss_burst_packets(1), seed(0)
    {
    }

    wearable_timebase_config::wearable_timebase_config() : ecg_profile_id("clinical_12lead_reference_v1"), ecg(), ppg(), accelerometer()
    {
    }

    wearable_source_channel::wearable_source_channel() : name(), unit(), samples(0)
    {
    }

    wearable_source_channel::wearable_source_channel(const std::string& channel_name, const std::string& channel_unit, const double* channel_samples)
        : name(channel_name), unit(channel_unit), samples(channel_samples)
    {
    }

    wearable_sample_mapping::wearable_sample_mapping()
        : sample_index(0), packet_index(0), latent_time_seconds(0.0), ideal_device_time_seconds(0.0), reported_device_time_seconds(0.0), received(false)
    {
    }

    wearable_packet_annotation::wearable_packet_annotation()
        : packet_index(0), first_sample_index(0), sample_count(0), first_latent_time_seconds(0.0), last_latent_time_seconds(0.0), first_reported_device_time_seconds(0.0), last_reported_device_time_seconds(0.0), dropped(false)
    {
    }

    wearable_stream_record::wearable_stream_record()
        : kind(wearable_stream_ecg), config(), profile_id(), channel_names(), channel_units(), channel_samples(), channel_clipping_counts(), samples(), packets(), fingerprint()
    {
    }

    unsigned int wearable_stream_record::channel_count() const
    {
        return static_cast<unsigned int>(channel_samples.size());
    }

    unsigned int wearable_stream_record::sample_count() const
    {
        return static_cast<unsigned int>(samples.size());
    }

    unsigned int wearable_stream_record::received_sample_count() const
    {
        unsigned int count = 0;
        for (std::size_t i = 0; i < samples.size(); ++i)
            if (samples[i].received)
                ++count;
        return count;
    }

    const double* wearable_stream_record::channel_data(unsigned int channel) const
    {
        return channel < channel_samples.size() && !channel_samples[channel].empty() ? &channel_samples[channel][0] : 0;
    }

    wearable_event_mapping::wearable_event_mapping()
        : present(false), latent_time_seconds(0.0), sample_index(0), reported_device_time_seconds(0.0), received(false)
    {
    }

    wearable_alignment_annotation::wearable_alignment_annotation()
        : ecg_beat_index(0), intentionally_missing(false), ecg_r(), ppg_onset(), ppg_peak(), has_physiological_onset_delay(false), physiological_onset_delay_seconds(0.0), has_physiological_peak_delay(false), physiological_peak_delay_seconds(0.0), has_observed_onset_device_delta(false), observed_onset_device_delta_seconds(0.0), onset_observed_minus_physiological_seconds(0.0), has_observed_peak_device_delta(false), observed_peak_device_delta_seconds(0.0), peak_observed_minus_physiological_seconds(0.0)
    {
    }

    wearable_timebase_record::wearable_timebase_record()
        : duration_seconds(0.0), latent_sample_rate_hz(0), streams(), alignments(), fingerprint()
    {
    }

    const wearable_stream_record* wearable_timebase_record::stream(wearable_stream_kind kind) const
    {
        for (std::size_t i = 0; i < streams.size(); ++i)
            if (streams[i].kind == kind)
                return &streams[i];
        return 0;
    }

    bool wearable_stream_config_is_default(const wearable_stream_config& config)
    {
        const wearable_stream_config defaults;
        return config.enabled == defaults.enabled
            && config.sample_rate_hz == defaults.sample_rate_hz
            && config.clock_offset_ms == defaults.clock_offset_ms
            && config.clock_drift_ppm == defaults.clock_drift_ppm
            && config.timestamp_jitter_ms == defaults.timestamp_jitter_ms
            && config.packet_size_samples == defaults.packet_size_samples
            && config.packet_loss_probability == defaults.packet_loss_probability
            && config.packet_loss_burst_packets == defaults.packet_loss_burst_packets
            && config.seed == defaults.seed;
    }

    bool wearable_timebase_config_is_default(const wearable_timebase_config& config)
    {
        const wearable_timebase_config defaults;
        return config.ecg_profile_id == defaults.ecg_profile_id
            && wearable_stream_config_is_default(config.ecg)
            && wearable_stream_config_is_default(config.ppg)
            && wearable_stream_config_is_default(config.accelerometer);
    }

    unsigned int wearable_stream_sample_count(const wearable_stream_config& config, double duration_seconds)
    {
        if (!config.enabled || config.sample_rate_hz == 0 || !finite_value(duration_seconds) || duration_seconds <= 0.0 || !finite_value(config.clock_drift_ppm))
            return 0;
        const double clock_scale = 1.0 + config.clock_drift_ppm * 1e-6;
        const double count = std::ceil(duration_seconds * config.sample_rate_hz * clock_scale - 1e-12);
        if (!finite_value(count) || count < 1.0 || count > static_cast<double>(std::numeric_limits<unsigned int>::max()))
            return 0;
        return static_cast<unsigned int>(count);
    }

    bool validate_wearable_timebase_config(const wearable_timebase_config& config, double duration_seconds, unsigned int latent_sample_rate_hz, bool ppg_available, bool accelerometer_available)
    {
        const wearable_stream_config* streams[] = {&config.ecg, &config.ppg, &config.accelerometer};
        if (!config.ecg.enabled || !latent_sample_rate_hz || !finite_value(duration_seconds) || duration_seconds <= 0.0)
            return false;
        if (!validate_wearable_ecg_profile(config.ecg_profile_id.c_str(), config.ecg.sample_rate_hz))
            return false;
        if (config.ppg.enabled && !ppg_available)
            return false;
        if (config.accelerometer.enabled && !accelerometer_available)
            return false;
        for (std::size_t i = 0; i < sizeof(streams) / sizeof(streams[0]); ++i)
        {
            const wearable_stream_config& stream = *streams[i];
            if (!stream.enabled)
            {
                if (!wearable_stream_config_is_default(stream))
                    return false;
                continue;
            }
            if (stream.sample_rate_hz == 0 || stream.sample_rate_hz > latent_sample_rate_hz
                || !finite_value(stream.clock_offset_ms) || std::fabs(stream.clock_offset_ms) > 86400000.0
                || !finite_value(stream.clock_drift_ppm) || std::fabs(stream.clock_drift_ppm) > 100000.0
                || !finite_value(stream.timestamp_jitter_ms) || stream.timestamp_jitter_ms < 0.0
                || stream.timestamp_jitter_ms > 450.0 / stream.sample_rate_hz
                || stream.packet_size_samples == 0 || stream.packet_size_samples > 1000000u
                || !finite_value(stream.packet_loss_probability) || stream.packet_loss_probability < 0.0 || stream.packet_loss_probability > 1.0
                || stream.packet_loss_burst_packets == 0 || stream.packet_loss_burst_packets > 1000u
                || wearable_stream_sample_count(stream, duration_seconds) == 0)
                return false;
        }
        return true;
    }

    bool render_wearable_stream(const wearable_stream_config& config, wearable_stream_kind kind, double duration_seconds, unsigned int source_sample_rate_hz, unsigned int source_sample_count, const std::vector<wearable_source_channel>& source_channels, unsigned int chunk_size_samples, wearable_stream_record& output)
    {
        wearable_stream_record fresh;
        if (!config.enabled || source_channels.empty() || !source_sample_rate_hz || !source_sample_count || config.sample_rate_hz > source_sample_rate_hz)
            return false;
        const unsigned int count = wearable_stream_sample_count(config, duration_seconds);
        if (!count || static_cast<double>(source_sample_count) / source_sample_rate_hz + 1e-12 < duration_seconds)
            return false;
        for (std::size_t channel = 0; channel < source_channels.size(); ++channel)
            if (!source_channels[channel].samples || source_channels[channel].name.empty() || source_channels[channel].unit.empty())
                return false;
        if (!chunk_size_samples)
            chunk_size_samples = 4096u;

        try
        {
            fresh.kind = kind;
            fresh.config = config;
            fresh.samples.resize(count);
            fresh.channel_samples.assign(source_channels.size(), std::vector<double>(count, 0.0));
            for (std::size_t channel = 0; channel < source_channels.size(); ++channel)
            {
                fresh.channel_names.push_back(source_channels[channel].name);
                fresh.channel_units.push_back(source_channels[channel].unit);
            }

            const unsigned int packet_count = 1u + (count - 1u) / config.packet_size_samples;
            std::vector<bool> dropped(packet_count, false);
            for (unsigned int packet = 0; packet < packet_count; ++packet)
                dropped[packet] = packet_is_dropped(config, kind, packet);

            const double clock_scale = 1.0 + config.clock_drift_ppm * 1e-6;
            const double offset_seconds = config.clock_offset_ms * 0.001;
            const double jitter_seconds = config.timestamp_jitter_ms * 0.001;
            unsigned int first = 0;
            while (first < count)
            {
                const unsigned int past = first + std::min(chunk_size_samples, count - first);
                for (unsigned int sample = first; sample < past; ++sample)
                {
                    wearable_sample_mapping& mapping = fresh.samples[sample];
                    mapping.sample_index = sample;
                    mapping.packet_index = sample / config.packet_size_samples;
                    mapping.latent_time_seconds = static_cast<double>(sample) / (config.sample_rate_hz * clock_scale);
                    mapping.ideal_device_time_seconds = offset_seconds + static_cast<double>(sample) / config.sample_rate_hz;
                    const double jitter = jitter_seconds * (2.0 * unit_from_index(config.seed, sample, stream_tag(kind) ^ 0x4a49545445525f31ULL) - 1.0);
                    mapping.reported_device_time_seconds = mapping.ideal_device_time_seconds + jitter;
                    mapping.received = !dropped[static_cast<std::size_t>(mapping.packet_index)];
                    for (std::size_t channel = 0; channel < source_channels.size(); ++channel)
                        fresh.channel_samples[channel][sample] = interpolate(source_channels[channel].samples, source_sample_count, source_sample_rate_hz, mapping.latent_time_seconds);
                }
                first = past;
            }

            fresh.packets.reserve(packet_count);
            for (unsigned int packet = 0; packet < packet_count; ++packet)
            {
                wearable_packet_annotation annotation;
                annotation.packet_index = packet;
                annotation.first_sample_index = static_cast<unsigned long long>(packet) * config.packet_size_samples;
                annotation.sample_count = std::min(config.packet_size_samples, count - static_cast<unsigned int>(annotation.first_sample_index));
                const unsigned int first = static_cast<unsigned int>(annotation.first_sample_index);
                const unsigned int last = first + annotation.sample_count - 1u;
                annotation.first_latent_time_seconds = fresh.samples[first].latent_time_seconds;
                annotation.last_latent_time_seconds = fresh.samples[last].latent_time_seconds;
                annotation.first_reported_device_time_seconds = fresh.samples[first].reported_device_time_seconds;
                annotation.last_reported_device_time_seconds = fresh.samples[last].reported_device_time_seconds;
                annotation.dropped = dropped[packet];
                fresh.packets.push_back(annotation);
            }
            fresh.channel_clipping_counts.assign(fresh.channel_samples.size(), 0u);
            fresh.fingerprint = compute_stream_fingerprint(fresh);
        }
        catch (...)
        {
            return false;
        }
        output = fresh;
        return true;
    }

    bool map_wearable_event(const wearable_stream_record& stream, double latent_time_seconds, wearable_event_mapping& output)
    {
        wearable_event_mapping fresh;
        if (!finite_value(latent_time_seconds) || latent_time_seconds < 0.0 || stream.samples.empty())
            return false;
        const double clock_scale = 1.0 + stream.config.clock_drift_ppm * 1e-6;
        const double raw = latent_time_seconds * stream.config.sample_rate_hz * clock_scale;
        unsigned long long index = raw <= 0.0 ? 0u : static_cast<unsigned long long>(std::floor(raw + 0.5));
        if (index >= stream.samples.size())
            index = stream.samples.size() - 1u;
        fresh.present = true;
        fresh.latent_time_seconds = latent_time_seconds;
        fresh.sample_index = index;
        fresh.reported_device_time_seconds = stream.samples[static_cast<std::size_t>(index)].reported_device_time_seconds;
        fresh.received = stream.samples[static_cast<std::size_t>(index)].received;
        output = fresh;
        return true;
    }

    bool build_wearable_alignment_truth(const clinical_ecg_record& ecg, const ppg_record& ppg, const wearable_stream_record& ecg_stream, const wearable_stream_record& ppg_stream, std::vector<wearable_alignment_annotation>& output)
    {
        std::vector<wearable_alignment_annotation> fresh;
        if (ecg_stream.kind != wearable_stream_ecg || ppg_stream.kind != wearable_stream_ppg || !ecg.beats() || !ppg.pulses())
            return false;
        try
        {
            fresh.reserve(ppg.pulse_count());
            for (unsigned int i = 0; i < ppg.pulse_count(); ++i)
            {
                const ppg_pulse_annotation& pulse = ppg.pulses()[i];
                wearable_alignment_annotation alignment;
                alignment.ecg_beat_index = pulse.ecg_beat_index;
                alignment.intentionally_missing = pulse.intentionally_missing || pulse.state == ppg_pulse_missing;
                if (!map_wearable_event(ecg_stream, pulse.ecg_r_time_seconds, alignment.ecg_r))
                    return false;
                alignment.has_physiological_onset_delay = true;
                alignment.physiological_onset_delay_seconds = pulse.pulse_delay_seconds;
                if (pulse.generated && pulse.state != ppg_pulse_out_of_record && !alignment.intentionally_missing)
                {
                    if (!map_wearable_event(ppg_stream, pulse.expected_onset_time_seconds, alignment.ppg_onset))
                        return false;
                    alignment.has_observed_onset_device_delta = true;
                    alignment.observed_onset_device_delta_seconds = alignment.ppg_onset.reported_device_time_seconds - alignment.ecg_r.reported_device_time_seconds;
                    alignment.onset_observed_minus_physiological_seconds = alignment.observed_onset_device_delta_seconds - alignment.physiological_onset_delay_seconds;
                }
                const ppg_annotation* peak = measured_peak(ppg, pulse.ecg_beat_index);
                if (peak)
                {
                    alignment.has_physiological_peak_delay = true;
                    alignment.physiological_peak_delay_seconds = peak->time_seconds - pulse.ecg_r_time_seconds;
                    if (!map_wearable_event(ppg_stream, peak->time_seconds, alignment.ppg_peak))
                        return false;
                    alignment.has_observed_peak_device_delta = true;
                    alignment.observed_peak_device_delta_seconds = alignment.ppg_peak.reported_device_time_seconds - alignment.ecg_r.reported_device_time_seconds;
                    alignment.peak_observed_minus_physiological_seconds = alignment.observed_peak_device_delta_seconds - alignment.physiological_peak_delay_seconds;
                }
                fresh.push_back(alignment);
            }
        }
        catch (...)
        {
            return false;
        }
        output.swap(fresh);
        return true;
    }

    std::string wearable_stream_record_fingerprint(const wearable_stream_record& record)
    {
        return compute_stream_fingerprint(record);
    }

    std::string wearable_timebase_record_fingerprint(const wearable_timebase_record& record)
    {
        fnv64 hash;
        hash.add_string("synsigra_wearable_timebase_v3");
        hash.add_double(record.duration_seconds);
        hash.add_u64(record.latent_sample_rate_hz);
        for (std::size_t stream = 0; stream < record.streams.size(); ++stream)
            hash.add_string(record.streams[stream].fingerprint);
        for (std::size_t i = 0; i < record.alignments.size(); ++i)
        {
            const wearable_alignment_annotation& item = record.alignments[i];
            hash.add_u64(item.ecg_beat_index);
            hash.add_bool(item.intentionally_missing);
            const wearable_event_mapping* events[] = {&item.ecg_r, &item.ppg_onset, &item.ppg_peak};
            for (std::size_t event = 0; event < sizeof(events) / sizeof(events[0]); ++event)
            {
                hash.add_bool(events[event]->present);
                hash.add_double(events[event]->latent_time_seconds);
                hash.add_u64(events[event]->sample_index);
                hash.add_double(events[event]->reported_device_time_seconds);
                hash.add_bool(events[event]->received);
            }
            hash.add_bool(item.has_physiological_onset_delay);
            hash.add_double(item.physiological_onset_delay_seconds);
            hash.add_bool(item.has_physiological_peak_delay);
            hash.add_double(item.physiological_peak_delay_seconds);
            hash.add_bool(item.has_observed_onset_device_delta);
            hash.add_double(item.observed_onset_device_delta_seconds);
            hash.add_double(item.onset_observed_minus_physiological_seconds);
            hash.add_bool(item.has_observed_peak_device_delta);
            hash.add_double(item.observed_peak_device_delta_seconds);
            hash.add_double(item.peak_observed_minus_physiological_seconds);
        }
        return hash.text();
    }
}
