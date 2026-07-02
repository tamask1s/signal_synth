#include "clinical_ecg.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>
#include <vector>

namespace signal_synth
{
    namespace
    {
        const double PI = 3.14159265358979323846;
        const double TWO_PI = 2.0 * PI;
        const unsigned long long NO_BEAT = std::numeric_limits<unsigned long long>::max();

        struct vec3
        {
            double x;
            double y;
            double z;
        };

        struct generated_clinical_data
        {
            unsigned int sampling_rate_hz;
            unsigned int sample_count;
            std::vector<double> leads[clinical_lead_count];
            std::vector<double> sources[clinical_source_count][clinical_axis_count];
            std::vector<double> vcg[clinical_axis_count];
            std::vector<clinical_atrial_event> atrial_events;
            std::vector<clinical_beat_annotation> beats;
            std::vector<clinical_fiducial_annotation> fiducials;
            std::vector<clinical_episode_annotation> episodes;
        };

        bool finite(double value)
        {
            return std::isfinite(value);
        }

        double radians(double degrees)
        {
            return degrees * PI / 180.0;
        }

        vec3 add(const vec3& left, const vec3& right)
        {
            return vec3{left.x + right.x, left.y + right.y, left.z + right.z};
        }

        vec3 scale(const vec3& value, double factor)
        {
            return vec3{value.x * factor, value.y * factor, value.z * factor};
        }

        double dot(const vec3& left, const vec3& right)
        {
            return left.x * right.x + left.y * right.y + left.z * right.z;
        }

        vec3 spatial_vector(double amplitude, double axis_degrees, double elevation_degrees)
        {
            const double axis = radians(axis_degrees);
            const double elevation = radians(elevation_degrees);
            const double horizontal = std::cos(elevation);
            return vec3{amplitude * horizontal * std::cos(axis), amplitude * horizontal * std::sin(axis), amplitude * std::sin(elevation)};
        }

        vec3 source_vector(const clinical_ecg_config& config, clinical_ecg_source source, double amplitude, double axis_degrees, double elevation_degrees)
        {
            return spatial_vector(amplitude * config.sources.gain[source], axis_degrees + config.sources.axis_offset_degrees[source], elevation_degrees + config.sources.elevation_offset_degrees[source]);
        }

        vec3 rotate_vector(const vec3& input, const clinical_lead_config& config)
        {
            const double yaw = radians(config.yaw_degrees);
            const double pitch = radians(config.pitch_degrees);
            const double roll = radians(config.roll_degrees);
            const double cy = std::cos(yaw);
            const double sy = std::sin(yaw);
            const double cp = std::cos(pitch);
            const double sp = std::sin(pitch);
            const double cr = std::cos(roll);
            const double sr = std::sin(roll);
            const vec3 yawed = vec3{cy * input.x - sy * input.y, sy * input.x + cy * input.y, input.z};
            const vec3 pitched = vec3{cp * yawed.x + sp * yawed.z, yawed.y, -sp * yawed.x + cp * yawed.z};
            return vec3{pitched.x, cr * pitched.y - sr * pitched.z, sr * pitched.y + cr * pitched.z};
        }

        double smoothstep(double value)
        {
            value = std::max(0.0, std::min(1.0, value));
            return value * value * (3.0 - 2.0 * value);
        }

        vec3 cubic_hermite(const vec3& first_value, const vec3& last_value, const vec3& first_derivative, const vec3& last_derivative, double duration, double position)
        {
            position = std::max(0.0, std::min(1.0, position));
            const double position2 = position * position;
            const double position3 = position2 * position;
            const double first_value_weight = 2.0 * position3 - 3.0 * position2 + 1.0;
            const double first_derivative_weight = position3 - 2.0 * position2 + position;
            const double last_value_weight = -2.0 * position3 + 3.0 * position2;
            const double last_derivative_weight = position3 - position2;
            return add(add(scale(first_value, first_value_weight), scale(first_derivative, first_derivative_weight * duration)), add(scale(last_value, last_value_weight), scale(last_derivative, last_derivative_weight * duration)));
        }

        std::uint64_t mix64(std::uint64_t value)
        {
            value += 0x9e3779b97f4a7c15ULL;
            value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
            value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
            return value ^ (value >> 31);
        }

        double deterministic_unit(unsigned long long seed, unsigned long long index)
        {
            const std::uint64_t value = mix64(static_cast<std::uint64_t>(seed) ^ mix64(static_cast<std::uint64_t>(index)));
            return static_cast<double>(value >> 11) * (1.0 / 9007199254740992.0);
        }

        double deterministic_normal(unsigned long long seed, unsigned long long index)
        {
            const double first = std::max(deterministic_unit(seed, index * 2), std::numeric_limits<double>::min());
            const double second = deterministic_unit(seed, index * 2 + 1);
            return std::sqrt(-2.0 * std::log(first)) * std::cos(TWO_PI * second);
        }

        template <typename enum_type>
        typename std::underlying_type<enum_type>::type enum_value(const enum_type& value)
        {
            typename std::underlying_type<enum_type>::type result = 0;
            static_assert(sizeof(result) == sizeof(value), "Unexpected enum representation");
            std::memcpy(&result, &value, sizeof(result));
            return result;
        }

        double corrected_qt_seconds(const clinical_ecg_config& config, double rr_seconds)
        {
            const double qtc = config.timing.qtc_ms * 0.001;
            const double heart_rate = 60.0 / rr_seconds;
            switch (config.timing.qt_correction)
            {
            case clinical_qt_bazett:
                return qtc * std::sqrt(rr_seconds);
            case clinical_qt_fridericia:
                return qtc * std::cbrt(rr_seconds);
            case clinical_qt_framingham:
                return qtc - 0.154 * (1.0 - rr_seconds);
            case clinical_qt_hodges:
                return qtc - 0.00175 * (heart_rate - 60.0);
            case clinical_qt_fixed:
            default:
                return config.timing.qt_interval_ms * 0.001;
            }
        }

