#include "hrv_scoring.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>

namespace
{
    const char* scoring_version = "synsigra_hrv_score_v2";

    struct json_value
    {
        enum kind { null_kind, bool_kind, number_kind, string_kind, array_kind, object_kind };
        kind type;
        double number;
        std::string string;
        std::vector<json_value> array;
        std::vector<std::pair<std::string, json_value> > object;
        json_value() : type(null_kind), number(0.0) {}
    };

    struct json_parser
    {
        explicit json_parser(const std::string& input) : text(input), offset(0), error() {}

        const std::string& text;
        std::size_t offset;
        std::string error;

        void skip_space()
        {
            while (offset < text.size() && (text[offset] == ' ' || text[offset] == '\t' || text[offset] == '\r' || text[offset] == '\n'))
                ++offset;
        }

        bool fail(const char* message)
        {
            error = message;
            return false;
        }

        bool parse_string(std::string& value)
        {
            if (offset >= text.size() || text[offset] != '"')
                return fail("expected string");
            ++offset;
            value.clear();
            while (offset < text.size())
            {
                const unsigned char ch = static_cast<unsigned char>(text[offset++]);
                if (ch == '"')
                    return true;
                if (ch == '\\')
                {
                    if (offset >= text.size())
                        return fail("truncated escape");
                    const char escaped = text[offset++];
                    switch (escaped)
                    {
                    case '"': value.push_back('"'); break;
                    case '\\': value.push_back('\\'); break;
                    case '/': value.push_back('/'); break;
                    case 'b': value.push_back('\b'); break;
                    case 'f': value.push_back('\f'); break;
                    case 'n': value.push_back('\n'); break;
                    case 'r': value.push_back('\r'); break;
                    case 't': value.push_back('\t'); break;
                    default: return fail("unsupported escape");
                    }
                }
                else if (ch < 0x20u)
                    return fail("control character in string");
                else
                    value.push_back(static_cast<char>(ch));
            }
            return fail("unterminated string");
        }

        bool parse_number(json_value& value)
        {
            const std::size_t start = offset;
            if (offset < text.size() && text[offset] == '-')
                ++offset;
            if (offset >= text.size())
                return fail("invalid number");
            if (text[offset] == '0')
                ++offset;
            else if (text[offset] >= '1' && text[offset] <= '9')
                while (offset < text.size() && text[offset] >= '0' && text[offset] <= '9')
                    ++offset;
            else
                return fail("invalid number");
            if (offset < text.size() && text[offset] == '.')
            {
                ++offset;
                const std::size_t digits = offset;
                while (offset < text.size() && text[offset] >= '0' && text[offset] <= '9')
                    ++offset;
                if (offset == digits)
                    return fail("invalid number");
            }
            if (offset < text.size() && (text[offset] == 'e' || text[offset] == 'E'))
            {
                ++offset;
                if (offset < text.size() && (text[offset] == '+' || text[offset] == '-'))
                    ++offset;
                const std::size_t digits = offset;
                while (offset < text.size() && text[offset] >= '0' && text[offset] <= '9')
                    ++offset;
                if (offset == digits)
                    return fail("invalid exponent");
            }
            const std::string token = text.substr(start, offset - start);
            errno = 0;
            char* end = 0;
            value.number = std::strtod(token.c_str(), &end);
            if (!end || *end || errno == ERANGE || !std::isfinite(value.number))
                return fail("invalid finite number");
            value.type = json_value::number_kind;
            return true;
        }

        bool parse_array(json_value& value)
        {
            value.type = json_value::array_kind;
            ++offset;
            skip_space();
            if (offset < text.size() && text[offset] == ']')
            {
                ++offset;
                return true;
            }
            while (offset < text.size())
            {
                json_value item;
                if (!parse_value(item))
                    return false;
                value.array.push_back(item);
                skip_space();
                if (offset < text.size() && text[offset] == ',')
                {
                    ++offset;
                    continue;
                }
                if (offset < text.size() && text[offset] == ']')
                {
                    ++offset;
                    return true;
                }
                return fail("expected comma or array end");
            }
            return fail("unterminated array");
        }

