#include "realism_validation.h"

#include "ecg_render.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <map>
#include <sstream>

namespace
{
    const double pi = 3.141592653589793238462643383279502884;

    std::string json_string(const std::string& value)
    {
        std::ostringstream output;
        output << '"';
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            const unsigned char c = static_cast<unsigned char>(value[i]);
            if (c == '"' || c == '\\') output << '\\' << static_cast<char>(c);
            else if (c == '\n') output << "\\n";
            else if (c == '\r') output << "\\r";
            else if (c == '\t') output << "\\t";
            else if (c < 0x20u) output << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<unsigned int>(c) << std::dec;
            else output << static_cast<char>(c);
        }
        output << '"';
        return output.str();
    }

    std::string html_text(const std::string& value)
    {
        std::string output;
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            if (value[i] == '&') output += "&amp;";
            else if (value[i] == '<') output += "&lt;";
            else if (value[i] == '>') output += "&gt;";
            else if (value[i] == '"') output += "&quot;";
            else output += value[i];
        }
        return output;
    }

    std::string number_text(double value, unsigned int precision)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(precision) << (value == 0.0 ? 0.0 : value);
        return output.str();
    }

    const double* final_lead(const signal_synth::ecg_render_bundle& render, unsigned int lead)
    {
        return lead < render.signal_quality.ecg_leads.size() && render.signal_quality.ecg_leads[lead].size() == render.record.sample_count()
            ? &render.signal_quality.ecg_leads[lead][0] : render.record.lead_data(lead);
    }

    void add_metric(signal_synth::realism_analysis_result& result, const char* name, signal_synth::realism_metric_domain domain, const char* unit, bool evaluable, double value)
    {
        signal_synth::realism_metric metric;
        metric.name = name;
        metric.domain = domain;
        metric.unit = unit;
        metric.evaluable = evaluable && std::isfinite(value);
        metric.value = metric.evaluable ? value : 0.0;
        result.metrics.push_back(metric);
    }

    double rms(const double* samples, unsigned int count)
    {
        if (!samples || !count) return 0.0;
        long double sum = 0.0;
        for (unsigned int i = 0; i < count; ++i) sum += static_cast<long double>(samples[i]) * samples[i];
        return std::sqrt(static_cast<double>(sum / count));
    }

    double standard_deviation(const std::vector<double>& values)
    {
        if (values.size() < 2u) return 0.0;
        long double sum = 0.0;
        for (std::size_t i = 0; i < values.size(); ++i) sum += values[i];
        const long double mean = sum / values.size();
        long double squared = 0.0;
        for (std::size_t i = 0; i < values.size(); ++i)
        {
            const long double difference = values[i] - mean;
            squared += difference * difference;
        }
        return std::sqrt(static_cast<double>(squared / values.size()));
    }

    double coefficient_of_variation(const std::vector<double>& values)
    {
        if (values.empty()) return 0.0;
        long double sum = 0.0;
        for (std::size_t i = 0; i < values.size(); ++i) sum += values[i];
        const double mean = static_cast<double>(sum / values.size());
        return std::fabs(mean) > 1e-15 ? standard_deviation(values) / std::fabs(mean) : 0.0;
    }

    double spectral_power(const double* samples, unsigned int count, unsigned int sample_rate_hz, double frequency_hz)
    {
        if (!samples || !count || !sample_rate_hz || frequency_hz <= 0.0 || frequency_hz >= sample_rate_hz * 0.5) return 0.0;
        long double real = 0.0, imaginary = 0.0;
        for (unsigned int i = 0; i < count; ++i)
        {
            const double phase = 2.0 * pi * frequency_hz * i / sample_rate_hz;
            real += samples[i] * std::cos(phase);
            imaginary -= samples[i] * std::sin(phase);
        }
        return static_cast<double>((real * real + imaginary * imaginary) / (static_cast<long double>(count) * count));
    }

    void add_interlead_metrics(const signal_synth::ecg_render_bundle& render, signal_synth::realism_analysis_result& result)
    {
        const unsigned int count = render.record.sample_count();
        const double* lead_i = final_lead(render, signal_synth::clinical_lead_i);
        const double* lead_ii = final_lead(render, signal_synth::clinical_lead_ii);
        const double* lead_iii = final_lead(render, signal_synth::clinical_lead_iii);
        const double* avr = final_lead(render, signal_synth::clinical_lead_avr);
        const double* avl = final_lead(render, signal_synth::clinical_lead_avl);
        const double* avf = final_lead(render, signal_synth::clinical_lead_avf);
        long double einthoven = 0.0, goldberger = 0.0;
        for (unsigned int i = 0; i < count; ++i)
        {
            const double e = lead_ii[i] - lead_i[i] - lead_iii[i];
            einthoven += e * e;
            const double r1 = avr[i] + 0.5 * (lead_i[i] + lead_ii[i]);
            const double r2 = avl[i] - lead_i[i] + 0.5 * lead_ii[i];
            const double r3 = avf[i] - lead_ii[i] + 0.5 * lead_i[i];
            goldberger += (r1 * r1 + r2 * r2 + r3 * r3) / 3.0;
        }
        add_metric(result, "einthoven_identity_rms", signal_synth::realism_consistency, "mV", count != 0, count ? std::sqrt(static_cast<double>(einthoven / count)) : 0.0);
        add_metric(result, "goldberger_identity_rms", signal_synth::realism_consistency, "mV", count != 0, count ? std::sqrt(static_cast<double>(goldberger / count)) : 0.0);
    }

    void add_morphology_diversity(const signal_synth::ecg_render_bundle& render, signal_synth::realism_analysis_result& result)
    {
        std::vector<double> r, t, qrs;
        const signal_synth::ecg_lead_morphology* entries = render.morphology.entries();
        for (unsigned int i = 0; entries && i < render.morphology.entry_count(); ++i)
            if (entries[i].lead_index == signal_synth::clinical_lead_ii)
            {
                r.push_back(entries[i].r_amplitude_mv);
                qrs.push_back(entries[i].qrs_rms_mv);
                if (entries[i].t_present) t.push_back(entries[i].t_amplitude_mv);
            }
        add_metric(result, "lead_ii_r_amplitude_cv", signal_synth::realism_diversity, "ratio", r.size() > 1u, coefficient_of_variation(r));
        add_metric(result, "lead_ii_qrs_rms_cv", signal_synth::realism_diversity, "ratio", qrs.size() > 1u, coefficient_of_variation(qrs));
        add_metric(result, "lead_ii_t_amplitude_cv", signal_synth::realism_diversity, "ratio", t.size() > 1u, coefficient_of_variation(t));
    }

    void add_cross_signal_metrics(const signal_synth::ecg_render_bundle& render, signal_synth::realism_analysis_result& result)
    {
        std::vector<double> onset, peak, amplitude;
        const signal_synth::ppg_pulse_annotation* pulses = render.ppg.pulses();
        for (unsigned int i = 0; pulses && i < render.ppg.pulse_count(); ++i)
            if (pulses[i].generated && !pulses[i].intentionally_missing && pulses[i].state != signal_synth::ppg_pulse_out_of_record)
            {
                onset.push_back(pulses[i].expected_onset_time_seconds - pulses[i].ecg_r_time_seconds);
                amplitude.push_back(pulses[i].effective_amplitude_au);
            }
        const signal_synth::ppg_annotation* annotations = render.ppg.annotations();
        for (unsigned int i = 0; annotations && i < render.ppg.annotation_count(); ++i)
            if (annotations[i].kind == signal_synth::ppg_systolic_peak && annotations[i].source == signal_synth::ppg_fiducial_measurement)
                peak.push_back(annotations[i].time_seconds - annotations[i].ecg_r_time_seconds);
        add_metric(result, "ecg_ppg_onset_delay_std", signal_synth::realism_cross_signal, "s", onset.size() > 1u, standard_deviation(onset));
        add_metric(result, "ecg_ppg_peak_delay_std", signal_synth::realism_cross_signal, "s", peak.size() > 1u, standard_deviation(peak));
        add_metric(result, "ppg_pulse_amplitude_cv", signal_synth::realism_cross_signal, "ratio", amplitude.size() > 1u, coefficient_of_variation(amplitude));
    }

    void add_wearable_metrics(const signal_synth::ecg_render_bundle& render, signal_synth::realism_analysis_result& result)
    {
        for (std::size_t i = 0; i < render.wearable.streams.size(); ++i)
        {
            const signal_synth::wearable_stream_record& stream = render.wearable.streams[i];
            const std::string prefix = std::string("wearable_") + signal_synth::wearable_stream_kind_name(stream.kind);
            const double loss = stream.sample_count() ? 1.0 - static_cast<double>(stream.received_sample_count()) / stream.sample_count() : 0.0;
            add_metric(result, (prefix + "_sample_loss_fraction").c_str(), signal_synth::realism_downstream_utility, "ratio", stream.sample_count() != 0, loss);
            const double final_error = stream.samples.empty() ? 0.0 : stream.samples.back().reported_device_time_seconds - stream.samples.back().latent_time_seconds;
            add_metric(result, (prefix + "_final_clock_error").c_str(), signal_synth::realism_consistency, "s", !stream.samples.empty(), final_error);
        }
    }

    std::string csv_cell(const std::string& value)
    {
        if (value.find_first_of(",\"\r\n") == std::string::npos) return value;
        std::string output = "\"";
        for (std::size_t i = 0; i < value.size(); ++i) output += (value[i] == '"' ? "\"\"" : std::string(1, value[i]));
        return output + '"';
    }

    void write_string_array(std::ostringstream& output, const std::vector<std::string>& values)
    {
        output << '[';
        for (std::size_t i = 0; i < values.size(); ++i) output << (i ? "," : "") << json_string(values[i]);
        output << ']';
    }

    void write_reference_cohorts(std::ostringstream& output, const std::vector<signal_synth::realism_reference_cohort>& cohorts)
    {
        output << '[';
        for (std::size_t i = 0; i < cohorts.size(); ++i)
        {
            const signal_synth::realism_reference_cohort& cohort = cohorts[i];
            output << (i ? "," : "") << "{\"id\":" << json_string(cohort.id) << ",\"version\":" << json_string(cohort.version)
                   << ",\"source_uri\":" << json_string(cohort.source_uri) << ",\"license\":" << json_string(cohort.license)
                   << ",\"content_sha256\":" << json_string(cohort.content_sha256) << ",\"inclusion_criteria\":";
            write_string_array(output, cohort.inclusion_criteria);
            output << ",\"exclusions\":";
            write_string_array(output, cohort.exclusions);
            output << '}';
        }
        output << ']';
    }

    bool same_reference(const signal_synth::realism_reference_cohort& left, const signal_synth::realism_reference_cohort& right)
    {
        return left.id == right.id && left.version == right.version && left.content_sha256 == right.content_sha256;
    }
}

