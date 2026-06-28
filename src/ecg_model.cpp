#include "ecg_model.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

// Independent implementation of the equations published in
// McSharry et al., IEEE TBME 50(3), 2003, DOI 10.1109/TBME.2003.808805.
// The GPL-licensed ECGSYN source code is not an implementation input.

namespace signal_synth
{
    namespace
    {
        const double PI = 3.14159265358979323846;
        const double TWO_PI = 2.0 * PI;

        struct model_state
        {
            double x;
            double y;
            double z;
        };

        struct derivative
        {
            double x;
            double y;
            double z;
        };

        struct rr_oscillator
        {
            double frequency_hz;
            double amplitude_seconds;
            double phase_radians;
        };

        bool finite(double value)
        {
            return std::isfinite(value);
        }

        bool valid_hrv_config(const ecg_hrv_config& config)
        {
            return finite(config.rr_standard_deviation_seconds) &&
                finite(config.lf_hf_ratio) &&
                finite(config.lf_center_frequency_hz) &&
                finite(config.hf_center_frequency_hz) &&
                finite(config.lf_bandwidth_hz) &&
                finite(config.hf_bandwidth_hz) &&
                finite(config.minimum_rr_seconds) &&
                finite(config.maximum_rr_seconds) &&
                config.rr_standard_deviation_seconds >= 0.0 &&
                config.lf_hf_ratio >= 0.0 &&
                config.lf_center_frequency_hz > 0.0 &&
                config.hf_center_frequency_hz > 0.0 &&
                config.lf_bandwidth_hz >= 0.0 &&
                config.hf_bandwidth_hz >= 0.0 &&
                config.lf_center_frequency_hz >
                    2.5 * config.lf_bandwidth_hz &&
                config.hf_center_frequency_hz >
                    2.5 * config.hf_bandwidth_hz &&
                config.minimum_rr_seconds > 0.0 &&
                config.maximum_rr_seconds >=
                    config.minimum_rr_seconds;
        }

        bool valid_scenario_config(const ecg_scenario_config& config)
        {
            return finite(config.premature_probability) &&
                finite(config.premature_rr_ratio) &&
                finite(config.compensatory_pause_ratio) &&
                finite(config.premature_p_amplitude_scale) &&
                finite(config.premature_qrs_amplitude_scale) &&
                finite(config.premature_qrs_width_scale) &&
                finite(config.premature_t_amplitude_scale) &&
                config.premature_probability >= 0.0 &&
                config.premature_probability <= 1.0 &&
                config.premature_rr_ratio > 0.0 &&
                config.compensatory_pause_ratio > 0.0 &&
                config.premature_qrs_width_scale > 0.0;
        }

        std::uint64_t mix64(std::uint64_t value)
        {
            value += 0x9e3779b97f4a7c15ULL;
            value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
            value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
            return value ^ (value >> 31);
        }

        double deterministic_unit(
            unsigned long long seed,
            unsigned long long index)
        {
            const std::uint64_t value = mix64(
                static_cast<std::uint64_t>(seed) ^
                mix64(static_cast<std::uint64_t>(index)));
            return static_cast<double>(value >> 11) *
                (1.0 / 9007199254740992.0);
        }

        double wrapped_angle(double angle)
        {
            angle = std::fmod(angle + PI, TWO_PI);
            if (angle < 0.0)
                angle += TWO_PI;
            return angle - PI;
        }