        bool valid_config(const clinical_ecg_config& config)
        {
            if (config.sampling_rate_hz < 100 || config.sampling_rate_hz > 1000000)
                return false;
            const int rhythm = enum_value(config.rhythm.rhythm);
            const int av_conduction = enum_value(config.rhythm.av_conduction);
            const int intraventricular_conduction = enum_value(config.rhythm.intraventricular_conduction);
            const int preexcitation = enum_value(config.rhythm.preexcitation);
            const int episode_kind = enum_value(config.scenario.episode_kind);
            const int premature_origin = enum_value(config.scenario.premature_origin);
            const int qt_correction = enum_value(config.timing.qt_correction);
            if (rhythm < clinical_rhythm_sinus || rhythm > clinical_rhythm_paced || av_conduction < clinical_av_normal || av_conduction > clinical_av_complete_block || intraventricular_conduction < clinical_iv_normal || intraventricular_conduction > clinical_iv_nonspecific_delay || preexcitation < clinical_preexcitation_none || preexcitation > clinical_preexcitation_wpw || episode_kind < clinical_episode_none || episode_kind > clinical_episode_svarr || premature_origin < clinical_origin_pac || premature_origin > clinical_origin_paced || qt_correction < clinical_qt_fixed || qt_correction > clinical_qt_hodges)
                return false;
            const double timing_values[] = {config.timing.p_duration_ms, config.timing.pr_interval_ms, config.timing.qrs_duration_ms, config.timing.qrs_q_fraction, config.timing.qrs_r_fraction, config.timing.qrs_s_fraction, config.timing.t_duration_ms, config.timing.t_peak_fraction, config.timing.qt_interval_ms, config.timing.qtc_ms};
            const double morphology_values[] = {config.morphology.p_amplitude_mv, config.morphology.q_amplitude_mv, config.morphology.r_amplitude_mv, config.morphology.s_amplitude_mv, config.morphology.t_amplitude_mv, config.morphology.st_j_amplitude_mv, config.morphology.st_slope_mv_per_second, config.morphology.p_axis_degrees, config.morphology.qrs_axis_degrees, config.morphology.t_axis_degrees, config.morphology.p_elevation_degrees, config.morphology.qrs_elevation_degrees, config.morphology.t_elevation_degrees, config.morphology.presence_threshold_mv};
            const double rhythm_values[] = {config.rhythm.heart_rate_bpm, config.rhythm.atrial_rate_bpm, config.rhythm.ventricular_escape_rate_bpm, config.rhythm.rr_variability_seconds, config.rhythm.minimum_rr_seconds, config.rhythm.maximum_rr_seconds, config.rhythm.first_degree_pr_ms, config.rhythm.wenckebach_pr_increment_ms};
            const double scenario_values[] = {config.scenario.premature_coupling_ratio, config.scenario.compensatory_pause_ratio, config.scenario.sinus_pause_ratio, config.scenario.episode_start_seconds, config.scenario.episode_duration_seconds, config.scenario.episode_rate_bpm};
            for (double value : timing_values)
                if (!finite(value))
                    return false;
            for (double value : morphology_values)
                if (!finite(value))
                    return false;
            for (double value : rhythm_values)
                if (!finite(value))
                    return false;
            for (double value : scenario_values)
                if (!finite(value))
                    return false;
            for (int lead = 0; lead < clinical_lead_count; ++lead)
                if (!finite(config.leads.lead_gain[lead]) || config.leads.lead_gain[lead] <= 0.0)
                    return false;
            for (int source = 0; source < clinical_source_count; ++source)
                if (!finite(config.sources.gain[source]) || config.sources.gain[source] < 0.0 || !finite(config.sources.axis_offset_degrees[source]) || !finite(config.sources.elevation_offset_degrees[source]))
                    return false;
            if (!finite(config.leads.yaw_degrees) || !finite(config.leads.pitch_degrees) || !finite(config.leads.roll_degrees))
                return false;
            if (config.timing.p_duration_ms <= 0.0 || config.timing.pr_interval_ms < config.timing.p_duration_ms || config.timing.qrs_duration_ms <= 0.0 || config.timing.t_duration_ms <= 0.0 || config.timing.qt_interval_ms <= 0.0 || config.timing.qtc_ms <= 0.0)
                return false;
            if (!(config.timing.qrs_q_fraction > 0.0 && config.timing.qrs_q_fraction < config.timing.qrs_r_fraction && config.timing.qrs_r_fraction < config.timing.qrs_s_fraction && config.timing.qrs_s_fraction < 1.0))
                return false;
            if (!(config.timing.t_peak_fraction > 0.0 && config.timing.t_peak_fraction < 1.0))
                return false;
            if (config.rhythm.heart_rate_bpm < 10.0 || config.rhythm.heart_rate_bpm > 400.0 || config.rhythm.atrial_rate_bpm < 10.0 || config.rhythm.atrial_rate_bpm > 600.0 || config.rhythm.ventricular_escape_rate_bpm < 10.0 || config.rhythm.ventricular_escape_rate_bpm > 200.0)
                return false;
            if (config.rhythm.rr_variability_seconds < 0.0 || config.rhythm.minimum_rr_seconds <= 0.0 || config.rhythm.maximum_rr_seconds <= config.rhythm.minimum_rr_seconds || config.rhythm.first_degree_pr_ms < config.timing.p_duration_ms || config.rhythm.wenckebach_pr_increment_ms < 0.0 || config.morphology.presence_threshold_mv < 0.0 || config.rhythm.mobitz_cycle_length < 2 || config.rhythm.flutter_conduction_ratio < 1)
                return false;
            if (config.scenario.premature_coupling_ratio <= 0.0 || config.scenario.premature_coupling_ratio >= 1.0 || config.scenario.compensatory_pause_ratio < 1.0 || config.scenario.sinus_pause_ratio <= 1.0)
                return false;
            if (config.scenario.episode_kind != clinical_episode_none && (config.scenario.episode_start_seconds < 0.0 || config.scenario.episode_duration_seconds <= 0.0 || config.scenario.episode_rate_bpm <= 100.0 || config.scenario.episode_rate_bpm > 400.0 || config.scenario.episode_rate_bpm <= config.rhythm.heart_rate_bpm))
                return false;
            if (rhythm != clinical_rhythm_sinus && av_conduction != clinical_av_normal)
                return false;
            if (rhythm != clinical_rhythm_sinus && (config.scenario.premature_every_n_beats > 0 || config.scenario.sinus_pause_every_n_beats > 0))
                return false;
            if (config.scenario.episode_kind != clinical_episode_none && (rhythm != clinical_rhythm_sinus || av_conduction != clinical_av_normal || config.scenario.premature_every_n_beats > 0 || config.scenario.sinus_pause_every_n_beats > 0))
                return false;
            return true;
        }

        double adjusted_qrs_duration(const clinical_ecg_config& config, clinical_ventricular_origin origin)
        {
            double duration = config.timing.qrs_duration_ms * 0.001;
            if (config.rhythm.intraventricular_conduction == clinical_iv_lbbb)
                duration = std::max(duration, 0.140);
            if (config.rhythm.intraventricular_conduction == clinical_iv_rbbb)
                duration = std::max(duration, 0.130);
            if (config.rhythm.intraventricular_conduction == clinical_iv_incomplete_lbbb)
                duration = std::max(duration, 0.112);
            if (config.rhythm.intraventricular_conduction == clinical_iv_incomplete_rbbb)
                duration = std::max(duration, 0.100);
            if (config.rhythm.intraventricular_conduction == clinical_iv_left_anterior_fascicular || config.rhythm.intraventricular_conduction == clinical_iv_left_posterior_fascicular)
                duration = std::max(duration, 0.100);
            if (config.rhythm.intraventricular_conduction == clinical_iv_nonspecific_delay)
                duration = std::max(duration, 0.122);
            if (config.rhythm.preexcitation == clinical_preexcitation_wpw)
                duration = std::max(duration, 0.120);
            if (origin == clinical_origin_pvc || origin == clinical_origin_ventricular_escape || origin == clinical_origin_vt)
                duration = std::max(duration, 0.150);
            if (origin == clinical_origin_paced)
                duration = std::max(duration, 0.140);
            return duration;
        }

        double adjusted_pr_interval(const clinical_ecg_config& config)
        {
            double interval = config.timing.pr_interval_ms * 0.001;
            if (config.rhythm.preexcitation == clinical_preexcitation_wpw && config.rhythm.av_conduction == clinical_av_normal)
                interval = std::min(interval, 0.110);
            return interval;
        }

        clinical_beat_annotation make_beat(const clinical_ecg_config& config, unsigned long long beat_index, double r_peak_time, double rr_seconds, long long linked_atrial_index, double pr_seconds, clinical_ventricular_origin origin)
        {
            clinical_beat_annotation beat = {};
            const double qrs_duration = adjusted_qrs_duration(config, origin);
            const double qrs_onset = r_peak_time - config.timing.qrs_r_fraction * qrs_duration;
            double qt = corrected_qt_seconds(config, std::max(0.2, rr_seconds));
            qt = std::max(qt, qrs_duration + config.timing.t_duration_ms * 0.001 + 0.020);
            const double t_offset = qrs_onset + qt;
            const double t_duration = std::min(config.timing.t_duration_ms * 0.001, std::max(0.040, qt - qrs_duration - 0.020));
            const double t_onset = t_offset - t_duration;
            beat.beat_index = beat_index;
            beat.linked_atrial_index = linked_atrial_index;
            beat.rhythm = config.rhythm.rhythm;
            beat.av_conduction = config.rhythm.av_conduction;
            beat.intraventricular_conduction = config.rhythm.intraventricular_conduction;
            beat.origin = origin;
            beat.rr_interval_seconds = rr_seconds;
            beat.pr_interval_seconds = pr_seconds;
            beat.qrs_duration_seconds = qrs_duration;
            beat.qt_interval_seconds = qt;
            beat.qtc_interval_seconds = config.timing.qtc_ms * 0.001;
            beat.qrs_onset_time_seconds = qrs_onset;
            beat.q_peak_time_seconds = qrs_onset + config.timing.qrs_q_fraction * qrs_duration;
            beat.r_peak_time_seconds = r_peak_time;
            beat.s_peak_time_seconds = qrs_onset + config.timing.qrs_s_fraction * qrs_duration;
            beat.j_point_time_seconds = qrs_onset + qrs_duration;
            beat.qrs_offset_time_seconds = beat.j_point_time_seconds;
            beat.t_onset_time_seconds = std::max(beat.j_point_time_seconds + 0.010, t_onset);
            beat.t_peak_time_seconds = beat.t_onset_time_seconds + config.timing.t_peak_fraction * (t_offset - beat.t_onset_time_seconds);
            beat.t_offset_time_seconds = t_offset;
            beat.p_present = linked_atrial_index >= 0 && origin != clinical_origin_pvc && origin != clinical_origin_paced && origin != clinical_origin_vt;
            beat.qrs_present = true;
            beat.t_present = true;
            beat.rr_was_clipped = false;
            return beat;
        }