namespace signal_synth
{
    const char* realism_metric_domain_name(realism_metric_domain domain)
    {
        switch (domain)
        {
        case realism_fidelity: return "fidelity";
        case realism_diversity: return "diversity";
        case realism_consistency: return "consistency";
        case realism_cross_signal: return "cross_signal";
        case realism_downstream_utility: return "downstream_utility";
        }
        return "unknown";
    }

    realism_metric::realism_metric() : name(), domain(realism_fidelity), unit(), evaluable(false), value(0.0) {}
    realism_analysis_result::realism_analysis_result() : contract("synsigra_realism_metrics_v1"), render_identity(), reference_kind("synthetic_internal_ground_truth"), reference_cohorts(), metrics(), messages() {}
    realism_population_metric::realism_population_metric() : name(), domain(realism_fidelity), unit(), count(0), minimum(0.0), maximum(0.0), mean(0.0), standard_deviation(0.0) {}
    realism_population_summary::realism_population_summary() : contract("synsigra_realism_population_v1"), case_count(0), reference_cohorts(), metrics() {}

    const realism_metric* realism_analysis_result::find(const std::string& name) const
    {
        for (std::size_t i = 0; i < metrics.size(); ++i) if (metrics[i].name == name) return &metrics[i];
        return 0;
    }