        bool valid_config(const ecg_model_config& config)
        {
            if (config.sampling_rate_hz == 0 ||
                config.sampling_rate_hz > 1000000 ||
                !finite(config.heart_rate_bpm) ||
                !finite(config.baseline_amplitude_mv) ||
                !finite(config.respiration_frequency_hz) ||
                config.heart_rate_bpm < 1.0 ||
                config.heart_rate_bpm > 1000.0 ||
                config.baseline_amplitude_mv < 0.0 ||
                config.respiration_frequency_hz < 0.0 ||
                !valid_hrv_config(config.hrv) ||
                !valid_scenario_config(config.scenario))
            {
                return false;
            }

            const double phase_step =
                TWO_PI /
                (config.hrv.minimum_rr_seconds *
                    static_cast<double>(config.sampling_rate_hz));
            if (!finite(phase_step) || phase_step > PI / 4.0 ||
                config.respiration_frequency_hz >
                    std::min(
                        5.0,
                        0.5 * static_cast<double>(
                            config.sampling_rate_hz)))
            {
                return false;
            }

            double previous_phase = -PI;
            for (int i = 0; i < ecg_wave_count; ++i)
            {
                const ecg_wave_config& wave = config.waves[i];
                if (!finite(wave.phase_radians) ||
                    !finite(wave.amplitude) ||
                    !finite(wave.width_radians) ||
                    wave.phase_radians <= previous_phase ||
                    wave.phase_radians >= PI ||
                    wave.width_radians <= 0.0 ||
                    wave.width_radians > PI)
                {
                    return false;
                }
                previous_phase = wave.phase_radians;
            }
            return true;
        }

        derivative evaluate(
            const ecg_model_config& config,
            ecg_beat_kind beat_kind,
            const model_state& state,
            double time_seconds,
            double unwrapped_phase,
            double omega)
        {
            const double radius =
                std::sqrt(state.x * state.x + state.y * state.y);
            const double alpha = 1.0 - radius;
            const double theta = wrapped_angle(unwrapped_phase);

            double morphology = 0.0;
            for (int i = 0; i < ecg_wave_count; ++i)
            {
                const ecg_wave_config& wave = config.waves[i];
                double amplitude_scale = 1.0;
                double width_scale = 1.0;
                if (beat_kind == ecg_beat_premature)
                {
                    if (i == ecg_wave_p)
                        amplitude_scale =
                            config.scenario
                                .premature_p_amplitude_scale;
                    else if (i == ecg_wave_t)
                        amplitude_scale =
                            config.scenario
                                .premature_t_amplitude_scale;
                    else
                    {
                        amplitude_scale =
                            config.scenario
                                .premature_qrs_amplitude_scale;
                        width_scale =
                            config.scenario
                                .premature_qrs_width_scale;
                    }
                }
                const double delta =
                    wrapped_angle(theta - wave.phase_radians);
                const double normalized =
                    delta / (wave.width_radians * width_scale);
                morphology += wave.amplitude * amplitude_scale * delta *
                    std::exp(-0.5 * normalized * normalized);
            }

            const double baseline =
                config.baseline_amplitude_mv *
                std::sin(
                    TWO_PI * config.respiration_frequency_hz *
                    time_seconds);

            derivative result;
            result.x = alpha * state.x - omega * state.y;
            result.y = alpha * state.y + omega * state.x;
            result.z = -morphology - (state.z - baseline);
            return result;
        }

        model_state add_scaled(
            const model_state& state,
            const derivative& change,
            double scale)
        {
            model_state result;
            result.x = state.x + scale * change.x;
            result.y = state.y + scale * change.y;
            result.z = state.z + scale * change.z;
            return result;
        }

        void integrate_rk4(
            const ecg_model_config& config,
            ecg_beat_kind beat_kind,
            model_state& state,
            double time_seconds,
            double unwrapped_phase,
            double omega,
            double step_seconds)
        {
            const derivative k1 = evaluate(
                config,
                beat_kind,
                state,
                time_seconds,
                unwrapped_phase,
                omega);
            const derivative k2 = evaluate(
                config,
                beat_kind,
                add_scaled(state, k1, 0.5 * step_seconds),
                time_seconds + 0.5 * step_seconds,
                unwrapped_phase + 0.5 * omega * step_seconds,
                omega);
            const derivative k3 = evaluate(
                config,
                beat_kind,
                add_scaled(state, k2, 0.5 * step_seconds),
                time_seconds + 0.5 * step_seconds,
                unwrapped_phase + 0.5 * omega * step_seconds,
                omega);
            const derivative k4 = evaluate(
                config,
                beat_kind,
                add_scaled(state, k3, step_seconds),
                time_seconds + step_seconds,
                unwrapped_phase + omega * step_seconds,
                omega);

            const double scale = step_seconds / 6.0;
            state.x += scale * (k1.x + 2.0 * k2.x + 2.0 * k3.x + k4.x);
            state.y += scale * (k1.y + 2.0 * k2.y + 2.0 * k3.y + k4.y);
            state.z += scale * (k1.z + 2.0 * k2.z + 2.0 * k3.z + k4.z);
        }