        void generate_sequential_timeline(const clinical_ecg_config& config, double duration_seconds, generated_clinical_data& output)
        {
            const double nominal_rr = 60.0 / config.rhythm.heart_rate_bpm;
            double previous_r = -1.0;
            double next_r = 0.5;
            bool previous_premature = false;
            unsigned long long beat_index = 0;
            while (next_r < duration_seconds)
            {
                double rr = nominal_rr;
                bool rr_was_clipped = false;
                clinical_ventricular_origin origin = clinical_origin_conducted;
                if (config.rhythm.rhythm == clinical_rhythm_ventricular_tachycardia)
                    origin = clinical_origin_vt;
                else if (config.rhythm.rhythm == clinical_rhythm_paced)
                    origin = clinical_origin_paced;
                if (beat_index > 0)
                {
                    rr += config.rhythm.rr_variability_seconds * deterministic_normal(config.rhythm.seed, beat_index);
                    if (previous_premature)
                    {
                        rr = nominal_rr * config.scenario.compensatory_pause_ratio;
                        previous_premature = false;
                    }
                    else if (config.scenario.premature_every_n_beats > 0 && (beat_index + 1) % config.scenario.premature_every_n_beats == 0)
                    {
                        rr = nominal_rr * config.scenario.premature_coupling_ratio;
                        origin = config.scenario.premature_origin;
                        previous_premature = true;
                    }
                    else if (config.scenario.sinus_pause_every_n_beats > 0 && (beat_index + 1) % config.scenario.sinus_pause_every_n_beats == 0)
                        rr = nominal_rr * config.scenario.sinus_pause_ratio;
                    const double unclipped_rr = rr;
                    rr = std::max(config.rhythm.minimum_rr_seconds, std::min(config.rhythm.maximum_rr_seconds, rr));
                    rr_was_clipped = rr != unclipped_rr;
                    next_r = previous_r + rr;
                }
                else
                {
                    const double unclipped_rr = rr;
                    rr = std::max(config.rhythm.minimum_rr_seconds, std::min(config.rhythm.maximum_rr_seconds, rr));
                    rr_was_clipped = rr != unclipped_rr;
                }
                if (next_r >= duration_seconds)
                    break;
                const double qrs_duration = adjusted_qrs_duration(config, origin);
                const double qrs_onset = next_r - config.timing.qrs_r_fraction * qrs_duration;
                double pr = adjusted_pr_interval(config);
                bool p_visible = config.rhythm.rhythm == clinical_rhythm_sinus;
                if (config.rhythm.rhythm == clinical_rhythm_supraventricular_tachycardia)
                    p_visible = false;
                if (origin == clinical_origin_pvc || origin == clinical_origin_paced || origin == clinical_origin_vt)
                    p_visible = false;
                long long atrial_index = -1;
                if (p_visible || origin == clinical_origin_pac)
                {
                    clinical_atrial_event atrial = {};
                    atrial.atrial_index = output.atrial_events.size();
                    atrial.onset_time_seconds = qrs_onset - pr;
                    atrial.peak_time_seconds = atrial.onset_time_seconds + 0.5 * config.timing.p_duration_ms * 0.001;
                    atrial.offset_time_seconds = atrial.onset_time_seconds + config.timing.p_duration_ms * 0.001;
                    atrial.visible = true;
                    atrial.conducted = true;
                    atrial.linked_ventricular_index = static_cast<long long>(beat_index);
                    atrial_index = static_cast<long long>(atrial.atrial_index);
                    output.atrial_events.push_back(atrial);
                }
                clinical_beat_annotation beat = make_beat(config, beat_index, next_r, beat_index == 0 ? nominal_rr : next_r - previous_r, atrial_index, atrial_index >= 0 ? pr : 0.0, origin);
                beat.rr_was_clipped = rr_was_clipped;
                output.beats.push_back(beat);
                previous_r = next_r;
                ++beat_index;
            }
        }

        void add_sinus_beat(const clinical_ecg_config& config, double r_time, double previous_r, double nominal_rr, generated_clinical_data& output)
        {
            const unsigned long long beat_index = output.beats.size();
            const double qrs_duration = adjusted_qrs_duration(config, clinical_origin_conducted);
            const double qrs_onset = r_time - config.timing.qrs_r_fraction * qrs_duration;
            const double pr = adjusted_pr_interval(config);
            clinical_atrial_event atrial = {};
            atrial.atrial_index = output.atrial_events.size();
            atrial.onset_time_seconds = qrs_onset - pr;
            atrial.peak_time_seconds = atrial.onset_time_seconds + 0.5 * config.timing.p_duration_ms * 0.001;
            atrial.offset_time_seconds = atrial.onset_time_seconds + config.timing.p_duration_ms * 0.001;
            atrial.visible = true;
            atrial.conducted = true;
            atrial.linked_ventricular_index = static_cast<long long>(beat_index);
            output.atrial_events.push_back(atrial);
            output.beats.push_back(make_beat(config, beat_index, r_time, previous_r < 0.0 ? nominal_rr : r_time - previous_r, static_cast<long long>(atrial.atrial_index), pr, clinical_origin_conducted));
        }

        void add_svt_beat(const clinical_ecg_config& config, double r_time, double previous_r, double episode_rr, generated_clinical_data& output)
        {
            const unsigned long long beat_index = output.beats.size();
            clinical_beat_annotation beat = make_beat(config, beat_index, r_time, previous_r < 0.0 ? episode_rr : r_time - previous_r, -1, 0.0, clinical_origin_conducted);
            beat.rhythm = clinical_rhythm_supraventricular_tachycardia;
            beat.p_present = false;
            output.beats.push_back(beat);
        }

        void generate_episode_timeline(const clinical_ecg_config& config, double duration_seconds, generated_clinical_data& output)
        {
            const double baseline_rr = 60.0 / config.rhythm.heart_rate_bpm;
            const double episode_rr = 60.0 / config.scenario.episode_rate_bpm;
            const double episode_start = config.scenario.episode_start_seconds;
            const double episode_end = episode_start + config.scenario.episode_duration_seconds;
            double previous_r = -1.0;
            for (double r_time = 0.5; r_time < duration_seconds && r_time < episode_start; r_time += baseline_rr)
            {
                add_sinus_beat(config, r_time, previous_r, baseline_rr, output);
                previous_r = r_time;
            }

            clinical_episode_annotation episode = {};
            episode.kind = config.scenario.episode_kind;
            episode.start_time_seconds = episode_start;
            episode.end_time_seconds = episode_end;
            episode.first_beat_index = NO_BEAT;
            episode.last_beat_index = NO_BEAT;
            episode.present = false;
            for (double r_time = episode_start; r_time < duration_seconds && r_time < episode_end; r_time += episode_rr)
            {
                if (episode.first_beat_index == NO_BEAT)
                    episode.first_beat_index = output.beats.size();
                add_svt_beat(config, r_time, previous_r, episode_rr, output);
                previous_r = r_time;
                episode.last_beat_index = output.beats.back().beat_index;
                episode.present = true;
            }
            output.episodes.push_back(episode);

            const double post_start = previous_r < 0.0 ? 0.5 : previous_r + baseline_rr;
            for (double r_time = std::max(post_start, episode_end + 0.001); r_time < duration_seconds; r_time += baseline_rr)
            {
                add_sinus_beat(config, r_time, previous_r, baseline_rr, output);
                previous_r = r_time;
            }
        }

