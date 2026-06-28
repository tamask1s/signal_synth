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

    struct ecg_model_config
    {
        ecg_model_config();

        unsigned int sampling_rate_hz;
        double heart_rate_bpm;
        double baseline_amplitude_mv;
        double respiration_frequency_hz;
        ecg_wave_config waves[ecg_wave_count];
    };

    struct ecg_model_annotation
    {
        unsigned long long sample_index;
        unsigned long long beat_index;
        double time_seconds;
        double phase_error_radians;
        ecg_wave wave;
    };

    struct ecg_render_result
    {
        unsigned int samples_written;
        unsigned int annotations_written;
        unsigned int annotations_required;
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
}
