#include "external_noise.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <stdint.h>

namespace
{
    const double pi = 3.141592653589793238462643383279502884;

    bool is_finite(double value) { return std::isfinite(value); }

    const char* lead_name(unsigned int lead)
    {
        static const char* names[signal_synth::clinical_lead_count] = {"I","II","III","aVR","aVL","aVF","V1","V2","V3","V4","V5","V6"};
        return lead < signal_synth::clinical_lead_count ? names[lead] : "";
    }

    bool safe_identifier(const std::string& value)
    {
        if (value.empty() || value.size() > 128u) return false;
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            const unsigned char c = static_cast<unsigned char>(value[i]);
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) return false;
        }
        return true;
    }

    bool valid_sha256(const std::string& value)
    {
        if (value.size() != 71u || value.substr(0, 7) != "sha256:") return false;
        for (std::size_t i = 7; i < value.size(); ++i)
            if (!((value[i] >= '0' && value[i] <= '9') || (value[i] >= 'a' && value[i] <= 'f'))) return false;
        return true;
    }

    uint32_t rotate_right(uint32_t value, unsigned int count) { return (value >> count) | (value << (32u - count)); }

    std::string sha256(const std::string& input)
    {
        static const uint32_t constants[64] = {
            0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
            0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
            0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
            0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
            0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
            0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
            0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
            0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};
        std::vector<unsigned char> bytes(input.begin(), input.end());
        const uint64_t bit_length = static_cast<uint64_t>(bytes.size()) * 8u;
        bytes.push_back(0x80u);
        while ((bytes.size() % 64u) != 56u) bytes.push_back(0u);
        for (int shift = 56; shift >= 0; shift -= 8) bytes.push_back(static_cast<unsigned char>((bit_length >> shift) & 0xffu));
        uint32_t state[8] = {0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
        for (std::size_t block = 0; block < bytes.size(); block += 64u)
        {
            uint32_t words[64];
            for (unsigned int i = 0; i < 16u; ++i) words[i] = (static_cast<uint32_t>(bytes[block + 4u * i]) << 24) | (static_cast<uint32_t>(bytes[block + 4u * i + 1u]) << 16) | (static_cast<uint32_t>(bytes[block + 4u * i + 2u]) << 8) | bytes[block + 4u * i + 3u];
            for (unsigned int i = 16u; i < 64u; ++i)
            {
                const uint32_t s0 = rotate_right(words[i - 15u], 7u) ^ rotate_right(words[i - 15u], 18u) ^ (words[i - 15u] >> 3u);
                const uint32_t s1 = rotate_right(words[i - 2u], 17u) ^ rotate_right(words[i - 2u], 19u) ^ (words[i - 2u] >> 10u);
                words[i] = words[i - 16u] + s0 + words[i - 7u] + s1;
            }
            uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4], f = state[5], g = state[6], h = state[7];
            for (unsigned int i = 0; i < 64u; ++i)
            {
                const uint32_t s1 = rotate_right(e, 6u) ^ rotate_right(e, 11u) ^ rotate_right(e, 25u);
                const uint32_t choice = (e & f) ^ ((~e) & g);
                const uint32_t temp1 = h + s1 + choice + constants[i] + words[i];
                const uint32_t s0 = rotate_right(a, 2u) ^ rotate_right(a, 13u) ^ rotate_right(a, 22u);
                const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
                const uint32_t temp2 = s0 + majority;
                h = g; g = f; f = e; e = d + temp1; d = c; c = b; b = a; a = temp1 + temp2;
            }
            state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e; state[5] += f; state[6] += g; state[7] += h;
        }
        std::ostringstream output; output << "sha256:" << std::hex << std::setfill('0');
        for (unsigned int i = 0; i < 8u; ++i) output << std::setw(8) << state[i];
        return output.str();
    }

    std::string json_string(const std::string& value)
    {
        std::ostringstream output; output << '"';
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            const unsigned char c = static_cast<unsigned char>(value[i]);
            if (c == '"' || c == '\\') output << '\\' << static_cast<char>(c);
            else if (c == '\n') output << "\\n";
            else if (c == '\r') output << "\\r";
            else if (c == '\t') output << "\\t";
            else if (c >= 0x20u) output << static_cast<char>(c);
        }
        output << '"'; return output.str();
    }

    bool split_csv_line(const std::string& input, std::vector<std::string>& cells)
    {
        cells.clear();
        std::size_t first = 0;
        for (std::size_t i = 0; i <= input.size(); ++i)
            if (i == input.size() || input[i] == ',')
            {
                cells.push_back(input.substr(first, i - first));
                first = i + 1u;
            }
        return !cells.empty();
    }

    bool parse_number(const std::string& text, double& value)
    {
        if (text.empty()) return false;
        char* past = 0;
        value = std::strtod(text.c_str(), &past);
        return past == text.c_str() + text.size() && is_finite(value);
    }

    bool parse_asset_csv(const signal_synth::external_noise_asset_manifest& manifest, const std::string& content, std::vector<std::vector<double> >& channels, std::string& message)
    {
        if (sha256(content) != manifest.content_sha256)
        {
            message = "external noise asset checksum mismatch: " + manifest.id;
            return false;
        }
        std::istringstream input(content); input.imbue(std::locale::classic());
        std::string line;
        if (!std::getline(input, line)) { message = "external noise asset is empty: " + manifest.id; return false; }
        if (!line.empty() && line[line.size() - 1u] == '\r') line.erase(line.size() - 1u);
        std::vector<std::string> cells;
        split_csv_line(line, cells);
        if (cells != manifest.channels) { message = "external noise asset CSV header does not match manifest channels: " + manifest.id; return false; }
        channels.assign(manifest.channels.size(), std::vector<double>());
        while (std::getline(input, line))
        {
            if (!line.empty() && line[line.size() - 1u] == '\r') line.erase(line.size() - 1u);
            if (line.empty()) continue;
            split_csv_line(line, cells);
            if (cells.size() != channels.size()) { message = "external noise asset CSV row has the wrong channel count: " + manifest.id; return false; }
            for (std::size_t channel = 0; channel < cells.size(); ++channel)
            {
                double value = 0.0;
                if (!parse_number(cells[channel], value)) { message = "external noise asset CSV contains a non-finite or invalid value: " + manifest.id; return false; }
                channels[channel].push_back(value);
            }
        }
        if (channels.empty() || channels[0].size() < 2u) { message = "external noise asset requires at least two samples: " + manifest.id; return false; }
        return true;
    }

    double rms_ac(const std::vector<double>& values)
    {
        double mean = 0.0;
        for (std::size_t i = 0; i < values.size(); ++i) mean += values[i];
        mean /= values.size();
        double sum = 0.0;
        for (std::size_t i = 0; i < values.size(); ++i) { const double delta = values[i] - mean; sum += delta * delta; }
        return std::sqrt(sum / values.size());
    }

    double rms(const std::vector<double>& values)
    {
        double sum = 0.0;
        for (std::size_t i = 0; i < values.size(); ++i) sum += values[i] * values[i];
        return std::sqrt(sum / values.size());
    }

    double taper_weight(std::size_t index, std::size_t count, unsigned int taper_samples)
    {
        if (!taper_samples) return 1.0;
        const std::size_t right = count - 1u - index;
        const std::size_t edge = std::min(index, right);
        if (edge >= taper_samples) return 1.0;
        return 0.5 - 0.5 * std::cos(pi * static_cast<double>(edge + 1u) / static_cast<double>(taper_samples + 1u));
    }
}