    bool analyze_signal_realism(const ecg_render_bundle& render, realism_analysis_result& output)
    {
        return analyze_signal_realism(render, realism_analysis_options(), output);
    }

    bool analyze_signal_realism(const ecg_render_bundle& render, const realism_analysis_options& options, realism_analysis_result& output)
    {
        realism_analysis_result fresh;
        if (!render.record.sample_count() || render.record.lead_count() != clinical_lead_count) return false;
        fresh.render_identity = render.render_identity;
        fresh.reference_cohorts = options.reference_cohorts;
        if (fresh.reference_cohorts.empty()) fresh.messages.push_back("No external reference cohort supplied; metrics characterize generated signals only.");
        const double* lead_ii = final_lead(render, clinical_lead_ii);
        const unsigned int count = render.record.sample_count();
        unsigned int finite_count = 0;
        double minimum = 0.0, maximum = 0.0;
        bool have_finite = false;
        for (unsigned int i = 0; i < count; ++i)
        {
            if (!std::isfinite(lead_ii[i])) continue;
            ++finite_count;
            if (!have_finite) { minimum = maximum = lead_ii[i]; have_finite = true; }
            else { minimum = std::min(minimum, lead_ii[i]); maximum = std::max(maximum, lead_ii[i]); }
        }
        add_metric(fresh, "lead_ii_finite_fraction", realism_fidelity, "ratio", true, static_cast<double>(finite_count) / count);
        add_metric(fresh, "lead_ii_rms", realism_fidelity, "mV", finite_count == count, rms(lead_ii, count));
        add_metric(fresh, "lead_ii_peak_to_peak", realism_fidelity, "mV", have_finite, maximum - minimum);
        const double frequencies[] = {0.2, 1.0, 10.0, 25.0, 50.0};
        const char* names[] = {"lead_ii_power_0_2_hz", "lead_ii_power_1_hz", "lead_ii_power_10_hz", "lead_ii_power_25_hz", "lead_ii_power_50_hz"};
        for (unsigned int i = 0; i < 5u; ++i)
            add_metric(fresh, names[i], realism_fidelity, "mV2", frequencies[i] < render.record.sampling_rate_hz() * 0.5, spectral_power(lead_ii, count, render.record.sampling_rate_hz(), frequencies[i]));
        add_interlead_metrics(render, fresh);
        add_morphology_diversity(render, fresh);
        add_cross_signal_metrics(render, fresh);
        add_wearable_metrics(render, fresh);
        const double duration = static_cast<double>(count) / render.record.sampling_rate_hz();
        add_metric(fresh, "artifact_coverage_fraction", realism_downstream_utility, "ratio", duration > 0.0, duration > 0.0 ? render.metrics.total_artifact_seconds / duration : 0.0);
        output = fresh;
        return true;
    }