        void generate_atrial_conduction_timeline(const clinical_ecg_config& config, double duration_seconds, generated_clinical_data& output)
        {
            const double atrial_period = 60.0 / config.rhythm.atrial_rate_bpm;
            const unsigned int cycle_length = config.rhythm.mobitz_cycle_length;
            double atrial_onset = 0.2;
            double previous_r = -1.0;
            unsigned long long atrial_index = 0;
            while (atrial_onset < duration_seconds)
            {
                const unsigned int cycle_position = static_cast<unsigned int>(atrial_index % cycle_length);
                bool conducted = true;
                double pr = config.timing.pr_interval_ms * 0.001;
                if (config.rhythm.av_conduction == clinical_av_first_degree)
                    pr = config.rhythm.first_degree_pr_ms * 0.001;
                else if (config.rhythm.av_conduction == clinical_av_mobitz_i)
                {
                    conducted = cycle_position + 1 < cycle_length;
                    pr += cycle_position * config.rhythm.wenckebach_pr_increment_ms * 0.001;
                }
                else if (config.rhythm.av_conduction == clinical_av_mobitz_ii)
                    conducted = cycle_position + 1 < cycle_length;
                clinical_atrial_event atrial = {};
                atrial.atrial_index = atrial_index;
                atrial.onset_time_seconds = atrial_onset;
                atrial.peak_time_seconds = atrial_onset + 0.5 * config.timing.p_duration_ms * 0.001;
                atrial.offset_time_seconds = atrial_onset + config.timing.p_duration_ms * 0.001;
                atrial.visible = true;
                atrial.conducted = conducted;
                atrial.linked_ventricular_index = conducted ? static_cast<long long>(output.beats.size()) : -1;
                output.atrial_events.push_back(atrial);
                if (conducted)
                {
                    const double qrs_onset = atrial_onset + pr;
                    const double qrs_duration = adjusted_qrs_duration(config, clinical_origin_conducted);
                    const double r_time = qrs_onset + config.timing.qrs_r_fraction * qrs_duration;
                    const double rr = previous_r < 0.0 ? 60.0 / config.rhythm.heart_rate_bpm : r_time - previous_r;
                    if (r_time < duration_seconds)
                    {
                        output.beats.push_back(make_beat(config, output.beats.size(), r_time, rr, static_cast<long long>(atrial_index), pr, clinical_origin_conducted));
                        previous_r = r_time;
                    }
                    else
                    {
                        output.atrial_events.back().conducted = false;
                        output.atrial_events.back().linked_ventricular_index = -1;
                    }
                }
                ++atrial_index;
                atrial_onset += atrial_period;
            }
        }

        void generate_complete_block_timeline(const clinical_ecg_config& config, double duration_seconds, generated_clinical_data& output)
        {
            const double atrial_period = 60.0 / config.rhythm.atrial_rate_bpm;
            for (double onset = 0.2; onset < duration_seconds; onset += atrial_period)
            {
                clinical_atrial_event atrial = {};
                atrial.atrial_index = output.atrial_events.size();
                atrial.onset_time_seconds = onset;
                atrial.peak_time_seconds = onset + 0.5 * config.timing.p_duration_ms * 0.001;
                atrial.offset_time_seconds = onset + config.timing.p_duration_ms * 0.001;
                atrial.visible = true;
                atrial.conducted = false;
                atrial.linked_ventricular_index = -1;
                output.atrial_events.push_back(atrial);
            }
            const double ventricular_period = 60.0 / config.rhythm.ventricular_escape_rate_bpm;
            double previous_r = -1.0;
            for (double r_time = 0.6; r_time < duration_seconds; r_time += ventricular_period)
            {
                const double rr = previous_r < 0.0 ? ventricular_period : r_time - previous_r;
                output.beats.push_back(make_beat(config, output.beats.size(), r_time, rr, -1, 0.0, clinical_origin_ventricular_escape));
                previous_r = r_time;
            }
        }

        void generate_flutter_timeline(const clinical_ecg_config& config, double duration_seconds, generated_clinical_data& output)
        {
            const double atrial_period = 60.0 / config.rhythm.atrial_rate_bpm;
            double previous_r = -1.0;
            unsigned long long atrial_index = 0;
            for (double onset = 0.15; onset < duration_seconds; onset += atrial_period, ++atrial_index)
            {
                const bool conducted = (atrial_index + 1) % config.rhythm.flutter_conduction_ratio == 0;
                clinical_atrial_event atrial = {};
                atrial.atrial_index = atrial_index;
                atrial.onset_time_seconds = onset;
                atrial.peak_time_seconds = onset + 0.73 * atrial_period;
                atrial.offset_time_seconds = onset + atrial_period;
                atrial.visible = true;
                atrial.conducted = conducted;
                atrial.linked_ventricular_index = conducted ? static_cast<long long>(output.beats.size()) : -1;
                output.atrial_events.push_back(atrial);
                if (conducted)
                {
                    const double pr = 0.100;
                    const double qrs_onset = onset + pr;
                    const double qrs_duration = adjusted_qrs_duration(config, clinical_origin_conducted);
                    const double r_time = qrs_onset + config.timing.qrs_r_fraction * qrs_duration;
                    const double rr = previous_r < 0.0 ? atrial_period * config.rhythm.flutter_conduction_ratio : r_time - previous_r;
                    if (r_time < duration_seconds)
                    {
                        output.beats.push_back(make_beat(config, output.beats.size(), r_time, rr, static_cast<long long>(atrial_index), pr, clinical_origin_conducted));
                        output.beats.back().rhythm = clinical_rhythm_atrial_flutter;
                        previous_r = r_time;
                    }
                    else
                    {
                        output.atrial_events.back().conducted = false;
                        output.atrial_events.back().linked_ventricular_index = -1;
                    }
                }
            }
        }

        void generate_af_timeline(const clinical_ecg_config& config, double duration_seconds, generated_clinical_data& output)
        {
            const double nominal_rr = 60.0 / config.rhythm.heart_rate_bpm;
            double previous_r = -1.0;
            double r_time = 0.5;
            unsigned long long beat_index = 0;
            while (r_time < duration_seconds)
            {
                const double unbounded_rr = beat_index == 0 ? nominal_rr : nominal_rr + std::max(0.080, config.rhythm.rr_variability_seconds) * deterministic_normal(config.rhythm.seed, beat_index);
                const double rr = std::max(config.rhythm.minimum_rr_seconds, std::min(config.rhythm.maximum_rr_seconds, unbounded_rr));
                if (beat_index > 0)
                    r_time = previous_r + rr;
                if (r_time >= duration_seconds)
                    break;
                clinical_beat_annotation beat = make_beat(config, beat_index, r_time, rr, -1, 0.0, clinical_origin_conducted);
                beat.rhythm = clinical_rhythm_atrial_fibrillation;
                beat.p_present = false;
                beat.rr_was_clipped = rr != unbounded_rr;
                output.beats.push_back(beat);
                previous_r = r_time;
                ++beat_index;
            }
        }

        void apply_source_presence(const clinical_ecg_config& config, generated_clinical_data& output)
        {
            const bool p_source_present = config.sources.gain[clinical_source_atrial] > 0.0 && std::fabs(config.morphology.p_amplitude_mv) > 0.0;
            const bool t_source_present = config.sources.gain[clinical_source_repolarization] > 0.0 && std::fabs(config.morphology.t_amplitude_mv) > 0.0;
            for (clinical_atrial_event& atrial : output.atrial_events)
                atrial.visible = atrial.visible && p_source_present;
            for (clinical_beat_annotation& beat : output.beats)
            {
                beat.p_present = beat.p_present && beat.linked_atrial_index >= 0 && static_cast<unsigned long long>(beat.linked_atrial_index) < output.atrial_events.size() && output.atrial_events[beat.linked_atrial_index].visible;
                beat.t_present = beat.t_present && t_source_present;
            }
        }

        void render_compact_wave(std::vector<vec3>& source, unsigned int sampling_rate, double onset, double peak, double offset, const vec3& amplitude)
        {
            if (!(onset < peak && peak < offset))
                return;
            const int first = std::max(0, static_cast<int>(std::floor(onset * sampling_rate)));
            const int last = std::min(static_cast<int>(source.size()) - 1, static_cast<int>(std::ceil(offset * sampling_rate)));
            for (int sample = first; sample <= last; ++sample)
            {
                const double time = static_cast<double>(sample) / sampling_rate;
                double shape = 0.0;
                if (time >= onset && time <= peak)
                    shape = smoothstep((time - onset) / (peak - onset));
                else if (time > peak && time <= offset)
                    shape = 1.0 - smoothstep((time - peak) / (offset - peak));
                source[sample] = add(source[sample], scale(amplitude, shape));
            }
        }

