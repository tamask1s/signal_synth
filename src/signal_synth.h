namespace signal_synth
{
    struct qrs_params
    {
        double amplitude_p = 0.1;
        double amplitude_q = -0.1;
        double amplitude_r = 1.0;
        double amplitude_s = -0.2;
        double amplitude_t = 0.2;
        double st_elev = 0.0;
        double st_diff = 0.0;
        double curv_s = 5;
        double curv_st = 5;
        double len_p = 0.08;
        double len_pq = 0.08;
        double len_q = 0.007;
        double len_r = 0.1;
        double len_s = 0.007;
        double len_st = 0.1;
        double len_t = 0.16;
    };

    struct ecg_simulation_params
    {
        double heartbeat_frequency = 1.0;
        double alteration_frequency_for_DC_component = 0.01;
        double alteration_amplitude_for_DC_component = 0.2;
        double alteration_phase_for_DC_component_in_radians = 0;
        double amplitude_modulation_depth_for_QRS_by_HF = 0.2;
        double frequency_HF = 0.3;
        double frequency_LF = 0.12;
        double frequency_modulation_depth_HF = 0.3;
        double frequency_modulation_depth_LF = 0.3;
        double phase_HF_radians = 0;
        double phase_LF_radians = 0;
        double extrasys_frequency = 0.1;
        double extrasys_shift_after_last_QRS = 0.5;
        double QRS_interval_standard_deviation = 0.1;
        int skip_one_QRS_at_every = 7;
    };

    void print_gauss(double* data, unsigned int data_samples, double amplitude);
    void print_line(double* data, unsigned int data_samples, double amplitude);
    void print_curve(double* data, unsigned int data_samples, double amplitude, double curvature = 5.0);
    void print_semicircle(double* data, unsigned int data_samples, double amplitude, double curvature = 0.0);
    void simulate_qrs(double* data, unsigned int data_samples, unsigned int sampling_rate, double amp_mod, const qrs_params& params);
    void generate_ecg(double* data, unsigned int data_samples, unsigned int sampling_rate, double frequency, const qrs_params& params);
    void generate_modulated_ecg(double* data, unsigned int data_samples, unsigned int sampling_rate, const ecg_simulation_params& params, const qrs_params& qrs_params);
    void add_noise_to_signal(double* data, unsigned int data_samples, double noise_amplitude, unsigned int sampling_rate, double noise_frequency = 0.0);
    void butterworth_bandpass_filter(double* data, unsigned int data_samples, double sampling_rate, double low_freq, double high_freq);
    void butterworth_bandpass_filter_(double* data, unsigned int data_samples, unsigned int sampling_rate, double low_freq, double high_freq);
    void add_bandlimited_noise(double* data, unsigned int data_samples, double noise_amplitude, double sampling_rate, double low_freq, double high_freq);
    void add_bandlimited_noise_not_really_band_limited(double* data, unsigned int data_samples, double noise_amplitude, unsigned int sampling_rate, double low_freq, double high_freq);
    void create_modulated_sine(double* data, unsigned int data_len, unsigned int sampling_rate, double frequency, double amplitude, double modulation_frequency, double frequency_modulation_depth);
    void create_sine(double* data, unsigned int data_len, int sampling_rate, double frequency, double amplitude);
    void create_triangle(double* data, unsigned int data_len, int sampling_rate, double silence_before_triangle_in_msec, double duration_in_msec, double amplitude);
    void create_pulse(double* data, unsigned int data_len, int sampling_rate, double silence_before_pulse_in_msec, double duration_in_msec, double amplitude);
}