    bool aggregate_realism_population(const std::vector<realism_analysis_result>& cases, realism_population_summary& output)
    {
        realism_population_summary fresh;
        if (cases.empty()) return false;
        struct accumulator { realism_metric_domain domain; std::string unit; std::vector<double> values; };
        std::map<std::string, accumulator> values;
        for (std::size_t c = 0; c < cases.size(); ++c)
        {
            for (std::size_t r = 0; r < cases[c].reference_cohorts.size(); ++r)
            {
                bool found = false;
                for (std::size_t existing = 0; existing < fresh.reference_cohorts.size(); ++existing)
                    found = found || same_reference(fresh.reference_cohorts[existing], cases[c].reference_cohorts[r]);
                if (!found) fresh.reference_cohorts.push_back(cases[c].reference_cohorts[r]);
            }
            for (std::size_t i = 0; i < cases[c].metrics.size(); ++i)
                if (cases[c].metrics[i].evaluable)
                {
                    accumulator& item = values[cases[c].metrics[i].name];
                    item.domain = cases[c].metrics[i].domain;
                    item.unit = cases[c].metrics[i].unit;
                    item.values.push_back(cases[c].metrics[i].value);
                }
        }
        fresh.case_count = static_cast<unsigned int>(cases.size());
        for (std::map<std::string, accumulator>::const_iterator i = values.begin(); i != values.end(); ++i)
        {
            realism_population_metric metric;
            metric.name = i->first; metric.domain = i->second.domain; metric.unit = i->second.unit; metric.count = static_cast<unsigned int>(i->second.values.size());
            metric.minimum = *std::min_element(i->second.values.begin(), i->second.values.end());
            metric.maximum = *std::max_element(i->second.values.begin(), i->second.values.end());
            long double sum = 0.0; for (std::size_t j = 0; j < i->second.values.size(); ++j) sum += i->second.values[j];
            metric.mean = static_cast<double>(sum / i->second.values.size()); metric.standard_deviation = standard_deviation(i->second.values);
            fresh.metrics.push_back(metric);
        }
        output = fresh;
        return true;
    }

