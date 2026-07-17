#include "interval_io.h"

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
    struct json_value
    {
        enum kind { null_kind, bool_kind, number_kind, string_kind, array_kind, object_kind };
        kind type;
        double number;
        std::string token;
        std::string string;
        std::vector<json_value> array;
        std::vector<std::pair<std::string, json_value> > object;
        json_value() : type(null_kind), number(0.0) {}
    };

    struct parser
    {
        explicit parser(const std::string& input) : text(input), offset(0), code(signal_synth::interval_io_syntax), error() {}
        const std::string& text;
        std::size_t offset;
        signal_synth::interval_io_message_code code;
        std::string error;

        void skip_ws()
        {
            while (offset < text.size() && (text[offset] == ' ' || text[offset] == '\t' || text[offset] == '\r' || text[offset] == '\n'))
                ++offset;
        }

        bool fail(signal_synth::interval_io_message_code failure_code, const char* message)
        {
            code = failure_code;
            error = message;
            return false;
        }

        static bool valid_number_token(const std::string& token)
        {
            if (token.empty())
                return false;
            std::size_t index = 0;
            if (token[index] == '-')
            {
                ++index;
                if (index == token.size())
                    return false;
            }
            if (token[index] == '0')
            {
                ++index;
                if (index < token.size() && token[index] >= '0' && token[index] <= '9')
                    return false;
            }
            else if (token[index] >= '1' && token[index] <= '9')
            {
                while (index < token.size() && token[index] >= '0' && token[index] <= '9')
                    ++index;
            }
            else
                return false;
            if (index < token.size() && token[index] == '.')
            {
                ++index;
                const std::size_t start = index;
                while (index < token.size() && token[index] >= '0' && token[index] <= '9')
                    ++index;
                if (index == start)
                    return false;
            }
            if (index < token.size() && (token[index] == 'e' || token[index] == 'E'))
            {
                ++index;
                if (index < token.size() && (token[index] == '+' || token[index] == '-'))
                    ++index;
                const std::size_t start = index;
                while (index < token.size() && token[index] >= '0' && token[index] <= '9')
                    ++index;
                if (index == start)
                    return false;
            }
            return index == token.size();
        }

        bool parse_string(std::string& value)
        {
            if (offset >= text.size() || text[offset] != '"')
                return fail(signal_synth::interval_io_syntax, "expected string");
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
                        return fail(signal_synth::interval_io_syntax, "truncated escape");
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
                    default: return fail(signal_synth::interval_io_syntax, "unsupported escape");
                    }
                }
                else if (ch < 0x20)
                    return fail(signal_synth::interval_io_syntax, "control character in string");
                else
                    value.push_back(static_cast<char>(ch));
            }
            return fail(signal_synth::interval_io_syntax, "unterminated string");
        }

        bool parse_value(json_value& value)
        {
            skip_ws();
            if (offset >= text.size())
                return fail(signal_synth::interval_io_syntax, "unexpected end of input");
            if (text[offset] == '"')
            {
                value.type = json_value::string_kind;
                return parse_string(value.string);
            }
            if (text[offset] == '{')
                return parse_object(value);
            if (text[offset] == '[')
                return parse_array(value);
            const std::size_t start = offset;
            while (offset < text.size())
            {
                const char ch = text[offset];
                if (ch == ',' || ch == '}' || ch == ']' || ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
                    break;
                ++offset;
            }
            value.token = text.substr(start, offset - start);
            if (value.token == "null")
            {
                value.type = json_value::null_kind;
                return true;
            }
            if (value.token == "true" || value.token == "false")
            {
                value.type = json_value::bool_kind;
                return true;
            }
            if (!valid_number_token(value.token))
                return fail(signal_synth::interval_io_syntax, "invalid token");
            errno = 0;
            char* end = 0;
            value.number = std::strtod(value.token.c_str(), &end);
            if (end && *end == 0 && errno != ERANGE && std::isfinite(value.number))
            {
                value.type = json_value::number_kind;
                return true;
            }
            return fail(signal_synth::interval_io_syntax, "invalid token");
        }

        bool parse_array(json_value& value)
        {
            value.type = json_value::array_kind;
            ++offset;
            skip_ws();
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
                skip_ws();
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
                return fail(signal_synth::interval_io_syntax, "expected comma or array end");
            }
            return fail(signal_synth::interval_io_syntax, "unterminated array");
        }

        bool parse_object(json_value& value)
        {
            value.type = json_value::object_kind;
            ++offset;
            skip_ws();
            if (offset < text.size() && text[offset] == '}')
            {
                ++offset;
                return true;
            }
            std::set<std::string> keys;
            while (offset < text.size())
            {
                skip_ws();
                std::string key;
                if (!parse_string(key))
                    return false;
                if (!keys.insert(key).second)
                    return fail(signal_synth::interval_io_duplicate, "duplicate object key");
                skip_ws();
                if (offset >= text.size() || text[offset] != ':')
                    return fail(signal_synth::interval_io_syntax, "expected colon");
                ++offset;
                json_value item;
                if (!parse_value(item))
                    return false;
                value.object.push_back(std::make_pair(key, item));
                skip_ws();
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
                return fail(signal_synth::interval_io_syntax, "expected comma or object end");
            }
            return fail(signal_synth::interval_io_syntax, "unterminated object");
        }

        bool parse_root(json_value& value)
        {
            if (!parse_value(value))
                return false;
            skip_ws();
            return offset == text.size() || fail(signal_synth::interval_io_syntax, "trailing data");
        }
    };

    const json_value* member(const json_value& object, const char* name)
    {
        for (std::size_t i = 0; i < object.object.size(); ++i)
            if (object.object[i].first == name)
                return &object.object[i].second;
        return 0;
    }

    void add_message(signal_synth::interval_io_result& result, signal_synth::interval_io_message_code code, const std::string& path, const std::string& message)
    {
        signal_synth::interval_io_message item;
        item.code = code;
        item.path = path;
        item.message = message;
        result.messages.push_back(item);
    }

    std::string index_text(std::size_t index)
    {
        std::ostringstream output;
        output << index;
        return output.str();
    }

    const json_value* required(const json_value& object, const char* name, json_value::kind kind, const std::string& path, signal_synth::interval_io_result& result)
    {
        const json_value* value = member(object, name);
        if (!value)
        {
            add_message(result, signal_synth::interval_io_missing_field, path + "." + name, "required field is missing");
            return 0;
        }
        if (value->type != kind)
        {
            add_message(result, signal_synth::interval_io_type, path + "." + name, "field has the wrong JSON type");
            return 0;
        }
        return value;
    }

    bool allowed_fields(const json_value& object, const char* const* names, std::size_t count, const std::string& path, signal_synth::interval_io_result& result)
    {
        bool valid = true;
        for (std::size_t i = 0; i < object.object.size(); ++i)
        {
            bool found = false;
            for (std::size_t n = 0; n < count; ++n)
                found = found || object.object[i].first == names[n];
            if (!found)
            {
                add_message(result, signal_synth::interval_io_unknown_field, path + "." + object.object[i].first, "unknown field");
                valid = false;
            }
        }
        return valid;
    }

    bool parse_unsigned(const json_value& value, unsigned int& output)
    {
        if (value.type != json_value::number_kind || value.number < 0.0 || value.number > 4294967295.0)
            return false;
        const unsigned int integer = static_cast<unsigned int>(value.number);
        if (static_cast<double>(integer) != value.number)
            return false;
        output = integer;
        return true;
    }

    std::string trim_ascii(const std::string& value)
    {
        std::size_t first = 0;
        while (first < value.size() && (value[first] == ' ' || value[first] == '\t' || value[first] == '\r'))
            ++first;
        std::size_t last = value.size();
        while (last > first && (value[last - 1] == ' ' || value[last - 1] == '\t' || value[last - 1] == '\r'))
            --last;
        return value.substr(first, last - first);
    }

    bool parse_double(const std::string& text, double& value)
    {
        const std::string trimmed = trim_ascii(text);
        if (trimmed.empty())
            return false;
        errno = 0;
        char* end = 0;
        const double parsed = std::strtod(trimmed.c_str(), &end);
        if (end == trimmed.c_str() || errno == ERANGE || !std::isfinite(parsed))
            return false;
        while (end && (*end == ' ' || *end == '\t' || *end == '\r'))
            ++end;
        if (end && *end != 0)
            return false;
        value = parsed;
        return true;
    }

    bool read_csv_record(const std::string& csv, std::size_t& offset, std::vector<std::string>& cells, std::string& message)
    {
        cells.clear();
        std::string cell;
        bool quoted = false;
        bool saw_any = false;
        while (offset < csv.size())
        {
            saw_any = true;
            const char ch = csv[offset++];
            if (quoted)
            {
                if (ch == '"')
                {
                    if (offset < csv.size() && csv[offset] == '"')
                    {
                        cell.push_back('"');
                        ++offset;
                    }
                    else
                        quoted = false;
                }
                else
                    cell.push_back(ch);
            }
            else if (ch == '"')
            {
                if (!trim_ascii(cell).empty())
                {
                    message = "quote must start a CSV cell";
                    return false;
                }
                quoted = true;
                cell.clear();
            }
            else if (ch == ',')
            {
                cells.push_back(trim_ascii(cell));
                cell.clear();
            }
            else if (ch == '\n')
            {
                cells.push_back(trim_ascii(cell));
                return true;
            }
            else
                cell.push_back(ch);
        }
        if (quoted)
        {
            message = "unterminated quoted CSV cell";
            return false;
        }
        if (!saw_any)
            return false;
        cells.push_back(trim_ascii(cell));
        return true;
    }

    bool empty_record(const std::vector<std::string>& cells)
    {
        for (std::size_t i = 0; i < cells.size(); ++i)
            if (!trim_ascii(cells[i]).empty())
                return false;
        return true;
    }

    std::string json_string(const std::string& value)
    {
        std::ostringstream output;
        output << '"';
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            const unsigned char ch = static_cast<unsigned char>(value[i]);
            switch (ch)
            {
            case '"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\b': output << "\\b"; break;
            case '\f': output << "\\f"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (ch < 0x20)
                    output << "\\u00" << "0123456789abcdef"[ch >> 4] << "0123456789abcdef"[ch & 15u];
                else
                    output << static_cast<char>(ch);
                break;
            }
        }
        output << '"';
        return output.str();
    }

    std::string csv_cell(const std::string& value)
    {
        if (value.find_first_of(",\"\r\n") == std::string::npos)
            return value;
        std::string output = "\"";
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            if (value[i] == '"')
                output += '"';
            output += value[i];
        }
        return output + '"';
    }

    bool interval_less(const signal_synth::interval_output_event& left, const signal_synth::interval_output_event& right)
    {
        if (left.start_seconds != right.start_seconds) return left.start_seconds < right.start_seconds;
        if (left.end_seconds != right.end_seconds) return left.end_seconds < right.end_seconds;
        if (left.channel != right.channel) return left.channel < right.channel;
        if (left.label != right.label) return left.label < right.label;
        return left.original_index < right.original_index;
    }

    std::string duplicate_key(const signal_synth::interval_output_event& interval)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(17) << interval.start_seconds << '|' << interval.end_seconds << '|' << interval.label << '|' << interval.channel;
        return output.str();
    }

    bool validate(signal_synth::interval_output_document& document, signal_synth::interval_io_result& result)
    {
        signal_synth::interval_target target;
        if (document.schema_version != 1)
            add_message(result, signal_synth::interval_io_range, "$.schema_version", "only schema version 1 is supported");
        if (!signal_synth::interval_target_from_name(document.target_name, target))
            add_message(result, signal_synth::interval_io_range, "$.target", "target must be rhythm_episode or signal_quality");
        if (document.algorithm.name.size() > 128)
            add_message(result, signal_synth::interval_io_range, "$.algorithm.name", "algorithm name must contain at most 128 characters");
        if (document.algorithm.version.size() > 128)
            add_message(result, signal_synth::interval_io_range, "$.algorithm.version", "algorithm version must contain at most 128 characters");
        std::set<std::string> duplicates;
        for (std::size_t i = 0; i < document.intervals.size(); ++i)
        {
            signal_synth::interval_output_event& interval = document.intervals[i];
            const std::string path = "$.intervals[" + index_text(i) + "]";
            if (interval.channel.empty())
                interval.channel = "global";
            if (!std::isfinite(interval.start_seconds) || interval.start_seconds < 0.0)
                add_message(result, signal_synth::interval_io_range, path + ".start_seconds", "start_seconds must be finite and non-negative");
            if (!std::isfinite(interval.end_seconds) || interval.end_seconds <= interval.start_seconds)
                add_message(result, signal_synth::interval_io_range, path + ".end_seconds", "end_seconds must be finite and greater than start_seconds");
            if (interval.label.empty() || interval.label.size() > 64)
                add_message(result, signal_synth::interval_io_range, path + ".label", "label must contain 1 to 64 characters");
            if (interval.channel.size() > 64)
                add_message(result, signal_synth::interval_io_range, path + ".channel", "channel must contain at most 64 characters");
            if (interval.has_confidence && (!std::isfinite(interval.confidence) || interval.confidence < 0.0 || interval.confidence > 1.0))
                add_message(result, signal_synth::interval_io_range, path + ".confidence", "confidence must be in the closed interval [0,1]");
            if (document.target_name == "rhythm_episode" && interval.channel != "global")
                add_message(result, signal_synth::interval_io_range, path + ".channel", "rhythm_episode intervals must use channel global");
            if (!duplicates.insert(duplicate_key(interval)).second)
                add_message(result, signal_synth::interval_io_duplicate, path, "duplicate interval with identical bounds, label, and channel");
        }
        if (!result.messages.empty())
            return false;
        std::stable_sort(document.intervals.begin(), document.intervals.end(), interval_less);
        return true;
    }

    std::string canonical_json(const signal_synth::interval_output_document& document)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10)
               << "{\"schema_version\":1,\"algorithm\":{\"name\":" << json_string(document.algorithm.name)
               << ",\"version\":" << json_string(document.algorithm.version) << "},\"target\":" << json_string(document.target_name) << ",\"intervals\":[";
        for (std::size_t i = 0; i < document.intervals.size(); ++i)
        {
            const signal_synth::interval_output_event& interval = document.intervals[i];
            output << (i ? "," : "") << "{\"start_seconds\":" << interval.start_seconds << ",\"end_seconds\":" << interval.end_seconds
                   << ",\"label\":" << json_string(interval.label) << ",\"channel\":" << json_string(interval.channel);
            if (interval.has_confidence)
                output << ",\"confidence\":" << interval.confidence;
            output << '}';
        }
        output << "]}";
        return output.str();
    }

    std::string canonical_csv(const signal_synth::interval_output_document& document)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10) << "start_seconds,end_seconds,label,channel,confidence\n";
        for (std::size_t i = 0; i < document.intervals.size(); ++i)
        {
            const signal_synth::interval_output_event& interval = document.intervals[i];
            output << interval.start_seconds << ',' << interval.end_seconds << ',' << csv_cell(interval.label) << ',' << csv_cell(interval.channel) << ',';
            if (interval.has_confidence)
                output << interval.confidence;
            output << '\n';
        }
        return output.str();
    }
}

