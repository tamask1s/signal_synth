#include "ecg_scenario.h"

#include "clinical_ecg.h"
#include "ecg_morphology.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

namespace signal_synth
{
    namespace
    {
        const unsigned int SCENARIO_SCHEMA_VERSION = 2;
        const unsigned int SCENARIO_ENGINE_VERSION = 16;
        const unsigned long long DEFAULT_SEED = 0x5343454e4152494fULL;
        const ecg_condition_code NO_CONDITION = ecg_condition_count;

        struct scenario_condition
        {
            ecg_condition_code code;
            double severity;
        };

        struct effective_condition_entry
        {
            ecg_condition_code code;
            double severity;
            bool inferred;
        };

        struct report_issue
        {
            ecg_scenario_issue_severity severity;
            ecg_scenario_issue_code code;
            ecg_condition_code condition;
            ecg_condition_code related;
            std::string message;
        };

        struct phenotype_assertion
        {
            ecg_condition_code condition;
            ecg_phenotype_assertion_code code;
            ecg_phenotype_assertion_status status;
            double measured;
            double minimum;
            double maximum;
            std::string name;
            std::string unit;
        };

        template <typename enum_type>
        typename std::underlying_type<enum_type>::type enum_value(const enum_type& value)
        {
            typename std::underlying_type<enum_type>::type result = 0;
            static_assert(sizeof(result) == sizeof(value), "Unexpected enum representation");
            std::memcpy(&result, &value, sizeof(result));
            return result;
        }

        bool valid_condition(ecg_condition_code code)
        {
            const int value = enum_value(code);
            return value >= 0 && value < ecg_condition_count;
        }

        bool valid_fidelity(ecg_scenario_fidelity_policy value)
        {
            const int raw = enum_value(value);
            return raw >= ecg_fidelity_native_only && raw <= ecg_fidelity_allow_parameterized;
        }

        bool valid_second_degree_pattern(ecg_second_degree_av_pattern value)
        {
            const int raw = enum_value(value);
            return raw >= ecg_second_degree_unspecified && raw <= ecg_second_degree_mobitz_ii;
        }

        bool valid_q_wave_territory(ecg_q_wave_territory value)
        {
            const int raw = enum_value(value);
            return raw >= ecg_q_wave_unspecified && raw <= ecg_q_wave_lateral;
        }

        bool valid_episode_type(ecg_rhythm_episode_type value)
        {
            const int raw = enum_value(value);
            return raw >= ecg_episode_afib && raw <= ecg_episode_asystole;
        }

        bool valid_morphology_component_type(ecg_morphology_component_type value)
        {
            const int raw = enum_value(value);
            return raw >= ecg_component_p_biphasic && raw <= ecg_component_u_wave;
        }

        clinical_morphology_component_kind clinical_component_type(ecg_morphology_component_type value)
        {
            return static_cast<clinical_morphology_component_kind>(enum_value(value));
        }

        clinical_episode_kind clinical_episode_type(ecg_rhythm_episode_type value)
        {
            switch (value)
            {
            case ecg_episode_afib: return clinical_episode_afib;
            case ecg_episode_psvt: return clinical_episode_psvt;
            case ecg_episode_svarr: return clinical_episode_svarr;
            case ecg_episode_vt: return clinical_episode_vt;
            case ecg_episode_vf: return clinical_episode_vf;
            case ecg_episode_asystole: return clinical_episode_asystole;
            }
            return clinical_episode_none;
        }

        bool valid_flutter_pattern(ecg_flutter_conduction_pattern value)
        {
            const int raw = enum_value(value);
            return raw >= ecg_flutter_fixed && raw <= ecg_flutter_cycle_2_3_4;
        }

        bool valid_pacing_mode(ecg_pacing_mode value)
        {
            const int raw = enum_value(value);
            return raw >= ecg_pacing_ventricular && raw <= ecg_pacing_dual_chamber;
        }

        bool valid_qt_adaptation_model(ecg_qt_adaptation_model value)
        {
            const int raw = enum_value(value);
            return raw >= ecg_qt_adaptation_fixed && raw <= ecg_qt_adaptation_hodges;
        }

        // Catalog order and statement names follow PTB-XL 1.0.3 scp_statements.csv (CC BY 4.0).
        const ecg_condition_info condition_catalog[ecg_condition_count] = {
            {ecg_condition_ndt, "NDT", "non-diagnostic T abnormalities", ecg_category_ischemia_repolarization, true, true, false, ecg_support_parameterized},
            {ecg_condition_nst, "NST_", "non-specific ST changes", ecg_category_ischemia_repolarization, true, true, false, ecg_support_parameterized},
            {ecg_condition_dig, "DIG", "digitalis effect", ecg_category_ischemia_repolarization, true, true, false, ecg_support_parameterized},
            {ecg_condition_lngqt, "LNGQT", "long QT interval", ecg_category_ischemia_repolarization, true, true, false, ecg_support_parameterized},
            {ecg_condition_norm, "NORM", "normal ECG", ecg_category_normal, true, false, false, ecg_support_native},
            {ecg_condition_imi, "IMI", "inferior myocardial infarction", ecg_category_infarction_injury, true, false, false, ecg_support_parameterized},
            {ecg_condition_asmi, "ASMI", "anteroseptal myocardial infarction", ecg_category_infarction_injury, true, false, false, ecg_support_parameterized},
            {ecg_condition_lvh, "LVH", "left ventricular hypertrophy", ecg_category_hypertrophy, true, false, false, ecg_support_parameterized},
            {ecg_condition_lafb, "LAFB", "left anterior fascicular block", ecg_category_conduction, true, false, false, ecg_support_parameterized},
            {ecg_condition_isc, "ISC_", "non-specific ischemic ST-T changes", ecg_category_ischemia_repolarization, true, false, false, ecg_support_parameterized},
            {ecg_condition_irbbb, "IRBBB", "incomplete right bundle branch block", ecg_category_conduction, true, false, false, ecg_support_parameterized},
            {ecg_condition_1avb, "1AVB", "first degree AV block", ecg_category_conduction, true, false, false, ecg_support_native},
            {ecg_condition_ivcd, "IVCD", "non-specific intraventricular conduction disturbance", ecg_category_conduction, true, false, false, ecg_support_parameterized},
            {ecg_condition_iscal, "ISCAL", "ischemic ST-T changes in anterolateral leads", ecg_category_ischemia_repolarization, true, false, false, ecg_support_parameterized},
            {ecg_condition_crbbb, "CRBBB", "complete right bundle branch block", ecg_category_conduction, true, false, false, ecg_support_parameterized},
            {ecg_condition_clbbb, "CLBBB", "complete left bundle branch block", ecg_category_conduction, true, false, false, ecg_support_parameterized},
            {ecg_condition_ilmi, "ILMI", "inferolateral myocardial infarction", ecg_category_infarction_injury, true, false, false, ecg_support_parameterized},
            {ecg_condition_lao_lae, "LAO/LAE", "left atrial overload or enlargement", ecg_category_hypertrophy, true, false, false, ecg_support_parameterized},
            {ecg_condition_ami, "AMI", "anterior myocardial infarction", ecg_category_infarction_injury, true, false, false, ecg_support_parameterized},
            {ecg_condition_almi, "ALMI", "anterolateral myocardial infarction", ecg_category_infarction_injury, true, false, false, ecg_support_parameterized},
            {ecg_condition_iscin, "ISCIN", "ischemic ST-T changes in inferior leads", ecg_category_ischemia_repolarization, true, false, false, ecg_support_parameterized},
            {ecg_condition_injas, "INJAS", "subendocardial injury in anteroseptal leads", ecg_category_infarction_injury, true, false, false, ecg_support_parameterized},
            {ecg_condition_lmi, "LMI", "lateral myocardial infarction", ecg_category_infarction_injury, true, false, false, ecg_support_parameterized},
            {ecg_condition_iscil, "ISCIL", "ischemic ST-T changes in inferolateral leads", ecg_category_ischemia_repolarization, true, false, false, ecg_support_parameterized},
            {ecg_condition_lpfb, "LPFB", "left posterior fascicular block", ecg_category_conduction, true, false, false, ecg_support_parameterized},
            {ecg_condition_iscas, "ISCAS", "ischemic ST-T changes in anteroseptal leads", ecg_category_ischemia_repolarization, true, false, false, ecg_support_parameterized},
            {ecg_condition_injal, "INJAL", "subendocardial injury in anterolateral leads", ecg_category_infarction_injury, true, false, false, ecg_support_parameterized},
            {ecg_condition_iscla, "ISCLA", "ischemic ST-T changes in lateral leads", ecg_category_ischemia_repolarization, true, false, false, ecg_support_parameterized},
            {ecg_condition_rvh, "RVH", "right ventricular hypertrophy", ecg_category_hypertrophy, true, false, false, ecg_support_parameterized},
            {ecg_condition_aneur, "ANEUR", "ST-T changes compatible with ventricular aneurysm", ecg_category_ischemia_repolarization, true, false, false, ecg_support_parameterized},
            {ecg_condition_rao_rae, "RAO/RAE", "right atrial overload or enlargement", ecg_category_hypertrophy, true, false, false, ecg_support_parameterized},
            {ecg_condition_el, "EL", "electrolytic disturbance or drug effect", ecg_category_ischemia_repolarization, true, false, false, ecg_support_parameterized},
            {ecg_condition_wpw, "WPW", "Wolff-Parkinson-White syndrome", ecg_category_conduction, true, false, false, ecg_support_parameterized},
            {ecg_condition_ilbbb, "ILBBB", "incomplete left bundle branch block", ecg_category_conduction, true, false, false, ecg_support_parameterized},
            {ecg_condition_iplmi, "IPLMI", "inferoposterolateral myocardial infarction", ecg_category_infarction_injury, true, false, false, ecg_support_parameterized},
            {ecg_condition_iscan, "ISCAN", "ischemic ST-T changes in anterior leads", ecg_category_ischemia_repolarization, true, false, false, ecg_support_parameterized},
            {ecg_condition_ipmi, "IPMI", "inferoposterior myocardial infarction", ecg_category_infarction_injury, true, false, false, ecg_support_parameterized},
            {ecg_condition_sehyp, "SEHYP", "septal hypertrophy", ecg_category_hypertrophy, true, false, false, ecg_support_parameterized},
            {ecg_condition_injin, "INJIN", "subendocardial injury in inferior leads", ecg_category_infarction_injury, true, false, false, ecg_support_parameterized},
            {ecg_condition_injla, "INJLA", "subendocardial injury in lateral leads", ecg_category_infarction_injury, true, false, false, ecg_support_parameterized},
            {ecg_condition_pmi, "PMI", "posterior myocardial infarction", ecg_category_infarction_injury, true, false, false, ecg_support_parameterized},
            {ecg_condition_3avb, "3AVB", "third degree AV block", ecg_category_conduction, true, false, false, ecg_support_native},
            {ecg_condition_injil, "INJIL", "subendocardial injury in inferolateral leads", ecg_category_infarction_injury, true, false, false, ecg_support_parameterized},
            {ecg_condition_2avb, "2AVB", "second degree AV block", ecg_category_conduction, true, false, false, ecg_support_native},
            {ecg_condition_abqrs, "ABQRS", "abnormal QRS", ecg_category_morphology, false, true, false, ecg_support_catalog_only},
            {ecg_condition_pvc, "PVC", "ventricular premature complex", ecg_category_rhythm, false, true, false, ecg_support_native},
            {ecg_condition_std, "STD_", "non-specific ST depression", ecg_category_ischemia_repolarization, false, true, false, ecg_support_parameterized},
            {ecg_condition_vclvh, "VCLVH", "voltage criteria for left ventricular hypertrophy", ecg_category_hypertrophy, false, true, false, ecg_support_parameterized},
            {ecg_condition_qwave, "QWAVE", "Q waves present", ecg_category_morphology, false, true, false, ecg_support_parameterized},
            {ecg_condition_lowt, "LOWT", "low amplitude T waves", ecg_category_ischemia_repolarization, false, true, false, ecg_support_parameterized},
            {ecg_condition_nt, "NT_", "non-specific T-wave changes", ecg_category_ischemia_repolarization, false, true, false, ecg_support_parameterized},
            {ecg_condition_pac, "PAC", "atrial premature complex", ecg_category_rhythm, false, true, false, ecg_support_native},
            {ecg_condition_lpr, "LPR", "prolonged PR interval", ecg_category_conduction, false, true, false, ecg_support_parameterized},
            {ecg_condition_invt, "INVT", "inverted T waves", ecg_category_ischemia_repolarization, false, true, false, ecg_support_parameterized},
            {ecg_condition_lvolt, "LVOLT", "low QRS voltages", ecg_category_morphology, false, true, false, ecg_support_parameterized},
            {ecg_condition_hvolt, "HVOLT", "high QRS voltage", ecg_category_morphology, false, true, false, ecg_support_parameterized},
            {ecg_condition_tab, "TAB_", "T-wave abnormality", ecg_category_ischemia_repolarization, false, true, false, ecg_support_parameterized},
            {ecg_condition_ste, "STE_", "non-specific ST elevation", ecg_category_ischemia_repolarization, false, true, false, ecg_support_parameterized},
            {ecg_condition_prc, "PRC(S)", "premature complexes", ecg_category_rhythm, false, true, false, ecg_support_parameterized},
            {ecg_condition_sr, "SR", "sinus rhythm", ecg_category_rhythm, false, false, true, ecg_support_native},
            {ecg_condition_afib, "AFIB", "atrial fibrillation", ecg_category_rhythm, false, false, true, ecg_support_native},
            {ecg_condition_stach, "STACH", "sinus tachycardia", ecg_category_rhythm, false, false, true, ecg_support_parameterized},
            {ecg_condition_sarrh, "SARRH", "sinus arrhythmia", ecg_category_rhythm, false, false, true, ecg_support_parameterized},
            {ecg_condition_sbrad, "SBRAD", "sinus bradycardia", ecg_category_rhythm, false, false, true, ecg_support_parameterized},
            {ecg_condition_pace, "PACE", "normal functioning artificial pacemaker", ecg_category_rhythm, false, false, true, ecg_support_native},
            {ecg_condition_svarr, "SVARR", "supraventricular arrhythmia", ecg_category_rhythm, false, false, true, ecg_support_parameterized},
            {ecg_condition_bigu, "BIGU", "bigeminal pattern", ecg_category_rhythm, false, false, true, ecg_support_parameterized},
            {ecg_condition_aflt, "AFLT", "atrial flutter", ecg_category_rhythm, false, false, true, ecg_support_native},
            {ecg_condition_svtac, "SVTAC", "supraventricular tachycardia", ecg_category_rhythm, false, false, true, ecg_support_native},
            {ecg_condition_psvt, "PSVT", "paroxysmal supraventricular tachycardia", ecg_category_rhythm, false, false, true, ecg_support_native},
            {ecg_condition_trigu, "TRIGU", "trigeminal pattern", ecg_category_rhythm, false, false, true, ecg_support_parameterized}
        };

        void hash_byte(unsigned long long& hash, unsigned int value)
        {
            hash ^= value & 0xffU;
            hash *= 1099511628211ULL;
        }

        void hash_u64(unsigned long long& hash, unsigned long long value)
        {
            for (unsigned int byte = 0; byte < 8; ++byte)
                hash_byte(hash, static_cast<unsigned int>(value >> (byte * 8)));
        }

        unsigned long long quantize(double value, double scale)
        {
            return static_cast<unsigned long long>(std::llround(value * scale));
        }

        bool has_condition(const std::vector<scenario_condition>& conditions, ecg_condition_code code)
        {
            for (const scenario_condition& condition : conditions)
                if (condition.code == code)
                    return true;
            return false;
        }

        double condition_severity(const std::vector<scenario_condition>& conditions, ecg_condition_code code)
        {
            for (const scenario_condition& condition : conditions)
                if (condition.code == code)
                    return condition.severity;
            return 1.0;
        }

        bool supports_variable_severity(ecg_condition_code code)
        {
            return code == ecg_condition_lngqt || code == ecg_condition_lafb || code == ecg_condition_irbbb || code == ecg_condition_ivcd || code == ecg_condition_lpfb || code == ecg_condition_wpw || code == ecg_condition_ilbbb || code == ecg_condition_1avb || code == ecg_condition_lpr || code == ecg_condition_pac || code == ecg_condition_pvc || code == ecg_condition_stach || code == ecg_condition_sarrh || code == ecg_condition_sbrad || code == ecg_condition_qwave || code == ecg_condition_lvolt || code == ecg_condition_hvolt || code == ecg_condition_lvh || code == ecg_condition_rvh || code == ecg_condition_sehyp || code == ecg_condition_vclvh || code == ecg_condition_lao_lae || code == ecg_condition_rao_rae || code == ecg_condition_imi || code == ecg_condition_asmi || code == ecg_condition_ilmi || code == ecg_condition_ami || code == ecg_condition_almi || code == ecg_condition_injas || code == ecg_condition_lmi || code == ecg_condition_injal || code == ecg_condition_iplmi || code == ecg_condition_ipmi || code == ecg_condition_injin || code == ecg_condition_injla || code == ecg_condition_pmi || code == ecg_condition_injil || code == ecg_condition_ndt || code == ecg_condition_nst || code == ecg_condition_dig || code == ecg_condition_isc || code == ecg_condition_iscal || code == ecg_condition_iscin || code == ecg_condition_iscil || code == ecg_condition_iscas || code == ecg_condition_iscla || code == ecg_condition_aneur || code == ecg_condition_el || code == ecg_condition_iscan || code == ecg_condition_std || code == ecg_condition_lowt || code == ecg_condition_nt || code == ecg_condition_invt || code == ecg_condition_tab || code == ecg_condition_ste;
        }

        bool is_infarction_condition(ecg_condition_code code)
        {
            return code == ecg_condition_imi || code == ecg_condition_asmi || code == ecg_condition_ilmi || code == ecg_condition_ami || code == ecg_condition_almi || code == ecg_condition_lmi || code == ecg_condition_iplmi || code == ecg_condition_ipmi || code == ecg_condition_pmi;
        }

        bool is_injury_condition(ecg_condition_code code)
        {
            return code == ecg_condition_injas || code == ecg_condition_injal || code == ecg_condition_injin || code == ecg_condition_injla || code == ecg_condition_injil;
        }

        bool is_posterior_infarction(ecg_condition_code code)
        {
            return code == ecg_condition_pmi || code == ecg_condition_ipmi || code == ecg_condition_iplmi;
        }

        bool has_infarction_q_wave(ecg_condition_code code)
        {
            return is_infarction_condition(code) && code != ecg_condition_pmi;
        }

        bool is_territorial_ischemia(ecg_condition_code code)
        {
            return code == ecg_condition_isc || code == ecg_condition_iscal || code == ecg_condition_iscin || code == ecg_condition_iscil || code == ecg_condition_iscas || code == ecg_condition_iscla || code == ecg_condition_iscan;
        }

        bool is_new_repolarization_condition(ecg_condition_code code)
        {
            return code == ecg_condition_ndt || code == ecg_condition_nst || code == ecg_condition_dig || is_territorial_ischemia(code) || code == ecg_condition_aneur || code == ecg_condition_el || code == ecg_condition_std || code == ecg_condition_lowt || code == ecg_condition_nt || code == ecg_condition_invt || code == ecg_condition_tab || code == ecg_condition_ste;
        }

        bool is_clean_conduction_morphology(ecg_condition_code code)
        {
            return code == ecg_condition_lafb || code == ecg_condition_irbbb || code == ecg_condition_ivcd || code == ecg_condition_crbbb || code == ecg_condition_clbbb || code == ecg_condition_lpfb || code == ecg_condition_wpw || code == ecg_condition_ilbbb;
        }

        const ecg_condition_code repolarization_conditions[] = {
            ecg_condition_ndt,
            ecg_condition_nst,
            ecg_condition_dig,
            ecg_condition_lngqt,
            ecg_condition_isc,
            ecg_condition_iscal,
            ecg_condition_iscin,
            ecg_condition_iscil,
            ecg_condition_iscas,
            ecg_condition_iscla,
            ecg_condition_aneur,
            ecg_condition_el,
            ecg_condition_iscan,
            ecg_condition_std,
            ecg_condition_lowt,
            ecg_condition_nt,
            ecg_condition_invt,
            ecg_condition_tab,
            ecg_condition_ste};

        const ecg_condition_code infarction_injury_conditions[] = {
            ecg_condition_imi,
            ecg_condition_asmi,
            ecg_condition_ilmi,
            ecg_condition_ami,
            ecg_condition_almi,
            ecg_condition_injas,
            ecg_condition_lmi,
            ecg_condition_injal,
            ecg_condition_iplmi,
            ecg_condition_ipmi,
            ecg_condition_injin,
            ecg_condition_injla,
            ecg_condition_pmi,
            ecg_condition_injil};

        unsigned int lead_bit(unsigned int lead)
        {
            return 1U << lead;
        }

        unsigned int inferior_leads()
        {
            return lead_bit(clinical_lead_ii) | lead_bit(clinical_lead_iii) | lead_bit(clinical_lead_avf);
        }

        unsigned int septal_leads()
        {
            return lead_bit(clinical_lead_v1) | lead_bit(clinical_lead_v2);
        }

        unsigned int anterior_leads()
        {
            return lead_bit(clinical_lead_v2) | lead_bit(clinical_lead_v3) | lead_bit(clinical_lead_v4);
        }

        unsigned int anteroseptal_leads()
        {
            return septal_leads() | lead_bit(clinical_lead_v3) | lead_bit(clinical_lead_v4);
        }

        unsigned int lateral_leads()
        {
            return lead_bit(clinical_lead_i) | lead_bit(clinical_lead_avl) | lead_bit(clinical_lead_v5) | lead_bit(clinical_lead_v6);
        }

        unsigned int anterolateral_leads()
        {
            return lead_bit(clinical_lead_i) | lead_bit(clinical_lead_avl) | lead_bit(clinical_lead_v3) | lead_bit(clinical_lead_v4) | lead_bit(clinical_lead_v5) | lead_bit(clinical_lead_v6);
        }

        unsigned int inferolateral_leads()
        {
            return inferior_leads() | lateral_leads();
        }

        unsigned int posterior_reciprocal_leads()
        {
            return lead_bit(clinical_lead_v1) | lead_bit(clinical_lead_v2) | lead_bit(clinical_lead_v3);
        }

        unsigned int widespread_leads()
        {
            return lead_bit(clinical_lead_i) | lead_bit(clinical_lead_ii) | lead_bit(clinical_lead_avl) | lead_bit(clinical_lead_avf) | lead_bit(clinical_lead_v3) | lead_bit(clinical_lead_v4) | lead_bit(clinical_lead_v5) | lead_bit(clinical_lead_v6);
        }

        unsigned int all_leads()
        {
            return (1U << clinical_lead_count) - 1U;
        }

        unsigned int infarction_q_leads(ecg_condition_code code)
        {
            if (code == ecg_condition_imi || code == ecg_condition_ipmi)
                return inferior_leads();
            if (code == ecg_condition_asmi)
                return anteroseptal_leads();
            if (code == ecg_condition_ami)
                return anterior_leads();
            if (code == ecg_condition_lmi)
                return lateral_leads();
            if (code == ecg_condition_almi)
                return anterolateral_leads();
            if (code == ecg_condition_ilmi || code == ecg_condition_iplmi)
                return inferolateral_leads();
            return 0;
        }

        unsigned int injury_leads(ecg_condition_code code)
        {
            if (code == ecg_condition_injas)
                return anteroseptal_leads();
            if (code == ecg_condition_injal)
                return anterolateral_leads();
            if (code == ecg_condition_injin)
                return inferior_leads();
            if (code == ecg_condition_injla)
                return lateral_leads();
            if (code == ecg_condition_injil)
                return inferolateral_leads();
            return 0;
        }

        unsigned int ischemia_leads(ecg_condition_code code)
        {
            if (code == ecg_condition_isc)
                return widespread_leads();
            if (code == ecg_condition_iscal)
                return anterolateral_leads();
            if (code == ecg_condition_iscin)
                return inferior_leads();
            if (code == ecg_condition_iscil)
                return inferolateral_leads();
            if (code == ecg_condition_iscas)
                return anteroseptal_leads();
            if (code == ecg_condition_iscla)
                return lateral_leads();
            if (code == ecg_condition_iscan)
                return anterior_leads();
            return 0;
        }

        void add_effective(std::vector<effective_condition_entry>& conditions, ecg_condition_code code, double severity, bool inferred)
        {
            for (effective_condition_entry& condition : conditions)
            {
                if (condition.code == code)
                {
                    if (!inferred)
                    {
                        condition.severity = severity;
                        condition.inferred = false;
                    }
                    return;
                }
            }
            conditions.push_back(effective_condition_entry{code, severity, inferred});
        }

        bool effective_order(const effective_condition_entry& left, const effective_condition_entry& right)
        {
            return left.code < right.code;
        }

        void add_issue(ecg_scenario_report::implementation& report, ecg_scenario_issue_severity severity, ecg_scenario_issue_code code, ecg_condition_code condition, ecg_condition_code related, const char* message);
    }