        bool parse_object(json_value& value)
        {
            value.type = json_value::object_kind;
            ++offset;
            skip_space();
            if (offset < text.size() && text[offset] == '}')
            {
                ++offset;
                return true;
            }
            std::set<std::string> keys;
            while (offset < text.size())
            {
                skip_space();
                std::string key;
                if (!parse_string(key))
                    return false;
                if (!keys.insert(key).second)
                    return fail("duplicate object key");
                skip_space();
                if (offset >= text.size() || text[offset] != ':')
                    return fail("expected colon");
                ++offset;
                json_value item;
                if (!parse_value(item))
                    return false;
                value.object.push_back(std::make_pair(key, item));
                skip_space();
                if (offset < text.size() && text[offset] == ',')
                {
                    ++offset;
                    continue;
                }
                if (offset < text.size() && text[offset] == '}')
                {
                    ++offset;
                    return true;
                }
                return fail("expected comma or object end");
            }
            return fail("unterminated object");
        }

        bool parse_value(json_value& value)
        {
            skip_space();
            if (offset >= text.size())
                return fail("unexpected end of input");
            if (text[offset] == '"')
            {
                value.type = json_value::string_kind;
                return parse_string(value.string);
            }
            if (text[offset] == '{')
                return parse_object(value);
            if (text[offset] == '[')
                return parse_array(value);
            if (text.compare(offset, 4, "null") == 0)
            {
                offset += 4;
                value.type = json_value::null_kind;
                return true;
            }
            if (text.compare(offset, 4, "true") == 0)
            {
                offset += 4;
                value.type = json_value::bool_kind;
                return true;
            }
            if (text.compare(offset, 5, "false") == 0)
            {
                offset += 5;
                value.type = json_value::bool_kind;
                return true;
            }
            return parse_number(value);
        }

        bool parse(json_value& value)
        {
            if (!parse_value(value))
                return false;
            skip_space();
            return offset == text.size() || fail("trailing data");
        }
    };

    const json_value* member(const json_value& object, const char* name)
    {
        for (std::size_t i = 0; i < object.object.size(); ++i)
            if (object.object[i].first == name)
                return &object.object[i].second;
        return 0;
    }

    bool allowed_fields(const json_value& object, const char* const* fields, std::size_t count, const std::string& path, std::vector<std::string>& messages)
    {
        bool valid = true;
        for (std::size_t i = 0; i < object.object.size(); ++i)
        {
            bool found = false;
            for (std::size_t field = 0; field < count; ++field)
                found = found || object.object[i].first == fields[field];
            if (!found)
            {
                messages.push_back(path + "." + object.object[i].first + ": unknown field");
                valid = false;
            }
        }
        return valid;
    }

    const json_value* required(const json_value& object, const char* name, json_value::kind kind, const std::string& path, std::vector<std::string>& messages)
    {
        const json_value* value = member(object, name);
        if (!value)
        {
            messages.push_back(path + "." + name + ": missing required field");
            return 0;
        }
        if (value->type != kind)
        {
            messages.push_back(path + "." + name + ": wrong JSON type");
            return 0;
        }
        return value;
    }

    bool metric_from_name(const std::string& name, signal_synth::hrv_metric_kind& kind)
    {
        for (int i = 0; i < signal_synth::hrv_metric_count; ++i)
        {
            kind = static_cast<signal_synth::hrv_metric_kind>(i);
            if (name == signal_synth::hrv_metric_name(kind))
                return true;
        }
        return false;
    }