        void render_injury_wave(std::vector<vec3>& source, unsigned int sampling_rate, const clinical_beat_annotation& beat, const vec3& j_value, const vec3& st_slope)
        {
            const vec3 zero = vec3{0.0, 0.0, 0.0};
            const double terminal_duration = beat.j_point_time_seconds - beat.s_peak_time_seconds;
            const double t_return_duration = beat.t_offset_time_seconds - beat.t_onset_time_seconds;
            if (terminal_duration <= 0.0 || t_return_duration <= 0.0)
                return;
            const vec3 st_end = add(j_value, scale(st_slope, beat.t_onset_time_seconds - beat.j_point_time_seconds));
            const int first = std::max(0, static_cast<int>(std::floor(beat.s_peak_time_seconds * sampling_rate)));
            const int last = std::min(static_cast<int>(source.size()) - 1, static_cast<int>(std::ceil(beat.t_offset_time_seconds * sampling_rate)));
            for (int sample = first; sample <= last; ++sample)
            {
                const double time = static_cast<double>(sample) / sampling_rate;
                if (time < beat.s_peak_time_seconds || time > beat.t_offset_time_seconds)
                    continue;
                vec3 value = zero;
                if (time <= beat.j_point_time_seconds)
                    value = cubic_hermite(zero, j_value, zero, st_slope, terminal_duration, (time - beat.s_peak_time_seconds) / terminal_duration);
                else if (time <= beat.t_onset_time_seconds)
                    value = add(j_value, scale(st_slope, time - beat.j_point_time_seconds));
                else
                    value = cubic_hermite(st_end, zero, st_slope, zero, t_return_duration, (time - beat.t_onset_time_seconds) / t_return_duration);
                source[sample] = add(source[sample], value);
            }
        }

        void render_sources(const clinical_ecg_config& config, generated_clinical_data& output, std::vector<vec3> sources[clinical_source_count])
        {
            for (int source = 0; source < clinical_source_count; ++source)
                sources[source].assign(output.sample_count, vec3{0.0, 0.0, 0.0});
            if (config.rhythm.rhythm == clinical_rhythm_atrial_fibrillation)
            {
                const vec3 fibrillation_axis = source_vector(config, clinical_source_atrial, 0.015, config.morphology.p_axis_degrees, config.morphology.p_elevation_degrees);
                for (unsigned int sample = 0; sample < output.sample_count; ++sample)
                {
                    const double time = static_cast<double>(sample) / output.sampling_rate_hz;
                    const double value = std::sin(TWO_PI * 6.1 * time + 0.7) + 0.6 * std::sin(TWO_PI * 7.3 * time + 2.1);
                    sources[clinical_source_atrial][sample] = add(sources[clinical_source_atrial][sample], scale(fibrillation_axis, value));
                }
            }
            for (const clinical_atrial_event& atrial : output.atrial_events)
            {
                if (!atrial.visible)
                    continue;
                double amplitude = config.morphology.p_amplitude_mv;
                if (config.rhythm.rhythm == clinical_rhythm_atrial_flutter)
                    amplitude *= -0.8;
                render_compact_wave(sources[clinical_source_atrial], output.sampling_rate_hz, atrial.onset_time_seconds, atrial.peak_time_seconds, atrial.offset_time_seconds, source_vector(config, clinical_source_atrial, amplitude, config.morphology.p_axis_degrees, config.morphology.p_elevation_degrees));
            }
            for (const clinical_beat_annotation& beat : output.beats)
            {
                double septal_axis = config.morphology.qrs_axis_degrees;
                double ventricular_axis = config.morphology.qrs_axis_degrees;
                double terminal_axis = config.morphology.qrs_axis_degrees;
                double q_amplitude = config.morphology.q_amplitude_mv;
                double r_amplitude = config.morphology.r_amplitude_mv;
                double s_amplitude = config.morphology.s_amplitude_mv;
                double t_amplitude = config.morphology.t_amplitude_mv;
                bool render_delta = false;
                if (beat.intraventricular_conduction == clinical_iv_lbbb)
                {
                    septal_axis += 180.0;
                    ventricular_axis -= 35.0;
                    terminal_axis -= 55.0;
                    q_amplitude = 0.0;
                    r_amplitude *= 1.05;
                    s_amplitude = std::fabs(s_amplitude) * 0.80;
                    t_amplitude *= -1.0;
                }
                else if (beat.intraventricular_conduction == clinical_iv_incomplete_lbbb)
                {
                    septal_axis += 170.0;
                    ventricular_axis -= 25.0;
                    terminal_axis -= 45.0;
                    q_amplitude = 0.0;
                    r_amplitude *= 1.00;
                    s_amplitude = std::fabs(s_amplitude) * 0.55;
                    t_amplitude *= -0.65;
                }
                else if (beat.intraventricular_conduction == clinical_iv_rbbb)
                {
                    terminal_axis -= 45.0;
                    s_amplitude *= 1.8;
                }
                else if (beat.intraventricular_conduction == clinical_iv_incomplete_rbbb)
                {
                    terminal_axis -= 45.0;
                    s_amplitude *= 1.35;
                    t_amplitude *= 0.75;
                }
                else if (beat.intraventricular_conduction == clinical_iv_left_anterior_fascicular)
                {
                    septal_axis = 40.0;
                    ventricular_axis = -60.0;
                    terminal_axis = -70.0;
                    q_amplitude *= 0.70;
                    r_amplitude *= 0.95;
                    s_amplitude = std::fabs(s_amplitude) * 0.85;
                }
                else if (beat.intraventricular_conduction == clinical_iv_left_posterior_fascicular)
                {
                    septal_axis = 40.0;
                    ventricular_axis = 120.0;
                    terminal_axis = 130.0;
                    q_amplitude *= 0.70;
                    r_amplitude *= 0.95;
                    s_amplitude = std::fabs(s_amplitude) * 0.85;
                }
                else if (beat.intraventricular_conduction == clinical_iv_nonspecific_delay)
                {
                    septal_axis -= 10.0;
                    ventricular_axis += 15.0;
                    terminal_axis += 35.0;
                    q_amplitude *= 0.80;
                    r_amplitude *= 0.95;
                    s_amplitude *= 1.10;
                    t_amplitude *= 0.85;
                }
                if (config.rhythm.preexcitation == clinical_preexcitation_wpw && beat.origin == clinical_origin_conducted)
                {
                    render_delta = true;
                    q_amplitude = 0.0;
                    septal_axis = config.morphology.qrs_axis_degrees - 15.0;
                    ventricular_axis = config.morphology.qrs_axis_degrees + 10.0;
                    terminal_axis = config.morphology.qrs_axis_degrees + 35.0;
                    r_amplitude *= 0.95;
                    s_amplitude *= 0.90;
                    t_amplitude = -std::fabs(t_amplitude) * 0.90;
                }
                if (beat.origin == clinical_origin_pvc || beat.origin == clinical_origin_ventricular_escape || beat.origin == clinical_origin_vt || beat.origin == clinical_origin_paced)
                {
                    septal_axis += 90.0;
                    ventricular_axis += 90.0;
                    terminal_axis += 120.0;
                    q_amplitude *= 0.5;
                    r_amplitude *= 0.85;
                    s_amplitude *= 1.8;
                    t_amplitude *= -0.8;
                }
                if (render_delta)
                    render_compact_wave(sources[clinical_source_septal], output.sampling_rate_hz, beat.qrs_onset_time_seconds, beat.q_peak_time_seconds, beat.r_peak_time_seconds, source_vector(config, clinical_source_septal, 0.26, septal_axis, config.morphology.qrs_elevation_degrees));
                render_compact_wave(sources[clinical_source_septal], output.sampling_rate_hz, beat.qrs_onset_time_seconds, beat.q_peak_time_seconds, beat.r_peak_time_seconds, source_vector(config, clinical_source_septal, q_amplitude, septal_axis, config.morphology.qrs_elevation_degrees));
                render_compact_wave(sources[clinical_source_ventricular], output.sampling_rate_hz, beat.q_peak_time_seconds, beat.r_peak_time_seconds, beat.s_peak_time_seconds, source_vector(config, clinical_source_ventricular, r_amplitude, ventricular_axis, config.morphology.qrs_elevation_degrees));
                render_compact_wave(sources[clinical_source_terminal], output.sampling_rate_hz, beat.r_peak_time_seconds, beat.s_peak_time_seconds, beat.qrs_offset_time_seconds, source_vector(config, clinical_source_terminal, s_amplitude, terminal_axis, config.morphology.qrs_elevation_degrees));
                const vec3 j = source_vector(config, clinical_source_injury, config.morphology.st_j_amplitude_mv, config.morphology.t_axis_degrees, config.morphology.t_elevation_degrees);
                const vec3 st_slope = source_vector(config, clinical_source_injury, config.morphology.st_slope_mv_per_second, config.morphology.t_axis_degrees, config.morphology.t_elevation_degrees);
                render_injury_wave(sources[clinical_source_injury], output.sampling_rate_hz, beat, j, st_slope);
                render_compact_wave(sources[clinical_source_repolarization], output.sampling_rate_hz, beat.t_onset_time_seconds, beat.t_peak_time_seconds, beat.t_offset_time_seconds, source_vector(config, clinical_source_repolarization, t_amplitude, config.morphology.t_axis_degrees, config.morphology.t_elevation_degrees));
                if (beat.origin == clinical_origin_paced)
                {
                    const int spike_sample = static_cast<int>(std::llround(beat.qrs_onset_time_seconds * output.sampling_rate_hz));
                    if (spike_sample >= 0 && static_cast<unsigned int>(spike_sample) < sources[clinical_source_pacing].size())
                        sources[clinical_source_pacing][spike_sample] = add(sources[clinical_source_pacing][spike_sample], source_vector(config, clinical_source_pacing, 2.0, ventricular_axis, 20.0));
                }
            }
        }

