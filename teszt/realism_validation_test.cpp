#include "../src/challenge_assembly.h"
#include "../src/ecg_export.h"
#include "../src/realism_validation.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    bool check(bool condition, const char* name)
    {
        if (!condition) std::cerr << "FAIL: " << name << '\n';
        return condition;
    }
}

int main()
{
    bool ok = true;
    signal_synth::ecg_scenario_document document;
    document.scenario_id = "realism_validation";
    document.name = "Realism validation";
    document.description = "Deterministic engineering characterization";
    document.duration_seconds = 12.0;
    document.ecg.set_seed(79001);

    signal_synth::ecg_render_bundle clean;
    signal_synth::ecg_document_render_result render_result;
    ok &= check(signal_synth::render_ecg_document(document, clean, render_result), "render");

    signal_synth::realism_analysis_result clean_result;
    ok &= check(signal_synth::analyze_signal_realism(clean, clean_result), "analyze_clean");
    const signal_synth::realism_metric* finite = clean_result.find("lead_ii_finite_fraction");
    const signal_synth::realism_metric* clean_identity = clean_result.find("einthoven_identity_rms");
    ok &= check(finite && finite->evaluable && finite->value == 1.0, "finite_fraction");
    ok &= check(clean_identity && clean_identity->evaluable && clean_identity->value < 1e-12, "clean_interlead_identity");
    ok &= check(clean_result.find("artifact_coverage_fraction") != 0 && clean_result.find("lead_ii_r_amplitude_cv") != 0, "metric_domains");

    signal_synth::ecg_render_bundle corrupted = clean;
    for (unsigned int sample = 0; sample < corrupted.record.sample_count(); ++sample)
        corrupted.signal_quality.ecg_leads[signal_synth::clinical_lead_iii][sample] += 0.1;
    signal_synth::realism_analysis_result corrupted_result;
    ok &= check(signal_synth::analyze_signal_realism(corrupted, corrupted_result), "analyze_corrupted");
    const signal_synth::realism_metric* corrupted_identity = corrupted_result.find("einthoven_identity_rms");
    ok &= check(corrupted_identity && std::fabs(corrupted_identity->value - 0.1) < 1e-10, "detect_interlead_corruption");

    signal_synth::realism_analysis_options options;
    signal_synth::realism_reference_cohort cohort;
    cohort.id = "engineering_reference";
    cohort.version = "1";
    cohort.source_uri = "https://example.invalid/reference";
    cohort.license = "LicenseRef-Test";
    cohort.content_sha256 = "sha256:0123456789abcdef";
    cohort.inclusion_criteria.push_back("adult test records");
    cohort.exclusions.push_back("records with missing leads");
    options.reference_cohorts.push_back(cohort);
    signal_synth::realism_analysis_result referenced;
    ok &= check(signal_synth::analyze_signal_realism(clean, options, referenced) && referenced.reference_cohorts.size() == 1u, "reference_context");
    const std::string referenced_json = signal_synth::realism_analysis_json(referenced);
    ok &= check(referenced_json.find("\"single_score\":null") != std::string::npos && referenced_json.find("LicenseRef-Test") != std::string::npos && referenced_json.find("records with missing leads") != std::string::npos, "reference_json");

    std::vector<signal_synth::realism_analysis_result> cases;
    cases.push_back(referenced);
    cases.push_back(corrupted_result);
    signal_synth::realism_population_summary population;
    ok &= check(signal_synth::aggregate_realism_population(cases, population) && population.case_count == 2u && !population.metrics.empty(), "population_aggregate");
    const std::string population_json = signal_synth::realism_population_json(population);
    ok &= check(population_json.find("\"contract\":\"synsigra_realism_population_v1\"") != std::string::npos && population_json.find("\"single_score\":null") != std::string::npos, "population_json");

    signal_synth::ecg_export_bundle export_bundle;
    signal_synth::ecg_export_result export_result;
    ok &= check(signal_synth::build_ecg_export_bundle(clean, export_bundle, export_result)
        && export_bundle.find("realism_metrics.json") && export_bundle.find("realism_metrics.csv") && export_bundle.find("realism_report.html"), "export_artifacts");
    ok &= check(signal_synth::challenge_file_role_for_export_artifact("realism_metrics.json") == signal_synth::challenge_file_realism_metrics_json
        && signal_synth::challenge_file_role_for_export_artifact("realism_report.html") == signal_synth::challenge_file_realism_report_html, "challenge_roles");
    return ok ? 0 : 1;
}
