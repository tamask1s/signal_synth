#pragma once

namespace signal_synth
{
    struct clinical_ecg_config;
    class clinical_ecg_record;

    enum ecg_condition_code
    {
        ecg_condition_ndt = 0,
        ecg_condition_nst = 1,
        ecg_condition_dig = 2,
        ecg_condition_lngqt = 3,
        ecg_condition_norm = 4,
        ecg_condition_imi = 5,
        ecg_condition_asmi = 6,
        ecg_condition_lvh = 7,
        ecg_condition_lafb = 8,
        ecg_condition_isc = 9,
        ecg_condition_irbbb = 10,
        ecg_condition_1avb = 11,
        ecg_condition_ivcd = 12,
        ecg_condition_iscal = 13,
        ecg_condition_crbbb = 14,
        ecg_condition_clbbb = 15,
        ecg_condition_ilmi = 16,
        ecg_condition_lao_lae = 17,
        ecg_condition_ami = 18,
        ecg_condition_almi = 19,
        ecg_condition_iscin = 20,
        ecg_condition_injas = 21,
        ecg_condition_lmi = 22,
        ecg_condition_iscil = 23,
        ecg_condition_lpfb = 24,
        ecg_condition_iscas = 25,
        ecg_condition_injal = 26,
        ecg_condition_iscla = 27,
        ecg_condition_rvh = 28,
        ecg_condition_aneur = 29,
        ecg_condition_rao_rae = 30,
        ecg_condition_el = 31,
        ecg_condition_wpw = 32,
        ecg_condition_ilbbb = 33,
        ecg_condition_iplmi = 34,
        ecg_condition_iscan = 35,
        ecg_condition_ipmi = 36,
        ecg_condition_sehyp = 37,
        ecg_condition_injin = 38,
        ecg_condition_injla = 39,
        ecg_condition_pmi = 40,
        ecg_condition_3avb = 41,
        ecg_condition_injil = 42,
        ecg_condition_2avb = 43,
        ecg_condition_abqrs = 44,
        ecg_condition_pvc = 45,
        ecg_condition_std = 46,
        ecg_condition_vclvh = 47,
        ecg_condition_qwave = 48,
        ecg_condition_lowt = 49,
        ecg_condition_nt = 50,
        ecg_condition_pac = 51,
        ecg_condition_lpr = 52,
        ecg_condition_invt = 53,
        ecg_condition_lvolt = 54,
        ecg_condition_hvolt = 55,
        ecg_condition_tab = 56,
        ecg_condition_ste = 57,
        ecg_condition_prc = 58,
        ecg_condition_sr = 59,
        ecg_condition_afib = 60,
        ecg_condition_stach = 61,
        ecg_condition_sarrh = 62,
        ecg_condition_sbrad = 63,
        ecg_condition_pace = 64,
        ecg_condition_svarr = 65,
        ecg_condition_bigu = 66,
        ecg_condition_aflt = 67,
        ecg_condition_svtac = 68,
        ecg_condition_psvt = 69,
        ecg_condition_trigu = 70,
        ecg_condition_count = 71
    };

    enum ecg_condition_category
    {
        ecg_category_normal = 0,
        ecg_category_rhythm = 1,
        ecg_category_conduction = 2,
        ecg_category_morphology = 3,
        ecg_category_hypertrophy = 4,
        ecg_category_infarction_injury = 5,
        ecg_category_ischemia_repolarization = 6
    };

    enum ecg_condition_support
    {
        ecg_support_catalog_only = 0,
        ecg_support_parameterized = 1,
        ecg_support_native = 2
    };

    struct ecg_condition_info
    {
        ecg_condition_code code;
        const char* scp_code;
        const char* name;
        ecg_condition_category category;
        bool diagnostic_statement;
        bool form_statement;
        bool rhythm_statement;
        ecg_condition_support support;
    };

    const ecg_condition_info* ecg_condition_catalog();
    unsigned int ecg_condition_catalog_size();
    unsigned int ecg_scenario_engine_version();
    const ecg_condition_info* find_ecg_condition(ecg_condition_code code);
    const ecg_condition_info* find_ecg_condition(const char* scp_code);
    bool ecg_condition_supports_variable_severity(ecg_condition_code code);

    enum ecg_scenario_fidelity_policy
    {
        ecg_fidelity_native_only = 0,
        ecg_fidelity_allow_parameterized = 1
    };

    enum ecg_second_degree_av_pattern
    {
        ecg_second_degree_unspecified = 0,
        ecg_second_degree_mobitz_i = 1,
        ecg_second_degree_mobitz_ii = 2
    };