    struct ecg_qa_scenario::implementation
    {
        std::vector<scenario_condition> conditions;
        unsigned int sampling_rate_hz;
        unsigned long long seed;
        double heart_rate_bpm;
        double rr_variability_seconds;
        double minimum_rr_seconds;
        double maximum_rr_seconds;
        bool hrv_modulation_enabled;
        double hrv_lf_hf_ratio;
        double hrv_lf_center_hz;
        double hrv_lf_bandwidth_hz;
        double hrv_hf_center_hz;
        double hrv_hf_bandwidth_hz;
        double hrv_respiratory_frequency_hz;
        double hrv_respiratory_amplitude_seconds;
        double hrv_respiratory_phase_radians;
        bool morphology_enabled[ecg_morphology_control_count];
        double morphology_values[ecg_morphology_control_count];
        bool qt_adaptation_enabled;
        ecg_qt_adaptation_model qt_adaptation_model;
        double qt_adaptation_qtc_ms;
        std::vector<ecg_repolarization_episode> repolarization_episodes;
        double activity_start_seconds;
        double activity_duration_seconds;
        double activity_intensity;
        bool retain_source_channels;
        unsigned int ectopic_every_n_beats;
        ecg_second_degree_av_pattern second_degree_pattern;
        ecg_q_wave_territory q_wave_territory;
        std::vector<ecg_rhythm_episode> rhythm_episodes;
        std::vector<ecg_morphology_component> morphology_components;
        unsigned int fusion_every_n_beats;
        double fusion_ventricular_fraction;
        ecg_flutter_conduction_pattern flutter_conduction_pattern;
        ecg_pacing_mode pacing_mode;
        unsigned int pacing_non_capture_every_n_beats;
        ecg_scenario_fidelity_policy fidelity_policy;

        implementation()
            : sampling_rate_hz(500), seed(DEFAULT_SEED), heart_rate_bpm(0.0), rr_variability_seconds(0.0), minimum_rr_seconds(0.0), maximum_rr_seconds(0.0), hrv_modulation_enabled(false), hrv_lf_hf_ratio(1.0), hrv_lf_center_hz(0.10), hrv_lf_bandwidth_hz(0.04), hrv_hf_center_hz(0.25), hrv_hf_bandwidth_hz(0.12), hrv_respiratory_frequency_hz(0.25), hrv_respiratory_amplitude_seconds(0.0), hrv_respiratory_phase_radians(-1.0), qt_adaptation_enabled(false), qt_adaptation_model(ecg_qt_adaptation_fridericia), qt_adaptation_qtc_ms(400.0), activity_start_seconds(0.0), activity_duration_seconds(0.0), activity_intensity(0.0), retain_source_channels(true), ectopic_every_n_beats(0), second_degree_pattern(ecg_second_degree_unspecified), q_wave_territory(ecg_q_wave_unspecified), fusion_every_n_beats(0), fusion_ventricular_fraction(0.0), flutter_conduction_pattern(ecg_flutter_fixed), pacing_mode(ecg_pacing_ventricular), pacing_non_capture_every_n_beats(0), fidelity_policy(ecg_fidelity_allow_parameterized)
        {
            for (unsigned int index = 0; index < ecg_morphology_control_count; ++index)
            {
                morphology_enabled[index] = false;
                morphology_values[index] = 0.0;
            }
        }
    };

    struct ecg_scenario_report::implementation
    {
        bool success;
        bool phenotype_passed;
        unsigned long long fingerprint;
        std::vector<effective_condition_entry> effective_conditions;
        std::vector<report_issue> issues;
        std::vector<phenotype_assertion> assertions;
        unsigned int generated_sample_count;
        unsigned int engine_version;
        unsigned long long run_fingerprint;

        implementation()
            : success(false), phenotype_passed(false), fingerprint(0), generated_sample_count(0), engine_version(SCENARIO_ENGINE_VERSION), run_fingerprint(0)
        {
        }
    };

    namespace
    {
        void add_issue(ecg_scenario_report::implementation& report, ecg_scenario_issue_severity severity, ecg_scenario_issue_code code, ecg_condition_code condition, ecg_condition_code related, const char* message)
        {
            report.issues.push_back(report_issue{severity, code, condition, related, message});
        }

        void add_conflict(ecg_scenario_report::implementation& report, const std::vector<scenario_condition>& conditions, ecg_condition_code left, ecg_condition_code right, const char* message)
        {
            if (has_condition(conditions, left) && has_condition(conditions, right))
                add_issue(report, ecg_issue_error, ecg_issue_condition_conflict, left, right, message);
        }

        void normalize_conditions(const ecg_qa_scenario::implementation& scenario, ecg_scenario_report::implementation& report)
        {
            for (const scenario_condition& condition : scenario.conditions)
                add_effective(report.effective_conditions, condition.code, condition.severity, false);
            if (has_condition(scenario.conditions, ecg_condition_pac) || has_condition(scenario.conditions, ecg_condition_pvc))
            {
                const ecg_condition_code origin = has_condition(scenario.conditions, ecg_condition_pac) ? ecg_condition_pac : ecg_condition_pvc;
                add_effective(report.effective_conditions, ecg_condition_prc, condition_severity(scenario.conditions, origin), true);
            }
            if (has_condition(scenario.conditions, ecg_condition_psvt))
                add_effective(report.effective_conditions, ecg_condition_svarr, condition_severity(scenario.conditions, ecg_condition_psvt), true);
            if (has_condition(scenario.conditions, ecg_condition_svarr))
                add_effective(report.effective_conditions, ecg_condition_psvt, condition_severity(scenario.conditions, ecg_condition_svarr), true);
            if (has_condition(scenario.conditions, ecg_condition_norm) || has_condition(scenario.conditions, ecg_condition_stach) || has_condition(scenario.conditions, ecg_condition_sbrad) || has_condition(scenario.conditions, ecg_condition_sarrh) || has_condition(scenario.conditions, ecg_condition_1avb) || has_condition(scenario.conditions, ecg_condition_2avb) || has_condition(scenario.conditions, ecg_condition_3avb) || has_condition(scenario.conditions, ecg_condition_lpr))
                add_effective(report.effective_conditions, ecg_condition_sr, 1.0, true);
            if (has_condition(scenario.conditions, ecg_condition_clbbb) || has_condition(scenario.conditions, ecg_condition_crbbb) || has_condition(scenario.conditions, ecg_condition_lafb) || has_condition(scenario.conditions, ecg_condition_irbbb) || has_condition(scenario.conditions, ecg_condition_ivcd) || has_condition(scenario.conditions, ecg_condition_lpfb) || has_condition(scenario.conditions, ecg_condition_wpw) || has_condition(scenario.conditions, ecg_condition_ilbbb) || has_condition(scenario.conditions, ecg_condition_qwave) || has_condition(scenario.conditions, ecg_condition_lvolt) || has_condition(scenario.conditions, ecg_condition_hvolt) || has_condition(scenario.conditions, ecg_condition_lvh) || has_condition(scenario.conditions, ecg_condition_rvh) || has_condition(scenario.conditions, ecg_condition_sehyp) || has_condition(scenario.conditions, ecg_condition_vclvh))
            {
                ecg_condition_code morphology = ecg_condition_hvolt;
                if (has_condition(scenario.conditions, ecg_condition_clbbb))
                    morphology = ecg_condition_clbbb;
                else if (has_condition(scenario.conditions, ecg_condition_crbbb))
                    morphology = ecg_condition_crbbb;
                else if (has_condition(scenario.conditions, ecg_condition_lafb))
                    morphology = ecg_condition_lafb;
                else if (has_condition(scenario.conditions, ecg_condition_irbbb))
                    morphology = ecg_condition_irbbb;
                else if (has_condition(scenario.conditions, ecg_condition_ivcd))
                    morphology = ecg_condition_ivcd;
                else if (has_condition(scenario.conditions, ecg_condition_lpfb))
                    morphology = ecg_condition_lpfb;
                else if (has_condition(scenario.conditions, ecg_condition_wpw))
                    morphology = ecg_condition_wpw;
                else if (has_condition(scenario.conditions, ecg_condition_ilbbb))
                    morphology = ecg_condition_ilbbb;
                else if (has_condition(scenario.conditions, ecg_condition_qwave))
                    morphology = ecg_condition_qwave;
                else if (has_condition(scenario.conditions, ecg_condition_lvolt))
                    morphology = ecg_condition_lvolt;
                else if (has_condition(scenario.conditions, ecg_condition_lvh))
                    morphology = ecg_condition_lvh;
                else if (has_condition(scenario.conditions, ecg_condition_rvh))
                    morphology = ecg_condition_rvh;
                else if (has_condition(scenario.conditions, ecg_condition_sehyp))
                    morphology = ecg_condition_sehyp;
                else if (has_condition(scenario.conditions, ecg_condition_vclvh))
                    morphology = ecg_condition_vclvh;
                add_effective(report.effective_conditions, ecg_condition_abqrs, condition_severity(scenario.conditions, morphology), true);
            }
            for (const scenario_condition& condition : scenario.conditions)
            {
                if (is_infarction_condition(condition.code))
                {
                    add_effective(report.effective_conditions, ecg_condition_abqrs, condition.severity, true);
                    if (has_infarction_q_wave(condition.code))
                        add_effective(report.effective_conditions, ecg_condition_qwave, condition.severity, true);
                }
                if (is_territorial_ischemia(condition.code))
                {
                    add_effective(report.effective_conditions, ecg_condition_std, condition.severity, true);
                    add_effective(report.effective_conditions, ecg_condition_invt, condition.severity, true);
                }
                else if (condition.code == ecg_condition_dig)
                    add_effective(report.effective_conditions, ecg_condition_std, condition.severity, true);
                else if (condition.code == ecg_condition_aneur)
                {
                    add_effective(report.effective_conditions, ecg_condition_ste, condition.severity, true);
                    add_effective(report.effective_conditions, ecg_condition_qwave, condition.severity, true);
                    add_effective(report.effective_conditions, ecg_condition_abqrs, condition.severity, true);
                }
                else if (condition.code == ecg_condition_el)
                    add_effective(report.effective_conditions, ecg_condition_tab, condition.severity, true);
            }
            std::sort(report.effective_conditions.begin(), report.effective_conditions.end(), effective_order);
        }

        bool report_has_errors(const ecg_scenario_report::implementation& report)
        {
            for (const report_issue& issue : report.issues)
                if (issue.severity == ecg_issue_error)
                    return true;
            return false;
        }

