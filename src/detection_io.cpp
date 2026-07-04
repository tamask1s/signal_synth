#include "detection_io.h"

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
        explicit parser(const std::string& input) : text(input), offset(0), code(signal_synth::detection_io_syntax), failed(false) {}
        const std::string& text;
        std::size_t offset;
        signal_synth::detection_io_message_code code;
        std::string error;
        bool failed;

        void skip_ws()
        {
            while (offset < text.size() && (text[offset] == ' ' || text[offset] == '\t' || text[offset] == '\r' || text[offset] == '\n'))
                ++offset;
        }

        bool fail(signal_synth::detection_io_message_code failure_code, const char* message)
        {
            code = failure_code;
            error = message;
            failed = true;
            return false;
        }

        static bool valid_number_token(const std::string& token)
        {
            if (token.empty())
                return false;
            std::size_t offset = 0;
            if (token[offset] == '-')
            {
                ++offset;
                if (offset == token.size())
                    return false;
            }
            if (token[offset] == '0')
            {
                ++offset;
                if (offset < token.size() && token[offset] >= '0' && token[offset] <= '9')
                    return false;
            }
            else if (token[offset] >= '1' && token[offset] <= '9')
            {
                while (offset < token.size() && token[offset] >= '0' && token[offset] <= '9')
                    ++offset;
            }
            else
                return false;
            if (offset < token.size() && token[offset] == '.')
            {
                ++offset;
                const std::size_t start = offset;
                while (offset < token.size() && token[offset] >= '0' && token[offset] <= '9')
                    ++offset;
                if (offset == start)
                    return false;
            }
            if (offset < token.size() && (token[offset] == 'e' || token[offset] == 'E'))
            {
                ++offset;
                if (offset < token.size() && (token[offset] == '+' || token[offset] == '-'))
                    ++offset;
                const std::size_t start = offset;
                while (offset < token.size() && token[offset] >= '0' && token[offset] <= '9')
                    ++offset;
                if (offset == start)
                    return false;
            }
            return offset == token.size();
        }

        bool parse_string_token(std::string& value)
        {
            if (offset >= text.size() || text[offset] != '"')
                return fail(signal_synth::detection_io_syntax, "expected string");
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
                        return fail(signal_synth::detection_io_syntax, "truncated escape");
                    const char esc = text[offset++];
                    switch (esc)
                    {
                    case '"': value.push_back('"'); break;
                    case '\\': value.push_back('\\'); break;
                    case '/': value.push_back('/'); break;
                    case 'b': value.push_back('\b'); break;
                    case 'f': value.push_back('\f'); break;
                    case 'n': value.push_back('\n'); break;
                    case 'r': value.push_back('\r'); break;
                    case 't': value.push_back('\t'); break;
                    default: return fail(signal_synth::detection_io_syntax, "unsupported escape");
                    }
                }
                else if (ch < 0x20)
                    return fail(signal_synth::detection_io_syntax, "control character in string");
                else
                    value.push_back(static_cast<char>(ch));
            }
            return fail(signal_synth::detection_io_syntax, "unterminated string");
        }

        bool parse_value(json_value& value)
        {
            skip_ws();
            if (offset >= text.size())
                return fail(signal_synth::detection_io_syntax, "unexpected end of input");
            if (text[offset] == '"')
            {
                value.type = json_value::string_kind;
                return parse_string_token(value.string);
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
                return fail(signal_synth::detection_io_syntax, "invalid token");
            errno = 0;
            char* end = 0;
            value.number = std::strtod(value.token.c_str(), &end);
            if (end && *end == 0 && errno != ERANGE && std::isfinite(value.number))
            {
                value.type = json_value::number_kind;
                return true;
            }
            return fail(signal_synth::detection_io_syntax, "invalid token");
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
                return fail(signal_synth::detection_io_syntax, "expected comma or array end");
            }
            return fail(signal_synth::detection_io_syntax, "unterminated array");
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
                if (!parse_string_token(key))
                    return false;
                if (!keys.insert(key).second)
                    return fail(signal_synth::detection_io_duplicate_id, "duplicate object key");
                skip_ws();
                if (offset >= text.size() || text[offset] != ':')
                    return fail(signal_synth::detection_io_syntax, "expected colon");
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
                return fail(signal_synth::detection_io_syntax, "expected comma or object end");
            }
            return fail(signal_synth::detection_io_syntax, "unterminated object");
        }

        bool parse_root(json_value& value)
        {
            if (!parse_value(value))
                return false;
            skip_ws();
            if (offset != text.size())
                return fail(signal_synth::detection_io_syntax, "trailing data");
            return true;
        }
    };

    const json_value* member(const json_value& object, const char* name)
    {
        for (std::size_t i = 0; i < object.object.size(); ++i)
            if (object.object[i].first == name)
                return &object.object[i].second;
        return 0;
    }

    void add_message(signal_synth::detection_io_result& result, signal_synth::detection_io_message_code code, const std::string& path, const std::string& message)
    {
        signal_synth::detection_io_message item;
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

    bool finite_non_negative(double value)
    {
        return value >= 0.0 && value <= std::numeric_limits<double>::max();
    }

    bool parse_unsigned_integer(const json_value& value, unsigned int& output)
    {
        if (value.type != json_value::number_kind || value.number < 0.0 || value.number > 4294967295.0)
            return false;
        const unsigned int integer = static_cast<unsigned int>(value.number);
        if (static_cast<double>(integer) != value.number)
            return false;
        output = integer;
        return true;
    }

    bool allowed_fields(const json_value& object, const char* const* names, std::size_t count, const std::string& path, signal_synth::detection_io_result& result)
    {
        bool ok = true;
        for (std::size_t i = 0; i < object.object.size(); ++i)
        {
            bool found = false;
            for (std::size_t n = 0; n < count; ++n)
                if (object.object[i].first == names[n])
                    found = true;
            if (!found)
            {
                add_message(result, signal_synth::detection_io_unknown_field, path + "." + object.object[i].first, "unknown field");
                ok = false;
            }
        }
        return ok;
    }

    const json_value* required(const json_value& object, const char* name, json_value::kind kind, const std::string& path, signal_synth::detection_io_result& result)
    {
        const json_value* value = member(object, name);
        if (!value)
        {
            add_message(result, signal_synth::detection_io_missing_field, path + "." + name, "required field is missing");
            return 0;
        }
        if (value->type != kind)
        {
            add_message(result, signal_synth::detection_io_type, path + "." + name, "field has the wrong JSON type");
            return 0;
        }
        return value;
    }

    std::string escape_json(const std::string& value)
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

    std::string trim_ascii(const std::string& value)
    {
        std::string::size_type first = 0;
        while (first < value.size() && (value[first] == ' ' || value[first] == '\t' || value[first] == '\r'))
            ++first;
        std::string::size_type last = value.size();
        while (last > first && (value[last - 1] == ' ' || value[last - 1] == '\t' || value[last - 1] == '\r'))
            --last;
        return value.substr(first, last - first);
    }

    bool parse_double_cell(const std::string& text, double& value)
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
        if (end && *end != '\0')
            return false;
        value = parsed;
        return true;
    }

    bool parse_unsigned_cell(const std::string& text, unsigned int& value)
    {
        const std::string trimmed = trim_ascii(text);
        if (trimmed.empty())
            return false;
        for (std::size_t i = 0; i < trimmed.size(); ++i)
            if (trimmed[i] < '0' || trimmed[i] > '9')
                return false;
        char* end = 0;
        errno = 0;
        const unsigned long parsed = std::strtoul(trimmed.c_str(), &end, 10);
        if (end == trimmed.c_str() || errno == ERANGE || parsed > 4294967295ul)
            return false;
        while (end && (*end == ' ' || *end == '\t' || *end == '\r'))
            ++end;
        if (end && *end != '\0')
            return false;
        value = static_cast<unsigned int>(parsed);
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
                continue;
            }
            if (ch == '"')
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

    bool valid_target_name(const std::string& target)
    {
        signal_synth::ecg_compare_target ignored;
        return signal_synth::detection_compare_target_from_name(target, ignored);
    }

    std::string duplicate_key(const signal_synth::detection_io_event& event)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(17) << event.time_seconds << '|' << event.channel << '|' << event.label << '|';
        if (event.has_sample_index)
            output << event.sample_index;
        else
            output << '-';
        return output.str();
    }

    bool validate_document(signal_synth::detection_io_document& document, signal_synth::detection_io_result& result)
    {
        if (document.schema_version != 1)
            add_message(result, signal_synth::detection_io_range, "$.schema_version", "only schema version 1 is supported");
        if (!valid_target_name(document.target_name))
            add_message(result, signal_synth::detection_io_range, "$.target", "target must be r_peak or ppg_systolic_peak");
        document.has_compare_target = signal_synth::detection_compare_target_from_name(document.target_name, document.compare_target);
        if (document.events.empty())
            add_message(result, signal_synth::detection_io_range, "$.events", "at least one detection event is required");
        std::set<std::string> duplicates;
        for (std::size_t i = 0; i < document.events.size(); ++i)
        {
            signal_synth::detection_io_event& event = document.events[i];
            const std::string path = "$.events[" + index_text(i) + "]";
            if (!finite_non_negative(event.time_seconds))
                add_message(result, signal_synth::detection_io_range, path + ".time_seconds", "time_seconds must be finite and non-negative");
            if (event.has_confidence && !(event.confidence >= 0.0 && event.confidence <= 1.0))
                add_message(result, signal_synth::detection_io_range, path + ".confidence", "confidence must be in the closed interval [0,1]");
            if (event.channel.size() > 64)
                add_message(result, signal_synth::detection_io_range, path + ".channel", "channel must contain at most 64 characters");
            if (event.label.size() > 64)
                add_message(result, signal_synth::detection_io_range, path + ".label", "label must contain at most 64 characters");
            const std::string key = duplicate_key(event);
            if (!duplicates.insert(key).second)
                add_message(result, signal_synth::detection_io_duplicate_id, path, "duplicate identical detection event");
        }
        return result.messages.empty();
    }

    std::string canonical(const signal_synth::detection_io_document& document)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << "{\"schema_version\":1,\"algorithm\":{\"name\":" << escape_json(document.algorithm.name)
               << ",\"version\":" << escape_json(document.algorithm.version)
               << "},\"target\":" << escape_json(document.target_name)
               << ",\"events\":[";
        for (std::size_t i = 0; i < document.events.size(); ++i)
        {
            const signal_synth::detection_io_event& event = document.events[i];
            output << (i ? "," : "") << "{\"time_seconds\":" << std::setprecision(17) << event.time_seconds;
            if (event.has_sample_index)
                output << ",\"sample_index\":" << event.sample_index;
            if (!event.channel.empty())
                output << ",\"channel\":" << escape_json(event.channel);
            if (!event.label.empty())
                output << ",\"label\":" << escape_json(event.label);
            if (event.has_confidence)
                output << ",\"confidence\":" << std::setprecision(17) << event.confidence;
            output << "}";
        }
        output << "]}";
        return output.str();
    }
}

