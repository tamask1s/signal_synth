#pragma once

#include <string>
#include <vector>

namespace signal_synth
{
    struct ecg_render_bundle;

    struct realism_reference_cohort
    {
        std::string id;
        std::string version;
        std::string source_uri;
        std::string license;
        std::string content_sha256;
        std::vector<std::string> inclusion_criteria;
        std::vector<std::string> exclusions;
    };

    struct realism_analysis_options
    {
        std::vector<realism_reference_cohort> reference_cohorts;
    };

    enum realism_metric_domain
    {
        realism_fidelity = 0,
        realism_diversity = 1,
        realism_consistency = 2,
        realism_cross_signal = 3,
        realism_downstream_utility = 4
    };

    const char* realism_metric_domain_name(realism_metric_domain domain);

    struct realism_metric
    {
        realism_metric();

        std::string name;
        realism_metric_domain domain;
        std::string unit;
        bool evaluable;
        double value;
    };

    struct realism_analysis_result
    {
        realism_analysis_result();

        std::string contract;
        std::string render_identity;
        std::string reference_kind;
        std::vector<realism_reference_cohort> reference_cohorts;
        std::vector<realism_metric> metrics;
        std::vector<std::string> messages;

        const realism_metric* find(const std::string& name) const;
    };

    struct realism_population_metric
    {
        realism_population_metric();

        std::string name;
        realism_metric_domain domain;
        std::string unit;
        unsigned int count;
        double minimum;
        double maximum;
        double mean;
        double standard_deviation;
    };

    struct realism_population_summary
    {
        realism_population_summary();

        std::string contract;
        unsigned int case_count;
        std::vector<realism_reference_cohort> reference_cohorts;
        std::vector<realism_population_metric> metrics;
    };

    bool analyze_signal_realism(const ecg_render_bundle& render, realism_analysis_result& output);
    bool analyze_signal_realism(const ecg_render_bundle& render, const realism_analysis_options& options, realism_analysis_result& output);
    bool aggregate_realism_population(const std::vector<realism_analysis_result>& cases, realism_population_summary& output);
    std::string realism_analysis_json(const realism_analysis_result& result);
    std::string realism_analysis_csv(const realism_analysis_result& result);
    std::string realism_analysis_html(const realism_analysis_result& result);
    std::string realism_population_json(const realism_population_summary& summary);
}