    enum ecg_q_wave_territory
    {
        ecg_q_wave_unspecified = 0,
        ecg_q_wave_inferior = 1,
        ecg_q_wave_anterior = 2,
        ecg_q_wave_lateral = 3
    };

    enum ecg_episode_type
    {
        ecg_episode_none = 0,
        ecg_episode_psvt = 1,
        ecg_episode_svarr = 2
    };

    enum ecg_flutter_conduction_pattern
    {
        ecg_flutter_fixed = 0,
        ecg_flutter_alternate_2_3 = 1,
        ecg_flutter_cycle_2_3_4 = 2
    };

    enum ecg_pacing_mode
    {
        ecg_pacing_ventricular = 0,
        ecg_pacing_atrial = 1,
        ecg_pacing_dual_chamber = 2
    };

    enum ecg_scenario_issue_severity
    {
        ecg_issue_warning = 0,
        ecg_issue_error = 1
    };

    enum ecg_scenario_issue_code
    {
        ecg_issue_none = 0,
        ecg_issue_invalid_parameter = 1,
        ecg_issue_unsupported_condition = 2,
        ecg_issue_fidelity_policy = 3,
        ecg_issue_condition_conflict = 4,
        ecg_issue_missing_requirement = 5,
        ecg_issue_parameterized_condition = 6,
        ecg_issue_generation_failed = 7
    };

    enum ecg_phenotype_assertion_code
    {
        ecg_assert_rhythm = 0,
        ecg_assert_heart_rate = 1,
        ecg_assert_rr_variability = 2,
        ecg_assert_p_wave_presence = 3,
        ecg_assert_atrial_ventricular_ratio = 4,
        ecg_assert_ectopic_origin = 5,
        ecg_assert_ectopic_cadence = 6,
        ecg_assert_premature_coupling = 7,
        ecg_assert_pr_interval = 8,
        ecg_assert_dropped_atrial_events = 9,
        ecg_assert_av_pattern = 10,
        ecg_assert_ventricular_escape = 11,
        ecg_assert_qrs_duration = 12,
        ecg_assert_terminal_v1_polarity = 13,
        ecg_assert_terminal_source_polarity = ecg_assert_terminal_v1_polarity,
        ecg_assert_qtc_interval = 14,
        ecg_assert_pacing = 15,
        ecg_assert_q_wave_amplitude = 16,
        ecg_assert_q_wave_duration = 17,
        ecg_assert_q_wave_lead_count = 18,
        ecg_assert_low_qrs_voltage = 19,
        ecg_assert_high_qrs_voltage = 20,
        ecg_assert_left_ventricular_voltage = 21,
        ecg_assert_right_precordial_rs_ratio = 22,
        ecg_assert_septal_qrs_voltage = 23,
        ecg_assert_p_wave_duration = 24,
        ecg_assert_p_wave_amplitude = 25,
        ecg_assert_posterior_reciprocal_r_amplitude = 26,
        ecg_assert_posterior_reciprocal_lead_count = 27,
        ecg_assert_injury_st_deviation = 28,
        ecg_assert_injury_st_lead_count = 29,
        ecg_assert_st_deviation = 30,
        ecg_assert_st_lead_count = 31,
        ecg_assert_st_slope = 32,
        ecg_assert_t_amplitude = 33,
        ecg_assert_t_lead_count = 34,
        ecg_assert_t_polarity_dispersion = 35,
        ecg_assert_t_duration = 36,
        ecg_assert_frontal_axis = 37,
        ecg_assert_lateral_qrs_polarity = 38,
        ecg_assert_inferior_qrs_polarity = 39,
        ecg_assert_delta_wave = 40,
        ecg_assert_complete_bbb_exclusion = 41,
        ecg_assert_episode_coverage = 42,
        ecg_assertion_code_count = 43
    };

    enum ecg_phenotype_assertion_status
    {
        ecg_assertion_not_evaluated = 0,
        ecg_assertion_passed = 1,
        ecg_assertion_failed = 2
    };

    class ecg_qa_scenario
    {
    public:
        struct implementation;

        ecg_qa_scenario();
        ecg_qa_scenario(const ecg_qa_scenario& other);
        ecg_qa_scenario& operator=(const ecg_qa_scenario& other);
        ~ecg_qa_scenario();

        bool add_condition(ecg_condition_code code, double severity = 1.0);
        bool remove_condition(ecg_condition_code code);
        void clear_conditions();
        unsigned int condition_count() const;
        ecg_condition_code condition(unsigned int index) const;
        double condition_severity(unsigned int index) const;
        bool has_condition(ecg_condition_code code) const;