        unsigned int saturated_increment(unsigned int value)
        {
            return value == std::numeric_limits<unsigned int>::max()
                ? value
                : value + 1;
        }
    }

    ecg_hrv_config::ecg_hrv_config()
        : enabled(false),
          rr_standard_deviation_seconds(0.05),
          lf_hf_ratio(0.5),
          lf_center_frequency_hz(0.10),
          hf_center_frequency_hz(0.25),
          lf_bandwidth_hz(0.01),
          hf_bandwidth_hz(0.01),
          minimum_rr_seconds(0.30),
          maximum_rr_seconds(2.50),
          seed(0x4852565f53454544ULL)
    {
    }

    ecg_scenario_config::ecg_scenario_config()
        : premature_every_n_beats(0),
          premature_probability(0.0),
          premature_rr_ratio(0.65),
          compensatory_pause_ratio(1.35),
          premature_p_amplitude_scale(0.0),
          premature_qrs_amplitude_scale(0.8),
          premature_qrs_width_scale(1.6),
          premature_t_amplitude_scale(-0.7),
          seed(0x5343454e4152494fULL)
    {
    }

    struct ecg_rr_generator::implementation
    {
        implementation()
            : mean_heart_rate_bpm(60.0),
              configured(false),
              previous_kind(ecg_beat_sinus)
        {
        }

        double mean_heart_rate_bpm;
        ecg_hrv_config hrv;
        ecg_scenario_config scenario;
        bool configured;
        ecg_beat_kind previous_kind;
        std::vector<rr_oscillator> oscillators;
    };

    namespace
    {
        void append_oscillator_band(
            std::vector<rr_oscillator>& output,
            double center_frequency,
            double bandwidth,
            double variance,
            unsigned long long seed,
            unsigned long long seed_offset)
        {
            const unsigned int oscillator_count = 12;
            double weight_sum = 0.0;
            double weights[oscillator_count];
            double positions[oscillator_count];
            for (unsigned int i = 0; i < oscillator_count; ++i)
            {
                positions[i] = -2.5 +
                    5.0 * static_cast<double>(i) /
                    static_cast<double>(oscillator_count - 1);
                weights[i] =
                    std::exp(-0.5 * positions[i] * positions[i]);
                weight_sum += weights[i];
            }

            for (unsigned int i = 0; i < oscillator_count; ++i)
            {
                rr_oscillator oscillator;
                oscillator.frequency_hz =
                    center_frequency + bandwidth * positions[i];
                oscillator.amplitude_seconds = std::sqrt(
                    2.0 * variance * weights[i] / weight_sum);
                oscillator.phase_radians = TWO_PI * deterministic_unit(
                    seed, seed_offset + i);
                output.push_back(oscillator);
            }
        }
    }

    ecg_rr_generator::ecg_rr_generator()
        : implementation_(new implementation)
    {
        configure(
            60.0, ecg_hrv_config(), ecg_scenario_config());
    }

    ecg_rr_generator::ecg_rr_generator(
        double mean_heart_rate_bpm,
        const ecg_hrv_config& hrv,
        const ecg_scenario_config& scenario)
        : implementation_(new implementation)
    {
        configure(mean_heart_rate_bpm, hrv, scenario);
    }

    ecg_rr_generator::ecg_rr_generator(const ecg_rr_generator& other)
        : implementation_(new implementation(*other.implementation_))
    {
    }

    ecg_rr_generator& ecg_rr_generator::operator=(
        const ecg_rr_generator& other)
    {
        if (this != &other)
            *implementation_ = *other.implementation_;
        return *this;
    }

    ecg_rr_generator::~ecg_rr_generator()
    {
        delete implementation_;
    }