        void validate_conditions(const ecg_qa_scenario::implementation& scenario, ecg_scenario_report::implementation& report)
        {
            if (scenario.conditions.empty())
                add_issue(report, ecg_issue_error, ecg_issue_missing_requirement, NO_CONDITION, NO_CONDITION, "At least one ECG condition is required.");
            for (const scenario_condition& condition : scenario.conditions)
            {
                const ecg_condition_info& info = condition_catalog[condition.code];
                if (condition.severity != 1.0 && !supports_variable_severity(condition.code))
                    add_issue(report, ecg_issue_error, ecg_issue_invalid_parameter, condition.code, NO_CONDITION, "This condition does not yet support variable severity.");
                if (info.support == ecg_support_catalog_only)
                    add_issue(report, ecg_issue_error, ecg_issue_unsupported_condition, condition.code, NO_CONDITION, "The condition is cataloged but has no waveform implementation.");
                else if (info.support == ecg_support_parameterized && scenario.fidelity_policy == ecg_fidelity_native_only)
                    add_issue(report, ecg_issue_error, ecg_issue_fidelity_policy, condition.code, NO_CONDITION, "The fidelity policy rejects parameterized condition models.");
                else if (info.support == ecg_support_parameterized)
                    add_issue(report, ecg_issue_warning, ecg_issue_parameterized_condition, condition.code, NO_CONDITION, "The condition uses a canonical parameterized phenotype.");
            }

            if (has_condition(scenario.conditions, ecg_condition_norm))
            {
                for (const scenario_condition& condition : scenario.conditions)
                    if (condition.code != ecg_condition_norm && condition.code != ecg_condition_sr)
                        add_issue(report, ecg_issue_error, ecg_issue_condition_conflict, ecg_condition_norm, condition.code, "NORM cannot be combined with an abnormal ECG statement.");
            }

            const ecg_condition_code primary_rhythms[] = {ecg_condition_sr, ecg_condition_afib, ecg_condition_aflt, ecg_condition_svtac, ecg_condition_pace};
            for (unsigned int left = 0; left < sizeof(primary_rhythms) / sizeof(primary_rhythms[0]); ++left)
                for (unsigned int right = left + 1; right < sizeof(primary_rhythms) / sizeof(primary_rhythms[0]); ++right)
                    add_conflict(report, scenario.conditions, primary_rhythms[left], primary_rhythms[right], "Primary rhythm conditions are mutually exclusive.");
            add_conflict(report, scenario.conditions, ecg_condition_stach, ecg_condition_sbrad, "Sinus tachycardia and sinus bradycardia are mutually exclusive.");
            const ecg_condition_code sinus_modifiers[] = {ecg_condition_stach, ecg_condition_sarrh, ecg_condition_sbrad};
            const ecg_condition_code non_sinus_rhythms[] = {ecg_condition_afib, ecg_condition_aflt, ecg_condition_svtac, ecg_condition_pace};
            const ecg_condition_code episode_rhythms[] = {ecg_condition_svarr, ecg_condition_psvt};
            const bool episode_statement = has_condition(scenario.conditions, ecg_condition_svarr) || has_condition(scenario.conditions, ecg_condition_psvt);
            const bool episode_condition = episode_statement || !scenario.rhythm_episodes.empty();
            const bool flutter_condition = has_condition(scenario.conditions, ecg_condition_aflt);
            const bool paced_condition = has_condition(scenario.conditions, ecg_condition_pace);
            for (unsigned int modifier = 0; modifier < sizeof(sinus_modifiers) / sizeof(sinus_modifiers[0]); ++modifier)
            {
                for (unsigned int rhythm = 0; rhythm < sizeof(non_sinus_rhythms) / sizeof(non_sinus_rhythms[0]); ++rhythm)
                    add_conflict(report, scenario.conditions, sinus_modifiers[modifier], non_sinus_rhythms[rhythm], "Sinus rhythm modifiers cannot be combined with a non-sinus primary rhythm.");
                for (unsigned int rhythm = 0; rhythm < sizeof(episode_rhythms) / sizeof(episode_rhythms[0]); ++rhythm)
                    add_conflict(report, scenario.conditions, sinus_modifiers[modifier], episode_rhythms[rhythm], "The first episode timeline pack uses a fixed sinus baseline and cannot compose sinus modifiers.");
            }
            for (unsigned int rhythm = 0; rhythm < sizeof(non_sinus_rhythms) / sizeof(non_sinus_rhythms[0]); ++rhythm)
                for (unsigned int episode = 0; episode < sizeof(episode_rhythms) / sizeof(episode_rhythms[0]); ++episode)
                    add_conflict(report, scenario.conditions, non_sinus_rhythms[rhythm], episode_rhythms[episode], "Episode rhythm conditions cannot be combined with a non-sinus primary rhythm in this pack.");

            const ecg_condition_code av_blocks[] = {ecg_condition_1avb, ecg_condition_2avb, ecg_condition_3avb};
            for (unsigned int left = 0; left < sizeof(av_blocks) / sizeof(av_blocks[0]); ++left)
            {
                for (unsigned int right = left + 1; right < sizeof(av_blocks) / sizeof(av_blocks[0]); ++right)
                    add_conflict(report, scenario.conditions, av_blocks[left], av_blocks[right], "AV block degrees are mutually exclusive.");
                for (unsigned int rhythm = 0; rhythm < sizeof(non_sinus_rhythms) / sizeof(non_sinus_rhythms[0]); ++rhythm)
                    add_conflict(report, scenario.conditions, av_blocks[left], non_sinus_rhythms[rhythm], "The current AV block model requires a sinus atrial timeline.");
                for (unsigned int episode = 0; episode < sizeof(episode_rhythms) / sizeof(episode_rhythms[0]); ++episode)
                    add_conflict(report, scenario.conditions, av_blocks[left], episode_rhythms[episode], "The first episode timeline pack does not compose AV-block timelines.");
                add_conflict(report, scenario.conditions, av_blocks[left], ecg_condition_pac, "The current AV block timeline cannot compose periodic ectopy.");
                add_conflict(report, scenario.conditions, av_blocks[left], ecg_condition_pvc, "The current AV block timeline cannot compose periodic ectopy.");
            }
            add_conflict(report, scenario.conditions, ecg_condition_sarrh, ecg_condition_1avb, "The current AV block timeline does not apply beat-wise sinus arrhythmia.");
            add_conflict(report, scenario.conditions, ecg_condition_sarrh, ecg_condition_2avb, "The current AV block timeline does not apply beat-wise sinus arrhythmia.");
            add_conflict(report, scenario.conditions, ecg_condition_sarrh, ecg_condition_3avb, "The current AV block timeline does not apply beat-wise sinus arrhythmia.");
            add_conflict(report, scenario.conditions, ecg_condition_lpr, ecg_condition_3avb, "A prolonged PR interval is undefined during complete AV dissociation.");
            const ecg_condition_code clean_conduction_conditions[] = {ecg_condition_lafb, ecg_condition_irbbb, ecg_condition_ivcd, ecg_condition_crbbb, ecg_condition_clbbb, ecg_condition_lpfb, ecg_condition_wpw, ecg_condition_ilbbb};
            for (unsigned int left = 0; left < sizeof(clean_conduction_conditions) / sizeof(clean_conduction_conditions[0]); ++left)
            {
                for (unsigned int right = left + 1; right < sizeof(clean_conduction_conditions) / sizeof(clean_conduction_conditions[0]); ++right)
                    add_conflict(report, scenario.conditions, clean_conduction_conditions[left], clean_conduction_conditions[right], "The advanced conduction QA pack supports one clean intraventricular/pre-excitation phenotype per scenario.");
                for (unsigned int rhythm = 0; rhythm < sizeof(non_sinus_rhythms) / sizeof(non_sinus_rhythms[0]); ++rhythm)
                    add_conflict(report, scenario.conditions, clean_conduction_conditions[left], non_sinus_rhythms[rhythm], "The advanced conduction QA pack currently requires a sinus conducted rhythm.");
                for (unsigned int episode = 0; episode < sizeof(episode_rhythms) / sizeof(episode_rhythms[0]); ++episode)
                    add_conflict(report, scenario.conditions, clean_conduction_conditions[left], episode_rhythms[episode], "The first episode timeline pack does not compose advanced conduction phenotypes.");
                for (unsigned int block = 0; block < sizeof(av_blocks) / sizeof(av_blocks[0]); ++block)
                    add_conflict(report, scenario.conditions, clean_conduction_conditions[left], av_blocks[block], "The advanced conduction QA pack currently supports one conduction-family phenotype per scenario.");
                add_conflict(report, scenario.conditions, clean_conduction_conditions[left], ecg_condition_lpr, "The advanced conduction QA pack currently supports one conduction-family phenotype per scenario.");
                add_conflict(report, scenario.conditions, clean_conduction_conditions[left], ecg_condition_pac, "The advanced conduction QA pack currently rejects periodic ectopy composition.");
                add_conflict(report, scenario.conditions, clean_conduction_conditions[left], ecg_condition_pvc, "The advanced conduction QA pack currently rejects periodic ectopy composition.");
            }
            add_conflict(report, scenario.conditions, ecg_condition_bigu, ecg_condition_trigu, "Bigeminal and trigeminal patterns are mutually exclusive.");
            add_conflict(report, scenario.conditions, ecg_condition_pac, ecg_condition_pvc, "The current periodic ectopy scenario supports one ectopic origin.");
            for (unsigned int rhythm = 0; rhythm < sizeof(non_sinus_rhythms) / sizeof(non_sinus_rhythms[0]); ++rhythm)
            {
                add_conflict(report, scenario.conditions, ecg_condition_pac, non_sinus_rhythms[rhythm], "The current ectopy scenario requires sinus rhythm.");
                add_conflict(report, scenario.conditions, ecg_condition_pvc, non_sinus_rhythms[rhythm], "The current ectopy scenario requires sinus rhythm.");
            }
            for (unsigned int episode = 0; episode < sizeof(episode_rhythms) / sizeof(episode_rhythms[0]); ++episode)
            {
                add_conflict(report, scenario.conditions, ecg_condition_pac, episode_rhythms[episode], "The first episode timeline pack does not compose periodic ectopy.");
                add_conflict(report, scenario.conditions, ecg_condition_pvc, episode_rhythms[episode], "The first episode timeline pack does not compose periodic ectopy.");
                add_conflict(report, scenario.conditions, ecg_condition_prc, episode_rhythms[episode], "The first episode timeline pack does not compose premature-complex statements.");
                add_conflict(report, scenario.conditions, ecg_condition_bigu, episode_rhythms[episode], "The first episode timeline pack does not compose bigeminy.");
                add_conflict(report, scenario.conditions, ecg_condition_trigu, episode_rhythms[episode], "The first episode timeline pack does not compose trigeminy.");
            }

            const bool pattern = has_condition(scenario.conditions, ecg_condition_bigu) || has_condition(scenario.conditions, ecg_condition_trigu);
            const bool ectopic_origin = has_condition(scenario.conditions, ecg_condition_pac) || has_condition(scenario.conditions, ecg_condition_pvc);
            if (pattern && !ectopic_origin)
                add_issue(report, ecg_issue_error, ecg_issue_missing_requirement, has_condition(scenario.conditions, ecg_condition_bigu) ? ecg_condition_bigu : ecg_condition_trigu, NO_CONDITION, "Bigeminy or trigeminy requires PAC or PVC origin.");
            if (has_condition(scenario.conditions, ecg_condition_prc) && !ectopic_origin)
                add_issue(report, ecg_issue_error, ecg_issue_missing_requirement, ecg_condition_prc, NO_CONDITION, "PRC(S) requires an explicit PAC or PVC origin.");
            if (has_condition(scenario.conditions, ecg_condition_2avb) && scenario.second_degree_pattern == ecg_second_degree_unspecified)
                add_issue(report, ecg_issue_error, ecg_issue_missing_requirement, ecg_condition_2avb, NO_CONDITION, "2AVB requires an explicit Mobitz I or Mobitz II scenario pattern.");
            if (!has_condition(scenario.conditions, ecg_condition_2avb) && scenario.second_degree_pattern != ecg_second_degree_unspecified)
                add_issue(report, ecg_issue_error, ecg_issue_invalid_parameter, NO_CONDITION, ecg_condition_2avb, "A Mobitz pattern requires the 2AVB condition.");
            if (has_condition(scenario.conditions, ecg_condition_qwave) && scenario.q_wave_territory == ecg_q_wave_unspecified)
                add_issue(report, ecg_issue_error, ecg_issue_missing_requirement, ecg_condition_qwave, NO_CONDITION, "QWAVE requires an explicit inferior, anterior, or lateral territory.");
            if (!has_condition(scenario.conditions, ecg_condition_qwave) && scenario.q_wave_territory != ecg_q_wave_unspecified)
                add_issue(report, ecg_issue_error, ecg_issue_invalid_parameter, NO_CONDITION, ecg_condition_qwave, "q_wave_territory requires the QWAVE condition.");
            if (!ectopic_origin && scenario.ectopic_every_n_beats != 0)
                add_issue(report, ecg_issue_error, ecg_issue_missing_requirement, NO_CONDITION, ecg_condition_prc, "ectopic_every_n_beats requires PAC or PVC.");
            if (!flutter_condition && scenario.flutter_conduction_pattern != ecg_flutter_fixed)
                add_issue(report, ecg_issue_error, ecg_issue_missing_requirement, NO_CONDITION, ecg_condition_aflt, "flutter_conduction_pattern requires AFLT.");
            if (!paced_condition && (scenario.pacing_mode != ecg_pacing_ventricular || scenario.pacing_non_capture_every_n_beats != 0))
                add_issue(report, ecg_issue_error, ecg_issue_missing_requirement, NO_CONDITION, ecg_condition_pace, "Pacing parameters require PACE.");
            if (episode_condition)
            {
                if (scenario.rhythm_episodes.empty())
                    add_issue(report, ecg_issue_error, ecg_issue_missing_requirement, NO_CONDITION, NO_CONDITION, "PSVT and SVARR statements require a matching rhythm episode.");
                if (scenario.rhythm_episodes.size() > clinical_rhythm_episode_max)
                    add_issue(report, ecg_issue_error, ecg_issue_invalid_parameter, NO_CONDITION, NO_CONDITION, "Too many rhythm episodes.");
                bool has_psvt_episode = false;
                bool has_svarr_episode = false;
                for (std::size_t left = 0; left < scenario.rhythm_episodes.size(); ++left)
                {
                    const ecg_rhythm_episode& episode = scenario.rhythm_episodes[left];
                    const bool no_rate = episode.type == ecg_episode_vf || episode.type == ecg_episode_asystole;
                    const bool tachycardia = episode.type == ecg_episode_psvt || episode.type == ecg_episode_svarr || episode.type == ecg_episode_vt;
                    const bool requires_waveform_transition = episode.type == ecg_episode_afib || episode.type == ecg_episode_vf;
                    has_psvt_episode = has_psvt_episode || episode.type == ecg_episode_psvt;
                    has_svarr_episode = has_svarr_episode || episode.type == ecg_episode_svarr;
                    if (!valid_episode_type(episode.type) || !std::isfinite(episode.start_seconds) || !std::isfinite(episode.duration_seconds) || !std::isfinite(episode.transition_seconds) || !std::isfinite(episode.rate_bpm) || episode.start_seconds < 0.0 || episode.duration_seconds <= 0.0 || episode.transition_seconds < 0.0 || episode.transition_seconds > 0.5 * episode.duration_seconds || (requires_waveform_transition && episode.transition_seconds < 0.02) || (no_rate ? episode.rate_bpm != 0.0 : episode.rate_bpm < 10.0 || episode.rate_bpm > 400.0) || (tachycardia && episode.rate_bpm <= 100.0))
                        add_issue(report, ecg_issue_error, ecg_issue_invalid_parameter, NO_CONDITION, NO_CONDITION, "Invalid rhythm episode configuration.");
                    if (tachycardia && scenario.heart_rate_bpm > 0.0 && episode.rate_bpm <= scenario.heart_rate_bpm)
                        add_issue(report, ecg_issue_error, ecg_issue_invalid_parameter, NO_CONDITION, NO_CONDITION, "Tachycardia episode rate must exceed the sinus baseline heart rate.");
                    if (!no_rate && episode.duration_seconds < 2.0 * 60.0 / std::max(1.0, episode.rate_bpm))
                        add_issue(report, ecg_issue_error, ecg_issue_invalid_parameter, NO_CONDITION, NO_CONDITION, "A rhythm episode with beats must contain at least two beats.");
                    for (std::size_t right = left + 1; right < scenario.rhythm_episodes.size(); ++right)
                    {
                        const double left_end = episode.start_seconds + episode.duration_seconds;
                        const double right_end = scenario.rhythm_episodes[right].start_seconds + scenario.rhythm_episodes[right].duration_seconds;
                        if (episode.start_seconds < right_end && scenario.rhythm_episodes[right].start_seconds < left_end)
                            add_issue(report, ecg_issue_error, ecg_issue_condition_conflict, NO_CONDITION, NO_CONDITION, "Rhythm episodes must not overlap.");
                    }
                }
                if (has_condition(scenario.conditions, ecg_condition_psvt) && !has_psvt_episode)
                    add_issue(report, ecg_issue_error, ecg_issue_missing_requirement, ecg_condition_psvt, NO_CONDITION, "PSVT requires a PSVT rhythm episode.");
                if (has_condition(scenario.conditions, ecg_condition_svarr) && !has_svarr_episode)
                    add_issue(report, ecg_issue_error, ecg_issue_missing_requirement, ecg_condition_svarr, NO_CONDITION, "SVARR requires a SVARR rhythm episode.");
                for (const scenario_condition& condition : scenario.conditions)
                    if (condition.code != ecg_condition_sr && condition.code != ecg_condition_psvt && condition.code != ecg_condition_svarr)
                        add_issue(report, ecg_issue_error, ecg_issue_condition_conflict, NO_CONDITION, condition.code, "Rhythm episodes currently compose only with an SR baseline and matching PSVT or SVARR statements.");
                if (scenario.rr_variability_seconds > 0.0)
                    add_issue(report, ecg_issue_error, ecg_issue_invalid_parameter, NO_CONDITION, NO_CONDITION, "The rhythm episode timeline does not apply rr_variability_seconds.");
            }
            const bool extended_morphology = !scenario.morphology_components.empty() || scenario.fusion_every_n_beats > 0;
            if (extended_morphology)
            {
                for (const scenario_condition& condition : scenario.conditions)
                    if (condition.code != ecg_condition_sr)
                        add_issue(report, ecg_issue_error, ecg_issue_condition_conflict, ecg_condition_sr, condition.code, "Extended morphology stress currently requires SR as its only condition.");
                if (!has_condition(scenario.conditions, ecg_condition_sr))
                    add_issue(report, ecg_issue_error, ecg_issue_missing_requirement, ecg_condition_sr, NO_CONDITION, "Extended morphology stress requires the SR condition.");
                if (scenario.ectopic_every_n_beats || !scenario.rhythm_episodes.empty() || scenario.pacing_non_capture_every_n_beats || !scenario.repolarization_episodes.empty())
                    add_issue(report, ecg_issue_error, ecg_issue_condition_conflict, ecg_condition_sr, NO_CONDITION, "Extended morphology stress does not compose with ectopy, rhythm episodes, pacing non-capture, or dynamic repolarization episodes.");
                if (scenario.morphology_components.size() > clinical_morphology_component_max)
                    add_issue(report, ecg_issue_error, ecg_issue_invalid_parameter, ecg_condition_sr, NO_CONDITION, "Too many extended morphology components.");
                const double p_duration = scenario.morphology_enabled[ecg_morphology_p_duration_ms] ? scenario.morphology_values[ecg_morphology_p_duration_ms] : 100.0;
                const double qrs_duration = scenario.morphology_enabled[ecg_morphology_qrs_duration_ms] ? scenario.morphology_values[ecg_morphology_qrs_duration_ms] : 90.0;
                const double t_duration = scenario.morphology_enabled[ecg_morphology_t_duration_ms] ? scenario.morphology_values[ecg_morphology_t_duration_ms] : 180.0;
                for (std::size_t index = 0; index < scenario.morphology_components.size(); ++index)
                {
                    const ecg_morphology_component& component = scenario.morphology_components[index];
                    const bool p_component = component.type == ecg_component_p_biphasic || component.type == ecg_component_p_notch;
                    const bool qrs_component = component.type == ecg_component_r_prime || component.type == ecg_component_qrs_fragment;
                    const bool t_component = component.type == ecg_component_t_biphasic || component.type == ecg_component_t_notch;
                    if (!valid_morphology_component_type(component.type) || !component.lead_mask || !std::isfinite(component.amplitude_mv) || !std::isfinite(component.offset_ms) || !std::isfinite(component.duration_ms) || (p_component && component.offset_ms + component.duration_ms > p_duration) || (qrs_component && component.offset_ms + component.duration_ms > qrs_duration) || (t_component && component.offset_ms + component.duration_ms > t_duration))
                        add_issue(report, ecg_issue_error, ecg_issue_invalid_parameter, ecg_condition_sr, NO_CONDITION, "Extended morphology component does not fit its parent wave.");
                }
                if (scenario.fusion_every_n_beats == 1 || (scenario.fusion_every_n_beats == 0 ? scenario.fusion_ventricular_fraction != 0.0 : !std::isfinite(scenario.fusion_ventricular_fraction) || scenario.fusion_ventricular_fraction < 0.1 || scenario.fusion_ventricular_fraction > 0.9))
                    add_issue(report, ecg_issue_error, ecg_issue_invalid_parameter, ecg_condition_sr, NO_CONDITION, "Invalid fusion-beat cadence or ventricular fraction.");
            }
            if (scenario.qt_adaptation_enabled)
            {
                if (!valid_qt_adaptation_model(scenario.qt_adaptation_model) || !std::isfinite(scenario.qt_adaptation_qtc_ms) || scenario.qt_adaptation_qtc_ms < 250.0 || scenario.qt_adaptation_qtc_ms > 700.0)
                    add_issue(report, ecg_issue_error, ecg_issue_invalid_parameter, NO_CONDITION, NO_CONDITION, "Invalid QT adaptation configuration.");
            }
            if (!scenario.repolarization_episodes.empty())
            {
                if (has_condition(scenario.conditions, ecg_condition_norm))
                    add_issue(report, ecg_issue_error, ecg_issue_condition_conflict, ecg_condition_norm, NO_CONDITION, "Dynamic repolarization episodes require an explicit non-normal baseline such as SR.");
                if (episode_condition)
                    add_issue(report, ecg_issue_error, ecg_issue_condition_conflict, NO_CONDITION, ecg_condition_svarr, "Dynamic repolarization episodes do not compose with rhythm episodes in this pack.");
                for (unsigned int index = 0; index < sizeof(non_sinus_rhythms) / sizeof(non_sinus_rhythms[0]); ++index)
                    if (has_condition(scenario.conditions, non_sinus_rhythms[index]))
                        add_issue(report, ecg_issue_error, ecg_issue_condition_conflict, NO_CONDITION, non_sinus_rhythms[index], "Dynamic repolarization episodes require the sinus timeline.");
                for (unsigned int index = 0; index < sizeof(av_blocks) / sizeof(av_blocks[0]); ++index)
                    if (has_condition(scenario.conditions, av_blocks[index]))
                        add_issue(report, ecg_issue_error, ecg_issue_condition_conflict, NO_CONDITION, av_blocks[index], "Dynamic repolarization episodes do not compose with AV block timelines in this pack.");
                for (unsigned int index = 0; index < sizeof(clean_conduction_conditions) / sizeof(clean_conduction_conditions[0]); ++index)
                    if (has_condition(scenario.conditions, clean_conduction_conditions[index]))
                        add_issue(report, ecg_issue_error, ecg_issue_condition_conflict, NO_CONDITION, clean_conduction_conditions[index], "Dynamic repolarization episodes do not compose with conduction morphology in this pack.");
                if (ectopic_origin || pattern)
                    add_issue(report, ecg_issue_error, ecg_issue_condition_conflict, NO_CONDITION, ecg_condition_prc, "Dynamic repolarization episodes do not compose with periodic ectopy in this pack.");
                if (scenario.repolarization_episodes.size() > clinical_repolarization_episode_max)
                    add_issue(report, ecg_issue_error, ecg_issue_invalid_parameter, NO_CONDITION, NO_CONDITION, "Too many dynamic repolarization episodes.");
                for (std::size_t left = 0; left < scenario.repolarization_episodes.size(); ++left)
                {
                    const ecg_repolarization_episode& episode = scenario.repolarization_episodes[left];
                    if (!valid_condition(episode.condition) || !(is_new_repolarization_condition(episode.condition) || episode.condition == ecg_condition_lngqt) || !std::isfinite(episode.start_seconds) || !std::isfinite(episode.duration_seconds) || !std::isfinite(episode.transition_seconds) || !std::isfinite(episode.peak_severity) || episode.start_seconds < 0.0 || episode.duration_seconds <= 0.0 || episode.transition_seconds < 0.0 || episode.transition_seconds > 0.5 * episode.duration_seconds || episode.peak_severity <= 0.0 || episode.peak_severity > 1.0)
                        add_issue(report, ecg_issue_error, ecg_issue_invalid_parameter, NO_CONDITION, NO_CONDITION, "Invalid dynamic repolarization episode.");
                    for (std::size_t right = left + 1; right < scenario.repolarization_episodes.size(); ++right)
                    {
                        const double left_end = scenario.repolarization_episodes[left].start_seconds + scenario.repolarization_episodes[left].duration_seconds;
                        const double right_end = scenario.repolarization_episodes[right].start_seconds + scenario.repolarization_episodes[right].duration_seconds;
                        if (scenario.repolarization_episodes[left].start_seconds < right_end && scenario.repolarization_episodes[right].start_seconds < left_end)
                            add_issue(report, ecg_issue_error, ecg_issue_condition_conflict, NO_CONDITION, NO_CONDITION, "Dynamic repolarization episodes must not overlap.");
                    }
                }
            }

            add_conflict(report, scenario.conditions, ecg_condition_lvolt, ecg_condition_hvolt, "Low and high QRS voltage conditions are mutually exclusive.");
            add_conflict(report, scenario.conditions, ecg_condition_qwave, ecg_condition_lvolt, "The first Q-wave phenotype does not compose with low voltage.");
            add_conflict(report, scenario.conditions, ecg_condition_qwave, ecg_condition_hvolt, "The first Q-wave phenotype does not compose with high voltage.");
            const ecg_condition_code hypertrophy_conditions[] = {ecg_condition_lvh, ecg_condition_rvh, ecg_condition_sehyp, ecg_condition_vclvh, ecg_condition_lao_lae, ecg_condition_rao_rae};
            for (unsigned int left = 0; left < sizeof(hypertrophy_conditions) / sizeof(hypertrophy_conditions[0]); ++left)
            {
                for (unsigned int right = left + 1; right < sizeof(hypertrophy_conditions) / sizeof(hypertrophy_conditions[0]); ++right)
                    add_conflict(report, scenario.conditions, hypertrophy_conditions[left], hypertrophy_conditions[right], "The first hypertrophy QA pack supports one hypertrophy phenotype per scenario.");
                add_conflict(report, scenario.conditions, hypertrophy_conditions[left], ecg_condition_qwave, "The first hypertrophy QA pack does not compose with Q-wave morphology.");
                add_conflict(report, scenario.conditions, hypertrophy_conditions[left], ecg_condition_lvolt, "The first hypertrophy QA pack does not compose with low QRS voltage.");
                add_conflict(report, scenario.conditions, hypertrophy_conditions[left], ecg_condition_hvolt, "The first hypertrophy QA pack does not compose with generic high QRS voltage.");
            }

            for (unsigned int left = 0; left < sizeof(infarction_injury_conditions) / sizeof(infarction_injury_conditions[0]); ++left)
            {
                for (unsigned int right = left + 1; right < sizeof(infarction_injury_conditions) / sizeof(infarction_injury_conditions[0]); ++right)
                    add_conflict(report, scenario.conditions, infarction_injury_conditions[left], infarction_injury_conditions[right], "The first infarction/injury QA pack supports one territorial phenotype per scenario.");
                add_conflict(report, scenario.conditions, infarction_injury_conditions[left], ecg_condition_qwave, "The infarction/injury QA pack owns its territorial Q/ST phenotype.");
                add_conflict(report, scenario.conditions, infarction_injury_conditions[left], ecg_condition_lvolt, "The infarction/injury QA pack does not compose with low QRS voltage.");
                add_conflict(report, scenario.conditions, infarction_injury_conditions[left], ecg_condition_hvolt, "The infarction/injury QA pack does not compose with generic high QRS voltage.");
                for (unsigned int hypertrophy = 0; hypertrophy < sizeof(hypertrophy_conditions) / sizeof(hypertrophy_conditions[0]); ++hypertrophy)
                    add_conflict(report, scenario.conditions, infarction_injury_conditions[left], hypertrophy_conditions[hypertrophy], "The first infarction/injury QA pack does not compose with a hypertrophy phenotype.");
            }

            for (unsigned int left = 0; left < sizeof(repolarization_conditions) / sizeof(repolarization_conditions[0]); ++left)
            {
                for (unsigned int right = left + 1; right < sizeof(repolarization_conditions) / sizeof(repolarization_conditions[0]); ++right)
                    add_conflict(report, scenario.conditions, repolarization_conditions[left], repolarization_conditions[right], "The first ST-T/repolarization QA pack supports one family phenotype per scenario.");
                add_conflict(report, scenario.conditions, repolarization_conditions[left], ecg_condition_qwave, "The ST-T/repolarization QA pack does not compose with explicit Q-wave morphology.");
                add_conflict(report, scenario.conditions, repolarization_conditions[left], ecg_condition_lvolt, "The ST-T/repolarization QA pack does not compose with low QRS voltage.");
                add_conflict(report, scenario.conditions, repolarization_conditions[left], ecg_condition_hvolt, "The ST-T/repolarization QA pack does not compose with high QRS voltage.");
                for (unsigned int hypertrophy = 0; hypertrophy < sizeof(hypertrophy_conditions) / sizeof(hypertrophy_conditions[0]); ++hypertrophy)
                    add_conflict(report, scenario.conditions, repolarization_conditions[left], hypertrophy_conditions[hypertrophy], "The first ST-T/repolarization QA pack does not compose with a hypertrophy phenotype.");
                for (unsigned int infarction = 0; infarction < sizeof(infarction_injury_conditions) / sizeof(infarction_injury_conditions[0]); ++infarction)
                    add_conflict(report, scenario.conditions, repolarization_conditions[left], infarction_injury_conditions[infarction], "The first ST-T/repolarization QA pack does not compose with an infarction/injury phenotype.");
            }

            const ecg_condition_code morphology_conditions[] = {ecg_condition_qwave, ecg_condition_lvolt, ecg_condition_hvolt, ecg_condition_lvh, ecg_condition_rvh, ecg_condition_sehyp, ecg_condition_vclvh, ecg_condition_lao_lae, ecg_condition_rao_rae, ecg_condition_imi, ecg_condition_asmi, ecg_condition_ilmi, ecg_condition_ami, ecg_condition_almi, ecg_condition_injas, ecg_condition_lmi, ecg_condition_injal, ecg_condition_iplmi, ecg_condition_ipmi, ecg_condition_injin, ecg_condition_injla, ecg_condition_pmi, ecg_condition_injil, ecg_condition_ndt, ecg_condition_nst, ecg_condition_dig, ecg_condition_lngqt, ecg_condition_isc, ecg_condition_iscal, ecg_condition_iscin, ecg_condition_iscil, ecg_condition_iscas, ecg_condition_iscla, ecg_condition_aneur, ecg_condition_el, ecg_condition_iscan, ecg_condition_std, ecg_condition_lowt, ecg_condition_nt, ecg_condition_invt, ecg_condition_tab, ecg_condition_ste};
            const ecg_condition_code morphology_conflicts[] = {ecg_condition_afib, ecg_condition_aflt, ecg_condition_svtac, ecg_condition_pace, ecg_condition_svarr, ecg_condition_psvt, ecg_condition_pac, ecg_condition_pvc, ecg_condition_lafb, ecg_condition_irbbb, ecg_condition_ivcd, ecg_condition_crbbb, ecg_condition_clbbb, ecg_condition_lpfb, ecg_condition_wpw, ecg_condition_ilbbb};
            for (unsigned int morphology = 0; morphology < sizeof(morphology_conditions) / sizeof(morphology_conditions[0]); ++morphology)
                for (unsigned int conflict = 0; conflict < sizeof(morphology_conflicts) / sizeof(morphology_conflicts[0]); ++conflict)
                    add_conflict(report, scenario.conditions, morphology_conditions[morphology], morphology_conflicts[conflict], "The first clean morphology phenotype does not compose with this rhythm or conduction condition.");

            if (scenario.heart_rate_bpm > 0.0)
            {
                if (episode_condition && scenario.heart_rate_bpm > 100.0)
                    add_issue(report, ecg_issue_error, ecg_issue_invalid_parameter, NO_CONDITION, NO_CONDITION, "Rhythm episodes use heart_rate_bpm for the sinus baseline; set each episode rate separately.");
                if (has_condition(scenario.conditions, ecg_condition_stach) && scenario.heart_rate_bpm <= 100.0)
                    add_issue(report, ecg_issue_error, ecg_issue_invalid_parameter, ecg_condition_stach, NO_CONDITION, "STACH requires heart_rate_bpm above 100.");
                if (has_condition(scenario.conditions, ecg_condition_sbrad) && scenario.heart_rate_bpm >= 60.0)
                    add_issue(report, ecg_issue_error, ecg_issue_invalid_parameter, ecg_condition_sbrad, NO_CONDITION, "SBRAD requires heart_rate_bpm below 60.");
                if (has_condition(scenario.conditions, ecg_condition_svtac) && scenario.heart_rate_bpm <= 100.0)
                    add_issue(report, ecg_issue_error, ecg_issue_invalid_parameter, ecg_condition_svtac, NO_CONDITION, "SVTAC requires heart_rate_bpm above 100.");
                const bool non_sinus = has_condition(scenario.conditions, ecg_condition_afib) || has_condition(scenario.conditions, ecg_condition_aflt) || has_condition(scenario.conditions, ecg_condition_svtac) || has_condition(scenario.conditions, ecg_condition_pace);
                if (!non_sinus && scenario.heart_rate_bpm > 100.0 && !has_condition(scenario.conditions, ecg_condition_stach))
                    add_issue(report, ecg_issue_error, ecg_issue_missing_requirement, NO_CONDITION, ecg_condition_stach, "Sinus heart rates above 100 require the STACH condition.");
                if (!non_sinus && scenario.heart_rate_bpm < 60.0 && !has_condition(scenario.conditions, ecg_condition_sbrad))
                    add_issue(report, ecg_issue_error, ecg_issue_missing_requirement, NO_CONDITION, ecg_condition_sbrad, "Sinus heart rates below 60 require the SBRAD condition.");
            }
            const bool av_block = has_condition(scenario.conditions, ecg_condition_1avb) || has_condition(scenario.conditions, ecg_condition_2avb) || has_condition(scenario.conditions, ecg_condition_3avb);
            if (scenario.rr_variability_seconds > 0.0 && (av_block || has_condition(scenario.conditions, ecg_condition_aflt)))
                add_issue(report, ecg_issue_error, ecg_issue_invalid_parameter, NO_CONDITION, NO_CONDITION, "The selected timeline does not currently apply rr_variability_seconds.");

            const unsigned int requested_cadence = scenario.ectopic_every_n_beats;
            if (has_condition(scenario.conditions, ecg_condition_bigu) && requested_cadence != 0 && requested_cadence != 2)
                add_issue(report, ecg_issue_error, ecg_issue_invalid_parameter, ecg_condition_bigu, NO_CONDITION, "BIGU requires ectopic_every_n_beats equal to 2.");
            if (has_condition(scenario.conditions, ecg_condition_trigu) && requested_cadence != 0 && requested_cadence != 3)
                add_issue(report, ecg_issue_error, ecg_issue_invalid_parameter, ecg_condition_trigu, NO_CONDITION, "TRIGU requires ectopic_every_n_beats equal to 3.");
        }

        void q_source_orientation(ecg_condition_code code, double& axis, double& elevation)
        {
            if (code == ecg_condition_imi || code == ecg_condition_ipmi)
            {
                axis = 60.0;
                elevation = 0.0;
            }
            else if (code == ecg_condition_asmi)
            {
                axis = 180.0;
                elevation = -70.0;
            }
            else if (code == ecg_condition_ami)
            {
                axis = 180.0;
                elevation = -45.0;
            }
            else if (code == ecg_condition_almi)
            {
                axis = 0.0;
                elevation = -35.0;
            }
            else if (code == ecg_condition_lmi)
            {
                axis = 0.0;
                elevation = -10.0;
            }
            else
            {
                axis = 45.0;
                elevation = -15.0;
            }
        }

        void injury_source_orientation(ecg_condition_code code, double& axis, double& elevation)
        {
            if (code == ecg_condition_injin)
            {
                axis = 60.0;
                elevation = 0.0;
            }
            else if (code == ecg_condition_injas)
            {
                axis = 180.0;
                elevation = -55.0;
            }
            else if (code == ecg_condition_injal)
            {
                axis = 0.0;
                elevation = -35.0;
            }
            else if (code == ecg_condition_injla)
            {
                axis = 0.0;
                elevation = -10.0;
            }
            else
            {
                axis = 45.0;
                elevation = -15.0;
            }
        }

        void compile_infarction_q(ecg_condition_code code, double severity, clinical_ecg_config& config)
        {
            double axis = 0.0;
            double elevation = 0.0;
            q_source_orientation(code, axis, elevation);
            config.morphology.q_amplitude_mv = -(0.32 + 0.38 * severity);
            config.timing.qrs_q_fraction = 0.24 + 0.06 * severity;
            config.timing.qrs_r_fraction = 0.62;
            config.timing.qrs_s_fraction = 0.82;
            config.sources.gain[clinical_source_septal] *= 1.2 + 0.5 * severity;
            config.sources.axis_offset_degrees[clinical_source_septal] = axis - config.morphology.qrs_axis_degrees;
            config.sources.elevation_offset_degrees[clinical_source_septal] = elevation - config.morphology.qrs_elevation_degrees;
        }

        void compile_posterior_proxy(double severity, clinical_ecg_config& config)
        {
            config.sources.gain[clinical_source_ventricular] *= 1.6 + 1.2 * severity;
            config.sources.gain[clinical_source_terminal] *= 1.0 + 0.3 * severity;
            config.sources.axis_offset_degrees[clinical_source_ventricular] = 190.0 - config.morphology.qrs_axis_degrees;
            config.sources.elevation_offset_degrees[clinical_source_ventricular] = -60.0 - config.morphology.qrs_elevation_degrees;
            config.sources.axis_offset_degrees[clinical_source_terminal] = 190.0 - config.morphology.qrs_axis_degrees;
            config.sources.elevation_offset_degrees[clinical_source_terminal] = -60.0 - config.morphology.qrs_elevation_degrees;
        }

        void compile_injury(ecg_condition_code code, double severity, clinical_ecg_config& config)
        {
            double axis = 0.0;
            double elevation = 0.0;
            injury_source_orientation(code, axis, elevation);
            config.morphology.st_j_amplitude_mv = -(0.08 + 0.18 * severity);
            config.sources.axis_offset_degrees[clinical_source_injury] = axis - config.morphology.t_axis_degrees;
            config.sources.elevation_offset_degrees[clinical_source_injury] = elevation - config.morphology.t_elevation_degrees;
        }

