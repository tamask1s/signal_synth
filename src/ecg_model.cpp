#include "ecg_model.h"

#include <algorithm>
#include <cmath>
#include <limits>

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

        bool finite(double value)
        {
            return std::isfinite(value);
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
                config.respiration_frequency_hz < 0.0)
            {
                return false;
            }

            const double phase_step =
                TWO_PI * config.heart_rate_bpm /
                (60.0 * static_cast<double>(config.sampling_rate_hz));
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
            const model_state& state,
            double time_seconds)
        {
            const double radius =
                std::sqrt(state.x * state.x + state.y * state.y);
            const double alpha = 1.0 - radius;
            const double omega = TWO_PI * config.heart_rate_bpm / 60.0;
            const double theta = std::atan2(state.y, state.x);

            double morphology = 0.0;
            for (int i = 0; i < ecg_wave_count; ++i)
            {
                const ecg_wave_config& wave = config.waves[i];
                const double delta =
                    wrapped_angle(theta - wave.phase_radians);
                const double normalized = delta / wave.width_radians;
                morphology += wave.amplitude * delta *
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
            model_state& state,
            double time_seconds,
            double step_seconds)
        {
            const derivative k1 = evaluate(config, state, time_seconds);
            const derivative k2 = evaluate(
                config,
                add_scaled(state, k1, 0.5 * step_seconds),
                time_seconds + 0.5 * step_seconds);
            const derivative k3 = evaluate(
                config,
                add_scaled(state, k2, 0.5 * step_seconds),
                time_seconds + 0.5 * step_seconds);
            const derivative k4 = evaluate(
                config,
                add_scaled(state, k3, step_seconds),
                time_seconds + step_seconds);

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
        implementation_->config = config;
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

        // Settle z for eight complete cycles while ending at phase -pi.
        const ecg_model_config& config = implementation_->config;
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
        for (unsigned int i = 0; i < steps; ++i)
        {
            integrate_rk4(config, implementation_->state, time, step);
            time += step;
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
        const double omega = TWO_PI * config.heart_rate_bpm / 60.0;
        const double period = TWO_PI / omega;

        for (unsigned int sample = 0; sample < sample_count; ++sample)
        {
            if (implementation_->has_emitted_sample)
            {
                const double next_time =
                    static_cast<double>(implementation_->samples_emitted) /
                    static_cast<double>(config.sampling_rate_hz);
                const double integration_step =
                    next_time - implementation_->time_seconds;
                integrate_rk4(
                    config,
                    implementation_->state,
                    implementation_->time_seconds,
                    integration_step);
                implementation_->time_seconds = next_time;
                implementation_->unwrapped_phase =
                    -PI + omega * next_time;

                struct pending_annotation
                {
                    double time_seconds;
                    unsigned long long beat_index;
                    int wave_index;
                };
                pending_annotation pending[ecg_wave_count];
                int pending_count = 0;
                const double time_tolerance =
                    32.0 * std::numeric_limits<double>::epsilon() *
                    std::max(1.0, std::fabs(next_time));
                for (int wave_index = 0;
                     wave_index < ecg_wave_count;
                     ++wave_index)
                {
                    const double phase =
                        config.waves[wave_index].phase_radians;
                    const unsigned long long beat =
                        implementation_->next_event_beat[wave_index];
                    const double event_time =
                        (phase + PI) / omega +
                        static_cast<double>(beat) * period;
                    if (event_time > next_time + time_tolerance)
                        continue;

                    pending[pending_count].time_seconds = event_time;
                    pending[pending_count].beat_index = beat;
                    pending[pending_count].wave_index = wave_index;
                    ++pending_count;
                    ++implementation_->next_event_beat[wave_index];
                }

                std::sort(
                    pending,
                    pending + pending_count,
                    [](const pending_annotation& left,
                       const pending_annotation& right) {
                        return left.time_seconds < right.time_seconds;
                    });

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
                        const double target_phase =
                            config.waves[event.wave_index].phase_radians +
                            static_cast<double>(event.beat_index) * TWO_PI;
                        annotation.phase_error_radians =
                            std::max(
                                0.0,
                                implementation_->unwrapped_phase -
                                    target_phase);
                        annotation.wave =
                            static_cast<ecg_wave>(event.wave_index);
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
}