    double truth_value(const signal_synth::hrv_metric_summary& metrics, signal_synth::hrv_metric_kind kind)
    {
        switch (kind)
        {
        case signal_synth::hrv_metric_mean_rr_seconds: return metrics.mean_rr_seconds;
        case signal_synth::hrv_metric_mean_heart_rate_bpm: return metrics.mean_heart_rate_bpm;
        case signal_synth::hrv_metric_sdnn_seconds: return metrics.sdnn_seconds;
        case signal_synth::hrv_metric_rmssd_seconds: return metrics.rmssd_seconds;
        case signal_synth::hrv_metric_pnn50_percent: return metrics.pnn50_percent;
        case signal_synth::hrv_metric_sd1_seconds: return metrics.sd1_seconds;
        case signal_synth::hrv_metric_sd2_seconds: return metrics.sd2_seconds;
        case signal_synth::hrv_metric_sd1_sd2_ratio: return metrics.sd1_sd2_ratio;
        case signal_synth::hrv_metric_vlf_power_seconds2: return metrics.vlf_power_seconds2;
        case signal_synth::hrv_metric_lf_power_seconds2: return metrics.lf_power_seconds2;
        case signal_synth::hrv_metric_hf_power_seconds2: return metrics.hf_power_seconds2;
        case signal_synth::hrv_metric_lf_hf_ratio: return metrics.lf_hf_ratio;
        case signal_synth::hrv_metric_lf_normalized_units: return metrics.lf_normalized_units;
        case signal_synth::hrv_metric_hf_normalized_units: return metrics.hf_normalized_units;
        case signal_synth::hrv_metric_total_power_seconds2: return metrics.total_power_seconds2;
        case signal_synth::hrv_metric_count: break;
        }
        return 0.0;
    }

    void tolerances(signal_synth::hrv_metric_kind kind, double& absolute, double& relative_percent)
    {
        relative_percent = 10.0;
        switch (kind)
        {
        case signal_synth::hrv_metric_mean_rr_seconds: absolute = 0.010; relative_percent = 2.0; break;
        case signal_synth::hrv_metric_mean_heart_rate_bpm: absolute = 1.0; relative_percent = 2.0; break;
        case signal_synth::hrv_metric_pnn50_percent: absolute = 2.0; break;
        case signal_synth::hrv_metric_sd1_sd2_ratio: absolute = 0.10; break;
        case signal_synth::hrv_metric_lf_hf_ratio: absolute = 0.20; relative_percent = 15.0; break;
        case signal_synth::hrv_metric_lf_normalized_units:
        case signal_synth::hrv_metric_hf_normalized_units: absolute = 2.0; relative_percent = 10.0; break;
        case signal_synth::hrv_metric_vlf_power_seconds2:
        case signal_synth::hrv_metric_lf_power_seconds2:
        case signal_synth::hrv_metric_hf_power_seconds2: absolute = 0.0005; relative_percent = 15.0; break;
        case signal_synth::hrv_metric_total_power_seconds2: absolute = 0.001; relative_percent = 15.0; break;
        default: absolute = 0.010; break;
        }
    }