        void repolarization_source_orientation(ecg_condition_code code, double& axis, double& elevation)
        {
            if (code == ecg_condition_iscin)
            {
                axis = 60.0;
                elevation = 0.0;
            }
            else if (code == ecg_condition_iscas)
            {
                axis = 180.0;
                elevation = -55.0;
            }
            else if (code == ecg_condition_iscan)
            {
                axis = 180.0;
                elevation = -45.0;
            }
            else if (code == ecg_condition_iscal)
            {
                axis = 0.0;
                elevation = -35.0;
            }
            else if (code == ecg_condition_iscla)
            {
                axis = 0.0;
                elevation = -10.0;
            }
            else
            {
                axis = 45.0;
                elevation = -15.0;
            }
        }

        void orient_repolarization_source(clinical_ecg_source source, double axis, double elevation, clinical_ecg_config& config)
        {
            config.sources.axis_offset_degrees[source] = axis - config.morphology.t_axis_degrees;
            config.sources.elevation_offset_degrees[source] = elevation - config.morphology.t_elevation_degrees;
        }

        void compile_territorial_ischemia(ecg_condition_code code, double severity, clinical_ecg_config& config)
        {
            double axis = 0.0;
            double elevation = 0.0;
            repolarization_source_orientation(code, axis, elevation);
            config.morphology.st_j_amplitude_mv = -(0.08 + 0.16 * severity);
            config.morphology.t_amplitude_mv = -(0.10 + 0.14 * severity);
            orient_repolarization_source(clinical_source_injury, axis, elevation, config);
            orient_repolarization_source(clinical_source_repolarization, axis, elevation, config);
        }

        void compile_repolarization_condition(ecg_condition_code code, double severity, clinical_ecg_config& config)
        {
            if (is_territorial_ischemia(code))
            {
                compile_territorial_ischemia(code, severity, config);
                return;
            }
            if (code == ecg_condition_nst)
            {
                config.morphology.st_j_amplitude_mv = -(0.025 + 0.075 * severity);
                config.morphology.st_slope_mv_per_second = -(0.05 + 0.15 * severity);
                orient_repolarization_source(clinical_source_injury, 45.0, -15.0, config);
            }
            else if (code == ecg_condition_dig)
            {
                config.morphology.st_j_amplitude_mv = -(0.08 + 0.12 * severity);
                config.morphology.st_slope_mv_per_second = -(0.25 + 0.35 * severity);
                config.morphology.t_amplitude_mv = 0.18 - 0.04 * severity;
                orient_repolarization_source(clinical_source_injury, 45.0, -15.0, config);
            }
            else if (code == ecg_condition_std)
            {
                config.morphology.st_j_amplitude_mv = -(0.06 + 0.14 * severity);
                orient_repolarization_source(clinical_source_injury, 45.0, -15.0, config);
            }
            else if (code == ecg_condition_ste)
            {
                config.morphology.st_j_amplitude_mv = 0.06 + 0.14 * severity;
                orient_repolarization_source(clinical_source_injury, 45.0, -15.0, config);
            }
            else if (code == ecg_condition_lowt)
                config.morphology.t_amplitude_mv = 0.08 - 0.04 * severity;
            else if (code == ecg_condition_invt)
                config.morphology.t_amplitude_mv = -(0.12 + 0.18 * severity);
            else if (code == ecg_condition_ndt)
            {
                config.morphology.t_amplitude_mv = 0.20 - 0.06 * severity;
                orient_repolarization_source(clinical_source_repolarization, 65.0 + 25.0 * severity, 10.0, config);
            }
            else if (code == ecg_condition_nt)
            {
                config.morphology.t_amplitude_mv = 0.20 - 0.08 * severity;
                orient_repolarization_source(clinical_source_repolarization, 100.0 + 30.0 * severity, -10.0, config);
            }
            else if (code == ecg_condition_tab)
            {
                config.morphology.t_amplitude_mv = 0.18;
                orient_repolarization_source(clinical_source_repolarization, 150.0 + 30.0 * severity, -35.0, config);
            }
            else if (code == ecg_condition_aneur)
            {
                compile_infarction_q(ecg_condition_ami, severity, config);
                config.morphology.st_j_amplitude_mv = 0.10 + 0.18 * severity;
                orient_repolarization_source(clinical_source_injury, 180.0, -55.0, config);
            }
            else if (code == ecg_condition_el)
            {
                config.morphology.t_amplitude_mv = 0.35 + 0.35 * severity;
                config.timing.t_duration_ms = 170.0 - 60.0 * severity;
            }
        }

        clinical_qt_correction clinical_qt_model(ecg_qt_adaptation_model model)
        {
            switch (model)
            {
            case ecg_qt_adaptation_bazett: return clinical_qt_bazett;
            case ecg_qt_adaptation_framingham: return clinical_qt_framingham;
            case ecg_qt_adaptation_hodges: return clinical_qt_hodges;
            case ecg_qt_adaptation_fixed: return clinical_qt_fixed;
            case ecg_qt_adaptation_fridericia:
            default:
                return clinical_qt_fridericia;
            }
        }

        void compile_repolarization_episode_target(const ecg_repolarization_episode& episode, const clinical_ecg_config& base, clinical_repolarization_episode_config& output)
        {
            clinical_ecg_config target = base;
            if (episode.condition == ecg_condition_lngqt)
            {
                target.timing.qtc_ms = 440.0 + 80.0 * episode.peak_severity;
                target.timing.qt_correction = clinical_qt_fridericia;
            }
            else
                compile_repolarization_condition(episode.condition, episode.peak_severity, target);
            output.start_seconds = episode.start_seconds;
            output.duration_seconds = episode.duration_seconds;
            output.transition_seconds = episode.transition_seconds;
            output.peak_severity = episode.peak_severity;
            output.target_qtc_ms = target.timing.qtc_ms;
            output.target_qt_interval_ms = target.timing.qt_interval_ms;
            output.target_qt_correction = target.timing.qt_correction;
            output.target_t_duration_ms = target.timing.t_duration_ms;
            output.target_t_amplitude_mv = target.morphology.t_amplitude_mv;
            output.target_st_j_amplitude_mv = target.morphology.st_j_amplitude_mv;
            output.target_st_slope_mv_per_second = target.morphology.st_slope_mv_per_second;
            output.target_repolarization_axis_offset_degrees = target.sources.axis_offset_degrees[clinical_source_repolarization];
            output.target_repolarization_elevation_offset_degrees = target.sources.elevation_offset_degrees[clinical_source_repolarization];
            output.target_injury_axis_offset_degrees = target.sources.axis_offset_degrees[clinical_source_injury];
            output.target_injury_elevation_offset_degrees = target.sources.elevation_offset_degrees[clinical_source_injury];
        }

        void compile_advanced_conduction(ecg_condition_code code, double severity, clinical_ecg_config& config)
        {
            if (code == ecg_condition_lafb)
            {
                config.rhythm.intraventricular_conduction = clinical_iv_left_anterior_fascicular;
                config.timing.qrs_duration_ms = 92.0 + 18.0 * severity;
                config.sources.gain[clinical_source_ventricular] *= 0.95 + 0.10 * severity;
                config.sources.gain[clinical_source_terminal] *= 0.90 + 0.10 * severity;
            }
            else if (code == ecg_condition_lpfb)
            {
                config.rhythm.intraventricular_conduction = clinical_iv_left_posterior_fascicular;
                config.timing.qrs_duration_ms = 92.0 + 18.0 * severity;
                config.sources.gain[clinical_source_ventricular] *= 0.95 + 0.10 * severity;
                config.sources.gain[clinical_source_terminal] *= 0.90 + 0.10 * severity;
            }
            else if (code == ecg_condition_irbbb)
            {
                config.rhythm.intraventricular_conduction = clinical_iv_incomplete_rbbb;
                config.timing.qrs_duration_ms = 100.0 + 18.0 * severity;
            }
            else if (code == ecg_condition_ilbbb)
            {
                config.rhythm.intraventricular_conduction = clinical_iv_incomplete_lbbb;
                config.timing.qrs_duration_ms = 110.0 + 8.0 * severity;
            }
            else if (code == ecg_condition_ivcd)
            {
                config.rhythm.intraventricular_conduction = clinical_iv_nonspecific_delay;
                config.timing.qrs_duration_ms = 120.0 + 20.0 * severity;
                config.timing.qrs_r_fraction = 0.48;
                config.timing.qrs_s_fraction = 0.78;
            }
            else if (code == ecg_condition_wpw)
            {
                config.rhythm.preexcitation = clinical_preexcitation_wpw;
                config.timing.p_duration_ms = 75.0;
                config.timing.pr_interval_ms = 112.0 - 27.0 * severity;
                config.timing.qrs_duration_ms = 118.0 + 18.0 * severity;
                config.timing.qrs_q_fraction = 0.32;
                config.timing.qrs_r_fraction = 0.58;
                config.timing.qrs_s_fraction = 0.82;
                config.morphology.q_amplitude_mv = 0.0;
                config.morphology.t_amplitude_mv = -(0.12 + 0.08 * severity);
            }
        }

        void apply_morphology_controls(const ecg_qa_scenario::implementation& scenario, clinical_ecg_config& config);

        void compile_conditions(const ecg_qa_scenario::implementation& scenario, clinical_ecg_config& config)
        {
            config.sampling_rate_hz = scenario.sampling_rate_hz;
            config.rhythm.seed = scenario.seed;
            config.rhythm.rr_variability_seconds = scenario.rr_variability_seconds;
            config.rhythm.hrv_modulation_enabled = scenario.hrv_modulation_enabled;
            config.rhythm.hrv_lf_hf_ratio = scenario.hrv_lf_hf_ratio;
            config.rhythm.hrv_lf_center_hz = scenario.hrv_lf_center_hz;
            config.rhythm.hrv_lf_bandwidth_hz = scenario.hrv_lf_bandwidth_hz;
            config.rhythm.hrv_hf_center_hz = scenario.hrv_hf_center_hz;
            config.rhythm.hrv_hf_bandwidth_hz = scenario.hrv_hf_bandwidth_hz;
            config.rhythm.hrv_respiratory_frequency_hz = scenario.hrv_respiratory_frequency_hz;
            config.rhythm.hrv_respiratory_amplitude_seconds = scenario.hrv_respiratory_amplitude_seconds;
            config.rhythm.hrv_respiratory_phase_radians = scenario.hrv_respiratory_phase_radians;
            config.rhythm.activity_start_seconds = scenario.activity_start_seconds;
            config.rhythm.activity_duration_seconds = scenario.activity_duration_seconds;
            config.rhythm.activity_intensity = scenario.activity_intensity;
            config.retain_source_channels = scenario.retain_source_channels;
            if (scenario.minimum_rr_seconds > 0.0)
                config.rhythm.minimum_rr_seconds = scenario.minimum_rr_seconds;
            if (scenario.maximum_rr_seconds > 0.0)
                config.rhythm.maximum_rr_seconds = scenario.maximum_rr_seconds;
            if (scenario.heart_rate_bpm > 0.0)
                config.rhythm.heart_rate_bpm = scenario.heart_rate_bpm;
            apply_morphology_controls(scenario, config);
            config.morphology.component_count = static_cast<unsigned int>(scenario.morphology_components.size());
            for (unsigned int index = 0; index < config.morphology.component_count; ++index)
            {
                const ecg_morphology_component& input = scenario.morphology_components[index];
                clinical_morphology_component_config& output = config.morphology.components[index];
                output.kind = clinical_component_type(input.type);
                output.lead_mask = input.lead_mask;
                output.amplitude_mv = input.amplitude_mv;
                output.offset_ms = input.offset_ms;
                output.duration_ms = input.duration_ms;
            }
            config.scenario.fusion_every_n_beats = scenario.fusion_every_n_beats;
            config.morphology.fusion_ventricular_fraction = scenario.fusion_ventricular_fraction;
            if (scenario.qt_adaptation_enabled)
            {
                config.timing.qtc_ms = scenario.qt_adaptation_qtc_ms;
                config.timing.qt_correction = clinical_qt_model(scenario.qt_adaptation_model);
                if (scenario.qt_adaptation_model == ecg_qt_adaptation_fixed)
                    config.timing.qt_interval_ms = scenario.qt_adaptation_qtc_ms;
            }
            config.scenario.rhythm_episode_count = static_cast<unsigned int>(scenario.rhythm_episodes.size());
            for (unsigned int index = 0; index < config.scenario.rhythm_episode_count; ++index)
            {
                const ecg_rhythm_episode& input = scenario.rhythm_episodes[index];
                clinical_rhythm_episode_config& output = config.scenario.rhythm_episodes[index];
                output.kind = clinical_episode_type(input.type);
                output.start_seconds = input.start_seconds;
                output.duration_seconds = input.duration_seconds;
                output.transition_seconds = input.transition_seconds;
                output.rate_bpm = input.rate_bpm;
                output.seed = input.seed ? input.seed : scenario.seed ^ (0x455049534f444500ULL + index);
            }

            if (has_condition(scenario.conditions, ecg_condition_afib))
                config.rhythm.rhythm = clinical_rhythm_atrial_fibrillation;
            else if (has_condition(scenario.conditions, ecg_condition_aflt))
            {
                config.rhythm.rhythm = clinical_rhythm_atrial_flutter;
                config.rhythm.flutter_conduction_pattern = scenario.flutter_conduction_pattern == ecg_flutter_alternate_2_3 ? clinical_flutter_alternate_2_3 : scenario.flutter_conduction_pattern == ecg_flutter_cycle_2_3_4 ? clinical_flutter_cycle_2_3_4 : clinical_flutter_fixed;
                if (scenario.heart_rate_bpm > 0.0)
                {
                    const unsigned int ratio = static_cast<unsigned int>(std::max(1.0, std::min(6.0, std::round(300.0 / scenario.heart_rate_bpm))));
                    config.rhythm.flutter_conduction_ratio = ratio;
                    config.rhythm.atrial_rate_bpm = scenario.heart_rate_bpm * ratio;
                }
                else
                {
                    config.rhythm.atrial_rate_bpm = 300.0;
                    config.rhythm.flutter_conduction_ratio = 2;
                }
            }
            else if (has_condition(scenario.conditions, ecg_condition_svtac))
            {
                config.rhythm.rhythm = clinical_rhythm_supraventricular_tachycardia;
                if (scenario.heart_rate_bpm == 0.0)
                    config.rhythm.heart_rate_bpm = 160.0;
            }
            else if (has_condition(scenario.conditions, ecg_condition_pace))
            {
                config.rhythm.rhythm = clinical_rhythm_paced;
                config.rhythm.pacing_mode = scenario.pacing_mode == ecg_pacing_atrial ? clinical_pacing_atrial : scenario.pacing_mode == ecg_pacing_dual_chamber ? clinical_pacing_dual_chamber : clinical_pacing_ventricular;
                config.scenario.pacing_non_capture_every_n_beats = scenario.pacing_non_capture_every_n_beats;
            }
            else
                config.rhythm.rhythm = clinical_rhythm_sinus;

            if (has_condition(scenario.conditions, ecg_condition_stach) && scenario.heart_rate_bpm == 0.0)
                config.rhythm.heart_rate_bpm = 100.0 + 60.0 * condition_severity(scenario.conditions, ecg_condition_stach);
            if (has_condition(scenario.conditions, ecg_condition_sbrad) && scenario.heart_rate_bpm == 0.0)
                config.rhythm.heart_rate_bpm = 60.0 - 30.0 * condition_severity(scenario.conditions, ecg_condition_sbrad);
            if (has_condition(scenario.conditions, ecg_condition_sarrh))
                config.rhythm.rr_variability_seconds = std::max(config.rhythm.rr_variability_seconds, 0.020 + 0.100 * condition_severity(scenario.conditions, ecg_condition_sarrh));

            if (has_condition(scenario.conditions, ecg_condition_3avb))
                config.rhythm.av_conduction = clinical_av_complete_block;
            else if (has_condition(scenario.conditions, ecg_condition_2avb))
                config.rhythm.av_conduction = scenario.second_degree_pattern == ecg_second_degree_mobitz_i ? clinical_av_mobitz_i : clinical_av_mobitz_ii;
            else if (has_condition(scenario.conditions, ecg_condition_1avb) || has_condition(scenario.conditions, ecg_condition_lpr))
            {
                double severity = has_condition(scenario.conditions, ecg_condition_1avb) ? condition_severity(scenario.conditions, ecg_condition_1avb) : 0.0;
                if (has_condition(scenario.conditions, ecg_condition_lpr))
                    severity = std::max(severity, condition_severity(scenario.conditions, ecg_condition_lpr));
                config.rhythm.av_conduction = clinical_av_first_degree;
                config.rhythm.first_degree_pr_ms = 200.0 + 80.0 * severity;
            }
            if (has_condition(scenario.conditions, ecg_condition_lpr) && has_condition(scenario.conditions, ecg_condition_2avb))
                config.timing.pr_interval_ms = 200.0 + 80.0 * condition_severity(scenario.conditions, ecg_condition_lpr);
            if (config.rhythm.av_conduction != clinical_av_normal)
                config.rhythm.atrial_rate_bpm = config.rhythm.heart_rate_bpm;

            if (has_condition(scenario.conditions, ecg_condition_clbbb))
                config.rhythm.intraventricular_conduction = clinical_iv_lbbb;
            if (has_condition(scenario.conditions, ecg_condition_crbbb))
                config.rhythm.intraventricular_conduction = clinical_iv_rbbb;
            for (const scenario_condition& condition : scenario.conditions)
                if (is_clean_conduction_morphology(condition.code))
                    compile_advanced_conduction(condition.code, condition.severity, config);

            if (has_condition(scenario.conditions, ecg_condition_lngqt))
                config.timing.qtc_ms = 440.0 + 80.0 * condition_severity(scenario.conditions, ecg_condition_lngqt);

            if (has_condition(scenario.conditions, ecg_condition_lvolt))
            {
                const double gain = 0.45 - 0.25 * condition_severity(scenario.conditions, ecg_condition_lvolt);
                config.sources.gain[clinical_source_septal] *= gain;
                config.sources.gain[clinical_source_ventricular] *= gain;
                config.sources.gain[clinical_source_terminal] *= gain;
            }
            if (has_condition(scenario.conditions, ecg_condition_hvolt))
            {
                const double gain = 1.75 + 1.05 * condition_severity(scenario.conditions, ecg_condition_hvolt);
                config.sources.gain[clinical_source_septal] *= gain;
                config.sources.gain[clinical_source_ventricular] *= gain;
                config.sources.gain[clinical_source_terminal] *= gain;
            }
            if (has_condition(scenario.conditions, ecg_condition_lvh))
            {
                const double severity = condition_severity(scenario.conditions, ecg_condition_lvh);
                config.sources.gain[clinical_source_ventricular] *= 1.8 + 1.2 * severity;
                config.sources.gain[clinical_source_terminal] *= 1.2 + 0.4 * severity;
                config.sources.axis_offset_degrees[clinical_source_ventricular] -= 40.0;
                config.sources.elevation_offset_degrees[clinical_source_ventricular] -= 25.0;
                config.sources.axis_offset_degrees[clinical_source_terminal] -= 40.0;
                config.sources.elevation_offset_degrees[clinical_source_terminal] -= 25.0;
            }
            if (has_condition(scenario.conditions, ecg_condition_vclvh))
            {
                const double gain = 1.65 + condition_severity(scenario.conditions, ecg_condition_vclvh);
                config.sources.gain[clinical_source_septal] *= gain;
                config.sources.gain[clinical_source_ventricular] *= gain;
                config.sources.gain[clinical_source_terminal] *= gain;
            }
            if (has_condition(scenario.conditions, ecg_condition_rvh))
            {
                const double severity = condition_severity(scenario.conditions, ecg_condition_rvh);
                config.sources.gain[clinical_source_ventricular] *= 1.8 + 1.2 * severity;
                config.sources.gain[clinical_source_terminal] *= 1.0 + 0.3 * severity;
                config.sources.axis_offset_degrees[clinical_source_ventricular] += 145.0;
                config.sources.elevation_offset_degrees[clinical_source_ventricular] -= 80.0;
                config.sources.axis_offset_degrees[clinical_source_terminal] += 145.0;
                config.sources.elevation_offset_degrees[clinical_source_terminal] -= 80.0;
            }
            if (has_condition(scenario.conditions, ecg_condition_sehyp))
            {
                const double severity = condition_severity(scenario.conditions, ecg_condition_sehyp);
                config.sources.gain[clinical_source_septal] *= 2.0 + 1.5 * severity;
                config.sources.axis_offset_degrees[clinical_source_septal] += 145.0;
                config.sources.elevation_offset_degrees[clinical_source_septal] -= 80.0;
            }
            if (has_condition(scenario.conditions, ecg_condition_lao_lae))
            {
                const double severity = condition_severity(scenario.conditions, ecg_condition_lao_lae);
                config.timing.p_duration_ms = 120.0 + 30.0 * severity;
                config.morphology.p_amplitude_mv = 0.14 + 0.04 * severity;
                config.sources.axis_offset_degrees[clinical_source_atrial] -= 70.0;
                config.sources.elevation_offset_degrees[clinical_source_atrial] -= 10.0;
            }
            if (has_condition(scenario.conditions, ecg_condition_rao_rae))
            {
                const double severity = condition_severity(scenario.conditions, ecg_condition_rao_rae);
                config.morphology.p_amplitude_mv = 0.16 + 0.14 * severity;
                config.sources.axis_offset_degrees[clinical_source_atrial] += 20.0;
                config.sources.elevation_offset_degrees[clinical_source_atrial] -= 5.0;
            }
            for (const scenario_condition& condition : scenario.conditions)
            {
                if (has_infarction_q_wave(condition.code))
                    compile_infarction_q(condition.code, condition.severity, config);
                if (is_posterior_infarction(condition.code))
                    compile_posterior_proxy(condition.severity, config);
                if (is_injury_condition(condition.code))
                    compile_injury(condition.code, condition.severity, config);
                if (is_new_repolarization_condition(condition.code))
                    compile_repolarization_condition(condition.code, condition.severity, config);
            }
            if (has_condition(scenario.conditions, ecg_condition_qwave))
            {
                const double severity = condition_severity(scenario.conditions, ecg_condition_qwave);
                config.morphology.q_amplitude_mv = -(0.30 + 0.35 * severity);
                config.timing.qrs_q_fraction = 0.24 + 0.06 * severity;
                config.timing.qrs_r_fraction = 0.62;
                config.timing.qrs_s_fraction = 0.82;
                config.sources.gain[clinical_source_septal] *= 1.0 + 0.5 * severity;
                if (scenario.q_wave_territory == ecg_q_wave_inferior)
                {
                    config.sources.axis_offset_degrees[clinical_source_septal] = 15.0;
                    config.sources.elevation_offset_degrees[clinical_source_septal] = -20.0;
                }
                else if (scenario.q_wave_territory == ecg_q_wave_anterior)
                {
                    config.sources.axis_offset_degrees[clinical_source_septal] = 135.0;
                    config.sources.elevation_offset_degrees[clinical_source_septal] = -75.0;
                }
                else if (scenario.q_wave_territory == ecg_q_wave_lateral)
                {
                    config.sources.axis_offset_degrees[clinical_source_septal] = -45.0;
                    config.sources.elevation_offset_degrees[clinical_source_septal] = -30.0;
                }
            }

            config.scenario.repolarization_episode_count = static_cast<unsigned int>(std::min<std::size_t>(scenario.repolarization_episodes.size(), clinical_repolarization_episode_max));
            for (unsigned int index = 0; index < config.scenario.repolarization_episode_count; ++index)
                compile_repolarization_episode_target(scenario.repolarization_episodes[index], config, config.scenario.repolarization_episodes[index]);

            const bool pac = has_condition(scenario.conditions, ecg_condition_pac);
            const bool pvc = has_condition(scenario.conditions, ecg_condition_pvc);
            if (pac || pvc)
            {
                const double severity = condition_severity(scenario.conditions, pac ? ecg_condition_pac : ecg_condition_pvc);
                config.scenario.premature_origin = pac ? clinical_origin_pac : clinical_origin_pvc;
                config.scenario.premature_coupling_ratio = 0.85 - 0.25 * severity;
                config.scenario.compensatory_pause_ratio = 2.0 - config.scenario.premature_coupling_ratio;
                if (has_condition(scenario.conditions, ecg_condition_bigu))
                    config.scenario.premature_every_n_beats = 2;
                else if (has_condition(scenario.conditions, ecg_condition_trigu))
                    config.scenario.premature_every_n_beats = 3;
                else
                    config.scenario.premature_every_n_beats = scenario.ectopic_every_n_beats ? scenario.ectopic_every_n_beats : 5;
            }
        }