namespace signal_synth
{
    const char* external_noise_redistribution_name(external_noise_redistribution value)
    {
        switch (value)
        {
        case external_noise_local_only: return "local_only";
        case external_noise_rendered_output: return "rendered_output";
        case external_noise_source_and_output: return "source_and_output";
        }
        return "";
    }

    bool external_noise_redistribution_from_name(const std::string& name, external_noise_redistribution& value)
    {
        if (name == "local_only") value = external_noise_local_only;
        else if (name == "rendered_output") value = external_noise_rendered_output;
        else if (name == "source_and_output") value = external_noise_source_and_output;
        else return false;
        return true;
    }

    external_noise_asset_manifest::external_noise_asset_manifest() : id(), source_uri(), license(), content_sha256(), sample_rate_hz(0), channels(), redistribution(external_noise_local_only) {}
    external_noise_interval_config::external_noise_interval_config() : asset_id(), asset_channel(), start_seconds(0.0), duration_seconds(1.0), asset_offset_seconds(0.0), target_snr_db(10.0), taper_seconds(0.1), clip_limit_mv(0.0) { for (unsigned int lead = 0; lead < clinical_lead_count; ++lead) ecg_leads[lead] = false; }
    external_noise_channel_truth::external_noise_channel_truth() : lead(0), clean_rms_mv(0.0), added_noise_rms_mv(0.0), target_snr_db(0.0), achieved_snr_db(0.0), clipping_count(0) {}
    external_noise_interval_truth::external_noise_interval_truth() : asset_id(), asset_channel(), source_uri(), license(), content_sha256(), redistribution(external_noise_local_only), start_seconds(0.0), end_seconds(0.0), asset_offset_seconds(0.0), taper_seconds(0.0), channels() {}
    external_noise_result::external_noise_result() : release_allowed(true), intervals() {}