    std::string json_string(const std::string& value)
    {
        std::ostringstream output;
        output << '"';
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            switch (value[i])
            {
            case '"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default: output << value[i]; break;
            }
        }
        output << '"';
        return output.str();
    }

    std::string html_text(const std::string& value)
    {
        std::string output;
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            switch (value[i])
            {
            case '&': output += "&amp;"; break;
            case '<': output += "&lt;"; break;
            case '>': output += "&gt;"; break;
            case '"': output += "&quot;"; break;
            case '\'': output += "&#39;"; break;
            default: output.push_back(value[i]); break;
            }
        }
        return output;
    }

    void score_rr(const signal_synth::ecg_render_bundle& render, const signal_synth::hrv_user_output& user, signal_synth::hrv_rr_score& score)
    {
        if (user.rr_intervals.empty())
            return;
        score.evaluated = true;
        std::vector<const signal_synth::hrv_rr_interval*> truth;
        for (std::size_t i = 0; i < render.hrv.intervals.size(); ++i)
            if (!render.hrv.intervals[i].excluded)
                truth.push_back(&render.hrv.intervals[i]);
        score.ground_truth_count = static_cast<unsigned int>(truth.size());
        score.user_count = static_cast<unsigned int>(user.rr_intervals.size());
        std::vector<bool> truth_used(truth.size(), false);
        std::vector<bool> user_used(user.rr_intervals.size(), false);
        std::vector<double> errors;
        for (;;)
        {
            std::size_t best_truth = truth.size();
            std::size_t best_user = user.rr_intervals.size();
            double best_time_error = score.time_tolerance_seconds + 1.0;
            for (std::size_t ti = 0; ti < truth.size(); ++ti)
            {
                if (truth_used[ti])
                    continue;
                for (std::size_t ui = 0; ui < user.rr_intervals.size(); ++ui)
                {
                    if (user_used[ui])
                        continue;
                    const double time_error = std::fabs(user.rr_intervals[ui].beat_time_seconds - truth[ti]->beat_time_seconds);
                    if (time_error <= score.time_tolerance_seconds && time_error < best_time_error)
                    {
                        best_time_error = time_error;
                        best_truth = ti;
                        best_user = ui;
                    }
                }
            }
            if (best_truth == truth.size())
                break;
            truth_used[best_truth] = true;
            user_used[best_user] = true;
            ++score.matched_count;
            const double absolute_error = std::fabs(user.rr_intervals[best_user].rr_seconds - truth[best_truth]->rr_seconds);
            const double relative_error = truth[best_truth]->rr_seconds > 0.0 ? 100.0 * absolute_error / truth[best_truth]->rr_seconds : 0.0;
            if (absolute_error <= score.absolute_tolerance_seconds || relative_error <= score.relative_tolerance_percent)
                ++score.passed_count;
            errors.push_back(absolute_error);
        }
        score.missing_count = score.ground_truth_count - score.matched_count;
        score.extra_count = score.user_count - score.matched_count;
        if (!errors.empty())
        {
            double sum = 0.0;
            double squared_sum = 0.0;
            for (std::size_t i = 0; i < errors.size(); ++i)
            {
                sum += errors[i];
                squared_sum += errors[i] * errors[i];
                score.max_absolute_error_seconds = std::max(score.max_absolute_error_seconds, errors[i]);
            }
            score.mean_absolute_error_seconds = sum / errors.size();
            score.rms_error_seconds = std::sqrt(squared_sum / errors.size());
        }
    }
}

namespace signal_synth
{
    hrv_user_rr_interval::hrv_user_rr_interval() : beat_time_seconds(0.0), rr_seconds(0.0), original_index(0) {}
    hrv_user_output::hrv_user_output() : schema_version(1), algorithm_name(), algorithm_version(), metrics(), rr_intervals() {}
    hrv_metric_score::hrv_metric_score() : kind(hrv_metric_mean_rr_seconds), ground_truth_value(0.0), user_value(0.0), absolute_error(0.0), relative_error_percent(0.0), absolute_tolerance(0.0), relative_tolerance_percent(0.0), passed(false) {}
    hrv_rr_score::hrv_rr_score() : evaluated(false), ground_truth_count(0), user_count(0), matched_count(0), missing_count(0), extra_count(0), passed_count(0), time_tolerance_seconds(0.050), absolute_tolerance_seconds(0.020), relative_tolerance_percent(5.0), mean_absolute_error_seconds(0.0), rms_error_seconds(0.0), max_absolute_error_seconds(0.0) {}
    hrv_score_result::hrv_score_result() : success(false), scoring_version(), scenario_id(), document_fingerprint(), render_identity(), algorithm_name(), algorithm_version(), metric_definition_version(), exclusion_policy(), spectral_method(), metrics(), rr(), passed_metric_count(0), metric_pass_fraction(0.0), messages() {}