        void apply_morphology_controls(const ecg_qa_scenario::implementation& scenario, clinical_ecg_config& config)
        {
            for (unsigned int index = 0; index < ecg_morphology_control_count; ++index)
            {
                if (!scenario.morphology_enabled[index])
                    continue;
                const double value = scenario.morphology_values[index];
                switch (static_cast<ecg_morphology_control>(index))
                {
                case ecg_morphology_p_amplitude_mv: config.morphology.p_amplitude_mv = value; break;
                case ecg_morphology_q_amplitude_mv: config.morphology.q_amplitude_mv = value; break;
                case ecg_morphology_r_amplitude_mv: config.morphology.r_amplitude_mv = value; break;
                case ecg_morphology_s_amplitude_mv: config.morphology.s_amplitude_mv = value; break;
                case ecg_morphology_t_amplitude_mv: config.morphology.t_amplitude_mv = value; break;
                case ecg_morphology_st_j_amplitude_mv: config.morphology.st_j_amplitude_mv = value; break;
                case ecg_morphology_st_slope_mv_per_second: config.morphology.st_slope_mv_per_second = value; break;
                case ecg_morphology_p_axis_degrees: config.morphology.p_axis_degrees = value; break;
                case ecg_morphology_qrs_axis_degrees: config.morphology.qrs_axis_degrees = value; break;
                case ecg_morphology_t_axis_degrees: config.morphology.t_axis_degrees = value; break;
                case ecg_morphology_p_duration_ms: config.timing.p_duration_ms = value; break;
                case ecg_morphology_qrs_duration_ms: config.timing.qrs_duration_ms = value; break;
                case ecg_morphology_qt_interval_ms: config.timing.qt_interval_ms = value; config.timing.qtc_ms = value; config.timing.qt_correction = clinical_qt_fixed; break;
                case ecg_morphology_t_duration_ms: config.timing.t_duration_ms = value; break;
                case ecg_morphology_control_count: break;
                }
            }
        }

        void add_assertion(ecg_scenario_report::implementation& report, ecg_condition_code condition, ecg_phenotype_assertion_code code, double measured, double minimum, double maximum, const char* name, const char* unit)
        {
            const double tolerance = 1e-9 * std::max(1.0, std::max(std::fabs(minimum), std::fabs(maximum)));
            const bool passed = std::isfinite(measured) && measured + tolerance >= minimum && measured - tolerance <= maximum;
            const ecg_condition_info* info = find_ecg_condition(condition);
            const std::string full_name = std::string(info ? info->scp_code : "") + ": " + name;
            report.assertions.push_back(phenotype_assertion{condition, code, passed ? ecg_assertion_passed : ecg_assertion_failed, measured, minimum, maximum, full_name, unit});
        }

        double mean_heart_rate(const clinical_ecg_record& record)
        {
            const clinical_beat_annotation* beats = record.beats();
            if (!beats || record.beat_count() < 2)
                return -1.0;
            double rr_sum = 0.0;
            for (unsigned int index = 1; index < record.beat_count(); ++index)
                rr_sum += beats[index].r_peak_time_seconds - beats[index - 1].r_peak_time_seconds;
            return 60.0 * (record.beat_count() - 1) / rr_sum;
        }

        double median_atrial_rate(const clinical_ecg_record& record)
        {
            const clinical_atrial_event* events = record.atrial_events();
            if (!events || record.atrial_event_count() < 2)
                return -1.0;
            std::vector<double> intervals;
            intervals.reserve(record.atrial_event_count() - 1);
            for (unsigned int index = 1; index < record.atrial_event_count(); ++index)
                intervals.push_back(events[index].onset_time_seconds - events[index - 1].onset_time_seconds);
            std::sort(intervals.begin(), intervals.end());
            const double median = intervals[intervals.size() / 2];
            return median > 0.0 ? 60.0 / median : -1.0;
        }

        double rr_standard_deviation(const clinical_ecg_record& record)
        {
            const clinical_beat_annotation* beats = record.beats();
            if (!beats || record.beat_count() < 3)
                return -1.0;
            double mean = 0.0;
            for (unsigned int index = 1; index < record.beat_count(); ++index)
                mean += beats[index].r_peak_time_seconds - beats[index - 1].r_peak_time_seconds;
            mean /= record.beat_count() - 1;
            double variance = 0.0;
            for (unsigned int index = 1; index < record.beat_count(); ++index)
            {
                const double difference = beats[index].r_peak_time_seconds - beats[index - 1].r_peak_time_seconds - mean;
                variance += difference * difference;
            }
            return std::sqrt(variance / (record.beat_count() - 1));
        }

        double beat_fraction(const clinical_ecg_record& record, clinical_rhythm rhythm)
        {
            const clinical_beat_annotation* beats = record.beats();
            if (!beats || record.beat_count() == 0)
                return -1.0;
            unsigned int matching = 0;
            for (unsigned int index = 0; index < record.beat_count(); ++index)
                matching += beats[index].rhythm == rhythm ? 1U : 0U;
            return static_cast<double>(matching) / record.beat_count();
        }

        double p_wave_fraction(const clinical_ecg_record& record)
        {
            const clinical_beat_annotation* beats = record.beats();
            if (!beats || record.beat_count() == 0)
                return -1.0;
            unsigned int present = 0;
            for (unsigned int index = 0; index < record.beat_count(); ++index)
                present += beats[index].p_present ? 1U : 0U;
            return static_cast<double>(present) / record.beat_count();
        }

        const clinical_episode_annotation* first_matching_episode(const clinical_ecg_record& record, ecg_condition_code condition)
        {
            const clinical_episode_annotation* episodes = record.episodes();
            for (unsigned int index = 0; episodes && index < record.episode_count(); ++index)
            {
                if (!episodes[index].present)
                    continue;
                if (condition == ecg_condition_svarr || episodes[index].kind == clinical_episode_psvt)
                    return &episodes[index];
            }
            return 0;
        }

        bool beat_in_episode(const clinical_beat_annotation& beat, const clinical_episode_annotation& episode)
        {
            return beat.r_peak_time_seconds >= episode.start_time_seconds && beat.r_peak_time_seconds < episode.end_time_seconds;
        }

        double episode_count_metric(const clinical_ecg_record& record, ecg_condition_code condition)
        {
            return first_matching_episode(record, condition) ? 1.0 : 0.0;
        }

        double rhythm_fraction_inside_episode(const clinical_ecg_record& record, ecg_condition_code condition, clinical_rhythm rhythm)
        {
            const clinical_episode_annotation* episode = first_matching_episode(record, condition);
            const clinical_beat_annotation* beats = record.beats();
            if (!episode || !beats)
                return -1.0;
            unsigned int total = 0;
            unsigned int matching = 0;
            for (unsigned int index = 0; index < record.beat_count(); ++index)
            {
                if (!beat_in_episode(beats[index], *episode))
                    continue;
                ++total;
                matching += beats[index].rhythm == rhythm ? 1U : 0U;
            }
            return total ? static_cast<double>(matching) / total : -1.0;
        }

        double sinus_fraction_outside_episode(const clinical_ecg_record& record, ecg_condition_code condition)
        {
            const clinical_episode_annotation* episode = first_matching_episode(record, condition);
            const clinical_beat_annotation* beats = record.beats();
            if (!episode || !beats)
                return -1.0;
            unsigned int total = 0;
            unsigned int matching = 0;
            for (unsigned int index = 0; index < record.beat_count(); ++index)
            {
                if (beat_in_episode(beats[index], *episode))
                    continue;
                ++total;
                matching += beats[index].rhythm == clinical_rhythm_sinus ? 1U : 0U;
            }
            return total ? static_cast<double>(matching) / total : -1.0;
        }

        double episode_p_wave_fraction(const clinical_ecg_record& record, ecg_condition_code condition)
        {
            const clinical_episode_annotation* episode = first_matching_episode(record, condition);
            const clinical_beat_annotation* beats = record.beats();
            if (!episode || !beats)
                return -1.0;
            unsigned int total = 0;
            unsigned int present = 0;
            for (unsigned int index = 0; index < record.beat_count(); ++index)
            {
                if (!beat_in_episode(beats[index], *episode))
                    continue;
                ++total;
                present += beats[index].p_present ? 1U : 0U;
            }
            return total ? static_cast<double>(present) / total : -1.0;
        }

        double mean_episode_qrs_duration(const clinical_ecg_record& record, ecg_condition_code condition)
        {
            const clinical_episode_annotation* episode = first_matching_episode(record, condition);
            const clinical_beat_annotation* beats = record.beats();
            if (!episode || !beats)
                return -1.0;
            double sum = 0.0;
            unsigned int count = 0;
            for (unsigned int index = 0; index < record.beat_count(); ++index)
            {
                if (beat_in_episode(beats[index], *episode))
                {
                    sum += beats[index].qrs_duration_seconds;
                    ++count;
                }
            }
            return count ? sum / count : -1.0;
        }

        double episode_heart_rate(const clinical_ecg_record& record, ecg_condition_code condition)
        {
            const clinical_episode_annotation* episode = first_matching_episode(record, condition);
            const clinical_beat_annotation* beats = record.beats();
            if (!episode || !beats)
                return -1.0;
            double sum = 0.0;
            unsigned int intervals = 0;
            int previous = -1;
            for (unsigned int index = 0; index < record.beat_count(); ++index)
            {
                if (!beat_in_episode(beats[index], *episode))
                    continue;
                if (previous >= 0)
                {
                    sum += beats[index].r_peak_time_seconds - beats[previous].r_peak_time_seconds;
                    ++intervals;
                }
                previous = static_cast<int>(index);
            }
            return intervals && sum > 0.0 ? 60.0 * intervals / sum : -1.0;
        }

        double mean_annotation_value(const clinical_ecg_record& record, ecg_phenotype_assertion_code code)
        {
            const clinical_beat_annotation* beats = record.beats();
            if (!beats || record.beat_count() == 0)
                return -1.0;
            double sum = 0.0;
            unsigned int count = 0;
            for (unsigned int index = 0; index < record.beat_count(); ++index)
            {
                double value = -1.0;
                if (code == ecg_assert_pr_interval && beats[index].linked_atrial_index >= 0)
                    value = beats[index].pr_interval_seconds;
                else if (code == ecg_assert_qrs_duration)
                    value = beats[index].qrs_duration_seconds;
                else if (code == ecg_assert_qtc_interval)
                    value = beats[index].qtc_interval_seconds;
                if (value >= 0.0)
                {
                    sum += value;
                    ++count;
                }
            }
            return count ? sum / count : -1.0;
        }

        unsigned int origin_count(const clinical_ecg_record& record, clinical_ventricular_origin origin)
        {
            const clinical_beat_annotation* beats = record.beats();
            unsigned int count = 0;
            for (unsigned int index = 0; beats && index < record.beat_count(); ++index)
                count += beats[index].origin == origin ? 1U : 0U;
            return count;
        }

        double ectopic_cadence_fraction(const clinical_ecg_record& record, clinical_ventricular_origin origin, unsigned int cadence)
        {
            const clinical_beat_annotation* beats = record.beats();
            unsigned int ectopic_count = 0;
            unsigned int matching = 0;
            for (unsigned int index = 0; beats && index < record.beat_count(); ++index)
            {
                if (beats[index].origin != origin)
                    continue;
                ++ectopic_count;
                matching += (index + 1) % cadence == 0 ? 1U : 0U;
            }
            return ectopic_count ? static_cast<double>(matching) / ectopic_count : -1.0;
        }

        bool extended_component_peak(clinical_fiducial_kind kind)
        {
            return kind == clinical_p_secondary_peak || kind == clinical_p_notch || kind == clinical_r_prime || kind == clinical_qrs_fragment || kind == clinical_t_secondary_peak || kind == clinical_t_notch || kind == clinical_u_peak;
        }

        double extended_component_measurement_fraction(const clinical_ecg_record& record)
        {
            const clinical_fiducial_annotation* fiducials = record.fiducials();
            unsigned int construction = 0;
            unsigned int measured = 0;
            for (unsigned int index = 0; fiducials && index < record.fiducial_count(); ++index)
            {
                const clinical_fiducial_annotation& item = fiducials[index];
                if (!extended_component_peak(item.kind) || !item.present)
                    continue;
                construction += item.source == clinical_fiducial_construction ? 1U : 0U;
                measured += item.source == clinical_fiducial_lead_measurement ? 1U : 0U;
            }
            return construction ? std::min(1.0, static_cast<double>(measured) / construction) : -1.0;
        }

        double premature_coupling_ratio(const clinical_ecg_record& record, clinical_ventricular_origin origin)
        {
            const clinical_beat_annotation* beats = record.beats();
            double sum = 0.0;
            unsigned int count = 0;
            for (unsigned int index = 1; beats && index < record.beat_count(); ++index)
            {
                if (beats[index].origin == origin && beats[index - 1].rr_interval_seconds > 0.0)
                {
                    sum += beats[index].rr_interval_seconds / beats[index - 1].rr_interval_seconds;
                    ++count;
                }
            }
            return count ? sum / count : -1.0;
        }

        double dropped_atrial_count(const clinical_ecg_record& record)
        {
            const clinical_atrial_event* events = record.atrial_events();
            unsigned int count = 0;
            for (unsigned int index = 0; events && index < record.atrial_event_count(); ++index)
                count += events[index].conducted ? 0U : 1U;
            return count;
        }

        double mobitz_pattern_metric(const clinical_ecg_record& record, ecg_second_degree_av_pattern pattern)
        {
            const clinical_beat_annotation* beats = record.beats();
            if (!beats || record.beat_count() < 2)
                return -1.0;
            if (pattern == ecg_second_degree_mobitz_i)
            {
                unsigned int increases = 0;
                for (unsigned int index = 1; index < record.beat_count(); ++index)
                    increases += beats[index].pr_interval_seconds > beats[index - 1].pr_interval_seconds + 1e-9 ? 1U : 0U;
                return increases;
            }
            double mean = 0.0;
            for (unsigned int index = 0; index < record.beat_count(); ++index)
                mean += beats[index].pr_interval_seconds;
            mean /= record.beat_count();
            double variance = 0.0;
            for (unsigned int index = 0; index < record.beat_count(); ++index)
            {
                const double difference = beats[index].pr_interval_seconds - mean;
                variance += difference * difference;
            }
            return std::sqrt(variance / record.beat_count());
        }

        double terminal_v1_polarity(const clinical_ecg_record& record)
        {
            const clinical_beat_annotation* beats = record.beats();
            const double* v1 = record.lead_data(clinical_lead_v1);
            if (!beats || !v1 || record.beat_count() == 0)
                return 0.0;
            double sum = 0.0;
            unsigned int count = 0;
            for (unsigned int index = 0; index < record.beat_count(); ++index)
            {
                const unsigned long long sample = static_cast<unsigned long long>(std::llround(beats[index].s_peak_time_seconds * record.sampling_rate_hz()));
                if (sample < record.sample_count())
                {
                    sum += v1[sample];
                    ++count;
                }
            }
            return count ? sum / count : 0.0;
        }

        double pacing_evidence_count(const clinical_ecg_record& record)
        {
            unsigned int captured = 0;
            const clinical_pacing_event* events = record.pacing_events();
            for (unsigned int index = 0; events && index < record.pacing_event_count(); ++index)
                captured += events[index].captured && events[index].linked_ventricular_index >= 0 ? 1U : 0U;
            return static_cast<double>(captured);
        }

        enum morphology_value
        {
            morphology_p_amplitude,
            morphology_p_duration,
            morphology_q_amplitude,
            morphology_q_duration,
            morphology_r_amplitude,
            morphology_s_amplitude,
            morphology_qrs_peak_to_peak,
            morphology_st_j,
            morphology_st_j60,
            morphology_t_amplitude,
            morphology_t_duration
        };

        ecg_lead_region q_wave_region(ecg_q_wave_territory territory)
        {
            if (territory == ecg_q_wave_inferior)
                return ecg_region_inferior;
            if (territory == ecg_q_wave_anterior)
                return ecg_region_anterior;
            if (territory == ecg_q_wave_lateral)
                return ecg_region_lateral;
            return ecg_region_all;
        }

        double morphology_entry_value(const ecg_lead_morphology& entry, morphology_value value)
        {
            if (value == morphology_p_amplitude)
                return entry.p_amplitude_mv;
            if (value == morphology_p_duration)
                return entry.p_duration_seconds;
            if (value == morphology_q_amplitude)
                return entry.q_amplitude_mv;
            if (value == morphology_q_duration)
                return entry.q_duration_seconds;
            if (value == morphology_r_amplitude)
                return entry.r_amplitude_mv;
            if (value == morphology_s_amplitude)
                return entry.s_amplitude_mv;
            if (value == morphology_st_j)
                return entry.st_j_mv;
            if (value == morphology_st_j60)
                return entry.st_j60_mv;
            if (value == morphology_t_amplitude)
                return entry.t_amplitude_mv;
            if (value == morphology_t_duration)
                return entry.t_duration_seconds;
            return entry.qrs_peak_to_peak_mv;
        }

        double morphology_region_extreme(const ecg_morphology_report& morphology, ecg_lead_region region, morphology_value value, bool maximum)
        {
            const ecg_lead_morphology* entries = morphology.entries();
            double result = maximum ? -std::numeric_limits<double>::max() : std::numeric_limits<double>::max();
            bool found = false;
            for (unsigned int index = 0; entries && index < morphology.entry_count(); ++index)
            {
                if (!ecg_lead_is_in_region(entries[index].lead_index, region))
                    continue;
                const double measured = morphology_entry_value(entries[index], value);
                result = maximum ? std::max(result, measured) : std::min(result, measured);
                found = true;
            }
            return found ? result : -1.0;
        }

        double morphology_lead_mean(const ecg_morphology_report& morphology, unsigned int lead, morphology_value value)
        {
            const ecg_lead_morphology* entries = morphology.entries();
            double sum = 0.0;
            unsigned int count = 0;
            for (unsigned int index = 0; entries && index < morphology.entry_count(); ++index)
            {
                if (entries[index].lead_index == lead)
                {
                    sum += morphology_entry_value(entries[index], value);
                    ++count;
                }
            }
            return count ? sum / count : -1.0;
        }

        double morphology_mask_extreme(const ecg_morphology_report& morphology, unsigned int leads, morphology_value value, bool maximum)
        {
            const ecg_lead_morphology* entries = morphology.entries();
            double result = maximum ? -std::numeric_limits<double>::max() : std::numeric_limits<double>::max();
            bool found = false;
            for (unsigned int index = 0; entries && index < morphology.entry_count(); ++index)
            {
                if ((leads & lead_bit(entries[index].lead_index)) == 0)
                    continue;
                const double measured = morphology_entry_value(entries[index], value);
                result = maximum ? std::max(result, measured) : std::min(result, measured);
                found = true;
            }
            return found ? result : -1.0;
        }

        double pathological_q_lead_count_mask(const ecg_morphology_report& morphology, unsigned int leads)
        {
            const ecg_lead_morphology* entries = morphology.entries();
            double amplitude_sum[clinical_lead_count] = {};
            double duration_sum[clinical_lead_count] = {};
            unsigned int count[clinical_lead_count] = {};
            for (unsigned int index = 0; entries && index < morphology.entry_count(); ++index)
            {
                const unsigned int lead = entries[index].lead_index;
                if ((leads & lead_bit(lead)) == 0)
                    continue;
                amplitude_sum[lead] += entries[index].q_amplitude_mv;
                duration_sum[lead] += entries[index].q_duration_seconds;
                ++count[lead];
            }
            unsigned int matching = 0;
            for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
                if (count[lead] && amplitude_sum[lead] / count[lead] <= -0.10 && duration_sum[lead] / count[lead] >= 0.020)
                    ++matching;
            return matching;
        }

        double posterior_reciprocal_r_amplitude(const ecg_morphology_report& morphology)
        {
            double maximum = -1.0;
            const unsigned int leads = posterior_reciprocal_leads();
            for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
                if ((leads & lead_bit(lead)) != 0)
                    maximum = std::max(maximum, morphology_lead_mean(morphology, lead, morphology_r_amplitude));
            return maximum;
        }

        double posterior_reciprocal_lead_count(const ecg_morphology_report& morphology)
        {
            unsigned int matching = 0;
            const unsigned int leads = posterior_reciprocal_leads();
            for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
            {
                if ((leads & lead_bit(lead)) == 0)
                    continue;
                const double r = morphology_lead_mean(morphology, lead, morphology_r_amplitude);
                const double s = morphology_lead_mean(morphology, lead, morphology_s_amplitude);
                if (r >= 0.0 && s < 0.0 && r / std::max(std::fabs(s), 1e-12) >= 1.0)
                    ++matching;
            }
            return matching;
        }

        double depressed_st_lead_count(const ecg_morphology_report& morphology, unsigned int leads, double threshold = -0.03)
        {
            unsigned int matching = 0;
            for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
                if ((leads & lead_bit(lead)) != 0 && morphology_lead_mean(morphology, lead, morphology_st_j) <= threshold)
                    ++matching;
            return matching;
        }

        double elevated_st_lead_count(const ecg_morphology_report& morphology, unsigned int leads, double threshold = 0.03)
        {
            unsigned int matching = 0;
            for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
                if ((leads & lead_bit(lead)) != 0 && morphology_lead_mean(morphology, lead, morphology_st_j) >= threshold)
                    ++matching;
            return matching;
        }

        double negative_t_lead_count(const ecg_morphology_report& morphology, unsigned int leads, double threshold = -0.03)
        {
            unsigned int matching = 0;
            for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
                if ((leads & lead_bit(lead)) != 0 && morphology_lead_mean(morphology, lead, morphology_t_amplitude) <= threshold)
                    ++matching;
            return matching;
        }

        double positive_t_lead_count(const ecg_morphology_report& morphology, unsigned int leads, double threshold = 0.03)
        {
            unsigned int matching = 0;
            for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
                if ((leads & lead_bit(lead)) != 0 && morphology_lead_mean(morphology, lead, morphology_t_amplitude) >= threshold)
                    ++matching;
            return matching;
        }

        double maximum_absolute_t_amplitude(const ecg_morphology_report& morphology, unsigned int leads)
        {
            double maximum = -1.0;
            for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
                if ((leads & lead_bit(lead)) != 0)
                    maximum = std::max(maximum, std::fabs(morphology_lead_mean(morphology, lead, morphology_t_amplitude)));
            return maximum;
        }

        double st_slope_extreme(const ecg_morphology_report& morphology, unsigned int leads, bool maximum)
        {
            double result = maximum ? -std::numeric_limits<double>::max() : std::numeric_limits<double>::max();
            bool found = false;
            for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
            {
                if ((leads & lead_bit(lead)) == 0)
                    continue;
                const double slope = morphology_lead_mean(morphology, lead, morphology_st_j60) - morphology_lead_mean(morphology, lead, morphology_st_j);
                result = maximum ? std::max(result, slope) : std::min(result, slope);
                found = true;
            }
            return found ? result : -1.0;
        }

        void q_component_masks(ecg_condition_code code, unsigned int& first, unsigned int& second)
        {
            first = infarction_q_leads(code);
            second = 0;
            if (code == ecg_condition_almi)
            {
                first = anterior_leads();
                second = lateral_leads();
            }
            else if (code == ecg_condition_ilmi || code == ecg_condition_iplmi)
            {
                first = inferior_leads();
                second = lateral_leads();
            }
        }

        void injury_component_masks(ecg_condition_code code, unsigned int& first, unsigned int& second)
        {
            first = injury_leads(code);
            second = 0;
            if (code == ecg_condition_injal)
            {
                first = anterior_leads();
                second = lateral_leads();
            }
            else if (code == ecg_condition_injil)
            {
                first = inferior_leads();
                second = lateral_leads();
            }
        }

        void ischemia_component_masks(ecg_condition_code code, unsigned int& first, unsigned int& second)
        {
            first = ischemia_leads(code);
            second = 0;
            if (code == ecg_condition_iscal)
            {
                first = anterior_leads();
                second = lateral_leads();
            }
            else if (code == ecg_condition_iscil)
            {
                first = inferior_leads();
                second = lateral_leads();
            }
        }

        void add_infarction_assertions(ecg_condition_code code, const ecg_morphology_report& morphology, bool available, ecg_scenario_report::implementation& report)
        {
            const unsigned int leads = infarction_q_leads(code);
            if (leads)
            {
                unsigned int first = 0;
                unsigned int second = 0;
                q_component_masks(code, first, second);
                add_assertion(report, code, ecg_assert_q_wave_amplitude, available ? morphology_mask_extreme(morphology, leads, morphology_q_amplitude, false) : 0.0, -10.0, -0.10, "territorial Q amplitude", "mV");
                add_assertion(report, code, ecg_assert_q_wave_duration, available ? morphology_mask_extreme(morphology, leads, morphology_q_duration, true) : -1.0, 0.030, 0.5, "territorial Q duration", "s");
                add_assertion(report, code, ecg_assert_q_wave_lead_count, available ? pathological_q_lead_count_mask(morphology, first) : 0.0, 2.0, clinical_lead_count, second ? "first territorial Q lead group" : "territorial Q leads", "count");
                if (second)
                    add_assertion(report, code, ecg_assert_q_wave_lead_count, available ? pathological_q_lead_count_mask(morphology, second) : 0.0, 2.0, clinical_lead_count, "second territorial Q lead group", "count");
            }
            if (is_posterior_infarction(code))
            {
                add_assertion(report, code, ecg_assert_posterior_reciprocal_r_amplitude, available ? posterior_reciprocal_r_amplitude(morphology) : -1.0, 1.0, 20.0, "posterior reciprocal R-wave proxy", "mV");
                add_assertion(report, code, ecg_assert_posterior_reciprocal_lead_count, available ? posterior_reciprocal_lead_count(morphology) : 0.0, 2.0, 3.0, "posterior reciprocal R/S proxy leads", "count");
            }
        }

