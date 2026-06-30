#include "ecg_scenario.h"

#include "clinical_ecg.h"

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
        const unsigned int SCENARIO_SCHEMA_VERSION = 1;
        const unsigned int SCENARIO_ENGINE_VERSION = 1;
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

        // Catalog order and statement names follow PTB-XL 1.0.3 scp_statements.csv (CC BY 4.0).
        const ecg_condition_info condition_catalog[ecg_condition_count] = {
            {ecg_condition_ndt, "NDT", "non-diagnostic T abnormalities", ecg_category_ischemia_repolarization, true, true, false, ecg_support_catalog_only},
            {ecg_condition_nst, "NST_", "non-specific ST changes", ecg_category_ischemia_repolarization, true, true, false, ecg_support_catalog_only},
            {ecg_condition_dig, "DIG", "digitalis effect", ecg_category_ischemia_repolarization, true, true, false, ecg_support_catalog_only},
            {ecg_condition_lngqt, "LNGQT", "long QT interval", ecg_category_ischemia_repolarization, true, true, false, ecg_support_parameterized},
            {ecg_condition_norm, "NORM", "normal ECG", ecg_category_normal, true, false, false, ecg_support_native},
            {ecg_condition_imi, "IMI", "inferior myocardial infarction", ecg_category_infarction_injury, true, false, false, ecg_support_catalog_only},
            {ecg_condition_asmi, "ASMI", "anteroseptal myocardial infarction", ecg_category_infarction_injury, true, false, false, ecg_support_catalog_only},
            {ecg_condition_lvh, "LVH", "left ventricular hypertrophy", ecg_category_hypertrophy, true, false, false, ecg_support_catalog_only},
            {ecg_condition_lafb, "LAFB", "left anterior fascicular block", ecg_category_conduction, true, false, false, ecg_support_catalog_only},
            {ecg_condition_isc, "ISC_", "non-specific ischemic ST-T changes", ecg_category_ischemia_repolarization, true, false, false, ecg_support_catalog_only},
            {ecg_condition_irbbb, "IRBBB", "incomplete right bundle branch block", ecg_category_conduction, true, false, false, ecg_support_catalog_only},
            {ecg_condition_1avb, "1AVB", "first degree AV block", ecg_category_conduction, true, false, false, ecg_support_native},
            {ecg_condition_ivcd, "IVCD", "non-specific intraventricular conduction disturbance", ecg_category_conduction, true, false, false, ecg_support_catalog_only},
            {ecg_condition_iscal, "ISCAL", "ischemic ST-T changes in anterolateral leads", ecg_category_ischemia_repolarization, true, false, false, ecg_support_catalog_only},
            {ecg_condition_crbbb, "CRBBB", "complete right bundle branch block", ecg_category_conduction, true, false, false, ecg_support_parameterized},
            {ecg_condition_clbbb, "CLBBB", "complete left bundle branch block", ecg_category_conduction, true, false, false, ecg_support_parameterized},
            {ecg_condition_ilmi, "ILMI", "inferolateral myocardial infarction", ecg_category_infarction_injury, true, false, false, ecg_support_catalog_only},
            {ecg_condition_lao_lae, "LAO/LAE", "left atrial overload or enlargement", ecg_category_hypertrophy, true, false, false, ecg_support_catalog_only},
            {ecg_condition_ami, "AMI", "anterior myocardial infarction", ecg_category_infarction_injury, true, false, false, ecg_support_catalog_only},
            {ecg_condition_almi, "ALMI", "anterolateral myocardial infarction", ecg_category_infarction_injury, true, false, false, ecg_support_catalog_only},
            {ecg_condition_iscin, "ISCIN", "ischemic ST-T changes in inferior leads", ecg_category_ischemia_repolarization, true, false, false, ecg_support_catalog_only},
            {ecg_condition_injas, "INJAS", "subendocardial injury in anteroseptal leads", ecg_category_infarction_injury, true, false, false, ecg_support_catalog_only},
            {ecg_condition_lmi, "LMI", "lateral myocardial infarction", ecg_category_infarction_injury, true, false, false, ecg_support_catalog_only},
            {ecg_condition_iscil, "ISCIL", "ischemic ST-T changes in inferolateral leads", ecg_category_ischemia_repolarization, true, false, false, ecg_support_catalog_only},
            {ecg_condition_lpfb, "LPFB", "left posterior fascicular block", ecg_category_conduction, true, false, false, ecg_support_catalog_only},
            {ecg_condition_iscas, "ISCAS", "ischemic ST-T changes in anteroseptal leads", ecg_category_ischemia_repolarization, true, false, false, ecg_support_catalog_only},
            {ecg_condition_injal, "INJAL", "subendocardial injury in anterolateral leads", ecg_category_infarction_injury, true, false, false, ecg_support_catalog_only},
            {ecg_condition_iscla, "ISCLA", "ischemic ST-T changes in lateral leads", ecg_category_ischemia_repolarization, true, false, false, ecg_support_catalog_only},
            {ecg_condition_rvh, "RVH", "right ventricular hypertrophy", ecg_category_hypertrophy, true, false, false, ecg_support_catalog_only},
            {ecg_condition_aneur, "ANEUR", "ST-T changes compatible with ventricular aneurysm", ecg_category_ischemia_repolarization, true, false, false, ecg_support_catalog_only},
            {ecg_condition_rao_rae, "RAO/RAE", "right atrial overload or enlargement", ecg_category_hypertrophy, true, false, false, ecg_support_catalog_only},
            {ecg_condition_el, "EL", "electrolytic disturbance or drug effect", ecg_category_ischemia_repolarization, true, false, false, ecg_support_catalog_only},
            {ecg_condition_wpw, "WPW", "Wolff-Parkinson-White syndrome", ecg_category_conduction, true, false, false, ecg_support_catalog_only},
            {ecg_condition_ilbbb, "ILBBB", "incomplete left bundle branch block", ecg_category_conduction, true, false, false, ecg_support_catalog_only},
            {ecg_condition_iplmi, "IPLMI", "inferoposterolateral myocardial infarction", ecg_category_infarction_injury, true, false, false, ecg_support_catalog_only},
            {ecg_condition_iscan, "ISCAN", "ischemic ST-T changes in anterior leads", ecg_category_ischemia_repolarization, true, false, false, ecg_support_catalog_only},
            {ecg_condition_ipmi, "IPMI", "inferoposterior myocardial infarction", ecg_category_infarction_injury, true, false, false, ecg_support_catalog_only},
            {ecg_condition_sehyp, "SEHYP", "septal hypertrophy", ecg_category_hypertrophy, true, false, false, ecg_support_catalog_only},
            {ecg_condition_injin, "INJIN", "subendocardial injury in inferior leads", ecg_category_infarction_injury, true, false, false, ecg_support_catalog_only},
            {ecg_condition_injla, "INJLA", "subendocardial injury in lateral leads", ecg_category_infarction_injury, true, false, false, ecg_support_catalog_only},
            {ecg_condition_pmi, "PMI", "posterior myocardial infarction", ecg_category_infarction_injury, true, false, false, ecg_support_catalog_only},
            {ecg_condition_3avb, "3AVB", "third degree AV block", ecg_category_conduction, true, false, false, ecg_support_native},
            {ecg_condition_injil, "INJIL", "subendocardial injury in inferolateral leads", ecg_category_infarction_injury, true, false, false, ecg_support_catalog_only},
            {ecg_condition_2avb, "2AVB", "second degree AV block", ecg_category_conduction, true, false, false, ecg_support_native},
            {ecg_condition_abqrs, "ABQRS", "abnormal QRS", ecg_category_morphology, false, true, false, ecg_support_catalog_only},
            {ecg_condition_pvc, "PVC", "ventricular premature complex", ecg_category_rhythm, false, true, false, ecg_support_native},
            {ecg_condition_std, "STD_", "non-specific ST depression", ecg_category_ischemia_repolarization, false, true, false, ecg_support_catalog_only},
            {ecg_condition_vclvh, "VCLVH", "voltage criteria for left ventricular hypertrophy", ecg_category_hypertrophy, false, true, false, ecg_support_catalog_only},
            {ecg_condition_qwave, "QWAVE", "Q waves present", ecg_category_morphology, false, true, false, ecg_support_catalog_only},
            {ecg_condition_lowt, "LOWT", "low amplitude T waves", ecg_category_ischemia_repolarization, false, true, false, ecg_support_catalog_only},
            {ecg_condition_nt, "NT_", "non-specific T-wave changes", ecg_category_ischemia_repolarization, false, true, false, ecg_support_catalog_only},
            {ecg_condition_pac, "PAC", "atrial premature complex", ecg_category_rhythm, false, true, false, ecg_support_native},
            {ecg_condition_lpr, "LPR", "prolonged PR interval", ecg_category_conduction, false, true, false, ecg_support_parameterized},
            {ecg_condition_invt, "INVT", "inverted T waves", ecg_category_ischemia_repolarization, false, true, false, ecg_support_catalog_only},
            {ecg_condition_lvolt, "LVOLT", "low QRS voltages", ecg_category_morphology, false, true, false, ecg_support_catalog_only},
            {ecg_condition_hvolt, "HVOLT", "high QRS voltage", ecg_category_morphology, false, true, false, ecg_support_catalog_only},
            {ecg_condition_tab, "TAB_", "T-wave abnormality", ecg_category_ischemia_repolarization, false, true, false, ecg_support_catalog_only},
            {ecg_condition_ste, "STE_", "non-specific ST elevation", ecg_category_ischemia_repolarization, false, true, false, ecg_support_catalog_only},
            {ecg_condition_prc, "PRC(S)", "premature complexes", ecg_category_rhythm, false, true, false, ecg_support_parameterized},
            {ecg_condition_sr, "SR", "sinus rhythm", ecg_category_rhythm, false, false, true, ecg_support_native},
            {ecg_condition_afib, "AFIB", "atrial fibrillation", ecg_category_rhythm, false, false, true, ecg_support_native},
            {ecg_condition_stach, "STACH", "sinus tachycardia", ecg_category_rhythm, false, false, true, ecg_support_parameterized},
            {ecg_condition_sarrh, "SARRH", "sinus arrhythmia", ecg_category_rhythm, false, false, true, ecg_support_parameterized},
            {ecg_condition_sbrad, "SBRAD", "sinus bradycardia", ecg_category_rhythm, false, false, true, ecg_support_parameterized},
            {ecg_condition_pace, "PACE", "normal functioning artificial pacemaker", ecg_category_rhythm, false, false, true, ecg_support_native},
            {ecg_condition_svarr, "SVARR", "supraventricular arrhythmia", ecg_category_rhythm, false, false, true, ecg_support_catalog_only},
            {ecg_condition_bigu, "BIGU", "bigeminal pattern", ecg_category_rhythm, false, false, true, ecg_support_parameterized},
            {ecg_condition_aflt, "AFLT", "atrial flutter", ecg_category_rhythm, false, false, true, ecg_support_native},
            {ecg_condition_svtac, "SVTAC", "supraventricular tachycardia", ecg_category_rhythm, false, false, true, ecg_support_native},
            {ecg_condition_psvt, "PSVT", "paroxysmal supraventricular tachycardia", ecg_category_rhythm, false, false, true, ecg_support_catalog_only},
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
            return code == ecg_condition_lngqt || code == ecg_condition_1avb || code == ecg_condition_lpr || code == ecg_condition_pac || code == ecg_condition_pvc || code == ecg_condition_stach || code == ecg_condition_sarrh || code == ecg_condition_sbrad;
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
        unsigned int ectopic_every_n_beats;
        ecg_second_degree_av_pattern second_degree_pattern;
        ecg_scenario_fidelity_policy fidelity_policy;

        implementation()
            : sampling_rate_hz(500), seed(DEFAULT_SEED), heart_rate_bpm(0.0), rr_variability_seconds(0.0), ectopic_every_n_beats(0), second_degree_pattern(ecg_second_degree_unspecified), fidelity_policy(ecg_fidelity_allow_parameterized)
        {
        }
    };

    struct ecg_scenario_report::implementation
    {
        bool success;
        unsigned long long fingerprint;
        std::vector<effective_condition_entry> effective_conditions;
        std::vector<report_issue> issues;
        unsigned int generated_sample_count;
        unsigned int engine_version;
        unsigned long long run_fingerprint;

        implementation()
            : success(false), fingerprint(0), generated_sample_count(0), engine_version(SCENARIO_ENGINE_VERSION), run_fingerprint(0)
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
            if (has_condition(scenario.conditions, ecg_condition_norm) || has_condition(scenario.conditions, ecg_condition_stach) || has_condition(scenario.conditions, ecg_condition_sbrad) || has_condition(scenario.conditions, ecg_condition_sarrh) || has_condition(scenario.conditions, ecg_condition_1avb) || has_condition(scenario.conditions, ecg_condition_2avb) || has_condition(scenario.conditions, ecg_condition_3avb) || has_condition(scenario.conditions, ecg_condition_lpr))
                add_effective(report.effective_conditions, ecg_condition_sr, 1.0, true);
            if (has_condition(scenario.conditions, ecg_condition_clbbb) || has_condition(scenario.conditions, ecg_condition_crbbb))
            {
                const ecg_condition_code block = has_condition(scenario.conditions, ecg_condition_clbbb) ? ecg_condition_clbbb : ecg_condition_crbbb;
                add_effective(report.effective_conditions, ecg_condition_abqrs, condition_severity(scenario.conditions, block), true);
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
            for (unsigned int modifier = 0; modifier < sizeof(sinus_modifiers) / sizeof(sinus_modifiers[0]); ++modifier)
                for (unsigned int rhythm = 0; rhythm < sizeof(non_sinus_rhythms) / sizeof(non_sinus_rhythms[0]); ++rhythm)
                    add_conflict(report, scenario.conditions, sinus_modifiers[modifier], non_sinus_rhythms[rhythm], "Sinus rhythm modifiers cannot be combined with a non-sinus primary rhythm.");

            const ecg_condition_code av_blocks[] = {ecg_condition_1avb, ecg_condition_2avb, ecg_condition_3avb};
            for (unsigned int left = 0; left < sizeof(av_blocks) / sizeof(av_blocks[0]); ++left)
            {
                for (unsigned int right = left + 1; right < sizeof(av_blocks) / sizeof(av_blocks[0]); ++right)
                    add_conflict(report, scenario.conditions, av_blocks[left], av_blocks[right], "AV block degrees are mutually exclusive.");
                for (unsigned int rhythm = 0; rhythm < sizeof(non_sinus_rhythms) / sizeof(non_sinus_rhythms[0]); ++rhythm)
                    add_conflict(report, scenario.conditions, av_blocks[left], non_sinus_rhythms[rhythm], "The current AV block model requires a sinus atrial timeline.");
                add_conflict(report, scenario.conditions, av_blocks[left], ecg_condition_pac, "The current AV block timeline cannot compose periodic ectopy.");
                add_conflict(report, scenario.conditions, av_blocks[left], ecg_condition_pvc, "The current AV block timeline cannot compose periodic ectopy.");
            }
            add_conflict(report, scenario.conditions, ecg_condition_sarrh, ecg_condition_1avb, "The current AV block timeline does not apply beat-wise sinus arrhythmia.");
            add_conflict(report, scenario.conditions, ecg_condition_sarrh, ecg_condition_2avb, "The current AV block timeline does not apply beat-wise sinus arrhythmia.");
            add_conflict(report, scenario.conditions, ecg_condition_sarrh, ecg_condition_3avb, "The current AV block timeline does not apply beat-wise sinus arrhythmia.");
            add_conflict(report, scenario.conditions, ecg_condition_lpr, ecg_condition_3avb, "A prolonged PR interval is undefined during complete AV dissociation.");
            add_conflict(report, scenario.conditions, ecg_condition_clbbb, ecg_condition_crbbb, "Complete left and right bundle branch blocks cannot share the current single conduction model.");
            add_conflict(report, scenario.conditions, ecg_condition_bigu, ecg_condition_trigu, "Bigeminal and trigeminal patterns are mutually exclusive.");
            add_conflict(report, scenario.conditions, ecg_condition_pac, ecg_condition_pvc, "The current periodic ectopy scenario supports one ectopic origin.");
            for (unsigned int rhythm = 0; rhythm < sizeof(non_sinus_rhythms) / sizeof(non_sinus_rhythms[0]); ++rhythm)
            {
                add_conflict(report, scenario.conditions, ecg_condition_pac, non_sinus_rhythms[rhythm], "The current ectopy scenario requires sinus rhythm.");
                add_conflict(report, scenario.conditions, ecg_condition_pvc, non_sinus_rhythms[rhythm], "The current ectopy scenario requires sinus rhythm.");
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
            if (!ectopic_origin && scenario.ectopic_every_n_beats != 0)
                add_issue(report, ecg_issue_error, ecg_issue_missing_requirement, NO_CONDITION, ecg_condition_prc, "ectopic_every_n_beats requires PAC or PVC.");

            if (scenario.heart_rate_bpm > 0.0)
            {
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

        void compile_conditions(const ecg_qa_scenario::implementation& scenario, clinical_ecg_config& config)
        {
            config.sampling_rate_hz = scenario.sampling_rate_hz;
            config.rhythm.seed = scenario.seed;
            config.rhythm.rr_variability_seconds = scenario.rr_variability_seconds;
            if (scenario.heart_rate_bpm > 0.0)
                config.rhythm.heart_rate_bpm = scenario.heart_rate_bpm;

            if (has_condition(scenario.conditions, ecg_condition_afib))
                config.rhythm.rhythm = clinical_rhythm_atrial_fibrillation;
            else if (has_condition(scenario.conditions, ecg_condition_aflt))
            {
                config.rhythm.rhythm = clinical_rhythm_atrial_flutter;
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
                config.rhythm.rhythm = clinical_rhythm_paced;
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

            if (has_condition(scenario.conditions, ecg_condition_lngqt))
                config.timing.qtc_ms = 440.0 + 80.0 * condition_severity(scenario.conditions, ecg_condition_lngqt);

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
        return valid_condition(code) ? &condition_catalog[enum_value(code)] : 0;
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
        hash_u64(hash, implementation_->ectopic_every_n_beats);
        hash_u64(hash, enum_value(implementation_->second_degree_pattern));
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
        clinical_ecg_generator generator(config);
        if (!generator.generate(sample_count, output))
        {
            add_issue(*report.implementation_, ecg_issue_error, ecg_issue_generation_failed, NO_CONDITION, NO_CONDITION, "Clinical ECG generation failed.");
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