        double axis_value(const vec3& value, int axis)
        {
            return axis == clinical_axis_x ? value.x : axis == clinical_axis_y ? value.y : value.z;
        }

        void combine_sources(const clinical_ecg_config& config, const std::vector<vec3> raw_sources[clinical_source_count], generated_clinical_data& output, std::vector<vec3>& total)
        {
            total.assign(output.sample_count, vec3{0.0, 0.0, 0.0});
            for (int source = 0; source < clinical_source_count; ++source)
                for (int axis = 0; axis < clinical_axis_count; ++axis)
                    output.sources[source][axis].assign(output.sample_count, 0.0);
            for (int axis = 0; axis < clinical_axis_count; ++axis)
                output.vcg[axis].assign(output.sample_count, 0.0);
            for (unsigned int sample = 0; sample < output.sample_count; ++sample)
            {
                for (int source = 0; source < clinical_source_count; ++source)
                {
                    const vec3 oriented = rotate_vector(raw_sources[source][sample], config.leads);
                    total[sample] = add(total[sample], oriented);
                    for (int axis = 0; axis < clinical_axis_count; ++axis)
                        output.sources[source][axis][sample] = axis_value(oriented, axis);
                }
                for (int axis = 0; axis < clinical_axis_count; ++axis)
                    output.vcg[axis][sample] = axis_value(total[sample], axis);
            }
        }

        void project_leads(const clinical_ecg_config& config, const std::vector<vec3>& source, generated_clinical_data& output)
        {
            const vec3 precordial_vectors[6] = {
                {-0.45, -0.08, -0.89},
                {-0.20, -0.05, -0.98},
                {0.15, 0.00, -0.92},
                {0.48, 0.04, -0.72},
                {0.78, 0.05, -0.38},
                {0.93, 0.03, -0.12}
            };
            for (int lead = 0; lead < clinical_lead_count; ++lead)
                output.leads[lead].assign(output.sample_count, 0.0);
            for (unsigned int sample = 0; sample < output.sample_count; ++sample)
            {
                const vec3 cardiac = source[sample];
                const double lead_i = cardiac.x;
                const double lead_ii = 0.5 * cardiac.x + 0.8660254037844386 * cardiac.y;
                output.leads[clinical_lead_i][sample] = config.leads.lead_gain[clinical_lead_i] * lead_i;
                output.leads[clinical_lead_ii][sample] = config.leads.lead_gain[clinical_lead_ii] * lead_ii;
                output.leads[clinical_lead_iii][sample] = config.leads.lead_gain[clinical_lead_iii] * (lead_ii - lead_i);
                output.leads[clinical_lead_avr][sample] = config.leads.lead_gain[clinical_lead_avr] * (-(lead_i + lead_ii) / 2.0);
                output.leads[clinical_lead_avl][sample] = config.leads.lead_gain[clinical_lead_avl] * (lead_i - lead_ii / 2.0);
                output.leads[clinical_lead_avf][sample] = config.leads.lead_gain[clinical_lead_avf] * (lead_ii - lead_i / 2.0);
                for (int chest = 0; chest < 6; ++chest)
                    output.leads[clinical_lead_v1 + chest][sample] = config.leads.lead_gain[clinical_lead_v1 + chest] * dot(cardiac, precordial_vectors[chest]);
            }
        }

        bool finite_output(const generated_clinical_data& output)
        {
            for (int lead = 0; lead < clinical_lead_count; ++lead)
                for (double sample : output.leads[lead])
                    if (!finite(sample))
                        return false;
            for (int source = 0; source < clinical_source_count; ++source)
                for (int axis = 0; axis < clinical_axis_count; ++axis)
                    for (double sample : output.sources[source][axis])
                        if (!finite(sample))
                            return false;
            for (int axis = 0; axis < clinical_axis_count; ++axis)
                for (double sample : output.vcg[axis])
                    if (!finite(sample))
                        return false;
            return true;
        }

        unsigned long long sample_index(double time_seconds, unsigned int sampling_rate, unsigned int sample_count)
        {
            if (time_seconds <= 0.0)
                return 0;
            const double value = std::llround(time_seconds * sampling_rate);
            if (value >= sample_count)
                return sample_count ? sample_count - 1 : 0;
            return static_cast<unsigned long long>(value);
        }

        void finalize_episode_samples(generated_clinical_data& output)
        {
            for (clinical_episode_annotation& episode : output.episodes)
            {
                episode.start_sample_index = sample_index(episode.start_time_seconds, output.sampling_rate_hz, output.sample_count);
                episode.end_sample_index = sample_index(episode.end_time_seconds, output.sampling_rate_hz, output.sample_count);
                episode.present = episode.present && episode.first_beat_index != NO_BEAT && episode.last_beat_index != NO_BEAT && episode.start_time_seconds < static_cast<double>(output.sample_count) / output.sampling_rate_hz;
            }
        }

        void add_fiducial(generated_clinical_data& output, unsigned long long beat_index, long long atrial_index, int lead_index, clinical_fiducial_kind kind, clinical_fiducial_source source, double time_seconds, double amplitude, bool present)
        {
            clinical_fiducial_annotation annotation = {};
            annotation.beat_index = beat_index;
            annotation.atrial_index = atrial_index;
            annotation.lead_index = lead_index;
            annotation.kind = kind;
            annotation.source = source;
            annotation.sample_index = sample_index(time_seconds, output.sampling_rate_hz, output.sample_count);
            annotation.time_seconds = time_seconds;
            annotation.amplitude_mv = amplitude;
            annotation.present = present && time_seconds >= 0.0 && time_seconds < static_cast<double>(output.sample_count) / output.sampling_rate_hz;
            output.fiducials.push_back(annotation);
        }