    bool ecg_rr_generator::configure(
        double mean_heart_rate_bpm,
        const ecg_hrv_config& hrv,
        const ecg_scenario_config& scenario)
    {
        if (!finite(mean_heart_rate_bpm) ||
            mean_heart_rate_bpm < 1.0 ||
            mean_heart_rate_bpm > 1000.0 ||
            !valid_hrv_config(hrv) ||
            !valid_scenario_config(scenario))
        {
            return false;
        }
        const double hrv_nyquist =
            mean_heart_rate_bpm / 120.0;
        if (hrv.enabled &&
            (hrv.lf_center_frequency_hz +
                    2.5 * hrv.lf_bandwidth_hz >= hrv_nyquist ||
                hrv.hf_center_frequency_hz +
                    2.5 * hrv.hf_bandwidth_hz >= hrv_nyquist))
        {
            return false;
        }

        implementation_->mean_heart_rate_bpm = mean_heart_rate_bpm;
        implementation_->hrv = hrv;
        implementation_->scenario = scenario;
        implementation_->oscillators.clear();

        if (hrv.enabled && hrv.rr_standard_deviation_seconds > 0.0)
        {
            const double total_variance =
                hrv.rr_standard_deviation_seconds *
                hrv.rr_standard_deviation_seconds;
            const double hf_variance =
                total_variance / (1.0 + hrv.lf_hf_ratio);
            const double lf_variance = total_variance - hf_variance;
            append_oscillator_band(
                implementation_->oscillators,
                hrv.lf_center_frequency_hz,
                hrv.lf_bandwidth_hz,
                lf_variance,
                hrv.seed,
                0);
            append_oscillator_band(
                implementation_->oscillators,
                hrv.hf_center_frequency_hz,
                hrv.hf_bandwidth_hz,
                hf_variance,
                hrv.seed,
                0x10000ULL);
        }

        implementation_->configured = true;
        reset();
        return true;
    }

    bool ecg_rr_generator::valid() const
    {
        return implementation_->configured;
    }

    void ecg_rr_generator::reset()
    {
        implementation_->previous_kind = ecg_beat_sinus;
    }

    ecg_beat_plan ecg_rr_generator::next(
        unsigned long long beat_index,
        double event_time_seconds)
    {
        ecg_beat_plan result;
        result.beat_index = beat_index;
        result.rr_interval_seconds = 0.0;
        result.rr_was_clipped = false;
        result.kind = ecg_beat_sinus;
        if (!implementation_->configured ||
            !finite(event_time_seconds))
        {
            return result;
        }

        const ecg_hrv_config& hrv = implementation_->hrv;
        double rr_interval =
            60.0 / implementation_->mean_heart_rate_bpm;
        if (hrv.enabled)
        {
            for (std::size_t i = 0;
                 i < implementation_->oscillators.size();
                 ++i)
            {
                const rr_oscillator& oscillator =
                    implementation_->oscillators[i];
                rr_interval += oscillator.amplitude_seconds *
                    std::sin(
                        TWO_PI * oscillator.frequency_hz *
                            event_time_seconds +
                        oscillator.phase_radians);
            }
        }

        const ecg_scenario_config& scenario =
            implementation_->scenario;
        if (implementation_->previous_kind == ecg_beat_premature)
        {
            result.kind = ecg_beat_compensatory;
            rr_interval *= scenario.compensatory_pause_ratio;
        }
        else
        {
            const bool periodic =
                scenario.premature_every_n_beats > 0 &&
                (beat_index + 1) %
                    scenario.premature_every_n_beats == 0;
            const bool random =
                scenario.premature_probability > 0.0 &&
                deterministic_unit(scenario.seed, beat_index) <
                    scenario.premature_probability;
            if (periodic || random)
            {
                result.kind = ecg_beat_premature;
                rr_interval *= scenario.premature_rr_ratio;
            }
        }

        result.rr_interval_seconds = std::max(
            hrv.minimum_rr_seconds,
            std::min(hrv.maximum_rr_seconds, rr_interval));
        result.rr_was_clipped =
            result.rr_interval_seconds != rr_interval;
        implementation_->previous_kind = result.kind;
        return result;
    }