namespace signal_synth
{
    interval_output_event::interval_output_event() : start_seconds(0.0), end_seconds(0.0), label(), channel("global"), has_confidence(false), confidence(0.0), original_index(0) {}
    interval_output_document::interval_output_document() : schema_version(1), target_name("rhythm_episode"), algorithm(), intervals() {}
    interval_io_result::interval_io_result() : success(false), messages(), canonical_json(), canonical_csv() {}

    const char* interval_target_name(interval_target target)
    {
        switch (target)
        {
        case interval_target_rhythm_episode: return "rhythm_episode";
        case interval_target_signal_quality: return "signal_quality";
        }
        return "";
    }

    bool interval_target_from_name(const std::string& name, interval_target& target)
    {
        if (name == "rhythm_episode")
        {
            target = interval_target_rhythm_episode;
            return true;
        }
        if (name == "signal_quality")
        {
            target = interval_target_signal_quality;
            return true;
        }
        return false;
    }

    const char* interval_io_message_code_name(interval_io_message_code code)
    {
        switch (code)
        {
        case interval_io_syntax: return "INTERVAL_IO_SYNTAX";
        case interval_io_type: return "INTERVAL_IO_TYPE";
        case interval_io_missing_field: return "INTERVAL_IO_MISSING_FIELD";
        case interval_io_unknown_field: return "INTERVAL_IO_UNKNOWN_FIELD";
        case interval_io_range: return "INTERVAL_IO_RANGE";
        case interval_io_duplicate: return "INTERVAL_IO_DUPLICATE";
        case interval_io_target_mismatch: return "INTERVAL_IO_TARGET_MISMATCH";
        }
        return "INTERVAL_IO_UNKNOWN";
    }