    const char* hrv_metric_name(hrv_metric_kind kind)
    {
        static const char* names[hrv_metric_count] = {"mean_rr_seconds","mean_heart_rate_bpm","sdnn_seconds","rmssd_seconds","pnn50_percent","sd1_seconds","sd2_seconds","sd1_sd2_ratio","vlf_power_seconds2","lf_power_seconds2","hf_power_seconds2","lf_hf_ratio","lf_normalized_units","hf_normalized_units","total_power_seconds2"};
        return kind >= 0 && kind < hrv_metric_count ? names[kind] : "unknown";
    }

    const char* hrv_metric_unit(hrv_metric_kind kind)
    {
        switch (kind)
        {
        case hrv_metric_mean_heart_rate_bpm: return "bpm";
        case hrv_metric_pnn50_percent: return "percent";
        case hrv_metric_lf_normalized_units:
        case hrv_metric_hf_normalized_units: return "nu";
        case hrv_metric_sd1_sd2_ratio:
        case hrv_metric_lf_hf_ratio: return "ratio";
        case hrv_metric_vlf_power_seconds2:
        case hrv_metric_lf_power_seconds2:
        case hrv_metric_hf_power_seconds2:
        case hrv_metric_total_power_seconds2: return "s2";
        default: return "s";
        }
    }

    bool parse_hrv_user_output_json(const std::string& json, hrv_user_output& output, std::vector<std::string>& messages)
    {
        std::vector<std::string> fresh_messages;
        json_value root;
        json_parser parser(json);
        if (!parser.parse(root) || root.type != json_value::object_kind)
        {
            fresh_messages.push_back(std::string("$: invalid JSON object: ") + (parser.error.empty() ? "wrong root type" : parser.error));
            messages = fresh_messages;
            return false;
        }
        const char* root_fields[] = {"schema_version","algorithm","metrics","rr_intervals"};
        allowed_fields(root, root_fields, 4, "$", fresh_messages);
        const json_value* schema = required(root, "schema_version", json_value::number_kind, "$", fresh_messages);
        const json_value* algorithm = required(root, "algorithm", json_value::object_kind, "$", fresh_messages);
        const json_value* metrics = required(root, "metrics", json_value::object_kind, "$", fresh_messages);
        const json_value* rr = member(root, "rr_intervals");
        if (rr && rr->type != json_value::array_kind)
            fresh_messages.push_back("$.rr_intervals: wrong JSON type");

        hrv_user_output fresh;
        if (schema && (schema->number != 1.0))
            fresh_messages.push_back("$.schema_version: only version 1 is supported");
        if (algorithm)
        {
            const char* algorithm_fields[] = {"name","version"};
            allowed_fields(*algorithm, algorithm_fields, 2, "$.algorithm", fresh_messages);
            const json_value* name = required(*algorithm, "name", json_value::string_kind, "$.algorithm", fresh_messages);
            const json_value* version = required(*algorithm, "version", json_value::string_kind, "$.algorithm", fresh_messages);
            if (name) fresh.algorithm_name = name->string;
            if (version) fresh.algorithm_version = version->string;
            if (fresh.algorithm_name.empty())
                fresh_messages.push_back("$.algorithm.name: must not be empty");
        }
        if (metrics)
        {
            std::set<int> seen;
            for (std::size_t i = 0; i < metrics->object.size(); ++i)
            {
                hrv_metric_kind kind;
                if (!metric_from_name(metrics->object[i].first, kind))
                    fresh_messages.push_back("$.metrics." + metrics->object[i].first + ": unknown HRV metric");
                else if (metrics->object[i].second.type != json_value::number_kind || metrics->object[i].second.number < 0.0)
                    fresh_messages.push_back("$.metrics." + metrics->object[i].first + ": metric must be finite and non-negative");
                else if (!seen.insert(static_cast<int>(kind)).second)
                    fresh_messages.push_back("$.metrics." + metrics->object[i].first + ": duplicate metric");
                else
                {
                    hrv_user_metric metric;
                    metric.kind = kind;
                    metric.value = metrics->object[i].second.number;
                    fresh.metrics.push_back(metric);
                }
            }
        }
        if (rr && rr->type == json_value::array_kind)
        {
            for (std::size_t i = 0; i < rr->array.size(); ++i)
            {
                std::ostringstream path;
                path << "$.rr_intervals[" << i << ']';
                if (rr->array[i].type != json_value::object_kind)
                {
                    fresh_messages.push_back(path.str() + ": interval must be an object");
                    continue;
                }
                const char* fields[] = {"beat_time_seconds","rr_seconds"};
                allowed_fields(rr->array[i], fields, 2, path.str(), fresh_messages);
                const json_value* time = required(rr->array[i], "beat_time_seconds", json_value::number_kind, path.str(), fresh_messages);
                const json_value* interval = required(rr->array[i], "rr_seconds", json_value::number_kind, path.str(), fresh_messages);
                if (time && interval)
                {
                    if (time->number < 0.0 || interval->number <= 0.0)
                        fresh_messages.push_back(path.str() + ": time must be non-negative and RR must be positive");
                    else
                    {
                        hrv_user_rr_interval item;
                        item.beat_time_seconds = time->number;
                        item.rr_seconds = interval->number;
                        item.original_index = static_cast<unsigned int>(i);
                        fresh.rr_intervals.push_back(item);
                    }
                }
            }
        }
        if (fresh.metrics.empty() && fresh.rr_intervals.empty())
            fresh_messages.push_back("$: at least one HRV metric or RR interval is required");
        if (!fresh_messages.empty())
        {
            messages = fresh_messages;
            return false;
        }
        output = fresh;
        messages.clear();
        return true;
    }