    bool validate_external_noise_config(const external_noise_config& config, double duration_seconds, unsigned int output_sample_rate_hz, std::vector<std::string>& messages)
    {
        messages.clear();
        if (!is_finite(duration_seconds) || duration_seconds <= 0.0 || !output_sample_rate_hz) { messages.push_back("invalid external noise render timebase"); return false; }
        std::set<std::string> asset_ids;
        for (std::size_t i = 0; i < config.assets.size(); ++i)
        {
            const external_noise_asset_manifest& asset = config.assets[i];
            if (!safe_identifier(asset.id) || !asset_ids.insert(asset.id).second) messages.push_back("external noise asset ids must be unique safe identifiers");
            if (asset.source_uri.empty() || asset.source_uri.size() > 2048u || asset.license.empty() || asset.license.size() > 512u) messages.push_back("external noise asset source_uri and license are required");
            if (!valid_sha256(asset.content_sha256)) messages.push_back("external noise asset sha256 must contain 64 lowercase hex characters");
            if (!asset.sample_rate_hz || asset.sample_rate_hz > 1000000u) messages.push_back("external noise asset sample rate is invalid");
            if (asset.channels.empty() || asset.channels.size() > 64u) messages.push_back("external noise asset requires 1 to 64 channels");
            std::set<std::string> channel_ids;
            for (std::size_t channel = 0; channel < asset.channels.size(); ++channel)
                if (!safe_identifier(asset.channels[channel]) || !channel_ids.insert(asset.channels[channel]).second) messages.push_back("external noise asset channels must be unique safe identifiers");
            if (!external_noise_redistribution_name(asset.redistribution)[0]) messages.push_back("external noise redistribution mode is invalid");
        }
        for (std::size_t i = 0; i < config.intervals.size(); ++i)
        {
            const external_noise_interval_config& interval = config.intervals[i];
            const external_noise_asset_manifest* asset = 0;
            for (std::size_t j = 0; j < config.assets.size(); ++j) if (config.assets[j].id == interval.asset_id) asset = &config.assets[j];
            if (!asset) messages.push_back("external noise interval references an unknown asset");
            else if (std::find(asset->channels.begin(), asset->channels.end(), interval.asset_channel) == asset->channels.end()) messages.push_back("external noise interval references an unknown asset channel");
            const double end = interval.start_seconds + interval.duration_seconds;
            if (!is_finite(interval.start_seconds) || !is_finite(interval.duration_seconds) || !is_finite(end) || interval.start_seconds < 0.0 || interval.duration_seconds <= 0.0 || end > duration_seconds + 1e-12) messages.push_back("external noise interval must fit inside the scenario");
            if (!is_finite(interval.asset_offset_seconds) || interval.asset_offset_seconds < 0.0) messages.push_back("external noise asset offset must be nonnegative");
            if (!is_finite(interval.target_snr_db) || interval.target_snr_db < -40.0 || interval.target_snr_db > 80.0) messages.push_back("external noise target SNR must be between -40 and 80 dB");
            if (!is_finite(interval.taper_seconds) || interval.taper_seconds < 0.0 || 2.0 * interval.taper_seconds > interval.duration_seconds + 1e-12) messages.push_back("external noise taper must fit twice inside the interval");
            if (!is_finite(interval.clip_limit_mv) || interval.clip_limit_mv < 0.0 || interval.clip_limit_mv > 1000.0) messages.push_back("external noise clip limit must be zero or positive");
            bool any_lead = false;
            for (unsigned int lead = 0; lead < clinical_lead_count; ++lead) any_lead = any_lead || interval.ecg_leads[lead];
            if (!any_lead) messages.push_back("external noise interval requires at least one ECG lead");
            for (std::size_t previous = 0; previous < i; ++previous)
            {
                const external_noise_interval_config& other = config.intervals[previous];
                const bool overlaps = interval.start_seconds < other.start_seconds + other.duration_seconds && other.start_seconds < end;
                bool shared_lead = false;
                for (unsigned int lead = 0; lead < clinical_lead_count; ++lead) shared_lead = shared_lead || (interval.ecg_leads[lead] && other.ecg_leads[lead]);
                if (overlaps && shared_lead) messages.push_back("external noise intervals cannot overlap on the same ECG lead");
            }
        }
        if (config.intervals.empty() != config.assets.empty()) messages.push_back("external noise assets and intervals must either both be empty or both be present");
        return messages.empty();
    }