    bool write_interval_output(const interval_output_document& document, interval_io_result& result)
    {
        interval_io_result fresh;
        interval_output_document normalized = document;
        if (!validate(normalized, fresh))
        {
            result = fresh;
            return false;
        }
        fresh.canonical_json = canonical_json(normalized);
        fresh.canonical_csv = canonical_csv(normalized);
        fresh.success = true;
        result = fresh;
        return true;
    }

    bool parse_interval_json_v1(const std::string& json, interval_output_document& output, interval_io_result& result)
    {
        interval_io_result fresh;
        json_value root;
        parser input(json);
        if (!input.parse_root(root))
        {
            add_message(fresh, input.code, "$", input.error);
            result = fresh;
            return false;
        }
        if (root.type != json_value::object_kind)
        {
            add_message(fresh, interval_io_type, "$", "root must be an object");
            result = fresh;
            return false;
        }
        const char* top_fields[] = {"schema_version","algorithm","target","intervals"};
        allowed_fields(root, top_fields, sizeof(top_fields) / sizeof(top_fields[0]), "$", fresh);
        const json_value* schema = required(root, "schema_version", json_value::number_kind, "$", fresh);
        const json_value* algorithm = required(root, "algorithm", json_value::object_kind, "$", fresh);
        const json_value* target = required(root, "target", json_value::string_kind, "$", fresh);
        const json_value* intervals = required(root, "intervals", json_value::array_kind, "$", fresh);
        if (!fresh.messages.empty())
        {
            result = fresh;
            return false;
        }
        interval_output_document document;
        if (!parse_unsigned(*schema, document.schema_version))
            add_message(fresh, interval_io_range, "$.schema_version", "schema_version must be integer 1");
        document.target_name = target->string;
        const char* algorithm_fields[] = {"name","version"};
        allowed_fields(*algorithm, algorithm_fields, sizeof(algorithm_fields) / sizeof(algorithm_fields[0]), "$.algorithm", fresh);
        const json_value* algorithm_name = required(*algorithm, "name", json_value::string_kind, "$.algorithm", fresh);
        const json_value* algorithm_version = required(*algorithm, "version", json_value::string_kind, "$.algorithm", fresh);
        if (algorithm_name) document.algorithm.name = algorithm_name->string;
        if (algorithm_version) document.algorithm.version = algorithm_version->string;
        for (std::size_t i = 0; i < intervals->array.size(); ++i)
        {
            const std::string path = "$.intervals[" + index_text(i) + "]";
            const json_value& item = intervals->array[i];
            if (item.type != json_value::object_kind)
            {
                add_message(fresh, interval_io_type, path, "interval must be an object");
                continue;
            }
            const char* interval_fields[] = {"start_seconds","end_seconds","label","channel","confidence"};
            allowed_fields(item, interval_fields, sizeof(interval_fields) / sizeof(interval_fields[0]), path, fresh);
            const json_value* start = required(item, "start_seconds", json_value::number_kind, path, fresh);
            const json_value* end = required(item, "end_seconds", json_value::number_kind, path, fresh);
            const json_value* label = required(item, "label", json_value::string_kind, path, fresh);
            interval_output_event interval;
            interval.original_index = static_cast<unsigned int>(i);
            if (start) interval.start_seconds = start->number;
            if (end) interval.end_seconds = end->number;
            if (label) interval.label = label->string;
            const json_value* channel = member(item, "channel");
            if (channel && channel->type != json_value::string_kind)
                add_message(fresh, interval_io_type, path + ".channel", "channel must be a string");
            else if (channel)
                interval.channel = channel->string;
            const json_value* confidence = member(item, "confidence");
            if (confidence && confidence->type != json_value::number_kind)
                add_message(fresh, interval_io_type, path + ".confidence", "confidence must be a number");
            else if (confidence)
            {
                interval.has_confidence = true;
                interval.confidence = confidence->number;
            }
            document.intervals.push_back(interval);
        }
        if (!validate(document, fresh))
        {
            result = fresh;
            return false;
        }
        fresh.canonical_json = canonical_json(document);
        fresh.canonical_csv = canonical_csv(document);
        fresh.success = true;
        output = document;
        result = fresh;
        return true;
    }