namespace signal_synth
{
    detection_io_event::detection_io_event() : time_seconds(0.0), has_sample_index(false), sample_index(0), channel(), label(), has_confidence(false), confidence(0.0), original_index(0) {}
    detection_io_document::detection_io_document() : schema_version(1), target_name("r_peak"), has_compare_target(true), compare_target(ecg_compare_r_peak), algorithm(), events() {}
    detection_io_result::detection_io_result() : success(false), messages(), canonical_json() {}

    const char* detection_io_message_code_name(detection_io_message_code code)
    {
        switch (code)
        {
        case detection_io_syntax: return "DETECTION_IO_SYNTAX";
        case detection_io_type: return "DETECTION_IO_TYPE";
        case detection_io_missing_field: return "DETECTION_IO_MISSING_FIELD";
        case detection_io_unknown_field: return "DETECTION_IO_UNKNOWN_FIELD";
        case detection_io_range: return "DETECTION_IO_RANGE";
        case detection_io_duplicate_id: return "DETECTION_IO_DUPLICATE_ID";
        case detection_io_target_mismatch: return "DETECTION_IO_TARGET_MISMATCH";
        }
        return "DETECTION_IO_UNKNOWN";
    }

    bool detection_compare_target_from_name(const std::string& name, ecg_compare_target& target)
    {
        if (name == "r_peak" || name == "rpeaks" || name == "r-peak")
        {
            target = ecg_compare_r_peak;
            return true;
        }
        if (name == "ppg_systolic_peak" || name == "ppg_peak" || name == "ppg-peaks" || name == "ppg-systolic-peak")
        {
            target = ecg_compare_ppg_systolic_peak;
            return true;
        }
        return false;
    }