    std::string realism_analysis_json(const realism_analysis_result& result)
    {
        std::ostringstream output; output.imbue(std::locale::classic()); output << std::setprecision(17);
        output << "{\"schema_version\":1,\"contract\":" << json_string(result.contract) << ",\"render_identity\":" << json_string(result.render_identity)
               << ",\"reference_kind\":" << json_string(result.reference_kind) << ",\"reference_cohorts\":";
        write_reference_cohorts(output, result.reference_cohorts);
        output << ",\"single_score\":null,\"metrics\":[";
        for (std::size_t i = 0; i < result.metrics.size(); ++i)
            output << (i ? "," : "") << "{\"name\":" << json_string(result.metrics[i].name) << ",\"domain\":" << json_string(realism_metric_domain_name(result.metrics[i].domain))
                   << ",\"unit\":" << json_string(result.metrics[i].unit) << ",\"evaluable\":" << (result.metrics[i].evaluable ? "true" : "false")
                   << ",\"value\":" << (result.metrics[i].evaluable ? number_text(result.metrics[i].value, 17u) : "null") << '}';
        output << "],\"messages\":";
        write_string_array(output, result.messages);
        output << ",\"claim_boundary\":\"Engineering characterization only; no clinical realism or validation claim.\"}";
        return output.str();
    }

    std::string realism_analysis_csv(const realism_analysis_result& result)
    {
        std::ostringstream output; output.imbue(std::locale::classic()); output << std::setprecision(17) << "name,domain,unit,evaluable,value\n";
        for (std::size_t i = 0; i < result.metrics.size(); ++i)
            output << csv_cell(result.metrics[i].name) << ',' << realism_metric_domain_name(result.metrics[i].domain) << ',' << csv_cell(result.metrics[i].unit) << ',' << (result.metrics[i].evaluable ? 1 : 0) << ',' << (result.metrics[i].evaluable ? result.metrics[i].value : 0.0) << '\n';
        return output.str();
    }

    std::string realism_analysis_html(const realism_analysis_result& result)
    {
        std::ostringstream output; output.imbue(std::locale::classic()); output << std::setprecision(8)
            << "<!doctype html><html><head><meta charset=\"utf-8\"><title>Signal characterization</title><style>.notice{border-left:4px solid #6b7280;padding:10px 14px;background:#f3f4f6;color:#374151}</style></head><body><h1>Signal characterization</h1><p class=\"notice\">Synthetic engineering QA evidence; not diagnosis, nor clinical evidence</p><table><tr><th>Domain</th><th>Metric</th><th>Value</th><th>Unit</th></tr>";
        for (std::size_t i = 0; i < result.metrics.size(); ++i)
            output << "<tr><td>" << realism_metric_domain_name(result.metrics[i].domain) << "</td><td>" << html_text(result.metrics[i].name) << "</td><td>" << (result.metrics[i].evaluable ? number_text(result.metrics[i].value, 8u) : "not evaluable") << "</td><td>" << html_text(result.metrics[i].unit) << "</td></tr>";
        output << "</table><h2>Reference cohorts</h2>";
        if (result.reference_cohorts.empty()) output << "<p>No external reference cohort supplied; metrics characterize generated signals only.</p>";
        else
            for (std::size_t i = 0; i < result.reference_cohorts.size(); ++i)
                output << "<p>" << html_text(result.reference_cohorts[i].id) << " " << html_text(result.reference_cohorts[i].version) << " | " << html_text(result.reference_cohorts[i].license) << " | " << html_text(result.reference_cohorts[i].source_uri) << "</p>";
        output << "</body></html>"; return output.str();
    }

    std::string realism_population_json(const realism_population_summary& summary)
    {
        std::ostringstream output; output.imbue(std::locale::classic()); output << std::setprecision(17)
            << "{\"schema_version\":1,\"contract\":" << json_string(summary.contract) << ",\"case_count\":" << summary.case_count << ",\"reference_cohorts\":";
        write_reference_cohorts(output, summary.reference_cohorts);
        output << ",\"single_score\":null,\"metrics\":[";
        for (std::size_t i = 0; i < summary.metrics.size(); ++i)
            output << (i ? "," : "") << "{\"name\":" << json_string(summary.metrics[i].name) << ",\"domain\":" << json_string(realism_metric_domain_name(summary.metrics[i].domain)) << ",\"unit\":" << json_string(summary.metrics[i].unit)
                   << ",\"count\":" << summary.metrics[i].count << ",\"minimum\":" << summary.metrics[i].minimum << ",\"maximum\":" << summary.metrics[i].maximum << ",\"mean\":" << summary.metrics[i].mean << ",\"standard_deviation\":" << summary.metrics[i].standard_deviation << '}';
        output << "]}"; return output.str();
    }
}