    bool score_hrv_user_output(const ecg_render_bundle& render, const hrv_user_output& user, hrv_score_result& result)
    {
        hrv_score_result fresh;
        if (!render.document_identity.success || !render.scenario_report.success() || !render.record.sample_count())
        {
            fresh.messages.push_back("render bundle is incomplete");
            result = fresh;
            return false;
        }
        if (user.metrics.empty() && user.rr_intervals.empty())
        {
            fresh.messages.push_back("at least one HRV metric or RR interval is required");
            result = fresh;
            return false;
        }
        fresh.scoring_version = scoring_version;
        fresh.scenario_id = render.document.scenario_id;
        fresh.document_fingerprint = render.document_identity.document_fingerprint;
        fresh.render_identity = render.render_identity;
        fresh.algorithm_name = user.algorithm_name;
        fresh.algorithm_version = user.algorithm_version;
        fresh.metric_definition_version = render.hrv.metric_definition_version;
        fresh.exclusion_policy = render.hrv.exclusion_policy;
        fresh.spectral_method = render.hrv.spectral_method;
        for (std::size_t i = 0; i < user.metrics.size(); ++i)
        {
            hrv_metric_score score;
            score.kind = user.metrics[i].kind;
            score.ground_truth_value = truth_value(render.hrv.metrics, score.kind);
            score.user_value = user.metrics[i].value;
            score.absolute_error = std::fabs(score.user_value - score.ground_truth_value);
            score.relative_error_percent = score.ground_truth_value > 0.0 ? 100.0 * score.absolute_error / score.ground_truth_value : (score.absolute_error == 0.0 ? 0.0 : std::numeric_limits<double>::max());
            tolerances(score.kind, score.absolute_tolerance, score.relative_tolerance_percent);
            score.passed = score.absolute_error <= score.absolute_tolerance || score.relative_error_percent <= score.relative_tolerance_percent;
            fresh.passed_metric_count += score.passed ? 1u : 0u;
            fresh.metrics.push_back(score);
        }
        if (!fresh.metrics.empty())
            fresh.metric_pass_fraction = static_cast<double>(fresh.passed_metric_count) / fresh.metrics.size();
        score_rr(render, user, fresh.rr);
        fresh.success = true;
        result = fresh;
        return true;
    }