        void add_injury_assertions(ecg_condition_code code, const ecg_morphology_report& morphology, bool available, ecg_scenario_report::implementation& report)
        {
            const unsigned int leads = injury_leads(code);
            unsigned int first = 0;
            unsigned int second = 0;
            injury_component_masks(code, first, second);
            add_assertion(report, code, ecg_assert_injury_st_deviation, available ? morphology_mask_extreme(morphology, leads, morphology_st_j, false) : 0.0, -10.0, -0.05, "territorial ST-J depression", "mV");
            add_assertion(report, code, ecg_assert_injury_st_lead_count, available ? depressed_st_lead_count(morphology, first) : 0.0, 2.0, clinical_lead_count, second ? "first depressed ST lead group" : "depressed ST leads", "count");
            if (second)
                add_assertion(report, code, ecg_assert_injury_st_lead_count, available ? depressed_st_lead_count(morphology, second) : 0.0, 2.0, clinical_lead_count, "second depressed ST lead group", "count");
        }

        void add_ischemia_assertions(ecg_condition_code code, double severity, const ecg_morphology_report& morphology, bool available, ecg_scenario_report::implementation& report)
        {
            const unsigned int leads = ischemia_leads(code);
            const double minimum_count = code == ecg_condition_isc ? 3.0 : 2.0;
            const double st_threshold = -(0.01 + 0.02 * severity);
            const double t_threshold = -(0.01 + 0.02 * severity);
            unsigned int first = 0;
            unsigned int second = 0;
            ischemia_component_masks(code, first, second);
            add_assertion(report, code, ecg_assert_st_deviation, available ? morphology_mask_extreme(morphology, leads, morphology_st_j, false) : 0.0, -10.0, -0.05, "territorial ST-J depression", "mV");
            add_assertion(report, code, ecg_assert_st_lead_count, available ? depressed_st_lead_count(morphology, first, st_threshold) : 0.0, minimum_count, clinical_lead_count, second ? "first depressed ST lead group" : "depressed ST leads", "count");
            if (second)
                add_assertion(report, code, ecg_assert_st_lead_count, available ? depressed_st_lead_count(morphology, second, st_threshold) : 0.0, 2.0, clinical_lead_count, "second depressed ST lead group", "count");
            add_assertion(report, code, ecg_assert_t_amplitude, available ? morphology_mask_extreme(morphology, leads, morphology_t_amplitude, false) : 0.0, -10.0, -0.05, "territorial negative T amplitude", "mV");
            add_assertion(report, code, ecg_assert_t_lead_count, available ? negative_t_lead_count(morphology, first, t_threshold) : 0.0, minimum_count, clinical_lead_count, second ? "first negative T lead group" : "negative T leads", "count");
            if (second)
                add_assertion(report, code, ecg_assert_t_lead_count, available ? negative_t_lead_count(morphology, second, t_threshold) : 0.0, 2.0, clinical_lead_count, "second negative T lead group", "count");
        }

        void add_repolarization_assertions(ecg_condition_code code, double severity, const ecg_morphology_report& morphology, bool available, ecg_scenario_report::implementation& report)
        {
            const unsigned int leads = widespread_leads();
            const double st_threshold = 0.01 + 0.02 * severity;
            const double t_threshold = 0.01 + 0.02 * severity;
            if (code == ecg_condition_nst || code == ecg_condition_dig || code == ecg_condition_std)
            {
                add_assertion(report, code, ecg_assert_st_deviation, available ? morphology_mask_extreme(morphology, leads, morphology_st_j, false) : 0.0, -10.0, code == ecg_condition_nst ? -0.015 : -0.05, "ST-J depression", "mV");
                add_assertion(report, code, ecg_assert_st_lead_count, available ? depressed_st_lead_count(morphology, leads, -st_threshold) : 0.0, code == ecg_condition_nst ? 1.0 : 3.0, clinical_lead_count, "depressed ST leads", "count");
                if (code == ecg_condition_nst || code == ecg_condition_dig)
                    add_assertion(report, code, ecg_assert_st_slope, available ? st_slope_extreme(morphology, leads, false) : 0.0, -10.0, code == ecg_condition_nst ? -(0.002 + 0.003 * severity) : -0.005, "downsloping ST change at J+60", "mV");
            }
            else if (code == ecg_condition_ste)
            {
                add_assertion(report, code, ecg_assert_st_deviation, available ? morphology_mask_extreme(morphology, leads, morphology_st_j, true) : 0.0, 0.05, 10.0, "ST-J elevation", "mV");
                add_assertion(report, code, ecg_assert_st_lead_count, available ? elevated_st_lead_count(morphology, leads, st_threshold) : 0.0, 3.0, clinical_lead_count, "elevated ST leads", "count");
            }
            else if (code == ecg_condition_lowt)
                add_assertion(report, code, ecg_assert_t_amplitude, available ? maximum_absolute_t_amplitude(morphology, leads) : -1.0, 0.005, 0.12, "maximum absolute T amplitude", "mV");
            else if (code == ecg_condition_invt)
            {
                add_assertion(report, code, ecg_assert_t_amplitude, available ? morphology_mask_extreme(morphology, leads, morphology_t_amplitude, false) : 0.0, -10.0, -0.08, "negative T amplitude", "mV");
                add_assertion(report, code, ecg_assert_t_lead_count, available ? negative_t_lead_count(morphology, leads, -t_threshold) : 0.0, 3.0, clinical_lead_count, "negative T leads", "count");
            }
            else if (code == ecg_condition_ndt || code == ecg_condition_nt)
            {
                add_assertion(report, code, ecg_assert_t_amplitude, available ? maximum_absolute_t_amplitude(morphology, all_leads()) : -1.0, 0.05, 0.40, "maximum absolute T amplitude", "mV");
                add_assertion(report, code, ecg_assert_t_lead_count, available ? negative_t_lead_count(morphology, all_leads(), -t_threshold) : 0.0, code == ecg_condition_ndt ? 1.0 : 2.0, clinical_lead_count, "negative T leads", "count");
            }
            else if (code == ecg_condition_tab)
            {
                add_assertion(report, code, ecg_assert_t_polarity_dispersion, available ? positive_t_lead_count(morphology, all_leads(), t_threshold) : 0.0, 2.0, clinical_lead_count, "positive T leads", "count");
                add_assertion(report, code, ecg_assert_t_polarity_dispersion, available ? negative_t_lead_count(morphology, all_leads(), -t_threshold) : 0.0, 2.0, clinical_lead_count, "negative T leads", "count");
            }
            else if (code == ecg_condition_aneur)
            {
                const unsigned int anterior = anterior_leads();
                add_assertion(report, code, ecg_assert_q_wave_amplitude, available ? morphology_mask_extreme(morphology, anterior, morphology_q_amplitude, false) : 0.0, -10.0, -0.10, "anterior Q amplitude", "mV");
                add_assertion(report, code, ecg_assert_q_wave_duration, available ? morphology_mask_extreme(morphology, anterior, morphology_q_duration, true) : -1.0, 0.030, 0.5, "anterior Q duration", "s");
                add_assertion(report, code, ecg_assert_q_wave_lead_count, available ? pathological_q_lead_count_mask(morphology, anterior) : 0.0, 2.0, clinical_lead_count, "anterior pathological Q leads", "count");
                add_assertion(report, code, ecg_assert_st_deviation, available ? morphology_mask_extreme(morphology, anterior, morphology_st_j, true) : 0.0, 0.05, 10.0, "persistent anterior ST-J elevation proxy", "mV");
                add_assertion(report, code, ecg_assert_st_lead_count, available ? elevated_st_lead_count(morphology, anterior) : 0.0, 2.0, clinical_lead_count, "elevated anterior ST leads", "count");
            }
            else if (code == ecg_condition_el)
            {
                add_assertion(report, code, ecg_assert_t_amplitude, available ? maximum_absolute_t_amplitude(morphology, leads) : -1.0, 0.25, 10.0, "maximum absolute T amplitude", "mV");
                add_assertion(report, code, ecg_assert_t_duration, available ? morphology_mask_extreme(morphology, leads, morphology_t_duration, true) : -1.0, 0.04, 0.17, "maximum T duration", "s");
            }
        }

        double left_ventricular_voltage_index(const ecg_morphology_report& morphology)
        {
            const double v1_s = morphology_lead_mean(morphology, clinical_lead_v1, morphology_s_amplitude);
            const double v5_r = morphology_lead_mean(morphology, clinical_lead_v5, morphology_r_amplitude);
            const double v6_r = morphology_lead_mean(morphology, clinical_lead_v6, morphology_r_amplitude);
            return std::fabs(v1_s) + std::max(std::fabs(v5_r), std::fabs(v6_r));
        }

        double right_precordial_rs_ratio(const ecg_morphology_report& morphology)
        {
            const double r = morphology_lead_mean(morphology, clinical_lead_v1, morphology_r_amplitude);
            const double s = morphology_lead_mean(morphology, clinical_lead_v1, morphology_s_amplitude);
            return r >= 0.0 && s < 0.0 ? r / std::max(std::fabs(s), 1e-12) : -1.0;
        }

        unsigned int record_sample_at(const clinical_ecg_record& record, double time_seconds)
        {
            if (time_seconds <= 0.0)
                return 0;
            const unsigned long long sample = static_cast<unsigned long long>(std::llround(time_seconds * record.sampling_rate_hz()));
            return static_cast<unsigned int>(std::min<unsigned long long>(sample, record.sample_count() ? record.sample_count() - 1 : 0));
        }

        double mean_lead_at_beat_time(const clinical_ecg_record& record, unsigned int lead, double clinical_beat_annotation::*time_member)
        {
            const clinical_beat_annotation* beats = record.beats();
            const double* signal = record.lead_data(lead);
            if (!beats || !signal || record.beat_count() == 0)
                return 0.0;
            double sum = 0.0;
            unsigned int count = 0;
            for (unsigned int index = 0; index < record.beat_count(); ++index)
            {
                const unsigned int sample = record_sample_at(record, beats[index].*time_member);
                if (sample < record.sample_count())
                {
                    sum += signal[sample];
                    ++count;
                }
            }
            return count ? sum / count : 0.0;
        }

        double frontal_axis_degrees(const clinical_ecg_record& record)
        {
            const double lead_i = mean_lead_at_beat_time(record, clinical_lead_i, &clinical_beat_annotation::r_peak_time_seconds);
            const double lead_ii = mean_lead_at_beat_time(record, clinical_lead_ii, &clinical_beat_annotation::r_peak_time_seconds);
            const double y = (lead_ii - 0.5 * lead_i) / 0.8660254037844386;
            double axis = std::atan2(y, lead_i) * 180.0 / 3.14159265358979323846;
            if (axis > 180.0)
                axis -= 360.0;
            if (axis < -180.0)
                axis += 360.0;
            return axis;
        }

        double minimum_mean_r_amplitude(const ecg_morphology_report& morphology, unsigned int leads)
        {
            double result = std::numeric_limits<double>::max();
            bool found = false;
            for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
            {
                if ((leads & lead_bit(lead)) == 0)
                    continue;
                result = std::min(result, morphology_lead_mean(morphology, lead, morphology_r_amplitude));
                found = true;
            }
            return found ? result : -1.0;
        }

        double minimum_mean_terminal_sample(const clinical_ecg_record& record, unsigned int leads)
        {
            double result = std::numeric_limits<double>::max();
            bool found = false;
            for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
            {
                if ((leads & lead_bit(lead)) == 0)
                    continue;
                result = std::min(result, mean_lead_at_beat_time(record, lead, &clinical_beat_annotation::s_peak_time_seconds));
                found = true;
            }
            return found ? result : 0.0;
        }

        double maximum_mean_terminal_sample(const clinical_ecg_record& record, unsigned int leads)
        {
            double result = -std::numeric_limits<double>::max();
            bool found = false;
            for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
            {
                if ((leads & lead_bit(lead)) == 0)
                    continue;
                result = std::max(result, mean_lead_at_beat_time(record, lead, &clinical_beat_annotation::s_peak_time_seconds));
                found = true;
            }
            return found ? result : 0.0;
        }

        double complete_bbb_contract_match(const clinical_ecg_record& record)
        {
            if (record.beat_count() < 1)
                return 0.0;
            const clinical_beat_annotation& beat = record.beats()[0];
            const unsigned int q = record_sample_at(record, beat.q_peak_time_seconds);
            const unsigned int r = record_sample_at(record, beat.r_peak_time_seconds);
            const unsigned int s = record_sample_at(record, beat.s_peak_time_seconds);
            const unsigned int t = record_sample_at(record, beat.t_peak_time_seconds);
            const double* lead_i = record.lead_data(clinical_lead_i);
            const double* v1 = record.lead_data(clinical_lead_v1);
            const double* v5 = record.lead_data(clinical_lead_v5);
            const double* v6 = record.lead_data(clinical_lead_v6);
            if (!lead_i || !v1 || !v5 || !v6)
                return 0.0;
            const bool rbbb = beat.qrs_duration_seconds >= 0.120 && v1[q] > 0.02 && v1[r] < -0.20 && v1[s] > 0.15 && lead_i[s] < -0.15 && v6[s] < -0.15 && v1[t] < -0.05 && lead_i[t] > 0.05 && v6[t] > 0.05;
            const bool lbbb = beat.qrs_duration_seconds >= 0.120 && std::fabs(lead_i[q]) < 0.01 && std::fabs(v6[q]) < 0.01 && v1[r] < -0.20 && v1[s] < -0.10 && lead_i[r] > 0.30 && lead_i[s] > 0.10 && v5[s] > 0.10 && v6[r] > 0.30 && v6[s] > 0.10 && v1[t] > 0.05 && lead_i[t] < -0.05 && v6[t] < -0.05;
            return rbbb || lbbb ? 1.0 : 0.0;
        }

        double delta_wave_evidence(const clinical_ecg_record& record)
        {
            const clinical_beat_annotation* beats = record.beats();
            const double* lead_ii = record.lead_data(clinical_lead_ii);
            if (!beats || !lead_ii || record.beat_count() == 0)
                return -1.0;
            double sum = 0.0;
            unsigned int count = 0;
            for (unsigned int index = 0; index < record.beat_count(); ++index)
            {
                const double early_time = std::min(beats[index].qrs_onset_time_seconds + 0.035, 0.5 * (beats[index].qrs_onset_time_seconds + beats[index].r_peak_time_seconds));
                const unsigned int onset = record_sample_at(record, beats[index].qrs_onset_time_seconds);
                const unsigned int early = record_sample_at(record, early_time);
                if (early < record.sample_count() && onset < record.sample_count())
                {
                    sum += std::fabs(lead_ii[early] - lead_ii[onset]);
                    ++count;
                }
            }
            return count ? sum / count : -1.0;
        }

        double pathological_q_lead_count(const ecg_morphology_report& morphology, ecg_lead_region region)
        {
            const ecg_lead_morphology* entries = morphology.entries();
            double amplitude_sum[clinical_lead_count] = {};
            double duration_sum[clinical_lead_count] = {};
            unsigned int count[clinical_lead_count] = {};
            for (unsigned int index = 0; entries && index < morphology.entry_count(); ++index)
            {
                const unsigned int lead = entries[index].lead_index;
                if (!ecg_lead_is_in_region(lead, region))
                    continue;
                amplitude_sum[lead] += entries[index].q_amplitude_mv;
                duration_sum[lead] += entries[index].q_duration_seconds;
                ++count[lead];
            }
            unsigned int matching = 0;
            for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
                if (count[lead] && amplitude_sum[lead] / count[lead] <= -0.10 && duration_sum[lead] / count[lead] >= 0.030)
                    ++matching;
            return matching;
        }

        void add_advanced_conduction_assertions(ecg_condition_code code, double severity, const clinical_ecg_record& record, const ecg_morphology_report& morphology, bool available, ecg_scenario_report::implementation& report)
        {
            const double qrs = mean_annotation_value(record, ecg_assert_qrs_duration);
            const unsigned int limb_lateral = lead_bit(clinical_lead_i) | lead_bit(clinical_lead_avl);
            const unsigned int lateral_terminal = lead_bit(clinical_lead_i) | lead_bit(clinical_lead_v6);
            const unsigned int inferior = inferior_leads();
            if (code == ecg_condition_irbbb)
            {
                add_assertion(report, code, ecg_assert_qrs_duration, qrs, 0.100, 0.1195, "incomplete RBBB QRS duration", "s");
                add_assertion(report, code, ecg_assert_terminal_v1_polarity, terminal_v1_polarity(record), 0.02, 10.0, "positive terminal V1 force", "mV");
                add_assertion(report, code, ecg_assert_lateral_qrs_polarity, maximum_mean_terminal_sample(record, lateral_terminal), -10.0, -0.03, "terminal lateral S force", "mV");
                add_assertion(report, code, ecg_assert_t_amplitude, available ? morphology_lead_mean(morphology, clinical_lead_v1, morphology_t_amplitude) : 0.0, -10.0, -0.01, "right-precordial secondary T", "mV");
            }
            else if (code == ecg_condition_ilbbb)
            {
                add_assertion(report, code, ecg_assert_qrs_duration, qrs, 0.110, 0.1195, "incomplete LBBB QRS duration", "s");
                add_assertion(report, code, ecg_assert_terminal_v1_polarity, terminal_v1_polarity(record), -10.0, -0.02, "negative terminal V1 force", "mV");
                add_assertion(report, code, ecg_assert_lateral_qrs_polarity, minimum_mean_terminal_sample(record, lateral_terminal), 0.03, 10.0, "positive lateral terminal activation", "mV");
                add_assertion(report, code, ecg_assert_t_amplitude, available ? morphology_lead_mean(morphology, clinical_lead_v6, morphology_t_amplitude) : 0.0, -10.0, -0.01, "lateral secondary T", "mV");
            }
            else if (code == ecg_condition_lafb)
            {
                add_assertion(report, code, ecg_assert_qrs_duration, qrs, 0.040, 0.1195, "sub-complete-BBB QRS duration", "s");
                add_assertion(report, code, ecg_assert_frontal_axis, frontal_axis_degrees(record), -90.0, -30.0, "left frontal QRS axis", "deg");
                add_assertion(report, code, ecg_assert_lateral_qrs_polarity, available ? minimum_mean_r_amplitude(morphology, limb_lateral) : -1.0, 0.08, 10.0, "qR-compatible lateral R", "mV");
                add_assertion(report, code, ecg_assert_inferior_qrs_polarity, maximum_mean_terminal_sample(record, inferior), -10.0, -0.03, "rS-compatible inferior terminal force", "mV");
            }
            else if (code == ecg_condition_lpfb)
            {
                add_assertion(report, code, ecg_assert_qrs_duration, qrs, 0.040, 0.1195, "sub-complete-BBB QRS duration", "s");
                add_assertion(report, code, ecg_assert_frontal_axis, frontal_axis_degrees(record), 90.0, 150.0, "right frontal QRS axis", "deg");
                add_assertion(report, code, ecg_assert_lateral_qrs_polarity, maximum_mean_terminal_sample(record, limb_lateral), -10.0, -0.03, "rS-compatible lateral terminal force", "mV");
                add_assertion(report, code, ecg_assert_inferior_qrs_polarity, available ? minimum_mean_r_amplitude(morphology, lead_bit(clinical_lead_iii) | lead_bit(clinical_lead_avf)) : -1.0, 0.08, 10.0, "qR-compatible inferior R", "mV");
            }
            else if (code == ecg_condition_ivcd)
            {
                add_assertion(report, code, ecg_assert_qrs_duration, qrs, 0.120, 0.170, "nonspecific wide QRS", "s");
                add_assertion(report, code, ecg_assert_complete_bbb_exclusion, complete_bbb_contract_match(record), 0.0, 0.0, "complete BBB contract match", "bool");
                add_assertion(report, code, ecg_assert_terminal_v1_polarity, terminal_v1_polarity(record), -0.15, 0.15, "nonspecific terminal V1 force", "mV");
            }
            else if (code == ecg_condition_wpw)
            {
                add_assertion(report, code, ecg_assert_pr_interval, mean_annotation_value(record, ecg_assert_pr_interval), 0.070, 0.1195, "short PR interval", "s");
                add_assertion(report, code, ecg_assert_qrs_duration, qrs, 0.120, 0.170, "pre-excited QRS duration", "s");
                add_assertion(report, code, ecg_assert_delta_wave, delta_wave_evidence(record), 0.025 + 0.020 * severity, 10.0, "early delta upstroke evidence", "mV");
                add_assertion(report, code, ecg_assert_t_lead_count, available ? negative_t_lead_count(morphology, widespread_leads(), -0.02) : 0.0, 2.0, clinical_lead_count, "secondary negative T leads", "count");
            }
        }