    ecg_model_config::ecg_model_config()
        : sampling_rate_hz(500),
          heart_rate_bpm(60.0),
          baseline_amplitude_mv(0.15),
          respiration_frequency_hz(0.25)
    {
        waves[ecg_wave_p] = ecg_wave_config{-PI / 3.0, 1.2, 0.25};
        waves[ecg_wave_q] = ecg_wave_config{-PI / 12.0, -5.0, 0.10};
        waves[ecg_wave_r] = ecg_wave_config{0.0, 30.0, 0.10};
        waves[ecg_wave_s] = ecg_wave_config{PI / 12.0, -7.5, 0.10};
        waves[ecg_wave_t] = ecg_wave_config{PI / 2.0, 0.75, 0.40};
    }

    struct ecg_model::implementation
    {
        implementation()
            : configured(false),
              time_seconds(0.0),
              unwrapped_phase(-PI),
              angular_velocity(TWO_PI),
              next_plan_ready(false),
              samples_emitted(0),
              has_emitted_sample(false)
        {
            state.x = -1.0;
            state.y = 0.0;
            state.z = 0.0;
            std::fill(next_event_beat, next_event_beat + ecg_wave_count, 0);
        }

        ecg_model_config config;
        bool configured;
        model_state state;
        double time_seconds;
        double unwrapped_phase;
        double angular_velocity;
        ecg_rr_generator rr_generator;
        ecg_beat_plan current_plan;
        ecg_beat_plan next_plan;
        bool next_plan_ready;
        unsigned long long samples_emitted;
        bool has_emitted_sample;
        unsigned long long next_event_beat[ecg_wave_count];
    };

    ecg_model::ecg_model()
        : implementation_(new implementation)
    {
        configure(ecg_model_config());
    }

    ecg_model::ecg_model(const ecg_model_config& config)
        : implementation_(new implementation)
    {
        configure(config);
    }

    ecg_model::ecg_model(const ecg_model& other)
        : implementation_(new implementation(*other.implementation_))
    {
    }

    ecg_model& ecg_model::operator=(const ecg_model& other)
    {
        if (this != &other)
            *implementation_ = *other.implementation_;
        return *this;
    }

    ecg_model::~ecg_model()
    {
        delete implementation_;
    }

    bool ecg_model::configure(const ecg_model_config& config)
    {
        if (!valid_config(config))
            return false;
        ecg_rr_generator configured_rr = implementation_->rr_generator;
        if (!configured_rr.configure(
                config.heart_rate_bpm,
                config.hrv,
                config.scenario))
        {
            return false;
        }
        implementation_->config = config;
        implementation_->rr_generator = configured_rr;
        implementation_->configured = true;
        reset();
        return true;
    }

    bool ecg_model::valid() const
    {
        return implementation_->configured;
    }

    void ecg_model::reset()
    {
        implementation_->time_seconds = 0.0;
        implementation_->unwrapped_phase = -PI;
        implementation_->angular_velocity =
            TWO_PI * implementation_->config.heart_rate_bpm / 60.0;
        implementation_->next_plan_ready = false;
        implementation_->samples_emitted = 0;
        implementation_->has_emitted_sample = false;
        std::fill(
            implementation_->next_event_beat,
            implementation_->next_event_beat + ecg_wave_count,
            0);
        implementation_->state.x = -1.0;
        implementation_->state.y = 0.0;
        implementation_->state.z = 0.0;

        if (!implementation_->configured)
            return;

        const ecg_model_config& config = implementation_->config;
        implementation_->rr_generator.reset();
        implementation_->current_plan =
            implementation_->rr_generator.next(0, 0.0);
        implementation_->angular_velocity =
            TWO_PI /
            implementation_->current_plan.rr_interval_seconds;

        // Settle z with sinus morphology while ending at phase -pi.
        const double period = 60.0 / config.heart_rate_bpm;
        const double duration = 8.0 * period;
        const double requested_steps_per_cycle = std::ceil(
            period * static_cast<double>(config.sampling_rate_hz));
        const unsigned int steps_per_cycle =
            static_cast<unsigned int>(std::max(
                8.0, std::min(requested_steps_per_cycle, 4096.0)));
        const unsigned int steps = 8 * steps_per_cycle;
        const double step = duration / static_cast<double>(steps);
        double time = -duration;
        double phase = -PI - 8.0 * TWO_PI;
        const double settling_omega = TWO_PI / period;
        for (unsigned int i = 0; i < steps; ++i)
        {
            integrate_rk4(
                config,
                ecg_beat_sinus,
                implementation_->state,
                time,
                phase,
                settling_omega,
                step);
            time += step;
            phase += settling_omega * step;
        }

        implementation_->state.x = -1.0;
        implementation_->state.y = 0.0;
    }