        void build_construction_fiducials(generated_clinical_data& output)
        {
            for (const clinical_atrial_event& atrial : output.atrial_events)
            {
                const unsigned long long beat_index = atrial.linked_ventricular_index >= 0 ? static_cast<unsigned long long>(atrial.linked_ventricular_index) : NO_BEAT;
                add_fiducial(output, beat_index, static_cast<long long>(atrial.atrial_index), -1, clinical_p_onset, clinical_fiducial_construction, atrial.onset_time_seconds, 0.0, atrial.visible);
                add_fiducial(output, beat_index, static_cast<long long>(atrial.atrial_index), -1, clinical_p_peak, clinical_fiducial_construction, atrial.peak_time_seconds, 0.0, atrial.visible);
                add_fiducial(output, beat_index, static_cast<long long>(atrial.atrial_index), -1, clinical_p_offset, clinical_fiducial_construction, atrial.offset_time_seconds, 0.0, atrial.visible);
            }
            for (const clinical_beat_annotation& beat : output.beats)
            {
                add_fiducial(output, beat.beat_index, beat.linked_atrial_index, -1, clinical_qrs_onset, clinical_fiducial_construction, beat.qrs_onset_time_seconds, 0.0, beat.qrs_present);
                add_fiducial(output, beat.beat_index, beat.linked_atrial_index, -1, clinical_q_peak, clinical_fiducial_construction, beat.q_peak_time_seconds, 0.0, beat.qrs_present);
                add_fiducial(output, beat.beat_index, beat.linked_atrial_index, -1, clinical_r_peak, clinical_fiducial_construction, beat.r_peak_time_seconds, 0.0, beat.qrs_present);
                add_fiducial(output, beat.beat_index, beat.linked_atrial_index, -1, clinical_s_peak, clinical_fiducial_construction, beat.s_peak_time_seconds, 0.0, beat.qrs_present);
                add_fiducial(output, beat.beat_index, beat.linked_atrial_index, -1, clinical_j_point, clinical_fiducial_construction, beat.j_point_time_seconds, 0.0, beat.qrs_present);
                add_fiducial(output, beat.beat_index, beat.linked_atrial_index, -1, clinical_qrs_offset, clinical_fiducial_construction, beat.qrs_offset_time_seconds, 0.0, beat.qrs_present);
                add_fiducial(output, beat.beat_index, beat.linked_atrial_index, -1, clinical_t_onset, clinical_fiducial_construction, beat.t_onset_time_seconds, 0.0, beat.t_present);
                add_fiducial(output, beat.beat_index, beat.linked_atrial_index, -1, clinical_t_peak, clinical_fiducial_construction, beat.t_peak_time_seconds, 0.0, beat.t_present);
                add_fiducial(output, beat.beat_index, beat.linked_atrial_index, -1, clinical_t_offset, clinical_fiducial_construction, beat.t_offset_time_seconds, 0.0, beat.t_present);
                if (beat.origin == clinical_origin_paced)
                    add_fiducial(output, beat.beat_index, beat.linked_atrial_index, -1, clinical_pacing_spike, clinical_fiducial_construction, beat.qrs_onset_time_seconds, 2.0, true);
            }
        }

        void measure_peak(generated_clinical_data& output, const clinical_ecg_config& config, unsigned long long beat_index, long long atrial_index, int lead_index, clinical_fiducial_kind kind, double start_time, double end_time)
        {
            if (!(start_time < end_time) || lead_index < 0 || lead_index >= clinical_lead_count)
                return;
            const double duration = static_cast<double>(output.sample_count) / output.sampling_rate_hz;
            if (start_time < 0.0 || end_time >= duration)
                return;
            const std::vector<double>& signal = output.leads[lead_index];
            const unsigned int start = static_cast<unsigned int>(sample_index(start_time, output.sampling_rate_hz, output.sample_count));
            const unsigned int end = static_cast<unsigned int>(sample_index(end_time, output.sampling_rate_hz, output.sample_count));
            if (end <= start)
                return;
            const double start_value = signal[start];
            const double end_value = signal[end];
            unsigned int best = start;
            double best_score = -1.0;
            for (unsigned int sample = start; sample <= end; ++sample)
            {
                const double position = static_cast<double>(sample - start) / static_cast<double>(end - start);
                const double baseline = start_value + position * (end_value - start_value);
                const double score = std::fabs(signal[sample] - baseline);
                if (score > best_score)
                {
                    best_score = score;
                    best = sample;
                }
            }
            const bool present = best_score >= config.morphology.presence_threshold_mv;
            add_fiducial(output, beat_index, atrial_index, lead_index, kind, clinical_fiducial_lead_measurement, static_cast<double>(best) / output.sampling_rate_hz, signal[best], present);
        }

        void build_lead_measurements(const clinical_ecg_config& config, generated_clinical_data& output)
        {
            for (const clinical_atrial_event& atrial : output.atrial_events)
            {
                if (!atrial.visible)
                    continue;
                const unsigned long long beat_index = atrial.linked_ventricular_index >= 0 ? static_cast<unsigned long long>(atrial.linked_ventricular_index) : NO_BEAT;
                for (int lead = 0; lead < clinical_lead_count; ++lead)
                    measure_peak(output, config, beat_index, static_cast<long long>(atrial.atrial_index), lead, clinical_p_peak, atrial.onset_time_seconds, atrial.offset_time_seconds);
            }
            for (const clinical_beat_annotation& beat : output.beats)
            {
                for (int lead = 0; lead < clinical_lead_count; ++lead)
                {
                    if (beat.qrs_present)
                    {
                        measure_peak(output, config, beat.beat_index, beat.linked_atrial_index, lead, clinical_q_peak, beat.qrs_onset_time_seconds, 0.5 * (beat.q_peak_time_seconds + beat.r_peak_time_seconds));
                        measure_peak(output, config, beat.beat_index, beat.linked_atrial_index, lead, clinical_r_peak, 0.5 * (beat.q_peak_time_seconds + beat.r_peak_time_seconds), 0.5 * (beat.r_peak_time_seconds + beat.s_peak_time_seconds));
                        measure_peak(output, config, beat.beat_index, beat.linked_atrial_index, lead, clinical_s_peak, 0.5 * (beat.r_peak_time_seconds + beat.s_peak_time_seconds), beat.qrs_offset_time_seconds);
                    }
                    if (beat.t_present)
                        measure_peak(output, config, beat.beat_index, beat.linked_atrial_index, lead, clinical_t_peak, beat.t_onset_time_seconds, beat.t_offset_time_seconds);
                }
            }
        }

        const char* lead_names[clinical_lead_count] = {"I", "II", "III", "aVR", "aVL", "aVF", "V1", "V2", "V3", "V4", "V5", "V6"};
        const char* source_names[clinical_source_count] = {"Atrial", "Septal", "Ventricular", "Terminal", "Repolarization", "Injury", "Pacing"};
    }

    clinical_timing_config::clinical_timing_config()
        : p_duration_ms(100.0), pr_interval_ms(160.0), qrs_duration_ms(90.0), qrs_q_fraction(0.18), qrs_r_fraction(0.42), qrs_s_fraction(0.72), t_duration_ms(180.0), t_peak_fraction(0.45), qt_interval_ms(400.0), qtc_ms(400.0), qt_correction(clinical_qt_fridericia)
    {
    }

    clinical_morphology_config::clinical_morphology_config()
        : p_amplitude_mv(0.12), q_amplitude_mv(-0.15), r_amplitude_mv(1.0), s_amplitude_mv(-0.28), t_amplitude_mv(0.30), st_j_amplitude_mv(0.0), st_slope_mv_per_second(0.0), p_axis_degrees(55.0), qrs_axis_degrees(45.0), t_axis_degrees(40.0), p_elevation_degrees(10.0), qrs_elevation_degrees(20.0), t_elevation_degrees(15.0), presence_threshold_mv(0.015)
    {
    }

    clinical_rhythm_config::clinical_rhythm_config()
        : rhythm(clinical_rhythm_sinus), av_conduction(clinical_av_normal), intraventricular_conduction(clinical_iv_normal), preexcitation(clinical_preexcitation_none), heart_rate_bpm(60.0), atrial_rate_bpm(75.0), ventricular_escape_rate_bpm(35.0), rr_variability_seconds(0.0), minimum_rr_seconds(0.25), maximum_rr_seconds(3.0), first_degree_pr_ms(240.0), mobitz_cycle_length(4), wenckebach_pr_increment_ms(40.0), flutter_conduction_ratio(2), seed(0x434c494e4943414cULL)
    {
    }

    clinical_scenario_config::clinical_scenario_config()
        : premature_every_n_beats(0), premature_origin(clinical_origin_pvc), premature_coupling_ratio(0.65), compensatory_pause_ratio(1.35), sinus_pause_every_n_beats(0), sinus_pause_ratio(2.0), episode_kind(clinical_episode_none), episode_start_seconds(2.0), episode_duration_seconds(4.0), episode_rate_bpm(170.0)
    {
    }

    clinical_lead_config::clinical_lead_config()
        : yaw_degrees(0.0), pitch_degrees(0.0), roll_degrees(0.0)
    {
        for (int lead = 0; lead < clinical_lead_count; ++lead)
            lead_gain[lead] = 1.0;
    }