    bool parse_interval_csv_v1(const std::string& csv, const std::string& target_name, interval_output_document& output, interval_io_result& result)
    {
        interval_io_result fresh;
        interval_output_document document;
        document.target_name = target_name;
        document.algorithm.name = "csv_interval_input";
        document.algorithm.version = "v1";
        std::size_t offset = 0;
        std::vector<std::string> header;
        std::string csv_message;
        while (read_csv_record(csv, offset, header, csv_message) && empty_record(header)) {}
        if (!csv_message.empty() || header.empty())
        {
            add_message(fresh, interval_io_syntax, "$", csv_message.empty() ? "interval CSV is empty" : csv_message);
            result = fresh;
            return false;
        }
        int start_column = -1, end_column = -1, label_column = -1, channel_column = -1, confidence_column = -1;
        std::set<std::string> seen;
        for (std::size_t i = 0; i < header.size(); ++i)
        {
            const std::string name = trim_ascii(header[i]);
            if (name.empty())
                add_message(fresh, interval_io_range, "$.header[" + index_text(i) + "]", "CSV header cell must not be empty");
            else if (!seen.insert(name).second)
                add_message(fresh, interval_io_duplicate, "$.header[" + index_text(i) + "]", "duplicate CSV column");
            else if (name == "start_seconds") start_column = static_cast<int>(i);
            else if (name == "end_seconds") end_column = static_cast<int>(i);
            else if (name == "label") label_column = static_cast<int>(i);
            else if (name == "channel") channel_column = static_cast<int>(i);
            else if (name == "confidence") confidence_column = static_cast<int>(i);
            else add_message(fresh, interval_io_unknown_field, "$.header[" + index_text(i) + "]", "unsupported CSV column");
        }
        if (start_column < 0) add_message(fresh, interval_io_missing_field, "$.header.start_seconds", "interval CSV must contain start_seconds");
        if (end_column < 0) add_message(fresh, interval_io_missing_field, "$.header.end_seconds", "interval CSV must contain end_seconds");
        if (label_column < 0) add_message(fresh, interval_io_missing_field, "$.header.label", "interval CSV must contain label");
        if (!fresh.messages.empty())
        {
            result = fresh;
            return false;
        }
        unsigned int physical_row = 1;
        unsigned int interval_index = 0;
        std::vector<std::string> cells;
        while (read_csv_record(csv, offset, cells, csv_message))
        {
            ++physical_row;
            if (empty_record(cells))
                continue;
            if (cells.size() > header.size())
            {
                add_message(fresh, interval_io_range, "$.rows[" + index_text(physical_row) + "]", "CSV row has more cells than the header");
                continue;
            }
            interval_output_event interval;
            interval.original_index = interval_index++;
            if (static_cast<std::size_t>(start_column) >= cells.size() || !parse_double(cells[static_cast<std::size_t>(start_column)], interval.start_seconds))
                add_message(fresh, interval_io_range, "$.rows[" + index_text(physical_row) + "].start_seconds", "invalid start_seconds");
            if (static_cast<std::size_t>(end_column) >= cells.size() || !parse_double(cells[static_cast<std::size_t>(end_column)], interval.end_seconds))
                add_message(fresh, interval_io_range, "$.rows[" + index_text(physical_row) + "].end_seconds", "invalid end_seconds");
            if (static_cast<std::size_t>(label_column) < cells.size()) interval.label = cells[static_cast<std::size_t>(label_column)];
            if (channel_column >= 0 && static_cast<std::size_t>(channel_column) < cells.size()) interval.channel = cells[static_cast<std::size_t>(channel_column)];
            if (confidence_column >= 0 && static_cast<std::size_t>(confidence_column) < cells.size() && !trim_ascii(cells[static_cast<std::size_t>(confidence_column)]).empty())
            {
                interval.has_confidence = parse_double(cells[static_cast<std::size_t>(confidence_column)], interval.confidence);
                if (!interval.has_confidence)
                    add_message(fresh, interval_io_range, "$.rows[" + index_text(physical_row) + "].confidence", "invalid confidence");
            }
            document.intervals.push_back(interval);
        }
        if (!csv_message.empty())
            add_message(fresh, interval_io_syntax, "$.rows", csv_message);
        if (!validate(document, fresh))
        {
            result = fresh;
            return false;
        }
        fresh.canonical_json = canonical_json(document);
        fresh.canonical_csv = canonical_csv(document);
        fresh.success = true;
        output = document;
        result = fresh;
        return true;
    }
}