        bool set_sampling_rate_hz(unsigned int value);
        unsigned int sampling_rate_hz() const;
        bool set_seed(unsigned long long value);
        unsigned long long seed() const;
        bool set_heart_rate_bpm(double value);
        double heart_rate_bpm() const;
        bool set_rr_variability_seconds(double value);
        double rr_variability_seconds() const;
        bool set_minimum_rr_seconds(double value);
        double minimum_rr_seconds() const;
        bool set_maximum_rr_seconds(double value);
        double maximum_rr_seconds() const;
        bool set_hrv_modulation(double lf_hf_ratio, double lf_center_hz, double lf_bandwidth_hz, double hf_center_hz, double hf_bandwidth_hz, double respiratory_frequency_hz, double respiratory_amplitude_seconds);
        bool set_activity_modulation(double start_seconds, double duration_seconds, double intensity);
        double activity_start_seconds() const;
        double activity_duration_seconds() const;
        double activity_intensity() const;
        void set_retain_source_channels(bool value);
        bool retain_source_channels() const;
        bool set_ectopic_every_n_beats(unsigned int value);
        unsigned int ectopic_every_n_beats() const;
        bool set_second_degree_av_pattern(ecg_second_degree_av_pattern value);
        ecg_second_degree_av_pattern second_degree_av_pattern() const;
        bool set_q_wave_territory(ecg_q_wave_territory value);
        ecg_q_wave_territory q_wave_territory() const;
        bool set_episode_type(ecg_episode_type value);
        ecg_episode_type episode_type() const;
        bool set_episode_start_seconds(double value);
        double episode_start_seconds() const;
        bool set_episode_duration_seconds(double value);
        double episode_duration_seconds() const;
        bool set_episode_rate_bpm(double value);
        double episode_rate_bpm() const;
        bool set_flutter_conduction_pattern(ecg_flutter_conduction_pattern value);
        ecg_flutter_conduction_pattern flutter_conduction_pattern() const;
        bool set_pacing_mode(ecg_pacing_mode value);
        ecg_pacing_mode pacing_mode() const;
        bool set_pacing_non_capture_every_n_beats(unsigned int value);
        unsigned int pacing_non_capture_every_n_beats() const;
        bool set_fidelity_policy(ecg_scenario_fidelity_policy value);
        ecg_scenario_fidelity_policy fidelity_policy() const;
        unsigned int schema_version() const;
        unsigned long long fingerprint() const;

    private:
        implementation* implementation_;
        friend class ecg_scenario_engine;
    };

    class ecg_scenario_report
    {
    public:
        struct implementation;

        ecg_scenario_report();
        ecg_scenario_report(const ecg_scenario_report& other);
        ecg_scenario_report& operator=(const ecg_scenario_report& other);
        ~ecg_scenario_report();

        bool success() const;
        unsigned long long scenario_fingerprint() const;
        unsigned int engine_version() const;
        unsigned long long run_fingerprint() const;
        unsigned int effective_condition_count() const;
        ecg_condition_code effective_condition(unsigned int index) const;
        double effective_condition_severity(unsigned int index) const;
        bool condition_was_inferred(unsigned int index) const;
        unsigned int issue_count() const;
        ecg_scenario_issue_severity issue_severity(unsigned int index) const;
        ecg_scenario_issue_code issue_code(unsigned int index) const;
        ecg_condition_code issue_condition(unsigned int index) const;
        ecg_condition_code issue_related_condition(unsigned int index) const;
        const char* issue_message(unsigned int index) const;
        unsigned int generated_sample_count() const;
        bool phenotype_passed() const;
        unsigned int assertion_count() const;
        ecg_condition_code assertion_condition(unsigned int index) const;
        ecg_phenotype_assertion_code assertion_code(unsigned int index) const;
        ecg_phenotype_assertion_status assertion_status(unsigned int index) const;
        double assertion_measured_value(unsigned int index) const;
        double assertion_minimum(unsigned int index) const;
        double assertion_maximum(unsigned int index) const;
        const char* assertion_name(unsigned int index) const;
        const char* assertion_unit(unsigned int index) const;

    private:
        implementation* implementation_;
        friend class ecg_scenario_engine;
    };

    class ecg_scenario_engine
    {
    public:
        bool validate(const ecg_qa_scenario& scenario, ecg_scenario_report& report) const;
        bool compile(const ecg_qa_scenario& scenario, clinical_ecg_config& output, ecg_scenario_report& report) const;
        bool generate(const ecg_qa_scenario& scenario, unsigned int sample_count, clinical_ecg_record& output, ecg_scenario_report& report) const;
    };
}