    bool apply_external_noise(const external_noise_config& config, const std::vector<external_noise_asset_input>& inputs, unsigned int sample_rate_hz, std::vector<std::vector<double> >& ecg_leads, external_noise_result& result, std::vector<std::string>& messages)
    {
        external_noise_result fresh;
        messages.clear();
        if (ecg_leads.size() != clinical_lead_count || !sample_rate_hz) { messages.push_back("external noise requires a complete ECG lead set"); return false; }
        const std::size_t sample_count = ecg_leads.empty() ? 0u : ecg_leads[0].size();
        for (unsigned int lead = 0; lead < clinical_lead_count; ++lead) if (ecg_leads[lead].size() != sample_count) { messages.push_back("external noise ECG lead lengths differ"); return false; }
        if (!validate_external_noise_config(config, static_cast<double>(sample_count) / sample_rate_hz, sample_rate_hz, messages)) return false;
        if (config.intervals.empty()) { result = fresh; return true; }
        std::set<std::string> input_ids;
        for (std::size_t i = 0; i < inputs.size(); ++i)
        {
            if (!input_ids.insert(inputs[i].id).second) { messages.push_back("duplicate external noise asset input: " + inputs[i].id); return false; }
            bool declared = false;
            for (std::size_t asset = 0; asset < config.assets.size(); ++asset) declared = declared || config.assets[asset].id == inputs[i].id;
            if (!declared) { messages.push_back("undeclared external noise asset input: " + inputs[i].id); return false; }
        }
        std::vector<std::vector<std::vector<double> > > parsed_assets(config.assets.size());
        for (std::size_t asset_index = 0; asset_index < config.assets.size(); ++asset_index)
        {
            const external_noise_asset_manifest& manifest = config.assets[asset_index];
            const external_noise_asset_input* input = 0;
            for (std::size_t i = 0; i < inputs.size(); ++i) if (inputs[i].id == manifest.id) input = &inputs[i];
            if (!input) { messages.push_back("missing external noise asset: " + manifest.id); return false; }
            std::string message;
            if (!parse_asset_csv(manifest, input->csv_content, parsed_assets[asset_index], message)) { messages.push_back(message); return false; }
        }
        for (std::size_t interval_index = 0; interval_index < config.intervals.size(); ++interval_index)
        {
            const external_noise_interval_config& interval = config.intervals[interval_index];
            std::size_t asset_index = 0;
            while (asset_index < config.assets.size() && config.assets[asset_index].id != interval.asset_id) ++asset_index;
            if (asset_index == config.assets.size()) { messages.push_back("unknown external noise asset during render"); return false; }
            const external_noise_asset_manifest& manifest = config.assets[asset_index];
            const std::size_t source_channel = static_cast<std::size_t>(std::find(manifest.channels.begin(), manifest.channels.end(), interval.asset_channel) - manifest.channels.begin());
            const std::vector<double>& source = parsed_assets[asset_index][source_channel];
            const std::size_t first = static_cast<std::size_t>(std::ceil(interval.start_seconds * sample_rate_hz - 1e-12));
            const std::size_t past = static_cast<std::size_t>(std::ceil((interval.start_seconds + interval.duration_seconds) * sample_rate_hz - 1e-12));
            if (first >= past || past > sample_count) { messages.push_back("external noise interval sample bounds are invalid"); return false; }
            const std::size_t count = past - first;
            std::vector<double> noise(count, 0.0);
            for (std::size_t sample = 0; sample < count; ++sample)
            {
                const double source_position = (interval.asset_offset_seconds + static_cast<double>(sample) / sample_rate_hz) * manifest.sample_rate_hz;
                const std::size_t left = static_cast<std::size_t>(std::floor(source_position));
                const double fraction = source_position - left;
                if (left >= source.size() || (left + 1u >= source.size() && fraction > 1e-12)) { messages.push_back("external noise asset is too short for interval: " + manifest.id); return false; }
                const std::size_t right = left + 1u < source.size() ? left + 1u : left;
                noise[sample] = source[left] * (1.0 - fraction) + source[right] * fraction;
            }
            double noise_mean = 0.0;
            for (std::size_t sample = 0; sample < count; ++sample) noise_mean += noise[sample];
            noise_mean /= count;
            const unsigned int taper_samples = static_cast<unsigned int>(std::floor(interval.taper_seconds * sample_rate_hz + 0.5));
            for (std::size_t sample = 0; sample < count; ++sample) noise[sample] = (noise[sample] - noise_mean) * taper_weight(sample, count, taper_samples);
            const double source_noise_rms = rms(noise);
            if (!(source_noise_rms > 1e-15)) { messages.push_back("external noise interval has zero usable RMS: " + manifest.id); return false; }

            external_noise_interval_truth truth;
            truth.asset_id = manifest.id; truth.asset_channel = interval.asset_channel; truth.source_uri = manifest.source_uri; truth.license = manifest.license; truth.content_sha256 = manifest.content_sha256; truth.redistribution = manifest.redistribution;
            truth.start_seconds = interval.start_seconds; truth.end_seconds = interval.start_seconds + interval.duration_seconds; truth.asset_offset_seconds = interval.asset_offset_seconds; truth.taper_seconds = interval.taper_seconds;
            fresh.release_allowed = fresh.release_allowed && manifest.redistribution != external_noise_local_only;
            for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
            {
                if (!interval.ecg_leads[lead]) continue;
                std::vector<double> clean(ecg_leads[lead].begin() + first, ecg_leads[lead].begin() + past);
                const double clean_rms = rms_ac(clean);
                if (!(clean_rms > 1e-15)) { messages.push_back("external noise target lead has zero clean AC RMS"); return false; }
                const double target_noise_rms = clean_rms * std::pow(10.0, -interval.target_snr_db / 20.0);
                const double scale = target_noise_rms / source_noise_rms;
                std::vector<double> added(count, 0.0);
                unsigned long long clipping_count = 0;
                for (std::size_t sample = 0; sample < count; ++sample)
                {
                    double mixed = clean[sample] + scale * noise[sample];
                    if (interval.clip_limit_mv > 0.0 && (mixed < -interval.clip_limit_mv || mixed > interval.clip_limit_mv))
                    {
                        ++clipping_count;
                        mixed = std::max(-interval.clip_limit_mv, std::min(interval.clip_limit_mv, mixed));
                    }
                    ecg_leads[lead][first + sample] = mixed;
                    added[sample] = mixed - clean[sample];
                }
                external_noise_channel_truth channel;
                channel.lead = lead; channel.clean_rms_mv = clean_rms; channel.added_noise_rms_mv = rms(added); channel.target_snr_db = interval.target_snr_db; channel.clipping_count = clipping_count;
                if (!(channel.added_noise_rms_mv > 1e-15)) { messages.push_back("external noise produced zero added RMS after clipping"); return false; }
                channel.achieved_snr_db = 20.0 * std::log10(clean_rms / channel.added_noise_rms_mv);
                truth.channels.push_back(channel);
            }
            fresh.intervals.push_back(truth);
        }
        result = fresh;
        return true;
    }

