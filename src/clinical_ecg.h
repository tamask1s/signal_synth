#pragma once

namespace signal_synth
{
    enum clinical_ecg_lead
    {
        clinical_lead_i = 0,
        clinical_lead_ii = 1,
        clinical_lead_iii = 2,
        clinical_lead_avr = 3,
        clinical_lead_avl = 4,
        clinical_lead_avf = 5,
        clinical_lead_v1 = 6,
        clinical_lead_v2 = 7,
        clinical_lead_v3 = 8,
        clinical_lead_v4 = 9,
        clinical_lead_v5 = 10,
        clinical_lead_v6 = 11,
        clinical_lead_count = 12
    };

    enum clinical_rhythm
    {
        clinical_rhythm_sinus = 0,
        clinical_rhythm_atrial_fibrillation = 1,
        clinical_rhythm_atrial_flutter = 2,
        clinical_rhythm_supraventricular_tachycardia = 3,
        clinical_rhythm_ventricular_tachycardia = 4,
        clinical_rhythm_paced = 5
    };

    enum clinical_av_conduction
    {
        clinical_av_normal = 0,
        clinical_av_first_degree = 1,
        clinical_av_mobitz_i = 2,
        clinical_av_mobitz_ii = 3,
        clinical_av_complete_block = 4
    };

    enum clinical_intraventricular_conduction
    {
        clinical_iv_normal = 0,
        clinical_iv_lbbb = 1,
        clinical_iv_rbbb = 2
    };

    enum clinical_ventricular_origin
    {
        clinical_origin_conducted = 0,
        clinical_origin_pac = 1,
        clinical_origin_pvc = 2,
        clinical_origin_junctional_escape = 3,
        clinical_origin_ventricular_escape = 4,
        clinical_origin_paced = 5,
        clinical_origin_vt = 6
    };

    enum clinical_qt_correction
    {
        clinical_qt_fixed = 0,
        clinical_qt_bazett = 1,
        clinical_qt_fridericia = 2,
        clinical_qt_framingham = 3,
        clinical_qt_hodges = 4
    };

    enum clinical_fiducial_kind
    {
        clinical_p_onset = 0,
        clinical_p_peak = 1,
        clinical_p_offset = 2,
        clinical_qrs_onset = 3,
        clinical_q_peak = 4,
        clinical_r_peak = 5,
        clinical_s_peak = 6,
        clinical_j_point = 7,
        clinical_qrs_offset = 8,
        clinical_t_onset = 9,
        clinical_t_peak = 10,
        clinical_t_offset = 11,
        clinical_pacing_spike = 12
    };

    enum clinical_fiducial_source
    {
        clinical_fiducial_construction = 0,
        clinical_fiducial_lead_measurement = 1
    };

    struct clinical_timing_config
    {
        clinical_timing_config();

        double p_duration_ms;
        double pr_interval_ms;
        double qrs_duration_ms;
        double qrs_q_fraction;
        double qrs_r_fraction;
        double qrs_s_fraction;
        double t_duration_ms;
        double t_peak_fraction;
        double qt_interval_ms;
        double qtc_ms;
        clinical_qt_correction qt_correction;
    };

    struct clinical_morphology_config
    {
        clinical_morphology_config();

        double p_amplitude_mv;
        double q_amplitude_mv;
        double r_amplitude_mv;
        double s_amplitude_mv;
        double t_amplitude_mv;
        double st_j_amplitude_mv;
        double st_slope_mv_per_second;
        double p_axis_degrees;
        double qrs_axis_degrees;
        double t_axis_degrees;
        double p_elevation_degrees;
        double qrs_elevation_degrees;
        double t_elevation_degrees;
        double presence_threshold_mv;
    };

    struct clinical_rhythm_config
    {
        clinical_rhythm_config();