    bool write_detection_json_v1(const detection_io_document& document, detection_io_result& result)
    {
        detection_io_result fresh;
        detection_io_document copy = document;
        if (!validate_document(copy, fresh))
        {
            result = fresh;
            return false;
        }
        fresh.canonical_json = canonical(copy);
        fresh.success = true;
        result = fresh;
        return true;
    }

    bool parse_detection_csv_v2(const std::string& csv, const std::string& target_name, detection_io_document& output, detection_io_result& result)
    {
        detection_io_result fresh;
        detection_io_document document;
        document.target_name = target_name;
        document.algorithm.name = "csv_detection_input";
        document.algorithm.version = "v2";
        std::size_t offset = 0;
        std::vector<std::string> header;
        std::string csv_message;
        while (read_csv_record(csv, offset, header, csv_message) && empty_record(header)) {}
        if (!csv_message.empty())
        {
            add_message(fresh, detection_io_syntax, "$", csv_message);
            result = fresh;
            return false;
        }
        if (header.empty())
        {
            add_message(fresh, detection_io_syntax, "$", "detection CSV is empty");
            result = fresh;
            return false;
        }
        int time_column = -1, sample_column = -1, channel_column = -1, label_column = -1, confidence_column = -1;
        std::set<std::string> seen_headers;
        for (std::size_t i = 0; i < header.size(); ++i)
        {
            const std::string name = trim_ascii(header[i]);
            if (name.empty())
                add_message(fresh, detection_io_range, "$.header[" + index_text(i) + "]", "CSV header cell must not be empty");
            else if (!seen_headers.insert(name).second)
                add_message(fresh, detection_io_duplicate_id, "$.header[" + index_text(i) + "]", "duplicate CSV column");
            else if (name == "time_seconds")
                time_column = static_cast<int>(i);
            else if (name == "sample_index")
                sample_column = static_cast<int>(i);
            else if (name == "channel")
                channel_column = static_cast<int>(i);
            else if (name == "label")
                label_column = static_cast<int>(i);
            else if (name == "confidence")
                confidence_column = static_cast<int>(i);
            else
                add_message(fresh, detection_io_unknown_field, "$.header[" + index_text(i) + "]", "unsupported CSV column");
        }
        if (time_column < 0)
            add_message(fresh, detection_io_missing_field, "$.header.time_seconds", "detection CSV must contain a time_seconds column");
        if (!fresh.messages.empty())
        {
            result = fresh;
            return false;
        }
        unsigned int physical_row = 1;
        unsigned int event_index = 0;
        std::vector<std::string> cells;
        while (read_csv_record(csv, offset, cells, csv_message))
        {
            ++physical_row;
            if (empty_record(cells))
                continue;
            if (cells.size() > header.size())
            {
                add_message(fresh, detection_io_range, "$.rows[" + index_text(physical_row) + "]", "CSV row has more cells than the header");
                continue;
            }
            detection_io_event event;
            event.original_index = event_index++;
            if (static_cast<std::size_t>(time_column) >= cells.size() || !parse_double_cell(cells[static_cast<std::size_t>(time_column)], event.time_seconds))
                add_message(fresh, detection_io_range, "$.rows[" + index_text(physical_row) + "].time_seconds", "invalid time_seconds");
            if (sample_column >= 0 && static_cast<std::size_t>(sample_column) < cells.size() && !trim_ascii(cells[static_cast<std::size_t>(sample_column)]).empty())
            {
                event.has_sample_index = parse_unsigned_cell(cells[static_cast<std::size_t>(sample_column)], event.sample_index);
                if (!event.has_sample_index)
                    add_message(fresh, detection_io_range, "$.rows[" + index_text(physical_row) + "].sample_index", "invalid sample_index");
            }
            if (channel_column >= 0 && static_cast<std::size_t>(channel_column) < cells.size())
                event.channel = cells[static_cast<std::size_t>(channel_column)];
            if (label_column >= 0 && static_cast<std::size_t>(label_column) < cells.size())
                event.label = cells[static_cast<std::size_t>(label_column)];
            if (confidence_column >= 0 && static_cast<std::size_t>(confidence_column) < cells.size() && !trim_ascii(cells[static_cast<std::size_t>(confidence_column)]).empty())
            {
                event.has_confidence = parse_double_cell(cells[static_cast<std::size_t>(confidence_column)], event.confidence);
                if (!event.has_confidence)
                    add_message(fresh, detection_io_range, "$.rows[" + index_text(physical_row) + "].confidence", "invalid confidence");
            }
            document.events.push_back(event);
        }
        if (!csv_message.empty())
            add_message(fresh, detection_io_syntax, "$.rows", csv_message);
        if (!validate_document(document, fresh))
        {
            result = fresh;
            return false;
        }
        fresh.canonical_json = canonical(document);
        fresh.success = true;
        output = document;
        result = fresh;
        return true;
    }