    std::string external_noise_truth_json(const external_noise_result& result)
    {
        std::ostringstream output; output.imbue(std::locale::classic()); output << std::setprecision(std::numeric_limits<double>::max_digits10);
        output << "{\"schema_version\":1,\"contract\":\"synsigra_external_noise_truth_v1\",\"release_allowed\":" << (result.release_allowed ? "true" : "false") << ",\"intervals\":[";
        for (std::size_t i = 0; i < result.intervals.size(); ++i)
        {
            const external_noise_interval_truth& interval = result.intervals[i];
            output << (i ? "," : "") << "{\"asset_id\":" << json_string(interval.asset_id) << ",\"asset_channel\":" << json_string(interval.asset_channel) << ",\"source_uri\":" << json_string(interval.source_uri) << ",\"license\":" << json_string(interval.license) << ",\"content_sha256\":" << json_string(interval.content_sha256) << ",\"redistribution\":" << json_string(external_noise_redistribution_name(interval.redistribution)) << ",\"start_seconds\":" << interval.start_seconds << ",\"end_seconds\":" << interval.end_seconds << ",\"asset_offset_seconds\":" << interval.asset_offset_seconds << ",\"taper_seconds\":" << interval.taper_seconds << ",\"channels\":[";
            for (std::size_t channel = 0; channel < interval.channels.size(); ++channel)
            {
                const external_noise_channel_truth& item = interval.channels[channel];
                output << (channel ? "," : "") << "{\"lead\":" << json_string(lead_name(item.lead)) << ",\"clean_rms_mv\":" << item.clean_rms_mv << ",\"added_noise_rms_mv\":" << item.added_noise_rms_mv << ",\"target_snr_db\":" << item.target_snr_db << ",\"achieved_snr_db\":" << item.achieved_snr_db << ",\"clipping_count\":" << item.clipping_count << '}';
            }
            output << "]}";
        }
        output << "]}";
        return output.str();
    }
}