    std::string hrv_score_result_json(const hrv_score_result& result)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10)
               << "{\"schema_version\":1,\"score_type\":\"hrv_algorithm_qa\",\"scoring_version\":" << json_string(result.scoring_version)
               << ",\"scenario\":{\"id\":" << json_string(result.scenario_id)
               << ",\"document_fingerprint\":" << json_string(result.document_fingerprint)
               << ",\"render_identity\":" << json_string(result.render_identity) << '}'
               << ",\"algorithm\":{\"name\":" << json_string(result.algorithm_name) << ",\"version\":" << json_string(result.algorithm_version) << '}'
               << ",\"metric_definition_version\":" << json_string(result.metric_definition_version)
               << ",\"exclusion_policy\":" << json_string(result.exclusion_policy)
               << ",\"spectral_method\":" << json_string(result.spectral_method)
               << ",\"limitations\":\"synthetic engineering QA evidence, not diagnosis or clinical validation certification\""
               << ",\"metric_pass_fraction\":" << result.metric_pass_fraction << ",\"metrics\":[";
        for (std::size_t i = 0; i < result.metrics.size(); ++i)
        {
            const hrv_metric_score& score = result.metrics[i];
            output << (i ? "," : "") << "{\"name\":" << json_string(hrv_metric_name(score.kind))
                   << ",\"unit\":" << json_string(hrv_metric_unit(score.kind))
                   << ",\"ground_truth\":" << score.ground_truth_value
                   << ",\"user\":" << score.user_value
                   << ",\"absolute_error\":" << score.absolute_error
                   << ",\"relative_error_percent\":" << score.relative_error_percent
                   << ",\"absolute_tolerance\":" << score.absolute_tolerance
                   << ",\"relative_tolerance_percent\":" << score.relative_tolerance_percent
                   << ",\"passed\":" << (score.passed ? "true" : "false") << '}';
        }
        output << "],\"rr\":{\"evaluated\":" << (result.rr.evaluated ? "true" : "false")
               << ",\"ground_truth_count\":" << result.rr.ground_truth_count
               << ",\"user_count\":" << result.rr.user_count
               << ",\"matched_count\":" << result.rr.matched_count
               << ",\"missing_count\":" << result.rr.missing_count
               << ",\"extra_count\":" << result.rr.extra_count
               << ",\"passed_count\":" << result.rr.passed_count
               << ",\"time_tolerance_seconds\":" << result.rr.time_tolerance_seconds
               << ",\"absolute_tolerance_seconds\":" << result.rr.absolute_tolerance_seconds
               << ",\"relative_tolerance_percent\":" << result.rr.relative_tolerance_percent
               << ",\"mean_absolute_error_seconds\":" << result.rr.mean_absolute_error_seconds
               << ",\"rms_error_seconds\":" << result.rr.rms_error_seconds
               << ",\"max_absolute_error_seconds\":" << result.rr.max_absolute_error_seconds << "}}";
        return output.str();
    }

    std::string hrv_score_result_csv(const hrv_score_result& result)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10)
               << "row_type,name,unit,ground_truth,user,absolute_error,relative_error_percent,absolute_tolerance,relative_tolerance_percent,passed,count\n";
        for (std::size_t i = 0; i < result.metrics.size(); ++i)
        {
            const hrv_metric_score& score = result.metrics[i];
            output << "metric," << hrv_metric_name(score.kind) << ',' << hrv_metric_unit(score.kind) << ',' << score.ground_truth_value << ',' << score.user_value
                   << ',' << score.absolute_error << ',' << score.relative_error_percent << ',' << score.absolute_tolerance << ',' << score.relative_tolerance_percent
                   << ',' << (score.passed ? 1 : 0) << ",\n";
        }
        output << "rr,matched,count,,,,,,," << (result.rr.evaluated ? 1 : 0) << ',' << result.rr.matched_count << '\n'
               << "rr,missing,count,,,,,,,," << result.rr.missing_count << '\n'
               << "rr,extra,count,,,,,,,," << result.rr.extra_count << '\n'
               << "rr,mean_absolute_error,seconds,,," << result.rr.mean_absolute_error_seconds << ",,,,," << result.rr.matched_count << '\n';
        return output.str();
    }

    std::string hrv_score_report_html(const hrv_score_result& result)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << "<!doctype html><html><head><meta charset=\"utf-8\"><title>HRV Algorithm QA Score</title>"
               << "<style>body{font-family:Arial,sans-serif;margin:24px;line-height:1.45}table{border-collapse:collapse;width:100%;margin:12px 0}th,td{border:1px solid #d1d5db;padding:6px 8px;text-align:left}th{background:#f3f4f6}code{font-family:monospace}.pass{color:#067647}.fail{color:#b42318}</style></head><body>"
               << "<h1>HRV Algorithm QA Score</h1><p>This report compares user HRV output against synthetic ground truth. It is engineering QA evidence, not diagnosis or clinical validation certification.</p>"
               << "<table><tr><th>Scenario</th><td>" << html_text(result.scenario_id) << "</td></tr><tr><th>Render identity</th><td><code>" << html_text(result.render_identity)
               << "</code></td></tr><tr><th>Algorithm</th><td>" << html_text(result.algorithm_name) << " " << html_text(result.algorithm_version)
               << "</td></tr><tr><th>Metric definition</th><td>" << html_text(result.metric_definition_version)
               << "</td></tr><tr><th>Exclusion policy</th><td>" << html_text(result.exclusion_policy)
               << "</td></tr><tr><th>Spectral method</th><td>" << html_text(result.spectral_method) << "</td></tr></table>"
               << "<h2>Metrics</h2><table><tr><th>Metric</th><th>GT</th><th>User</th><th>Abs error</th><th>Rel error %</th><th>Tolerance</th><th>Result</th></tr>";
        for (std::size_t i = 0; i < result.metrics.size(); ++i)
        {
            const hrv_metric_score& score = result.metrics[i];
            output << "<tr><td>" << hrv_metric_name(score.kind) << " (" << hrv_metric_unit(score.kind) << ")</td><td>" << score.ground_truth_value
                   << "</td><td>" << score.user_value << "</td><td>" << score.absolute_error << "</td><td>" << score.relative_error_percent
                   << "</td><td>abs " << score.absolute_tolerance << " or rel " << score.relative_tolerance_percent << "%</td><td class=\""
                   << (score.passed ? "pass\">PASS" : "fail\">FAIL") << "</td></tr>";
        }
        output << "</table><h2>RR Intervals</h2><table><tr><th>Evaluated</th><th>GT</th><th>User</th><th>Matched</th><th>Missing</th><th>Extra</th><th>Passed</th><th>MAE s</th></tr>"
               << "<tr><td>" << (result.rr.evaluated ? "yes" : "no") << "</td><td>" << result.rr.ground_truth_count << "</td><td>" << result.rr.user_count
               << "</td><td>" << result.rr.matched_count << "</td><td>" << result.rr.missing_count << "</td><td>" << result.rr.extra_count
               << "</td><td>" << result.rr.passed_count << "</td><td>" << result.rr.mean_absolute_error_seconds << "</td></tr></table>"
               << "<h2>Limitations</h2><p>Scores depend on the declared metric definition, analysis window, exclusion policy, and deterministic synthetic scenario. This report is not a clinical performance claim.</p>"
               << "<h2>Artifacts</h2><p>hrv_score.json, hrv_score.csv, hrv_score_report.html</p></body></html>";
        return output.str();
    }
}