        void evaluate_phenotype(const ecg_qa_scenario::implementation& scenario, const clinical_ecg_record& record, ecg_scenario_report::implementation& report)
        {
            const double count_limit = std::max(record.beat_count(), record.atrial_event_count());
            const clinical_ventricular_origin ectopic_origin = has_condition(scenario.conditions, ecg_condition_pac) ? clinical_origin_pac : clinical_origin_pvc;
            ecg_morphology_report morphology;
            const bool morphology_available = measure_ecg_morphology(record, morphology);
            for (const scenario_condition& requested : scenario.conditions)
            {
                switch (requested.code)
                {
                case ecg_condition_norm:
                    add_assertion(report, requested.code, ecg_assert_rhythm, beat_fraction(record, clinical_rhythm_sinus), 1.0, 1.0, "sinus rhythm beats", "ratio");
                    add_assertion(report, requested.code, ecg_assert_p_wave_presence, p_wave_fraction(record), 1.0, 1.0, "P waves present", "ratio");
                    add_assertion(report, requested.code, ecg_assert_qrs_duration, mean_annotation_value(record, ecg_assert_qrs_duration), 0.04, 0.12, "normal QRS duration", "s");
                    break;
                case ecg_condition_sr:
                    add_assertion(report, requested.code, ecg_assert_rhythm, beat_fraction(record, clinical_rhythm_sinus), 1.0, 1.0, "sinus rhythm beats", "ratio");
                    add_assertion(report, requested.code, ecg_assert_p_wave_presence, p_wave_fraction(record), 1.0, 1.0, "P waves present", "ratio");
                    break;
                case ecg_condition_afib:
                    add_assertion(report, requested.code, ecg_assert_rhythm, beat_fraction(record, clinical_rhythm_atrial_fibrillation), 1.0, 1.0, "AF rhythm beats", "ratio");
                    add_assertion(report, requested.code, ecg_assert_p_wave_presence, p_wave_fraction(record), 0.0, 0.0, "P waves absent", "ratio");
                    add_assertion(report, requested.code, ecg_assert_rr_variability, rr_standard_deviation(record), 0.03, 2.0, "irregular RR", "s");
                    break;
                case ecg_condition_aflt:
                    add_assertion(report, requested.code, ecg_assert_rhythm, beat_fraction(record, clinical_rhythm_atrial_flutter), 1.0, 1.0, "flutter rhythm beats", "ratio");
                    add_assertion(report, requested.code, ecg_assert_atrial_ventricular_ratio, record.beat_count() ? static_cast<double>(record.atrial_event_count()) / record.beat_count() : -1.0, 1.8, 6.5, "atrial to ventricular events", "ratio");
                    break;
                case ecg_condition_svtac:
                    add_assertion(report, requested.code, ecg_assert_rhythm, beat_fraction(record, clinical_rhythm_supraventricular_tachycardia), 1.0, 1.0, "SVT rhythm beats", "ratio");
                    add_assertion(report, requested.code, ecg_assert_heart_rate, mean_heart_rate(record), 100.0, 400.0, "tachycardic rate", "bpm");
                    add_assertion(report, requested.code, ecg_assert_qrs_duration, mean_annotation_value(record, ecg_assert_qrs_duration), 0.04, 0.12, "narrow QRS", "s");
                    break;
                case ecg_condition_svarr:
                case ecg_condition_psvt:
                    add_assertion(report, requested.code, ecg_assert_episode_coverage, episode_count_metric(record, requested.code), 1.0, 1.0, "episode interval present", "count");
                    add_assertion(report, requested.code, ecg_assert_rhythm, sinus_fraction_outside_episode(record, requested.code), 1.0, 1.0, "sinus baseline outside episode", "ratio");
                    add_assertion(report, requested.code, ecg_assert_rhythm, rhythm_fraction_inside_episode(record, requested.code, clinical_rhythm_supraventricular_tachycardia), 1.0, 1.0, "SVT rhythm inside episode", "ratio");
                    add_assertion(report, requested.code, ecg_assert_heart_rate, episode_heart_rate(record, requested.code), 100.0, 400.0, "episode tachycardic rate", "bpm");
                    add_assertion(report, requested.code, ecg_assert_qrs_duration, mean_episode_qrs_duration(record, requested.code), 0.04, 0.12, "episode narrow QRS", "s");
                    add_assertion(report, requested.code, ecg_assert_p_wave_presence, episode_p_wave_fraction(record, requested.code), 0.0, 0.0, "P waves suppressed inside episode", "ratio");
                    break;
                case ecg_condition_stach:
                    add_assertion(report, requested.code, ecg_assert_heart_rate, median_atrial_rate(record), 100.0, 400.0, "sinus tachycardia rate", "bpm");
                    break;
                case ecg_condition_sbrad:
                    add_assertion(report, requested.code, ecg_assert_heart_rate, median_atrial_rate(record), 10.0, 60.0, "sinus bradycardia rate", "bpm");
                    break;
                case ecg_condition_sarrh:
                    add_assertion(report, requested.code, ecg_assert_rr_variability, rr_standard_deviation(record), 0.015, 2.0, "RR variability", "s");
                    break;
                case ecg_condition_pac:
                case ecg_condition_pvc:
                case ecg_condition_prc:
                    add_assertion(report, requested.code, ecg_assert_ectopic_origin, origin_count(record, ectopic_origin), 1.0, count_limit, "expected ectopic beats", "count");
                    add_assertion(report, requested.code, ecg_assert_premature_coupling, premature_coupling_ratio(record, ectopic_origin), 0.0, 0.9, "premature coupling", "ratio");
                    break;
                case ecg_condition_bigu:
                    add_assertion(report, requested.code, ecg_assert_ectopic_cadence, ectopic_cadence_fraction(record, ectopic_origin, 2), 1.0, 1.0, "bigeminal cadence", "ratio");
                    break;
                case ecg_condition_trigu:
                    add_assertion(report, requested.code, ecg_assert_ectopic_cadence, ectopic_cadence_fraction(record, ectopic_origin, 3), 1.0, 1.0, "trigeminal cadence", "ratio");
                    break;
                case ecg_condition_1avb:
                case ecg_condition_lpr:
                    add_assertion(report, requested.code, ecg_assert_pr_interval, mean_annotation_value(record, ecg_assert_pr_interval), 0.2, 1.0, "prolonged PR interval", "s");
                    break;
                case ecg_condition_2avb:
                    add_assertion(report, requested.code, ecg_assert_dropped_atrial_events, dropped_atrial_count(record), 1.0, count_limit, "non-conducted atrial events", "count");
                    if (scenario.second_degree_pattern == ecg_second_degree_mobitz_i)
                        add_assertion(report, requested.code, ecg_assert_av_pattern, mobitz_pattern_metric(record, scenario.second_degree_pattern), 1.0, count_limit, "Wenckebach PR progression", "count");
                    else
                        add_assertion(report, requested.code, ecg_assert_av_pattern, mobitz_pattern_metric(record, scenario.second_degree_pattern), 0.0, 0.001, "Mobitz II stable PR", "s");
                    break;
                case ecg_condition_3avb:
                    add_assertion(report, requested.code, ecg_assert_dropped_atrial_events, record.atrial_event_count() ? dropped_atrial_count(record) / record.atrial_event_count() : -1.0, 1.0, 1.0, "complete AV dissociation", "ratio");
                    add_assertion(report, requested.code, ecg_assert_ventricular_escape, record.beat_count() ? static_cast<double>(origin_count(record, clinical_origin_ventricular_escape)) / record.beat_count() : -1.0, 1.0, 1.0, "ventricular escape beats", "ratio");
                    break;
                case ecg_condition_crbbb:
                    add_assertion(report, requested.code, ecg_assert_qrs_duration, mean_annotation_value(record, ecg_assert_qrs_duration), 0.13, 0.5, "wide QRS", "s");
                    add_assertion(report, requested.code, ecg_assert_terminal_v1_polarity, terminal_v1_polarity(record), 0.000001, 10.0, "positive terminal V1 force", "mV");
                    break;
                case ecg_condition_clbbb:
                    add_assertion(report, requested.code, ecg_assert_qrs_duration, mean_annotation_value(record, ecg_assert_qrs_duration), 0.14, 0.5, "wide QRS", "s");
                    add_assertion(report, requested.code, ecg_assert_terminal_v1_polarity, terminal_v1_polarity(record), -10.0, -0.000001, "negative terminal V1 force", "mV");
                    break;
                case ecg_condition_lafb:
                case ecg_condition_irbbb:
                case ecg_condition_ivcd:
                case ecg_condition_lpfb:
                case ecg_condition_wpw:
                case ecg_condition_ilbbb:
                    add_advanced_conduction_assertions(requested.code, requested.severity, record, morphology, morphology_available, report);
                    break;
                case ecg_condition_lngqt:
                    add_assertion(report, requested.code, ecg_assert_qtc_interval, mean_annotation_value(record, ecg_assert_qtc_interval), 0.44, 2.0, "prolonged QTc", "s");
                    break;
                case ecg_condition_isc:
                case ecg_condition_iscal:
                case ecg_condition_iscin:
                case ecg_condition_iscil:
                case ecg_condition_iscas:
                case ecg_condition_iscla:
                case ecg_condition_iscan:
                    add_ischemia_assertions(requested.code, requested.severity, morphology, morphology_available, report);
                    break;
                case ecg_condition_ndt:
                case ecg_condition_nst:
                case ecg_condition_dig:
                case ecg_condition_aneur:
                case ecg_condition_el:
                case ecg_condition_std:
                case ecg_condition_lowt:
                case ecg_condition_nt:
                case ecg_condition_invt:
                case ecg_condition_tab:
                case ecg_condition_ste:
                    add_repolarization_assertions(requested.code, requested.severity, morphology, morphology_available, report);
                    break;
                case ecg_condition_pace:
                    add_assertion(report, requested.code, ecg_assert_rhythm, beat_fraction(record, clinical_rhythm_paced), 1.0, 1.0, "paced rhythm beats", "ratio");
                    add_assertion(report, requested.code, ecg_assert_pacing, pacing_evidence_count(record), 1.0, count_limit, "paced beats with spike annotations", "count");
                    break;
                case ecg_condition_qwave:
                {
                    const ecg_lead_region region = q_wave_region(scenario.q_wave_territory);
                    add_assertion(report, requested.code, ecg_assert_q_wave_amplitude, morphology_available ? morphology_region_extreme(morphology, region, morphology_q_amplitude, false) : 0.0, -10.0, -0.10, "territorial Q amplitude", "mV");
                    add_assertion(report, requested.code, ecg_assert_q_wave_duration, morphology_available ? morphology_region_extreme(morphology, region, morphology_q_duration, true) : -1.0, 0.030, 0.5, "territorial Q duration", "s");
                    add_assertion(report, requested.code, ecg_assert_q_wave_lead_count, morphology_available ? pathological_q_lead_count(morphology, region) : 0.0, 2.0, clinical_lead_count, "matching territorial leads", "count");
                    break;
                }
                case ecg_condition_lvolt:
                    add_assertion(report, requested.code, ecg_assert_low_qrs_voltage, morphology_available ? morphology_region_extreme(morphology, ecg_region_limb, morphology_qrs_peak_to_peak, true) : -1.0, 0.0, 0.5, "maximum limb QRS voltage", "mV");
                    add_assertion(report, requested.code, ecg_assert_low_qrs_voltage, morphology_available ? morphology_region_extreme(morphology, ecg_region_precordial, morphology_qrs_peak_to_peak, true) : -1.0, 0.0, 1.0, "maximum precordial QRS voltage", "mV");
                    break;
                case ecg_condition_hvolt:
                    add_assertion(report, requested.code, ecg_assert_high_qrs_voltage, morphology_available ? morphology_region_extreme(morphology, ecg_region_all, morphology_qrs_peak_to_peak, true) : -1.0, 2.0, 20.0, "maximum QRS voltage", "mV");
                    break;
                case ecg_condition_lvh:
                    add_assertion(report, requested.code, ecg_assert_left_ventricular_voltage, morphology_available ? left_ventricular_voltage_index(morphology) : -1.0, 1.5, 20.0, "left ventricular voltage index", "mV");
                    break;
                case ecg_condition_vclvh:
                    add_assertion(report, requested.code, ecg_assert_left_ventricular_voltage, morphology_available ? morphology_region_extreme(morphology, ecg_region_all, morphology_qrs_peak_to_peak, true) : -1.0, 2.0, 20.0, "maximum QRS voltage", "mV");
                    break;
                case ecg_condition_rvh:
                    add_assertion(report, requested.code, ecg_assert_right_precordial_rs_ratio, morphology_available ? right_precordial_rs_ratio(morphology) : -1.0, 1.0, 100.0, "V1 R to S ratio", "ratio");
                    break;
                case ecg_condition_sehyp:
                    add_assertion(report, requested.code, ecg_assert_septal_qrs_voltage, morphology_available ? morphology_region_extreme(morphology, ecg_region_septal, morphology_qrs_peak_to_peak, true) : -1.0, 0.75, 20.0, "maximum septal QRS voltage", "mV");
                    break;
                case ecg_condition_lao_lae:
                    add_assertion(report, requested.code, ecg_assert_p_wave_duration, morphology_available ? morphology_region_extreme(morphology, ecg_region_all, morphology_p_duration, true) : -1.0, 0.10, 0.30, "maximum measured P duration", "s");
                    break;
                case ecg_condition_rao_rae:
                    add_assertion(report, requested.code, ecg_assert_p_wave_amplitude, morphology_available ? morphology_region_extreme(morphology, ecg_region_inferior, morphology_p_amplitude, true) : -1.0, 0.10, 2.0, "maximum inferior P amplitude", "mV");
                    break;
                case ecg_condition_imi:
                case ecg_condition_asmi:
                case ecg_condition_ilmi:
                case ecg_condition_ami:
                case ecg_condition_almi:
                case ecg_condition_lmi:
                case ecg_condition_iplmi:
                case ecg_condition_ipmi:
                case ecg_condition_pmi:
                    add_infarction_assertions(requested.code, morphology, morphology_available, report);
                    break;
                case ecg_condition_injas:
                case ecg_condition_injal:
                case ecg_condition_injin:
                case ecg_condition_injla:
                case ecg_condition_injil:
                    add_injury_assertions(requested.code, morphology, morphology_available, report);
                    break;
                default:
                    break;
                }
            }
            if (!scenario.morphology_components.empty())
                add_assertion(report, ecg_condition_sr, ecg_assert_extended_morphology, extended_component_measurement_fraction(record), 1.0, 1.0, "extended morphology components measurable", "ratio");
            if (scenario.fusion_every_n_beats)
                add_assertion(report, ecg_condition_sr, ecg_assert_fusion_cadence, ectopic_cadence_fraction(record, clinical_origin_fusion, scenario.fusion_every_n_beats), 1.0, 1.0, "fusion beat cadence", "ratio");
            report.phenotype_passed = !report.assertions.empty();
            for (const phenotype_assertion& assertion : report.assertions)
                report.phenotype_passed = report.phenotype_passed && assertion.status == ecg_assertion_passed;
        }
    }

    const ecg_condition_info* ecg_condition_catalog()
    {
        return condition_catalog;
    }

    unsigned int ecg_condition_catalog_size()
    {
        return ecg_condition_count;
    }

    unsigned int ecg_scenario_engine_version()
    {
        return SCENARIO_ENGINE_VERSION;
    }

    const ecg_condition_info* find_ecg_condition(ecg_condition_code code)
    {
        const int index = enum_value(code);
        return index >= 0 && index < ecg_condition_count ? &condition_catalog[index] : 0;
    }

    const ecg_condition_info* find_ecg_condition(const char* scp_code)
    {
        if (!scp_code)
            return 0;
        for (unsigned int index = 0; index < ecg_condition_count; ++index)
            if (std::strcmp(condition_catalog[index].scp_code, scp_code) == 0)
                return &condition_catalog[index];
        return 0;
    }

    bool ecg_condition_supports_variable_severity(ecg_condition_code code)
    {
        return supports_variable_severity(code);
    }

    const char* ecg_morphology_control_name(ecg_morphology_control control)
    {
        switch (control)
        {
        case ecg_morphology_p_amplitude_mv: return "p_amplitude_mv";
        case ecg_morphology_q_amplitude_mv: return "q_amplitude_mv";
        case ecg_morphology_r_amplitude_mv: return "r_amplitude_mv";
        case ecg_morphology_s_amplitude_mv: return "s_amplitude_mv";
        case ecg_morphology_t_amplitude_mv: return "t_amplitude_mv";
        case ecg_morphology_st_j_amplitude_mv: return "st_j_amplitude_mv";
        case ecg_morphology_st_slope_mv_per_second: return "st_slope_mv_per_second";
        case ecg_morphology_p_axis_degrees: return "p_axis_degrees";
        case ecg_morphology_qrs_axis_degrees: return "qrs_axis_degrees";
        case ecg_morphology_t_axis_degrees: return "t_axis_degrees";
        case ecg_morphology_p_duration_ms: return "p_duration_ms";
        case ecg_morphology_qrs_duration_ms: return "qrs_duration_ms";
        case ecg_morphology_qt_interval_ms: return "qt_interval_ms";
        case ecg_morphology_t_duration_ms: return "t_duration_ms";
        case ecg_morphology_control_count: break;
        }
        return "";
    }

    bool ecg_morphology_control_from_name(const char* name, ecg_morphology_control& control)
    {
        if (!name)
            return false;
        for (unsigned int index = 0; index < ecg_morphology_control_count; ++index)
        {
            const ecg_morphology_control candidate = static_cast<ecg_morphology_control>(index);
            if (std::strcmp(name, ecg_morphology_control_name(candidate)) == 0)
            {
                control = candidate;
                return true;
            }
        }
        return false;
    }

    bool ecg_morphology_control_bounds(ecg_morphology_control control, double& minimum, double& maximum)
    {
        switch (control)
        {
        case ecg_morphology_p_amplitude_mv: minimum = -0.5; maximum = 0.8; return true;
        case ecg_morphology_q_amplitude_mv: minimum = -1.5; maximum = 0.2; return true;
        case ecg_morphology_r_amplitude_mv: minimum = 0.05; maximum = 4.0; return true;
        case ecg_morphology_s_amplitude_mv: minimum = -3.0; maximum = 0.5; return true;
        case ecg_morphology_t_amplitude_mv: minimum = -1.5; maximum = 1.5; return true;
        case ecg_morphology_st_j_amplitude_mv: minimum = -0.8; maximum = 0.8; return true;
        case ecg_morphology_st_slope_mv_per_second: minimum = -4.0; maximum = 4.0; return true;
        case ecg_morphology_p_axis_degrees: minimum = -180.0; maximum = 180.0; return true;
        case ecg_morphology_qrs_axis_degrees: minimum = -180.0; maximum = 180.0; return true;
        case ecg_morphology_t_axis_degrees: minimum = -180.0; maximum = 180.0; return true;
        case ecg_morphology_p_duration_ms: minimum = 40.0; maximum = 150.0; return true;
        case ecg_morphology_qrs_duration_ms: minimum = 50.0; maximum = 180.0; return true;
        case ecg_morphology_qt_interval_ms: minimum = 260.0; maximum = 700.0; return true;
        case ecg_morphology_t_duration_ms: minimum = 60.0; maximum = 360.0; return true;
        case ecg_morphology_control_count: break;
        }
        return false;
    }

    const char* ecg_morphology_component_name(ecg_morphology_component_type type)
    {
        switch (type)
        {
        case ecg_component_p_biphasic: return "p_biphasic";
        case ecg_component_p_notch: return "p_notch";
        case ecg_component_r_prime: return "r_prime";
        case ecg_component_qrs_fragment: return "qrs_fragment";
        case ecg_component_t_biphasic: return "t_biphasic";
        case ecg_component_t_notch: return "t_notch";
        case ecg_component_u_wave: return "u_wave";
        }
        return "";
    }

    bool ecg_morphology_component_from_name(const char* name, ecg_morphology_component_type& type)
    {
        if (!name)
            return false;
        for (int value = ecg_component_p_biphasic; value <= ecg_component_u_wave; ++value)
        {
            const ecg_morphology_component_type candidate = static_cast<ecg_morphology_component_type>(value);
            if (std::strcmp(name, ecg_morphology_component_name(candidate)) == 0)
            {
                type = candidate;
                return true;
            }
        }
        return false;
    }

    unsigned int ecg_morphology_lead_mask(unsigned int lead)
    {
        return lead < clinical_lead_count ? 1u << lead : 0u;
    }

    ecg_qa_scenario::ecg_qa_scenario()
        : implementation_(new implementation)
    {
    }

    ecg_qa_scenario::ecg_qa_scenario(const ecg_qa_scenario& other)
        : implementation_(new implementation(*other.implementation_))
    {
    }

    ecg_qa_scenario& ecg_qa_scenario::operator=(const ecg_qa_scenario& other)
    {
        if (this != &other)
            *implementation_ = *other.implementation_;
        return *this;
    }

    ecg_qa_scenario::~ecg_qa_scenario()
    {
        delete implementation_;
    }

    bool ecg_qa_scenario::add_condition(ecg_condition_code code, double severity)
    {
        if (!valid_condition(code) || !std::isfinite(severity) || severity <= 0.0 || severity > 1.0)
            return false;
        for (scenario_condition& condition : implementation_->conditions)
        {
            if (condition.code == code)
            {
                condition.severity = severity;
                return true;
            }
        }
        implementation_->conditions.push_back(scenario_condition{code, severity});
        std::sort(implementation_->conditions.begin(), implementation_->conditions.end(), [](const scenario_condition& left, const scenario_condition& right) { return left.code < right.code; });
        return true;
    }

    bool ecg_qa_scenario::remove_condition(ecg_condition_code code)
    {
        if (!valid_condition(code))
            return false;
        for (std::vector<scenario_condition>::iterator it = implementation_->conditions.begin(); it != implementation_->conditions.end(); ++it)
        {
            if (it->code == code)
            {
                implementation_->conditions.erase(it);
                return true;
            }
        }
        return false;
    }

    void ecg_qa_scenario::clear_conditions()
    {
        implementation_->conditions.clear();
    }

    unsigned int ecg_qa_scenario::condition_count() const
    {
        return static_cast<unsigned int>(implementation_->conditions.size());
    }

    ecg_condition_code ecg_qa_scenario::condition(unsigned int index) const
    {
        return index < implementation_->conditions.size() ? implementation_->conditions[index].code : ecg_condition_count;
    }

    double ecg_qa_scenario::condition_severity(unsigned int index) const
    {
        return index < implementation_->conditions.size() ? implementation_->conditions[index].severity : 0.0;
    }

    bool ecg_qa_scenario::has_condition(ecg_condition_code code) const
    {
        return valid_condition(code) && ::signal_synth::has_condition(implementation_->conditions, code);
    }

    bool ecg_qa_scenario::set_sampling_rate_hz(unsigned int value)
    {
        if (value < 100 || value > 1000000)
            return false;
        implementation_->sampling_rate_hz = value;
        return true;
    }

    unsigned int ecg_qa_scenario::sampling_rate_hz() const
    {
        return implementation_->sampling_rate_hz;
    }

    bool ecg_qa_scenario::set_seed(unsigned long long value)
    {
        implementation_->seed = value;
        return true;
    }

    unsigned long long ecg_qa_scenario::seed() const
    {
        return implementation_->seed;
    }

    bool ecg_qa_scenario::set_heart_rate_bpm(double value)
    {
        if (!std::isfinite(value) || value < 0.0 || (value > 0.0 && value < 10.0) || value > 400.0)
            return false;
        implementation_->heart_rate_bpm = value;
        return true;
    }

    double ecg_qa_scenario::heart_rate_bpm() const
    {
        return implementation_->heart_rate_bpm;
    }

    bool ecg_qa_scenario::set_rr_variability_seconds(double value)
    {
        if (!std::isfinite(value) || value < 0.0 || value > 2.0)
            return false;
        implementation_->rr_variability_seconds = value;
        return true;
    }

    double ecg_qa_scenario::rr_variability_seconds() const
    {
        return implementation_->rr_variability_seconds;
    }

    bool ecg_qa_scenario::set_morphology_control(ecg_morphology_control control, double value)
    {
        double minimum = 0.0, maximum = 0.0;
        if (!ecg_morphology_control_bounds(control, minimum, maximum) || !std::isfinite(value) || value < minimum || value > maximum)
            return false;
        const int index = enum_value(control);
        implementation_->morphology_enabled[index] = true;
        implementation_->morphology_values[index] = value;
        return true;
    }

    bool ecg_qa_scenario::clear_morphology_control(ecg_morphology_control control)
    {
        const int index = enum_value(control);
        if (index < 0 || index >= ecg_morphology_control_count)
            return false;
        implementation_->morphology_enabled[index] = false;
        implementation_->morphology_values[index] = 0.0;
        return true;
    }

    bool ecg_qa_scenario::morphology_control_enabled(ecg_morphology_control control) const
    {
        const int index = enum_value(control);
        return index >= 0 && index < ecg_morphology_control_count && implementation_->morphology_enabled[index];
    }

    double ecg_qa_scenario::morphology_control_value(ecg_morphology_control control) const
    {
        const int index = enum_value(control);
        return index >= 0 && index < ecg_morphology_control_count && implementation_->morphology_enabled[index] ? implementation_->morphology_values[index] : 0.0;
    }

    bool ecg_qa_scenario::has_morphology_controls() const
    {
        for (unsigned int index = 0; index < ecg_morphology_control_count; ++index)
            if (implementation_->morphology_enabled[index])
                return true;
        return false;
    }

    bool ecg_qa_scenario::add_morphology_component(ecg_morphology_component_type type, unsigned int lead_mask, double amplitude_mv, double offset_ms, double duration_ms)
    {
        const unsigned int all_leads = (1u << clinical_lead_count) - 1u;
        if (!valid_morphology_component_type(type) || !lead_mask || (lead_mask & ~all_leads) || !std::isfinite(amplitude_mv) || !std::isfinite(offset_ms) || !std::isfinite(duration_ms) || std::fabs(amplitude_mv) < 0.02 || std::fabs(amplitude_mv) > 2.0 || offset_ms < 0.0 || offset_ms > 500.0 || duration_ms < 8.0 || duration_ms > 250.0 || (type == ecg_component_u_wave && (offset_ms < 10.0 || duration_ms < 30.0)) || implementation_->morphology_components.size() >= clinical_morphology_component_max)
            return false;
        for (std::size_t index = 0; index < implementation_->morphology_components.size(); ++index)
            if (implementation_->morphology_components[index].type == type && (implementation_->morphology_components[index].lead_mask & lead_mask))
                return false;
        implementation_->morphology_components.push_back(ecg_morphology_component{type, lead_mask, amplitude_mv, offset_ms, duration_ms});
        std::sort(implementation_->morphology_components.begin(), implementation_->morphology_components.end(), [](const ecg_morphology_component& left, const ecg_morphology_component& right) { if (left.type != right.type) return left.type < right.type; if (left.lead_mask != right.lead_mask) return left.lead_mask < right.lead_mask; return left.offset_ms < right.offset_ms; });
        return true;
    }

    void ecg_qa_scenario::clear_morphology_components()
    {
        implementation_->morphology_components.clear();
    }

    unsigned int ecg_qa_scenario::morphology_component_count() const
    {
        return static_cast<unsigned int>(implementation_->morphology_components.size());
    }

    bool ecg_qa_scenario::morphology_component(unsigned int index, ecg_morphology_component& output) const
    {
        if (index >= implementation_->morphology_components.size())
            return false;
        output = implementation_->morphology_components[index];
        return true;
    }

    bool ecg_qa_scenario::set_fusion_beats(unsigned int every_n_beats, double ventricular_fraction)
    {
        if (every_n_beats < 2 || every_n_beats > 1000000 || !std::isfinite(ventricular_fraction) || ventricular_fraction < 0.1 || ventricular_fraction > 0.9)
            return false;
        implementation_->fusion_every_n_beats = every_n_beats;
        implementation_->fusion_ventricular_fraction = ventricular_fraction;
        return true;
    }

    void ecg_qa_scenario::clear_fusion_beats()
    {
        implementation_->fusion_every_n_beats = 0;
        implementation_->fusion_ventricular_fraction = 0.0;
    }

    unsigned int ecg_qa_scenario::fusion_every_n_beats() const
    {
        return implementation_->fusion_every_n_beats;
    }

    double ecg_qa_scenario::fusion_ventricular_fraction() const
    {
        return implementation_->fusion_ventricular_fraction;
    }

    bool ecg_qa_scenario::set_qt_adaptation(ecg_qt_adaptation_model model, double qtc_ms)
    {
        if (!valid_qt_adaptation_model(model) || !std::isfinite(qtc_ms) || qtc_ms < 250.0 || qtc_ms > 700.0)
            return false;
        implementation_->qt_adaptation_enabled = true;
        implementation_->qt_adaptation_model = model;
        implementation_->qt_adaptation_qtc_ms = qtc_ms;
        return true;
    }

