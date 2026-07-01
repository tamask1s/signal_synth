#pragma once

namespace signal_synth
{
    class clinical_ecg_record;

    enum ecg_lead_region
    {
        ecg_region_all = 0,
        ecg_region_limb = 1,
        ecg_region_precordial = 2,
        ecg_region_inferior = 3,
        ecg_region_anterior = 4,
        ecg_region_septal = 5,
        ecg_region_lateral = 6,
        ecg_region_count = 7
    };

    struct ecg_lead_morphology
    {
        unsigned long long beat_index;
        unsigned int lead_index;
        bool p_present;
        double p_amplitude_mv;
        double p_duration_seconds;
        double q_amplitude_mv;
        double q_duration_seconds;
        double r_amplitude_mv;
        double s_amplitude_mv;
        double qrs_peak_to_peak_mv;
        double qrs_rms_mv;
        double st_j_mv;
        double st_j60_mv;
        bool t_present;
        double t_amplitude_mv;
        double t_duration_seconds;
    };

    bool ecg_lead_is_in_region(unsigned int lead_index, ecg_lead_region region);
    const char* ecg_lead_region_name(ecg_lead_region region);

    class ecg_morphology_report
    {
    public:
        ecg_morphology_report();
        ecg_morphology_report(const ecg_morphology_report& other);
        ecg_morphology_report& operator=(const ecg_morphology_report& other);
        ~ecg_morphology_report();

        unsigned int entry_count() const;
        const ecg_lead_morphology* entries() const;
        const ecg_lead_morphology* find(unsigned long long beat_index, unsigned int lead_index) const;

    private:
        struct implementation;
        implementation* implementation_;
        friend bool measure_ecg_morphology(const clinical_ecg_record&, ecg_morphology_report&);
    };

    bool measure_ecg_morphology(const clinical_ecg_record& record, ecg_morphology_report& output);
}