    clinical_source_config::clinical_source_config()
    {
        for (int source = 0; source < clinical_source_count; ++source)
        {
            gain[source] = 1.0;
            axis_offset_degrees[source] = 0.0;
            elevation_offset_degrees[source] = 0.0;
        }
    }

    clinical_ecg_config::clinical_ecg_config()
        : sampling_rate_hz(500)
    {
    }

    struct clinical_ecg_record::implementation
    {
        unsigned int sampling_rate_hz;
        unsigned int sample_count;
        std::vector<double> leads[clinical_lead_count];
        std::vector<double> sources[clinical_source_count][clinical_axis_count];
        std::vector<double> vcg[clinical_axis_count];
        std::vector<clinical_atrial_event> atrial_events;
        std::vector<clinical_beat_annotation> beats;
        std::vector<clinical_fiducial_annotation> fiducials;
        std::vector<clinical_episode_annotation> episodes;

        implementation()
            : sampling_rate_hz(0), sample_count(0)
        {
        }
    };

    clinical_ecg_record::clinical_ecg_record()
        : implementation_(new implementation)
    {
    }

    clinical_ecg_record::clinical_ecg_record(const clinical_ecg_record& other)
        : implementation_(new implementation(*other.implementation_))
    {
    }

    clinical_ecg_record& clinical_ecg_record::operator=(const clinical_ecg_record& other)
    {
        if (this != &other)
            *implementation_ = *other.implementation_;
        return *this;
    }

    clinical_ecg_record::~clinical_ecg_record()
    {
        delete implementation_;
    }

    unsigned int clinical_ecg_record::sampling_rate_hz() const
    {
        return implementation_->sampling_rate_hz;
    }

    unsigned int clinical_ecg_record::sample_count() const
    {
        return implementation_->sample_count;
    }

    unsigned int clinical_ecg_record::lead_count() const
    {
        return clinical_lead_count;
    }

    const char* clinical_ecg_record::lead_name(unsigned int lead_index) const
    {
        return lead_index < clinical_lead_count ? lead_names[lead_index] : "";
    }

    const double* clinical_ecg_record::lead_data(unsigned int lead_index) const
    {
        return lead_index < clinical_lead_count && !implementation_->leads[lead_index].empty() ? implementation_->leads[lead_index].data() : 0;
    }

    unsigned int clinical_ecg_record::source_count() const
    {
        return clinical_source_count;
    }

    const char* clinical_ecg_record::source_name(unsigned int source_index) const
    {
        return source_index < clinical_source_count ? source_names[source_index] : "";
    }

    const double* clinical_ecg_record::source_data(unsigned int source_index, unsigned int axis_index) const
    {
        return source_index < clinical_source_count && axis_index < clinical_axis_count && !implementation_->sources[source_index][axis_index].empty() ? implementation_->sources[source_index][axis_index].data() : 0;
    }

    const double* clinical_ecg_record::vcg_data(unsigned int axis_index) const
    {
        return axis_index < clinical_axis_count && !implementation_->vcg[axis_index].empty() ? implementation_->vcg[axis_index].data() : 0;
    }

    unsigned int clinical_ecg_record::atrial_event_count() const
    {
        return static_cast<unsigned int>(implementation_->atrial_events.size());
    }

    const clinical_atrial_event* clinical_ecg_record::atrial_events() const
    {
        return implementation_->atrial_events.empty() ? 0 : implementation_->atrial_events.data();
    }

    unsigned int clinical_ecg_record::beat_count() const
    {
        return static_cast<unsigned int>(implementation_->beats.size());
    }

    const clinical_beat_annotation* clinical_ecg_record::beats() const
    {
        return implementation_->beats.empty() ? 0 : implementation_->beats.data();
    }

    unsigned int clinical_ecg_record::fiducial_count() const
    {
        return static_cast<unsigned int>(implementation_->fiducials.size());
    }

    const clinical_fiducial_annotation* clinical_ecg_record::fiducials() const
    {
        return implementation_->fiducials.empty() ? 0 : implementation_->fiducials.data();
    }

    unsigned int clinical_ecg_record::episode_count() const
    {
        return static_cast<unsigned int>(implementation_->episodes.size());
    }

    const clinical_episode_annotation* clinical_ecg_record::episodes() const
    {
        return implementation_->episodes.empty() ? 0 : implementation_->episodes.data();
    }

    struct clinical_ecg_generator::implementation
    {
        clinical_ecg_config config;
        bool configured;

        implementation()
            : configured(false)
        {
        }
    };

    clinical_ecg_generator::clinical_ecg_generator()
        : implementation_(new implementation)
    {
        configure(clinical_ecg_config());
    }

    clinical_ecg_generator::clinical_ecg_generator(const clinical_ecg_config& config)
        : implementation_(new implementation)
    {
        configure(config);
    }

    clinical_ecg_generator::clinical_ecg_generator(const clinical_ecg_generator& other)
        : implementation_(new implementation(*other.implementation_))
    {
    }

    clinical_ecg_generator& clinical_ecg_generator::operator=(const clinical_ecg_generator& other)
    {
        if (this != &other)
            *implementation_ = *other.implementation_;
        return *this;
    }

    clinical_ecg_generator::~clinical_ecg_generator()
    {
        delete implementation_;
    }

    bool clinical_ecg_generator::configure(const clinical_ecg_config& config)
    {
        if (!valid_config(config))
            return false;
        implementation_->config = config;
        implementation_->configured = true;
        return true;
    }

    bool clinical_ecg_generator::valid() const
    {
        return implementation_->configured;
    }

    const clinical_ecg_config& clinical_ecg_generator::config() const
    {
        return implementation_->config;
    }

    bool clinical_ecg_generator::generate(unsigned int sample_count, clinical_ecg_record& output) const
    {
        if (!implementation_->configured || sample_count == 0)
            return false;
        try
        {
            generated_clinical_data generated = {};
            generated.sampling_rate_hz = implementation_->config.sampling_rate_hz;
            generated.sample_count = sample_count;
            const double duration = static_cast<double>(sample_count) / generated.sampling_rate_hz;
            if (implementation_->config.scenario.episode_kind != clinical_episode_none)
                generate_episode_timeline(implementation_->config, duration, generated);
            else if (implementation_->config.rhythm.rhythm == clinical_rhythm_atrial_fibrillation)
                generate_af_timeline(implementation_->config, duration, generated);
            else if (implementation_->config.rhythm.rhythm == clinical_rhythm_atrial_flutter)
                generate_flutter_timeline(implementation_->config, duration, generated);
            else if (implementation_->config.rhythm.av_conduction == clinical_av_complete_block)
                generate_complete_block_timeline(implementation_->config, duration, generated);
            else if (implementation_->config.rhythm.av_conduction != clinical_av_normal)
                generate_atrial_conduction_timeline(implementation_->config, duration, generated);
            else
                generate_sequential_timeline(implementation_->config, duration, generated);
            apply_source_presence(implementation_->config, generated);
            std::vector<vec3> raw_sources[clinical_source_count];
            std::vector<vec3> total_source;
            render_sources(implementation_->config, generated, raw_sources);
            combine_sources(implementation_->config, raw_sources, generated, total_source);
            project_leads(implementation_->config, total_source, generated);
            if (!finite_output(generated))
                return false;
            finalize_episode_samples(generated);
            build_construction_fiducials(generated);
            build_lead_measurements(implementation_->config, generated);
            clinical_ecg_record::implementation completed;
            completed.sampling_rate_hz = generated.sampling_rate_hz;
            completed.sample_count = generated.sample_count;
            for (int lead = 0; lead < clinical_lead_count; ++lead)
                completed.leads[lead].swap(generated.leads[lead]);
            for (int source = 0; source < clinical_source_count; ++source)
                for (int axis = 0; axis < clinical_axis_count; ++axis)
                    completed.sources[source][axis].swap(generated.sources[source][axis]);
            for (int axis = 0; axis < clinical_axis_count; ++axis)
                completed.vcg[axis].swap(generated.vcg[axis]);
            completed.atrial_events.swap(generated.atrial_events);
            completed.beats.swap(generated.beats);
            completed.fiducials.swap(generated.fiducials);
            completed.episodes.swap(generated.episodes);
            std::swap(*output.implementation_, completed);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
}