    bool parse_detection_json_v1(const std::string& json, detection_io_document& output, detection_io_result& result)
    {
        detection_io_result fresh;
        json_value root;
        parser p(json);
        if (!p.parse_root(root))
        {
            add_message(fresh, p.code, "$", p.error);
            result = fresh;
            return false;
        }
        if (root.type != json_value::object_kind)
        {
            add_message(fresh, detection_io_type, "$", "root must be an object");
            result = fresh;
            return false;
        }
        const char* top_fields[] = {"schema_version","algorithm","target","events"};
        allowed_fields(root, top_fields, sizeof(top_fields) / sizeof(top_fields[0]), "$", fresh);
        const json_value* schema = required(root, "schema_version", json_value::number_kind, "$", fresh);
        const json_value* algorithm = required(root, "algorithm", json_value::object_kind, "$", fresh);
        const json_value* target = required(root, "target", json_value::string_kind, "$", fresh);
        const json_value* events = required(root, "events", json_value::array_kind, "$", fresh);
        if (!fresh.messages.empty())
        {
            result = fresh;
            return false;
        }
        detection_io_document document;
        if (!parse_unsigned_integer(*schema, document.schema_version))
            add_message(fresh, detection_io_range, "$.schema_version", "schema_version must be integer 1");
        document.target_name = target->string;

        const char* algorithm_fields[] = {"name","version"};
        allowed_fields(*algorithm, algorithm_fields, sizeof(algorithm_fields) / sizeof(algorithm_fields[0]), "$.algorithm", fresh);
        const json_value* algorithm_name = required(*algorithm, "name", json_value::string_kind, "$.algorithm", fresh);
        const json_value* algorithm_version = required(*algorithm, "version", json_value::string_kind, "$.algorithm", fresh);
        if (algorithm_name)
            document.algorithm.name = algorithm_name->string;
        if (algorithm_version)
            document.algorithm.version = algorithm_version->string;

        for (std::size_t i = 0; i < events->array.size(); ++i)
        {
            const std::string path = "$.events[" + index_text(i) + "]";
            const json_value& item = events->array[i];
            if (item.type != json_value::object_kind)
            {
                add_message(fresh, detection_io_type, path, "event must be an object");
                continue;
            }
            const char* event_fields[] = {"time_seconds","sample_index","channel","label","confidence"};
            allowed_fields(item, event_fields, sizeof(event_fields) / sizeof(event_fields[0]), path, fresh);
            const json_value* time = required(item, "time_seconds", json_value::number_kind, path, fresh);
            detection_io_event event;
            event.original_index = static_cast<unsigned int>(i);
            if (time)
                event.time_seconds = time->number;
            const json_value* sample_index = member(item, "sample_index");
            if (sample_index)
            {
                event.has_sample_index = parse_unsigned_integer(*sample_index, event.sample_index);
                if (!event.has_sample_index)
                    add_message(fresh, detection_io_range, path + ".sample_index", "sample_index must be a non-negative integer");
            }
            const json_value* channel = member(item, "channel");
            if (channel)
            {
                if (channel->type != json_value::string_kind)
                    add_message(fresh, detection_io_type, path + ".channel", "channel must be a string");
                else
                    event.channel = channel->string;
            }
            const json_value* label = member(item, "label");
            if (label)
            {
                if (label->type != json_value::string_kind)
                    add_message(fresh, detection_io_type, path + ".label", "label must be a string");
                else
                    event.label = label->string;
            }
            const json_value* confidence = member(item, "confidence");
            if (confidence)
            {
                if (confidence->type != json_value::number_kind)
                    add_message(fresh, detection_io_type, path + ".confidence", "confidence must be a number");
                else
                {
                    event.has_confidence = true;
                    event.confidence = confidence->number;
                }
            }
            document.events.push_back(event);
        }
        if (!validate_document(document, fresh))
        {
            result = fresh;
            return false;
        }
        fresh.canonical_json = canonical(document);
        fresh.success = true;
        output = document;
        result = fresh;
        return true;
    }

    bool detection_events_for_compare(const detection_io_document& document, std::vector<ecg_detected_event>& output, detection_io_result& result)
    {
        detection_io_result fresh;
        detection_io_document copy = document;
        if (!validate_document(copy, fresh))
        {
            result = fresh;
            return false;
        }
        output.clear();
        output.reserve(document.events.size());
        for (std::size_t i = 0; i < document.events.size(); ++i)
        {
            ecg_detected_event event;
            event.time_seconds = document.events[i].time_seconds;
            event.label = document.events[i].label;
            event.original_index = document.events[i].original_index;
            event.has_original_index = true;
            output.push_back(event);
        }
        fresh.success = true;
        result = fresh;
        return true;
    }
}