    const ecg_model_config& ecg_model::config() const
    {
        return implementation_->config;
    }

    ecg_render_result ecg_model::render(
        double* samples,
        unsigned int sample_count,
        ecg_model_annotation* annotations,
        unsigned int annotation_capacity)
    {
        ecg_render_result result = {0, 0, 0};
        if (!implementation_->configured || !samples || sample_count == 0)
            return result;

        const ecg_model_config& config = implementation_->config;

        for (unsigned int sample = 0; sample < sample_count; ++sample)
        {
            if (implementation_->has_emitted_sample)
            {
                struct pending_annotation
                {
                    double time_seconds;
                    double target_phase;
                    unsigned long long beat_index;
                    double rr_interval_seconds;
                    bool rr_was_clipped;
                    ecg_beat_kind beat_kind;
                    int wave_index;
                };
                pending_annotation pending[ecg_wave_count];
                int pending_count = 0;
                const double next_time =
                    static_cast<double>(implementation_->samples_emitted) /
                    static_cast<double>(config.sampling_rate_hz);
                const double time_tolerance =
                    32.0 * std::numeric_limits<double>::epsilon() *
                    std::max(1.0, std::fabs(next_time));
                while (implementation_->time_seconds <
                    next_time - time_tolerance)
                {
                    double target_phase = std::numeric_limits<double>::infinity();
                    int target_wave = -1;
                    for (int wave_index = 0;
                         wave_index < ecg_wave_count;
                         ++wave_index)
                    {
                        const double wave_target =
                            config.waves[wave_index].phase_radians +
                            static_cast<double>(
                                implementation_
                                    ->next_event_beat[wave_index]) *
                                TWO_PI;
                        if (wave_target < target_phase)
                        {
                            target_phase = wave_target;
                            target_wave = wave_index;
                        }
                    }

                    const double wrap_phase =
                        PI + static_cast<double>(
                            implementation_->current_plan.beat_index) *
                            TWO_PI;
                    const bool wrap_is_next =
                        wrap_phase < target_phase;
                    if (wrap_is_next)
                    {
                        target_phase = wrap_phase;
                        target_wave = -1;
                    }

                    const double phase_distance =
                        std::max(
                            0.0,
                            target_phase -
                                implementation_->unwrapped_phase);
                    const double event_step =
                        phase_distance /
                        implementation_->angular_velocity;
                    const double remaining_step =
                        next_time - implementation_->time_seconds;
                    if (event_step > remaining_step + time_tolerance)
                    {
                        integrate_rk4(
                            config,
                            implementation_->current_plan.kind,
                            implementation_->state,
                            implementation_->time_seconds,
                            implementation_->unwrapped_phase,
                            implementation_->angular_velocity,
                            remaining_step);
                        implementation_->unwrapped_phase +=
                            implementation_->angular_velocity *
                            remaining_step;
                        implementation_->time_seconds = next_time;
                        break;
                    }

                    const double bounded_event_step =
                        std::min(event_step, remaining_step);
                    if (bounded_event_step > time_tolerance)
                    {
                        integrate_rk4(
                            config,
                            implementation_->current_plan.kind,
                            implementation_->state,
                            implementation_->time_seconds,
                            implementation_->unwrapped_phase,
                            implementation_->angular_velocity,
                            bounded_event_step);
                    }
                    implementation_->time_seconds +=
                        bounded_event_step;
                    implementation_->unwrapped_phase = target_phase;

                    if (wrap_is_next)
                    {
                        if (implementation_->next_plan_ready)
                        {
                            implementation_->current_plan =
                                implementation_->next_plan;
                            implementation_->next_plan_ready = false;
                        }
                        continue;
                    }

                    const unsigned long long event_beat =
                        implementation_->next_event_beat[target_wave]++;
                    pending[pending_count].time_seconds =
                        implementation_->time_seconds;
                    pending[pending_count].target_phase = target_phase;
                    pending[pending_count].beat_index = event_beat;
                    pending[pending_count].rr_interval_seconds =
                        implementation_->current_plan
                            .rr_interval_seconds;
                    pending[pending_count].rr_was_clipped =
                        implementation_->current_plan.rr_was_clipped;
                    pending[pending_count].beat_kind =
                        implementation_->current_plan.kind;
                    pending[pending_count].wave_index = target_wave;
                    ++pending_count;

                    if (target_wave == ecg_wave_r)
                    {
                        implementation_->next_plan =
                            implementation_->rr_generator.next(
                                event_beat + 1,
                                implementation_->time_seconds);
                        implementation_->next_plan_ready = true;
                        implementation_->angular_velocity =
                            TWO_PI /
                            implementation_->next_plan
                                .rr_interval_seconds;
                    }
                }

                const double final_step =
                    next_time - implementation_->time_seconds;
                if (final_step > 0.0)
                {
                    integrate_rk4(
                        config,
                        implementation_->current_plan.kind,
                        implementation_->state,
                        implementation_->time_seconds,
                        implementation_->unwrapped_phase,
                        implementation_->angular_velocity,
                        final_step);
                    implementation_->unwrapped_phase +=
                        implementation_->angular_velocity * final_step;
                    implementation_->time_seconds = next_time;
                }

                for (int pending_index = 0;
                     pending_index < pending_count;
                     ++pending_index)
                {
                    const pending_annotation& event =
                        pending[pending_index];
                    result.annotations_required =
                        saturated_increment(result.annotations_required);
                    if (annotations &&
                        result.annotations_written < annotation_capacity)
                    {
                        ecg_model_annotation& annotation =
                            annotations[result.annotations_written++];
                        annotation.sample_index =
                            implementation_->samples_emitted;
                        annotation.beat_index = event.beat_index;
                        annotation.time_seconds = event.time_seconds;
                        annotation.phase_error_radians =
                            std::max(
                                0.0,
                                implementation_->unwrapped_phase -
                                    event.target_phase);
                        annotation.wave =
                            static_cast<ecg_wave>(event.wave_index);
                        double amplitude_scale = 1.0;
                        if (event.beat_kind == ecg_beat_premature)
                        {
                            if (event.wave_index == ecg_wave_p)
                            {
                                amplitude_scale =
                                    config.scenario
                                        .premature_p_amplitude_scale;
                            }
                            else if (event.wave_index == ecg_wave_t)
                            {
                                amplitude_scale =
                                    config.scenario
                                        .premature_t_amplitude_scale;
                            }
                            else
                            {
                                amplitude_scale =
                                    config.scenario
                                        .premature_qrs_amplitude_scale;
                            }
                        }
                        annotation.present =
                            config.waves[event.wave_index].amplitude *
                                amplitude_scale !=
                            0.0;
                        annotation.beat_kind = event.beat_kind;
                        annotation.rr_interval_seconds =
                            event.rr_interval_seconds;
                        annotation.rr_was_clipped =
                            event.rr_was_clipped;
                    }
                }
            }

            samples[sample] = implementation_->state.z;
            ++implementation_->samples_emitted;
            implementation_->has_emitted_sample = true;
            ++result.samples_written;
        }
        return result;
    }

