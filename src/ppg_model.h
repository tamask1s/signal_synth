#pragma once

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
        unsigned long long seed;
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
}