    void ecg_qa_scenario::clear_qt_adaptation()
    {
        implementation_->qt_adaptation_enabled = false;
        implementation_->qt_adaptation_model = ecg_qt_adaptation_fridericia;
        implementation_->qt_adaptation_qtc_ms = 400.0;
    }

    bool ecg_qa_scenario::qt_adaptation_enabled() const
    {
        return implementation_->qt_adaptation_enabled;
    }

    ecg_qt_adaptation_model ecg_qa_scenario::qt_adaptation_model() const
    {
        return implementation_->qt_adaptation_model;
    }

    double ecg_qa_scenario::qt_adaptation_qtc_ms() const
    {
        return implementation_->qt_adaptation_qtc_ms;
    }

    bool ecg_qa_scenario::add_repolarization_episode(ecg_condition_code condition, double start_seconds, double duration_seconds, double transition_seconds, double peak_severity)
    {
        if (!valid_condition(condition) || !(is_new_repolarization_condition(condition) || condition == ecg_condition_lngqt) || implementation_->repolarization_episodes.size() >= clinical_repolarization_episode_max)
            return false;
        if (!std::isfinite(start_seconds) || !std::isfinite(duration_seconds) || !std::isfinite(transition_seconds) || !std::isfinite(peak_severity)
            || start_seconds < 0.0 || start_seconds > 86400.0 || duration_seconds <= 0.0 || duration_seconds > 86400.0 || transition_seconds < 0.0 || transition_seconds > 0.5 * duration_seconds || peak_severity <= 0.0 || peak_severity > 1.0)
            return false;
        ecg_repolarization_episode episode = {};
        episode.condition = condition;
        episode.start_seconds = start_seconds;
        episode.duration_seconds = duration_seconds;
        episode.transition_seconds = transition_seconds;
        episode.peak_severity = peak_severity;
        implementation_->repolarization_episodes.push_back(episode);
        return true;
    }

    void ecg_qa_scenario::clear_repolarization_episodes()
    {
        implementation_->repolarization_episodes.clear();
    }

    unsigned int ecg_qa_scenario::repolarization_episode_count() const
    {
        return static_cast<unsigned int>(implementation_->repolarization_episodes.size());
    }

    bool ecg_qa_scenario::repolarization_episode(unsigned int index, ecg_repolarization_episode& output) const
    {
        if (index >= implementation_->repolarization_episodes.size())
            return false;
        output = implementation_->repolarization_episodes[index];
        return true;
    }

    bool ecg_qa_scenario::set_minimum_rr_seconds(double value)
    {
        if (!std::isfinite(value) || value < 0.0 || value > 10.0)
            return false;
        implementation_->minimum_rr_seconds = value;
        return true;
    }

    double ecg_qa_scenario::minimum_rr_seconds() const
    {
        return implementation_->minimum_rr_seconds;
    }

    bool ecg_qa_scenario::set_maximum_rr_seconds(double value)
    {
        if (!std::isfinite(value) || value < 0.0 || value > 10.0)
            return false;
        implementation_->maximum_rr_seconds = value;
        return true;
    }

    double ecg_qa_scenario::maximum_rr_seconds() const
    {
        return implementation_->maximum_rr_seconds;
    }

    bool ecg_qa_scenario::set_hrv_modulation(double lf_hf_ratio, double lf_center_hz, double lf_bandwidth_hz, double hf_center_hz, double hf_bandwidth_hz, double respiratory_frequency_hz, double respiratory_amplitude_seconds, double respiratory_phase_radians)
    {
        const double values[] = {lf_hf_ratio, lf_center_hz, lf_bandwidth_hz, hf_center_hz, hf_bandwidth_hz, respiratory_frequency_hz, respiratory_amplitude_seconds, respiratory_phase_radians};
        for (unsigned int i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
            if (!std::isfinite(values[i]))
                return false;
        if (lf_hf_ratio < 0.0 || lf_hf_ratio > 100.0 || lf_center_hz <= 0.0 || lf_center_hz > 1.0 || lf_bandwidth_hz <= 0.0 || lf_bandwidth_hz > 1.0 || hf_center_hz <= 0.0 || hf_center_hz > 1.0 || hf_bandwidth_hz <= 0.0 || hf_bandwidth_hz > 1.0 || respiratory_frequency_hz <= 0.0 || respiratory_frequency_hz > 1.0 || respiratory_amplitude_seconds < 0.0 || respiratory_amplitude_seconds > 2.0 || respiratory_phase_radians < -1.0)
            return false;
        implementation_->hrv_modulation_enabled = true;
        implementation_->hrv_lf_hf_ratio = lf_hf_ratio;
        implementation_->hrv_lf_center_hz = lf_center_hz;
        implementation_->hrv_lf_bandwidth_hz = lf_bandwidth_hz;
        implementation_->hrv_hf_center_hz = hf_center_hz;
        implementation_->hrv_hf_bandwidth_hz = hf_bandwidth_hz;
        implementation_->hrv_respiratory_frequency_hz = respiratory_frequency_hz;
        implementation_->hrv_respiratory_amplitude_seconds = respiratory_amplitude_seconds;
        implementation_->hrv_respiratory_phase_radians = respiratory_phase_radians;
        return true;
    }

    bool ecg_qa_scenario::set_activity_modulation(double start_seconds, double duration_seconds, double intensity)
    {
        if (!std::isfinite(start_seconds) || !std::isfinite(duration_seconds) || !std::isfinite(intensity)
            || start_seconds < 0.0 || duration_seconds < 0.0 || intensity < 0.0 || intensity > 1.0
            || (intensity > 0.0 && duration_seconds <= 0.0))
            return false;
        implementation_->activity_start_seconds = start_seconds;
        implementation_->activity_duration_seconds = duration_seconds;
        implementation_->activity_intensity = intensity;
        return true;
    }

    double ecg_qa_scenario::activity_start_seconds() const { return implementation_->activity_start_seconds; }
    double ecg_qa_scenario::activity_duration_seconds() const { return implementation_->activity_duration_seconds; }
    double ecg_qa_scenario::activity_intensity() const { return implementation_->activity_intensity; }
    void ecg_qa_scenario::set_retain_source_channels(bool value) { implementation_->retain_source_channels = value; }
    bool ecg_qa_scenario::retain_source_channels() const { return implementation_->retain_source_channels; }

    bool ecg_qa_scenario::set_ectopic_every_n_beats(unsigned int value)
    {
        implementation_->ectopic_every_n_beats = value;
        return true;
    }

    unsigned int ecg_qa_scenario::ectopic_every_n_beats() const
    {
        return implementation_->ectopic_every_n_beats;
    }

    bool ecg_qa_scenario::set_second_degree_av_pattern(ecg_second_degree_av_pattern value)
    {
        if (!valid_second_degree_pattern(value))
            return false;
        implementation_->second_degree_pattern = value;
        return true;
    }

    ecg_second_degree_av_pattern ecg_qa_scenario::second_degree_av_pattern() const
    {
        return implementation_->second_degree_pattern;
    }

    bool ecg_qa_scenario::set_q_wave_territory(ecg_q_wave_territory value)
    {
        if (!valid_q_wave_territory(value))
            return false;
        implementation_->q_wave_territory = value;
        return true;
    }

    ecg_q_wave_territory ecg_qa_scenario::q_wave_territory() const
    {
        return implementation_->q_wave_territory;
    }

    bool ecg_qa_scenario::add_rhythm_episode(ecg_rhythm_episode_type type, double start_seconds, double duration_seconds, double transition_seconds, double rate_bpm, unsigned long long seed)
    {
        const bool no_rate = type == ecg_episode_vf || type == ecg_episode_asystole;
        const bool requires_waveform_transition = type == ecg_episode_afib || type == ecg_episode_vf;
        if (!valid_episode_type(type) || !std::isfinite(start_seconds) || !std::isfinite(duration_seconds) || !std::isfinite(transition_seconds) || !std::isfinite(rate_bpm) || start_seconds < 0.0 || start_seconds > 86400.0 || duration_seconds <= 0.0 || duration_seconds > 86400.0 || transition_seconds < 0.0 || transition_seconds > 0.5 * duration_seconds || (requires_waveform_transition && transition_seconds < 0.02) || (no_rate ? rate_bpm != 0.0 : rate_bpm < 10.0 || rate_bpm > 400.0) || implementation_->rhythm_episodes.size() >= clinical_rhythm_episode_max)
            return false;
        const double end_seconds = start_seconds + duration_seconds;
        for (std::size_t index = 0; index < implementation_->rhythm_episodes.size(); ++index)
        {
            const ecg_rhythm_episode& existing = implementation_->rhythm_episodes[index];
            if (start_seconds < existing.start_seconds + existing.duration_seconds && existing.start_seconds < end_seconds)
                return false;
        }
        ecg_rhythm_episode episode = {type, start_seconds, duration_seconds, transition_seconds, rate_bpm, seed};
        implementation_->rhythm_episodes.push_back(episode);
        std::sort(implementation_->rhythm_episodes.begin(), implementation_->rhythm_episodes.end(), [](const ecg_rhythm_episode& left, const ecg_rhythm_episode& right) { return left.start_seconds < right.start_seconds; });
        return true;
    }

    void ecg_qa_scenario::clear_rhythm_episodes()
    {
        implementation_->rhythm_episodes.clear();
    }

    unsigned int ecg_qa_scenario::rhythm_episode_count() const
    {
        return static_cast<unsigned int>(implementation_->rhythm_episodes.size());
    }

    bool ecg_qa_scenario::rhythm_episode(unsigned int index, ecg_rhythm_episode& output) const
    {
        if (index >= implementation_->rhythm_episodes.size())
            return false;
        output = implementation_->rhythm_episodes[index];
        return true;
    }

    bool ecg_qa_scenario::set_flutter_conduction_pattern(ecg_flutter_conduction_pattern value)
    {
        if (!valid_flutter_pattern(value))
            return false;
        implementation_->flutter_conduction_pattern = value;
        return true;
    }

    ecg_flutter_conduction_pattern ecg_qa_scenario::flutter_conduction_pattern() const
    {
        return implementation_->flutter_conduction_pattern;
    }

    bool ecg_qa_scenario::set_pacing_mode(ecg_pacing_mode value)
    {
        if (!valid_pacing_mode(value))
            return false;
        implementation_->pacing_mode = value;
        return true;
    }

    ecg_pacing_mode ecg_qa_scenario::pacing_mode() const
    {
        return implementation_->pacing_mode;
    }

    bool ecg_qa_scenario::set_pacing_non_capture_every_n_beats(unsigned int value)
    {
        if (value == 1)
            return false;
        implementation_->pacing_non_capture_every_n_beats = value;
        return true;
    }

    unsigned int ecg_qa_scenario::pacing_non_capture_every_n_beats() const
    {
        return implementation_->pacing_non_capture_every_n_beats;
    }

    bool ecg_qa_scenario::set_fidelity_policy(ecg_scenario_fidelity_policy value)
    {
        if (!valid_fidelity(value))
            return false;
        implementation_->fidelity_policy = value;
        return true;
    }

    ecg_scenario_fidelity_policy ecg_qa_scenario::fidelity_policy() const
    {
        return implementation_->fidelity_policy;
    }

    unsigned int ecg_qa_scenario::schema_version() const
    {
        return SCENARIO_SCHEMA_VERSION;
    }

    unsigned long long ecg_qa_scenario::fingerprint() const
    {
        unsigned long long hash = 14695981039346656037ULL;
        hash_u64(hash, SCENARIO_SCHEMA_VERSION);
        hash_u64(hash, implementation_->sampling_rate_hz);
        hash_u64(hash, implementation_->seed);
        hash_u64(hash, quantize(implementation_->heart_rate_bpm, 1000.0));
        hash_u64(hash, quantize(implementation_->rr_variability_seconds, 1000000.0));
        if (has_morphology_controls())
        {
            hash_u64(hash, 0x4d4f5250484f5631ULL);
            for (unsigned int index = 0; index < ecg_morphology_control_count; ++index)
            {
                if (implementation_->morphology_enabled[index])
                {
                    hash_u64(hash, index);
                    hash_u64(hash, quantize(implementation_->morphology_values[index], 1000000.0));
                }
            }
        }
        if (!implementation_->morphology_components.empty() || implementation_->fusion_every_n_beats)
        {
            hash_u64(hash, 0x4558544d4f525031ULL);
            hash_u64(hash, implementation_->morphology_components.size());
            for (const ecg_morphology_component& component : implementation_->morphology_components)
            {
                hash_u64(hash, enum_value(component.type));
                hash_u64(hash, component.lead_mask);
                hash_u64(hash, quantize(component.amplitude_mv, 1000000.0));
                hash_u64(hash, quantize(component.offset_ms, 1000.0));
                hash_u64(hash, quantize(component.duration_ms, 1000.0));
            }
            hash_u64(hash, implementation_->fusion_every_n_beats);
            hash_u64(hash, quantize(implementation_->fusion_ventricular_fraction, 1000000.0));
        }
        if (implementation_->qt_adaptation_enabled)
        {
            hash_u64(hash, 0x5154414441505431ULL);
            hash_u64(hash, enum_value(implementation_->qt_adaptation_model));
            hash_u64(hash, quantize(implementation_->qt_adaptation_qtc_ms, 1000.0));
        }
        if (!implementation_->repolarization_episodes.empty())
        {
            hash_u64(hash, 0x5245504f4c5631ULL);
            hash_u64(hash, implementation_->repolarization_episodes.size());
            for (const ecg_repolarization_episode& episode : implementation_->repolarization_episodes)
            {
                hash_u64(hash, enum_value(episode.condition));
                hash_u64(hash, quantize(episode.start_seconds, 1000000.0));
                hash_u64(hash, quantize(episode.duration_seconds, 1000000.0));
                hash_u64(hash, quantize(episode.transition_seconds, 1000000.0));
                hash_u64(hash, quantize(episode.peak_severity, 1000000.0));
            }
        }
        if (implementation_->hrv_modulation_enabled)
        {
            hash_u64(hash, 1);
            hash_u64(hash, quantize(implementation_->hrv_lf_hf_ratio, 1000000.0));
            hash_u64(hash, quantize(implementation_->hrv_lf_center_hz, 1000000.0));
            hash_u64(hash, quantize(implementation_->hrv_lf_bandwidth_hz, 1000000.0));
            hash_u64(hash, quantize(implementation_->hrv_hf_center_hz, 1000000.0));
            hash_u64(hash, quantize(implementation_->hrv_hf_bandwidth_hz, 1000000.0));
            hash_u64(hash, quantize(implementation_->hrv_respiratory_frequency_hz, 1000000.0));
            hash_u64(hash, quantize(implementation_->hrv_respiratory_amplitude_seconds, 1000000.0));
            if (implementation_->hrv_respiratory_phase_radians >= 0.0) hash_u64(hash, quantize(implementation_->hrv_respiratory_phase_radians, 1000000.0));
            hash_u64(hash, quantize(implementation_->activity_start_seconds, 1000000.0));
            hash_u64(hash, quantize(implementation_->activity_duration_seconds, 1000000.0));
            hash_u64(hash, quantize(implementation_->activity_intensity, 1000000.0));
            hash_u64(hash, implementation_->retain_source_channels ? 1u : 0u);
        }
        if (implementation_->minimum_rr_seconds > 0.0 || implementation_->maximum_rr_seconds > 0.0)
        {
            hash_u64(hash, quantize(implementation_->minimum_rr_seconds, 1000000.0));
            hash_u64(hash, quantize(implementation_->maximum_rr_seconds, 1000000.0));
        }
        hash_u64(hash, implementation_->ectopic_every_n_beats);
        hash_u64(hash, enum_value(implementation_->second_degree_pattern));
        hash_u64(hash, enum_value(implementation_->q_wave_territory));
        hash_u64(hash, implementation_->rhythm_episodes.size());
        for (const ecg_rhythm_episode& episode : implementation_->rhythm_episodes)
        {
            hash_u64(hash, enum_value(episode.type));
            hash_u64(hash, quantize(episode.start_seconds, 1000000.0));
            hash_u64(hash, quantize(episode.duration_seconds, 1000000.0));
            hash_u64(hash, quantize(episode.transition_seconds, 1000000.0));
            hash_u64(hash, quantize(episode.rate_bpm, 1000.0));
            hash_u64(hash, episode.seed);
        }
        hash_u64(hash, enum_value(implementation_->flutter_conduction_pattern));
        hash_u64(hash, enum_value(implementation_->pacing_mode));
        hash_u64(hash, implementation_->pacing_non_capture_every_n_beats);
        hash_u64(hash, enum_value(implementation_->fidelity_policy));
        for (const scenario_condition& condition : implementation_->conditions)
        {
            hash_u64(hash, enum_value(condition.code));
            hash_u64(hash, quantize(condition.severity, 1000000.0));
        }
        return hash;
    }

    ecg_scenario_report::ecg_scenario_report()
        : implementation_(new implementation)
    {
    }

    ecg_scenario_report::ecg_scenario_report(const ecg_scenario_report& other)
        : implementation_(new implementation(*other.implementation_))
    {
    }

    ecg_scenario_report& ecg_scenario_report::operator=(const ecg_scenario_report& other)
    {
        if (this != &other)
            *implementation_ = *other.implementation_;
        return *this;
    }

    ecg_scenario_report::~ecg_scenario_report()
    {
        delete implementation_;
    }

    bool ecg_scenario_report::success() const
    {
        return implementation_->success;
    }

    unsigned long long ecg_scenario_report::scenario_fingerprint() const
    {
        return implementation_->fingerprint;
    }

    unsigned int ecg_scenario_report::engine_version() const
    {
        return implementation_->engine_version;
    }

    unsigned long long ecg_scenario_report::run_fingerprint() const
    {
        return implementation_->run_fingerprint;
    }

    unsigned int ecg_scenario_report::effective_condition_count() const
    {
        return static_cast<unsigned int>(implementation_->effective_conditions.size());
    }

    ecg_condition_code ecg_scenario_report::effective_condition(unsigned int index) const
    {
        return index < implementation_->effective_conditions.size() ? implementation_->effective_conditions[index].code : ecg_condition_count;
    }

    double ecg_scenario_report::effective_condition_severity(unsigned int index) const
    {
        return index < implementation_->effective_conditions.size() ? implementation_->effective_conditions[index].severity : 0.0;
    }

    bool ecg_scenario_report::condition_was_inferred(unsigned int index) const
    {
        return index < implementation_->effective_conditions.size() && implementation_->effective_conditions[index].inferred;
    }

    unsigned int ecg_scenario_report::issue_count() const
    {
        return static_cast<unsigned int>(implementation_->issues.size());
    }

    ecg_scenario_issue_severity ecg_scenario_report::issue_severity(unsigned int index) const
    {
        return index < implementation_->issues.size() ? implementation_->issues[index].severity : ecg_issue_error;
    }

    ecg_scenario_issue_code ecg_scenario_report::issue_code(unsigned int index) const
    {
        return index < implementation_->issues.size() ? implementation_->issues[index].code : ecg_issue_none;
    }

    ecg_condition_code ecg_scenario_report::issue_condition(unsigned int index) const
    {
        return index < implementation_->issues.size() ? implementation_->issues[index].condition : ecg_condition_count;
    }

    ecg_condition_code ecg_scenario_report::issue_related_condition(unsigned int index) const
    {
        return index < implementation_->issues.size() ? implementation_->issues[index].related : ecg_condition_count;
    }

    const char* ecg_scenario_report::issue_message(unsigned int index) const
    {
        return index < implementation_->issues.size() ? implementation_->issues[index].message.c_str() : "";
    }

    unsigned int ecg_scenario_report::generated_sample_count() const
    {
        return implementation_->generated_sample_count;
    }

    bool ecg_scenario_report::phenotype_passed() const
    {
        return implementation_->phenotype_passed;
    }

    unsigned int ecg_scenario_report::assertion_count() const
    {
        return static_cast<unsigned int>(implementation_->assertions.size());
    }

    ecg_condition_code ecg_scenario_report::assertion_condition(unsigned int index) const
    {
        return index < implementation_->assertions.size() ? implementation_->assertions[index].condition : ecg_condition_count;
    }

    ecg_phenotype_assertion_code ecg_scenario_report::assertion_code(unsigned int index) const
    {
        return index < implementation_->assertions.size() ? implementation_->assertions[index].code : ecg_assertion_code_count;
    }

    ecg_phenotype_assertion_status ecg_scenario_report::assertion_status(unsigned int index) const
    {
        return index < implementation_->assertions.size() ? implementation_->assertions[index].status : ecg_assertion_not_evaluated;
    }

    double ecg_scenario_report::assertion_measured_value(unsigned int index) const
    {
        return index < implementation_->assertions.size() ? implementation_->assertions[index].measured : 0.0;
    }

    double ecg_scenario_report::assertion_minimum(unsigned int index) const
    {
        return index < implementation_->assertions.size() ? implementation_->assertions[index].minimum : 0.0;
    }

    double ecg_scenario_report::assertion_maximum(unsigned int index) const
    {
        return index < implementation_->assertions.size() ? implementation_->assertions[index].maximum : 0.0;
    }

    const char* ecg_scenario_report::assertion_name(unsigned int index) const
    {
        return index < implementation_->assertions.size() ? implementation_->assertions[index].name.c_str() : "";
    }

    const char* ecg_scenario_report::assertion_unit(unsigned int index) const
    {
        return index < implementation_->assertions.size() ? implementation_->assertions[index].unit.c_str() : "";
    }

    bool ecg_scenario_engine::validate(const ecg_qa_scenario& scenario, ecg_scenario_report& report) const
    {
        ecg_scenario_report::implementation fresh;
        fresh.fingerprint = scenario.fingerprint();
        fresh.run_fingerprint = fresh.fingerprint;
        hash_u64(fresh.run_fingerprint, SCENARIO_ENGINE_VERSION);
        *report.implementation_ = fresh;
        normalize_conditions(*scenario.implementation_, *report.implementation_);
        validate_conditions(*scenario.implementation_, *report.implementation_);
        report.implementation_->success = !report_has_errors(*report.implementation_);
        return report.implementation_->success;
    }

    bool ecg_scenario_engine::compile(const ecg_qa_scenario& scenario, clinical_ecg_config& output, ecg_scenario_report& report) const
    {
        if (!validate(scenario, report))
            return false;
        clinical_ecg_config compiled;
        compile_conditions(*scenario.implementation_, compiled);
        clinical_ecg_generator validation_generator(compiled);
        if (!validation_generator.valid())
        {
            add_issue(*report.implementation_, ecg_issue_error, ecg_issue_invalid_parameter, NO_CONDITION, NO_CONDITION, "Compiled clinical ECG configuration is invalid.");
            report.implementation_->success = false;
            return false;
        }
        output = compiled;
        report.implementation_->success = true;
        return true;
    }

    bool ecg_scenario_engine::generate(const ecg_qa_scenario& scenario, unsigned int sample_count, clinical_ecg_record& output, ecg_scenario_report& report) const
    {
        if (sample_count == 0)
        {
            ecg_scenario_report::implementation fresh;
            fresh.fingerprint = scenario.fingerprint();
            *report.implementation_ = fresh;
            add_issue(*report.implementation_, ecg_issue_error, ecg_issue_invalid_parameter, NO_CONDITION, NO_CONDITION, "Sample count must be greater than zero.");
            return false;
        }
        clinical_ecg_config config;
        if (!compile(scenario, config, report))
            return false;
        const double duration_seconds = static_cast<double>(sample_count) / config.sampling_rate_hz;
        for (std::size_t index = 0; index < scenario.implementation_->rhythm_episodes.size(); ++index)
        {
            const ecg_rhythm_episode& episode = scenario.implementation_->rhythm_episodes[index];
            if (episode.start_seconds + episode.duration_seconds > duration_seconds + 1e-12)
            {
                add_issue(*report.implementation_, ecg_issue_error, ecg_issue_invalid_parameter, NO_CONDITION, NO_CONDITION, "Rhythm episode extends beyond the generated record duration.");
                report.implementation_->success = false;
                return false;
            }
        }
        clinical_ecg_generator generator(config);
        clinical_ecg_record generated;
        if (!generator.generate(sample_count, generated))
        {
            add_issue(*report.implementation_, ecg_issue_error, ecg_issue_generation_failed, NO_CONDITION, NO_CONDITION, "Clinical ECG generation failed.");
            report.implementation_->success = false;
            return false;
        }
        try
        {
            evaluate_phenotype(*scenario.implementation_, generated, *report.implementation_);
            output = generated;
        }
        catch (...)
        {
            add_issue(*report.implementation_, ecg_issue_error, ecg_issue_generation_failed, NO_CONDITION, NO_CONDITION, "Phenotype assertion evaluation failed.");
            report.implementation_->success = false;
            return false;
        }
        report.implementation_->generated_sample_count = sample_count;
        report.implementation_->run_fingerprint = report.implementation_->fingerprint;
        hash_u64(report.implementation_->run_fingerprint, SCENARIO_ENGINE_VERSION);
        hash_u64(report.implementation_->run_fingerprint, sample_count);
        report.implementation_->success = true;
        return true;
    }
}