    ecg_fiducial_result measure_ecg_fiducials(
        const double* samples,
        unsigned int sample_count,
        unsigned int sampling_rate_hz,
        const ecg_model_annotation* model_annotations,
        unsigned int annotation_count,
        ecg_measured_fiducial* fiducials,
        unsigned int fiducial_capacity)
    {
        ecg_fiducial_result result = {0, 0};
        if (!samples || sample_count == 0 || sampling_rate_hz == 0 ||
            !model_annotations || annotation_count == 0)
        {
            return result;
        }

        for (unsigned int i = 0; i < annotation_count; ++i)
        {
            const ecg_model_annotation& model = model_annotations[i];
            if (!model.present || model.sample_index >= sample_count)
                continue;

            unsigned int left = 0;
            unsigned int right = sample_count - 1;
            if (i > 0)
            {
                left = static_cast<unsigned int>(
                    (model_annotations[i - 1].sample_index +
                        model.sample_index + 1) /
                    2);
            }
            if (i + 1 < annotation_count)
            {
                right = static_cast<unsigned int>(
                    (model.sample_index +
                        model_annotations[i + 1].sample_index) /
                    2);
            }
            left = std::min(left, sample_count - 1);
            right = std::min(right, sample_count - 1);
            if (right < left)
                continue;

            const double left_value = samples[left];
            const double right_value = samples[right];
            const double width =
                static_cast<double>(right - left);
            unsigned int best_sample = left;
            double best_score = -1.0;
            bool found_local_extremum = false;
            for (unsigned int sample = left + (left < right ? 1 : 0);
                 sample < right;
                 ++sample)
            {
                const double position = width > 0.0
                    ? static_cast<double>(sample - left) / width
                    : 0.0;
                const double baseline =
                    left_value +
                    position * (right_value - left_value);
                const double score =
                    std::fabs(samples[sample] - baseline);
                const double left_slope =
                    samples[sample] - samples[sample - 1];
                const double right_slope =
                    samples[sample + 1] - samples[sample];
                const bool local_extremum =
                    left_slope * right_slope <= 0.0 &&
                    (left_slope != 0.0 || right_slope != 0.0);
                if (local_extremum && score > best_score)
                {
                    best_score = score;
                    best_sample = sample;
                    found_local_extremum = true;
                }
            }
            if (!found_local_extremum)
            {
                for (unsigned int sample = left;
                     sample <= right;
                     ++sample)
                {
                    const double position = width > 0.0
                        ? static_cast<double>(sample - left) / width
                        : 0.0;
                    const double baseline =
                        left_value +
                        position * (right_value - left_value);
                    const double score =
                        std::fabs(samples[sample] - baseline);
                    if (score > best_score)
                    {
                        best_score = score;
                        best_sample = sample;
                    }
                }
            }

            double sub_sample_offset = 0.0;
            double interpolated_value = samples[best_sample];
            if (best_sample > 0 && best_sample + 1 < sample_count)
            {
                const double left_neighbor = samples[best_sample - 1];
                const double center = samples[best_sample];
                const double right_neighbor = samples[best_sample + 1];
                const double denominator =
                    left_neighbor - 2.0 * center + right_neighbor;
                if (std::fabs(denominator) >
                    32.0 * std::numeric_limits<double>::epsilon())
                {
                    sub_sample_offset =
                        0.5 * (left_neighbor - right_neighbor) /
                        denominator;
                    sub_sample_offset = std::max(
                        -0.5, std::min(0.5, sub_sample_offset));
                    interpolated_value =
                        center +
                        0.25 * (right_neighbor - left_neighbor) *
                            sub_sample_offset;
                }
            }

            result.fiducials_required =
                saturated_increment(result.fiducials_required);
            if (fiducials &&
                result.fiducials_written < fiducial_capacity)
            {
                ecg_measured_fiducial& measured =
                    fiducials[result.fiducials_written++];
                measured.model_sample_index = model.sample_index;
                measured.sample_index = best_sample;
                measured.beat_index = model.beat_index;
                measured.time_seconds =
                    (static_cast<double>(best_sample) +
                        sub_sample_offset) /
                    static_cast<double>(sampling_rate_hz);
                measured.sample_value = samples[best_sample];
                measured.interpolated_value = interpolated_value;
                measured.wave = model.wave;
                measured.beat_kind = model.beat_kind;
            }
        }
        return result;
    }
}
