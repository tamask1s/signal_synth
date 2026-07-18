#pragma once

namespace signal_synth
{
    enum ecg_wave
    {
        ecg_wave_p = 0,
        ecg_wave_q = 1,
        ecg_wave_r = 2,
        ecg_wave_s = 3,
        ecg_wave_t = 4,
        ecg_wave_count = 5
    };

    struct ecg_wave_config
    {
        double phase_radians;
        double amplitude;
        double width_radians;
    };

    enum ecg_beat_kind
    {
        ecg_beat_sinus = 0,
        ecg_beat_premature = 1,
        ecg_beat_compensatory = 2
    };

    struct ecg_hrv_config
    {
        ecg_hrv_config();

        bool enabled;
        double rr_standard_deviation_seconds;
        double vlf_power_fraction;
        double vlf_center_frequency_hz;
        double vlf_bandwidth_hz;
        double lf_hf_ratio;
        double lf_center_frequency_hz;
        double hf_center_frequency_hz;
        double lf_bandwidth_hz;
        double hf_bandwidth_hz;
        double minimum_rr_seconds;
        double maximum_rr_seconds;
        unsigned long long seed;
    };

    struct ecg_scenario_config
    {
        ecg_scenario_config();

        unsigned int premature_every_n_beats;
        double premature_probability;
        double premature_rr_ratio;
        double compensatory_pause_ratio;
        double premature_p_amplitude_scale;
        double premature_qrs_amplitude_scale;
        double premature_qrs_width_scale;
        double premature_t_amplitude_scale;
        unsigned long long seed;
    };

    struct ecg_beat_plan
    {
        unsigned long long beat_index;
        double rr_interval_seconds;
        bool rr_was_clipped;
        ecg_beat_kind kind;
    };

    class ecg_rr_generator
    {
    public:
        ecg_rr_generator();
        ecg_rr_generator(
            double mean_heart_rate_bpm,
            const ecg_hrv_config& hrv,
            const ecg_scenario_config& scenario);
        ecg_rr_generator(const ecg_rr_generator& other);
        ecg_rr_generator& operator=(const ecg_rr_generator& other);
        ~ecg_rr_generator();

        bool configure(
            double mean_heart_rate_bpm,
            const ecg_hrv_config& hrv,
            const ecg_scenario_config& scenario);
        bool valid() const;
        void reset();
        ecg_beat_plan next(
            unsigned long long beat_index,
            double event_time_seconds);

    private:
        struct implementation;
        implementation* implementation_;
    };

    struct ecg_model_config
    {
        ecg_model_config();

        unsigned int sampling_rate_hz;
        double heart_rate_bpm;
        double baseline_amplitude_mv;
        double respiration_frequency_hz;
        ecg_wave_config waves[ecg_wave_count];
        ecg_hrv_config hrv;
        ecg_scenario_config scenario;
    };

    struct ecg_model_annotation
    {
        unsigned long long sample_index;
        unsigned long long beat_index;
        double time_seconds;
        double phase_error_radians;
        ecg_wave wave;
        bool present;
        ecg_beat_kind beat_kind;
        double rr_interval_seconds;
        bool rr_was_clipped;
    };

    struct ecg_render_result
    {
        unsigned int samples_written;
        unsigned int annotations_written;
        unsigned int annotations_required;
    };

    struct ecg_measured_fiducial
    {
        unsigned long long model_sample_index;
        unsigned long long sample_index;
        unsigned long long beat_index;
        double time_seconds;
        double sample_value;
        double interpolated_value;
        unsigned long long onset_sample_index;
        unsigned long long offset_sample_index;
        double onset_time_seconds;
        double offset_time_seconds;
        bool has_onset_offset;
        ecg_wave wave;
        ecg_beat_kind beat_kind;
    };

    struct ecg_fiducial_result
    {
        unsigned int fiducials_written;
        unsigned int fiducials_required;
    };

    class ecg_model
    {
    public:
        ecg_model();
        explicit ecg_model(const ecg_model_config& config);
        ecg_model(const ecg_model& other);
        ecg_model& operator=(const ecg_model& other);
        ~ecg_model();

        bool configure(const ecg_model_config& config);
        bool valid() const;
        void reset();
        const ecg_model_config& config() const;

        ecg_render_result render(
            double* samples,
            unsigned int sample_count,
            ecg_model_annotation* annotations = 0,
            unsigned int annotation_capacity = 0);

    private:
        struct implementation;
        implementation* implementation_;
    };

    ecg_fiducial_result measure_ecg_fiducials(
        const double* samples,
        unsigned int sample_count,
        unsigned int sampling_rate_hz,
        const ecg_model_annotation* model_annotations,
        unsigned int annotation_count,
        ecg_measured_fiducial* fiducials = 0,
        unsigned int fiducial_capacity = 0);

    enum ecg_validation_channel
    {
        ecg_validation_signal = 0,
        ecg_validation_model_events = 1,
        ecg_validation_measured_fiducials = 2,
        ecg_validation_rr_intervals = 3,
        ecg_validation_beat_types = 4,
        ecg_validation_channel_count = 5
    };

    class ecg_validation_package
    {
    public:
        ecg_validation_package();
        ecg_validation_package(const ecg_validation_package& other);
        ecg_validation_package& operator=(
            const ecg_validation_package& other);
        ~ecg_validation_package();

        bool generate(
            const ecg_model_config& config,
            unsigned int sample_count);
        unsigned int sample_count() const;
        const double* channel(ecg_validation_channel channel) const;
        unsigned int model_annotation_count() const;
        const ecg_model_annotation* model_annotations() const;
        unsigned int measured_fiducial_count() const;
        const ecg_measured_fiducial* measured_fiducials() const;

    private:
        struct implementation;
        implementation* implementation_;
    };
}