        clinical_rhythm rhythm;
        clinical_av_conduction av_conduction;
        clinical_intraventricular_conduction intraventricular_conduction;
        double heart_rate_bpm;
        double atrial_rate_bpm;
        double ventricular_escape_rate_bpm;
        double rr_variability_seconds;
        double minimum_rr_seconds;
        double maximum_rr_seconds;
        double first_degree_pr_ms;
        unsigned int mobitz_cycle_length;
        double wenckebach_pr_increment_ms;
        unsigned int flutter_conduction_ratio;
        unsigned long long seed;
    };

    struct clinical_scenario_config
    {
        clinical_scenario_config();

        unsigned int premature_every_n_beats;
        clinical_ventricular_origin premature_origin;
        double premature_coupling_ratio;
        double compensatory_pause_ratio;
        unsigned int sinus_pause_every_n_beats;
        double sinus_pause_ratio;
    };

    struct clinical_lead_config
    {
        clinical_lead_config();

        double yaw_degrees;
        double pitch_degrees;
        double roll_degrees;
        double lead_gain[clinical_lead_count];
    };

    struct clinical_ecg_config
    {
        clinical_ecg_config();

        unsigned int sampling_rate_hz;
        clinical_timing_config timing;
        clinical_morphology_config morphology;
        clinical_rhythm_config rhythm;
        clinical_scenario_config scenario;
        clinical_lead_config leads;
    };

    struct clinical_atrial_event
    {
        unsigned long long atrial_index;
        double onset_time_seconds;
        double peak_time_seconds;
        double offset_time_seconds;
        bool visible;
        bool conducted;
        long long linked_ventricular_index;
    };

    struct clinical_beat_annotation
    {
        unsigned long long beat_index;
        long long linked_atrial_index;
        clinical_rhythm rhythm;
        clinical_av_conduction av_conduction;
        clinical_intraventricular_conduction intraventricular_conduction;
        clinical_ventricular_origin origin;
        double rr_interval_seconds;
        double pr_interval_seconds;
        double qrs_duration_seconds;
        double qt_interval_seconds;
        double qtc_interval_seconds;
        double qrs_onset_time_seconds;
        double q_peak_time_seconds;
        double r_peak_time_seconds;
        double s_peak_time_seconds;
        double j_point_time_seconds;
        double qrs_offset_time_seconds;
        double t_onset_time_seconds;
        double t_peak_time_seconds;
        double t_offset_time_seconds;
        bool p_present;
        bool qrs_present;
        bool t_present;
        bool rr_was_clipped;
    };

    struct clinical_fiducial_annotation
    {
        unsigned long long beat_index;
        long long atrial_index;
        int lead_index;
        clinical_fiducial_kind kind;
        clinical_fiducial_source source;
        unsigned long long sample_index;
        double time_seconds;
        double amplitude_mv;
        bool present;
    };

    class clinical_ecg_record
    {
    public:
        clinical_ecg_record();
        clinical_ecg_record(const clinical_ecg_record& other);
        clinical_ecg_record& operator=(const clinical_ecg_record& other);
        ~clinical_ecg_record();

        unsigned int sampling_rate_hz() const;
        unsigned int sample_count() const;
        unsigned int lead_count() const;
        const char* lead_name(unsigned int lead_index) const;
        const double* lead_data(unsigned int lead_index) const;
        unsigned int atrial_event_count() const;
        const clinical_atrial_event* atrial_events() const;
        unsigned int beat_count() const;
        const clinical_beat_annotation* beats() const;
        unsigned int fiducial_count() const;
        const clinical_fiducial_annotation* fiducials() const;

    private:
        struct implementation;
        implementation* implementation_;
        friend class clinical_ecg_generator;
    };

    class clinical_ecg_generator
    {
    public:
        clinical_ecg_generator();
        explicit clinical_ecg_generator(const clinical_ecg_config& config);
        clinical_ecg_generator(const clinical_ecg_generator& other);
        clinical_ecg_generator& operator=(const clinical_ecg_generator& other);
        ~clinical_ecg_generator();

        bool configure(const clinical_ecg_config& config);
        bool valid() const;
        const clinical_ecg_config& config() const;
        bool generate(unsigned int sample_count, clinical_ecg_record& output) const;

    private:
        struct implementation;
        implementation* implementation_;
    };
}
