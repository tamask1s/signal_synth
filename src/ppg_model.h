#pragma once

#include <vector>

namespace signal_synth
{
    class clinical_ecg_record;

    enum ppg_fiducial_kind
    {
        ppg_pulse_onset = 0,
        ppg_systolic_peak = 1,
        ppg_dicrotic_feature = 2,
        ppg_pulse_offset = 3
    };

    enum ppg_fiducial_source
    {
        ppg_fiducial_construction = 0,
        ppg_fiducial_measurement = 1
    };

    enum ppg_pulse_state
    {
        ppg_pulse_valid = 0,
        ppg_pulse_weak = 1,
        ppg_pulse_missing = 2,
        ppg_pulse_out_of_record = 3
    };

    const char* ppg_pulse_state_name(ppg_pulse_state state);

    struct ppg_perfusion_episode_config
    {
        ppg_perfusion_episode_config();

        double start_seconds;
        double duration_seconds;
        double amplitude_scale;
        double rise_time_scale;
        double decay_time_scale;
        unsigned int weak_pulse_every_n_beats;
        double weak_pulse_amplitude_scale;
        unsigned int missing_pulse_every_n_beats;
    };

    struct ppg_config
    {
        ppg_config();

        bool enabled;
        double pulse_delay_ms;
        double rise_time_ms;
        double decay_time_ms;
        double amplitude_au;
        double baseline_au;
        double dicrotic_delay_ms;
        double dicrotic_width_ms;
        double dicrotic_amplitude_ratio;
        double pulse_delay_variation_ms;
        double pulse_delay_variation_hz;
        unsigned int missing_pulse_every_n_beats;
        double clock_drift_ppm;
        double pulse_delay_jitter_ms;
        double low_frequency_amplitude_modulation_ratio;
        double low_frequency_amplitude_modulation_hz;
        double rise_time_variation_ratio;
        double decay_time_variation_ratio;
        double pac_pulse_amplitude_scale;
        double pvc_pulse_amplitude_scale;
        double paced_pulse_amplitude_scale;
        unsigned long long seed;
        std::vector<ppg_perfusion_episode_config> perfusion_episodes;
    };

    struct ppg_annotation
    {
        unsigned long long ecg_beat_index;
        double ecg_r_time_seconds;
        ppg_fiducial_kind kind;
        ppg_fiducial_source source;
        unsigned long long sample_index;
        double time_seconds;
        double value_au;
    };

    struct ppg_pulse_annotation
    {
        unsigned long long ecg_beat_index;
        double ecg_r_time_seconds;
        double pulse_delay_seconds;
        double expected_onset_time_seconds;
        double expected_peak_time_seconds;
        double expected_offset_time_seconds;
        double effective_amplitude_au;
        double effective_rise_time_seconds;
        double effective_decay_time_seconds;
        ppg_pulse_state state;
        bool low_perfusion;
        bool arrhythmia_linked;
        double arrhythmia_amplitude_scale;
        bool valid_for_peak_scoring;
        bool generated;
        bool intentionally_missing;
    };

    class ppg_record
    {
    public:
        ppg_record();
        ppg_record(const ppg_record& other);
        ppg_record& operator=(const ppg_record& other);
        ~ppg_record();

        unsigned int sampling_rate_hz() const;
        unsigned int sample_count() const;
        const char* channel_name() const;
        const char* unit() const;
        const double* samples() const;
        unsigned int annotation_count() const;
        const ppg_annotation* annotations() const;
        unsigned int pulse_count() const;
        const ppg_pulse_annotation* pulses() const;

    private:
        struct implementation;
        implementation* implementation_;
        friend class ppg_generator;
        friend bool remeasure_ppg_systolic_peaks(const double* samples, unsigned int sample_count, ppg_record& record);
        friend bool remeasure_ppg_fiducials(const double* samples, unsigned int sample_count, ppg_record& record);
    };

    class ppg_generator
    {
    public:
        ppg_generator();
        explicit ppg_generator(const ppg_config& config);

        bool configure(const ppg_config& config);
        bool valid() const;
        const ppg_config& config() const;
        bool generate(const clinical_ecg_record& timeline, ppg_record& output) const;

    private:
        ppg_config config_;
        bool valid_;
    };

    bool remeasure_ppg_systolic_peaks(const double* samples, unsigned int sample_count, ppg_record& record);
    bool remeasure_ppg_fiducials(const double* samples, unsigned int sample_count, ppg_record& record);
}
