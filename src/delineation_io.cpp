#include "delineation_io.h"

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
        explicit parser(const std::string& input) : text(input), offset(0), code(signal_synth::delineation_io_syntax), error() {}
        const std::string& text;
        std::size_t offset;
        signal_synth::delineation_io_message_code code;
        std::string error;

        void skip_ws()
        {
            while (offset < text.size() && (text[offset] == ' ' || text[offset] == '\t' || text[offset] == '\r' || text[offset] == '\n'))
                ++offset;
        }

        bool fail(signal_synth::delineation_io_message_code failure_code, const char* message)
        {
            code = failure_code;
            error = message;
            return false;
        }

        static bool valid_number_token(const std::string& value)
        {
            if (value.empty())
                return false;
            std::size_t index = value[0] == '-' ? 1u : 0u;
            if (index == value.size())
                return false;
            if (value[index] == '0')
            {
                ++index;
                if (index < value.size() && value[index] >= '0' && value[index] <= '9')
                    return false;
            }
            else if (value[index] >= '1' && value[index] <= '9')
            {
                while (index < value.size() && value[index] >= '0' && value[index] <= '9')
                    ++index;
            }
            else
                return false;
            if (index < value.size() && value[index] == '.')
            {
                const std::size_t start = ++index;
                while (index < value.size() && value[index] >= '0' && value[index] <= '9')
                    ++index;
                if (index == start)
                    return false;
            }
            if (index < value.size() && (value[index] == 'e' || value[index] == 'E'))
            {
                ++index;
                if (index < value.size() && (value[index] == '+' || value[index] == '-'))
                    ++index;
                const std::size_t start = index;
                while (index < value.size() && value[index] >= '0' && value[index] <= '9')
                    ++index;
                if (index == start)
                    return false;
            }
            return index == value.size();
        }

        bool parse_string(std::string& value)
        {
            if (offset >= text.size() || text[offset] != '"')
                return fail(signal_synth::delineation_io_syntax, "expected string");
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
                        return fail(signal_synth::delineation_io_syntax, "truncated escape");
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
                    default: return fail(signal_synth::delineation_io_syntax, "unsupported escape");
                    }
                }
                else if (ch < 0x20)
                    return fail(signal_synth::delineation_io_syntax, "control character in string");
                else
                    value.push_back(static_cast<char>(ch));
            }
            return fail(signal_synth::delineation_io_syntax, "unterminated string");
        }

        bool parse_value(json_value& value)
        {
            skip_ws();
            if (offset >= text.size())
                return fail(signal_synth::delineation_io_syntax, "unexpected end of input");
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
                return fail(signal_synth::delineation_io_syntax, "invalid token");
            errno = 0;
            char* end = 0;
            value.number = std::strtod(value.token.c_str(), &end);
            if (end && *end == 0 && errno != ERANGE && std::isfinite(value.number))
            {
                value.type = json_value::number_kind;
                return true;
            }
            return fail(signal_synth::delineation_io_syntax, "invalid number");
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
                return fail(signal_synth::delineation_io_syntax, "expected comma or array end");
            }
            return fail(signal_synth::delineation_io_syntax, "unterminated array");
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
                    return fail(signal_synth::delineation_io_duplicate, "duplicate object key");
                skip_ws();
                if (offset >= text.size() || text[offset] != ':')
                    return fail(signal_synth::delineation_io_syntax, "expected colon");
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
                return fail(signal_synth::delineation_io_syntax, "expected comma or object end");
            }
            return fail(signal_synth::delineation_io_syntax, "unterminated object");
        }

        bool parse_root(json_value& value)
        {
            if (!parse_value(value))
                return false;
            skip_ws();
            return offset == text.size() || fail(signal_synth::delineation_io_syntax, "trailing data");
        }
    };

    const json_value* member(const json_value& object, const char* name)
    {
        for (std::size_t i = 0; i < object.object.size(); ++i)
            if (object.object[i].first == name)
                return &object.object[i].second;
        return 0;
    }

    void add_message(signal_synth::delineation_io_result& result, signal_synth::delineation_io_message_code code, const std::string& path, const std::string& message)
    {
        signal_synth::delineation_io_message item;
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

    const json_value* required(const json_value& object, const char* name, json_value::kind kind, const std::string& path, signal_synth::delineation_io_result& result)
    {
        const json_value* value = member(object, name);
        if (!value)
            add_message(result, signal_synth::delineation_io_missing_field, path + "." + name, "required field is missing");
        else if (value->type != kind)
            add_message(result, signal_synth::delineation_io_type, path + "." + name, "field has the wrong JSON type");
        else
            return value;
        return 0;
    }

    bool allowed_fields(const json_value& object, const char* const* names, std::size_t count, const std::string& path, signal_synth::delineation_io_result& result)
    {
        bool valid = true;
        for (std::size_t i = 0; i < object.object.size(); ++i)
        {
            bool found = false;
            for (std::size_t n = 0; n < count; ++n)
                found = found || object.object[i].first == names[n];
            if (!found)
            {
                add_message(result, signal_synth::delineation_io_unknown_field, path + "." + object.object[i].first, "unknown field");
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

    bool parse_uint64(const std::string& value, unsigned long long& output)
    {
        if (value.empty() || (value.size() > 1u && value[0] == '0'))
            return false;
        for (std::size_t i = 0; i < value.size(); ++i)
            if (value[i] < '0' || value[i] > '9')
                return false;
        errno = 0;
        char* end = 0;
        const unsigned long long parsed = std::strtoull(value.c_str(), &end, 10);
        if (errno == ERANGE || !end || *end != 0)
            return false;
        output = parsed;
        return true;
    }

    std::string uint64_text(unsigned long long value)
    {
        std::ostringstream output;
        output << value;
        return output.str();
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

    int lead_rank(const std::string& lead)
    {
        const char* names[] = {"I","II","III","aVR","aVL","aVF","V1","V2","V3","V4","V5","V6"};
        for (int i = 0; i < 12; ++i)
            if (lead == names[i])
                return i;
        return -1;
    }

    bool lead_less(const std::string& left, const std::string& right)
    {
        return lead_rank(left) < lead_rank(right);
    }

    bool event_less(const signal_synth::delineation_event& left, const signal_synth::delineation_event& right)
    {
        if (left.beat_index != right.beat_index) return left.beat_index < right.beat_index;
        if (left.lead != right.lead) return lead_less(left.lead, right.lead);
        if (left.kind != right.kind) return left.kind < right.kind;
        return left.original_index < right.original_index;
    }

    std::string event_key(const signal_synth::delineation_event& event)
    {
        return uint64_text(event.beat_index) + "|" + event.lead + "|" + signal_synth::delineation_kind_name(event.kind);
    }

    bool validate(signal_synth::delineation_output_document& document, signal_synth::delineation_io_result& result)
    {
        if (document.schema_version != 1u)
            add_message(result, signal_synth::delineation_io_range, "$.schema_version", "only schema version 1 is supported");
        if (document.target_name != "ecg_delineation")
            add_message(result, signal_synth::delineation_io_range, "$.target", "target must be ecg_delineation");
        if (document.algorithm.name.size() > 128u)
            add_message(result, signal_synth::delineation_io_range, "$.algorithm.name", "algorithm name must contain at most 128 characters");
        if (document.algorithm.version.size() > 128u)
            add_message(result, signal_synth::delineation_io_range, "$.algorithm.version", "algorithm version must contain at most 128 characters");
        std::set<unsigned long long> beats;
        for (std::size_t i = 0; i < document.beat_indices.size(); ++i)
            if (!beats.insert(document.beat_indices[i]).second)
                add_message(result, signal_synth::delineation_io_duplicate, "$.scope.beat_indices[" + index_text(i) + "]", "duplicate beat index");
        if (document.scope_mode == signal_synth::delineation_scope_all_beats && !document.beat_indices.empty())
            add_message(result, signal_synth::delineation_io_scope, "$.scope.beat_indices", "all_beats scope must not contain beat_indices");
        if (document.scope_mode == signal_synth::delineation_scope_selected_beats && document.beat_indices.empty())
            add_message(result, signal_synth::delineation_io_scope, "$.scope.beat_indices", "selected_beats scope requires at least one beat index");
        if (document.leads.empty())
            add_message(result, signal_synth::delineation_io_scope, "$.scope.leads", "scope requires at least one ECG lead");
        std::set<std::string> leads;
        for (std::size_t i = 0; i < document.leads.size(); ++i)
        {
            if (lead_rank(document.leads[i]) < 0)
                add_message(result, signal_synth::delineation_io_range, "$.scope.leads[" + index_text(i) + "]", "lead must be a standard 12-lead ECG name");
            if (!leads.insert(document.leads[i]).second)
                add_message(result, signal_synth::delineation_io_duplicate, "$.scope.leads[" + index_text(i) + "]", "duplicate lead");
        }
        std::set<std::string> events;
        for (std::size_t i = 0; i < document.events.size(); ++i)
        {
            signal_synth::delineation_event& event = document.events[i];
            const std::string path = "$.events[" + index_text(i) + "]";
            if (lead_rank(event.lead) < 0)
                add_message(result, signal_synth::delineation_io_range, path + ".lead", "lead must be a standard 12-lead ECG name");
            else if (leads.find(event.lead) == leads.end())
                add_message(result, signal_synth::delineation_io_scope, path + ".lead", "event lead is outside the declared scope");
            if (document.scope_mode == signal_synth::delineation_scope_selected_beats && beats.find(event.beat_index) == beats.end())
                add_message(result, signal_synth::delineation_io_scope, path + ".beat_index", "event beat is outside the declared scope");
            if (event.kind < signal_synth::delineation_p_onset || event.kind >= signal_synth::delineation_kind_count)
                add_message(result, signal_synth::delineation_io_range, path + ".kind", "unsupported delineation kind");
            if (!std::isfinite(event.time_seconds) || event.time_seconds < 0.0)
                add_message(result, signal_synth::delineation_io_range, path + ".time_seconds", "time_seconds must be finite and non-negative");
            if (event.has_confidence && (!std::isfinite(event.confidence) || event.confidence < 0.0 || event.confidence > 1.0))
                add_message(result, signal_synth::delineation_io_range, path + ".confidence", "confidence must be in the closed interval [0,1]");
            if (!events.insert(event_key(event)).second)
                add_message(result, signal_synth::delineation_io_duplicate, path, "duplicate beat, lead, and kind identity");
        }
        if (!result.messages.empty())
            return false;
        std::sort(document.beat_indices.begin(), document.beat_indices.end());
        std::sort(document.leads.begin(), document.leads.end(), lead_less);
        std::stable_sort(document.events.begin(), document.events.end(), event_less);
        return true;
    }

    std::string canonical_json(const signal_synth::delineation_output_document& document)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10)
               << "{\"schema_version\":1,\"algorithm\":{\"name\":" << json_string(document.algorithm.name)
               << ",\"version\":" << json_string(document.algorithm.version) << "},\"target\":\"ecg_delineation\",\"scope\":{\"mode\":"
               << json_string(signal_synth::delineation_scope_mode_name(document.scope_mode));
        if (document.scope_mode == signal_synth::delineation_scope_selected_beats)
        {
            output << ",\"beat_indices\":[";
            for (std::size_t i = 0; i < document.beat_indices.size(); ++i)
                output << (i ? "," : "") << json_string(uint64_text(document.beat_indices[i]));
            output << ']';
        }
        output << ",\"leads\":[";
        for (std::size_t i = 0; i < document.leads.size(); ++i)
            output << (i ? "," : "") << json_string(document.leads[i]);
        output << "]},\"events\":[";
        for (std::size_t i = 0; i < document.events.size(); ++i)
        {
            const signal_synth::delineation_event& event = document.events[i];
            output << (i ? "," : "") << "{\"beat_index\":" << json_string(uint64_text(event.beat_index))
                   << ",\"lead\":" << json_string(event.lead) << ",\"kind\":" << json_string(signal_synth::delineation_kind_name(event.kind))
                   << ",\"time_seconds\":" << event.time_seconds;
            if (event.has_confidence)
                output << ",\"confidence\":" << event.confidence;
            output << '}';
        }
        output << "]}";
        return output.str();
    }

    std::string canonical_csv(const signal_synth::delineation_output_document& document)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10)
               << "row_type,scope_mode,evaluated_beat_index,beat_index,lead,kind,time_seconds,confidence\n";
        if (document.scope_mode == signal_synth::delineation_scope_all_beats)
        {
            for (std::size_t lead = 0; lead < document.leads.size(); ++lead)
                output << "scope,all_beats,,," << csv_cell(document.leads[lead]) << ",,,\n";
        }
        else
        {
            for (std::size_t beat = 0; beat < document.beat_indices.size(); ++beat)
                for (std::size_t lead = 0; lead < document.leads.size(); ++lead)
                    output << "scope,selected_beats," << document.beat_indices[beat] << ",," << csv_cell(document.leads[lead]) << ",,,\n";
        }
        for (std::size_t i = 0; i < document.events.size(); ++i)
        {
            const signal_synth::delineation_event& event = document.events[i];
            output << "event,,," << event.beat_index << ',' << csv_cell(event.lead) << ',' << signal_synth::delineation_kind_name(event.kind) << ',' << event.time_seconds << ',';
            if (event.has_confidence)
                output << event.confidence;
            output << '\n';
        }
        return output.str();
    }
}

namespace signal_synth
{
    delineation_event::delineation_event() : beat_index(0), lead(), kind(delineation_p_onset), time_seconds(0.0), has_confidence(false), confidence(0.0), original_index(0) {}
    delineation_output_document::delineation_output_document() : schema_version(1), target_name("ecg_delineation"), algorithm(), scope_mode(delineation_scope_all_beats), beat_indices(), leads(), events() {}
    delineation_io_result::delineation_io_result() : success(false), messages(), canonical_json(), canonical_csv() {}

    const char* delineation_kind_name(delineation_kind kind)
    {
        const char* names[] = {"p_onset","p_peak","p_offset","qrs_onset","j_point","qrs_offset","t_onset","t_peak","t_offset"};
        return kind >= delineation_p_onset && kind < delineation_kind_count ? names[static_cast<int>(kind)] : "";
    }

    bool delineation_kind_from_name(const std::string& name, delineation_kind& kind)
    {
        for (int i = 0; i < static_cast<int>(delineation_kind_count); ++i)
        {
            const delineation_kind candidate = static_cast<delineation_kind>(i);
            if (name == delineation_kind_name(candidate))
            {
                kind = candidate;
                return true;
            }
        }
        return false;
    }

    const char* delineation_scope_mode_name(delineation_scope_mode mode)
    {
        return mode == delineation_scope_all_beats ? "all_beats" : mode == delineation_scope_selected_beats ? "selected_beats" : "";
    }

    bool delineation_scope_mode_from_name(const std::string& name, delineation_scope_mode& mode)
    {
        if (name == "all_beats")
        {
            mode = delineation_scope_all_beats;
            return true;
        }
        if (name == "selected_beats")
        {
            mode = delineation_scope_selected_beats;
            return true;
        }
        return false;
    }

    const char* delineation_io_message_code_name(delineation_io_message_code code)
    {
        const char* names[] = {"DELINEATION_IO_SYNTAX","DELINEATION_IO_TYPE","DELINEATION_IO_MISSING_FIELD","DELINEATION_IO_UNKNOWN_FIELD","DELINEATION_IO_RANGE","DELINEATION_IO_DUPLICATE","DELINEATION_IO_SCOPE"};
        return code >= delineation_io_syntax && code <= delineation_io_scope ? names[static_cast<int>(code)] : "DELINEATION_IO_UNKNOWN";
    }

    bool write_delineation_output(const delineation_output_document& document, delineation_io_result& result)
    {
        delineation_io_result fresh;
        delineation_output_document normalized = document;
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

    bool parse_delineation_json_v1(const std::string& json, delineation_output_document& output, delineation_io_result& result)
    {
        delineation_io_result fresh;
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
            add_message(fresh, delineation_io_type, "$", "root must be an object");
            result = fresh;
            return false;
        }
        const char* top_fields[] = {"schema_version","algorithm","target","scope","events"};
        allowed_fields(root, top_fields, sizeof(top_fields) / sizeof(top_fields[0]), "$", fresh);
        const json_value* schema = required(root, "schema_version", json_value::number_kind, "$", fresh);
        const json_value* algorithm = required(root, "algorithm", json_value::object_kind, "$", fresh);
        const json_value* target = required(root, "target", json_value::string_kind, "$", fresh);
        const json_value* scope = required(root, "scope", json_value::object_kind, "$", fresh);
        const json_value* events = required(root, "events", json_value::array_kind, "$", fresh);
        if (!schema || !algorithm || !target || !scope || !events)
        {
            result = fresh;
            return false;
        }
        delineation_output_document document;
        if (!parse_unsigned(*schema, document.schema_version))
            add_message(fresh, delineation_io_range, "$.schema_version", "schema_version must be integer 1");
        document.target_name = target->string;
        const char* algorithm_fields[] = {"name","version"};
        allowed_fields(*algorithm, algorithm_fields, sizeof(algorithm_fields) / sizeof(algorithm_fields[0]), "$.algorithm", fresh);
        const json_value* algorithm_name = required(*algorithm, "name", json_value::string_kind, "$.algorithm", fresh);
        const json_value* algorithm_version = required(*algorithm, "version", json_value::string_kind, "$.algorithm", fresh);
        if (algorithm_name) document.algorithm.name = algorithm_name->string;
        if (algorithm_version) document.algorithm.version = algorithm_version->string;
        const char* scope_fields[] = {"mode","beat_indices","leads"};
        allowed_fields(*scope, scope_fields, sizeof(scope_fields) / sizeof(scope_fields[0]), "$.scope", fresh);
        const json_value* mode = required(*scope, "mode", json_value::string_kind, "$.scope", fresh);
        const json_value* leads = required(*scope, "leads", json_value::array_kind, "$.scope", fresh);
        if (mode && !delineation_scope_mode_from_name(mode->string, document.scope_mode))
            add_message(fresh, delineation_io_range, "$.scope.mode", "mode must be all_beats or selected_beats");
        const json_value* beats = member(*scope, "beat_indices");
        if (beats && beats->type != json_value::array_kind)
            add_message(fresh, delineation_io_type, "$.scope.beat_indices", "beat_indices must be an array");
        else if (beats)
        {
            for (std::size_t i = 0; i < beats->array.size(); ++i)
            {
                unsigned long long beat = 0;
                if (beats->array[i].type != json_value::string_kind || !parse_uint64(beats->array[i].string, beat))
                    add_message(fresh, delineation_io_range, "$.scope.beat_indices[" + index_text(i) + "]", "beat index must be a canonical unsigned decimal string");
                else
                    document.beat_indices.push_back(beat);
            }
        }
        if (leads)
        {
            for (std::size_t i = 0; i < leads->array.size(); ++i)
            {
                if (leads->array[i].type != json_value::string_kind)
                    add_message(fresh, delineation_io_type, "$.scope.leads[" + index_text(i) + "]", "lead must be a string");
                else
                    document.leads.push_back(leads->array[i].string);
            }
        }
        for (std::size_t i = 0; i < events->array.size(); ++i)
        {
            const std::string path = "$.events[" + index_text(i) + "]";
            const json_value& item = events->array[i];
            if (item.type != json_value::object_kind)
            {
                add_message(fresh, delineation_io_type, path, "event must be an object");
                continue;
            }
            const char* event_fields[] = {"beat_index","lead","kind","time_seconds","confidence"};
            allowed_fields(item, event_fields, sizeof(event_fields) / sizeof(event_fields[0]), path, fresh);
            const json_value* beat = required(item, "beat_index", json_value::string_kind, path, fresh);
            const json_value* lead = required(item, "lead", json_value::string_kind, path, fresh);
            const json_value* kind = required(item, "kind", json_value::string_kind, path, fresh);
            const json_value* time = required(item, "time_seconds", json_value::number_kind, path, fresh);
            delineation_event event;
            event.original_index = static_cast<unsigned int>(i);
            if (beat && !parse_uint64(beat->string, event.beat_index))
                add_message(fresh, delineation_io_range, path + ".beat_index", "beat index must be a canonical unsigned decimal string");
            if (lead) event.lead = lead->string;
            if (kind && !delineation_kind_from_name(kind->string, event.kind))
                add_message(fresh, delineation_io_range, path + ".kind", "unsupported delineation kind");
            if (time) event.time_seconds = time->number;
            const json_value* confidence = member(item, "confidence");
            if (confidence && confidence->type != json_value::number_kind)
                add_message(fresh, delineation_io_type, path + ".confidence", "confidence must be a number");
            else if (confidence)
            {
                event.has_confidence = true;
                event.confidence = confidence->number;
            }
            document.events.push_back(event);
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

    bool parse_delineation_csv_v1(const std::string& csv, delineation_output_document& output, delineation_io_result& result)
    {
        delineation_io_result fresh;
        delineation_output_document document;
        document.algorithm.name = "csv_delineation_input";
        document.algorithm.version = "v1";
        std::size_t offset = 0;
        std::vector<std::string> header;
        std::string csv_message;
        while (read_csv_record(csv, offset, header, csv_message) && empty_record(header)) {}
        if (!csv_message.empty() || header.empty())
        {
            add_message(fresh, delineation_io_syntax, "$", csv_message.empty() ? "delineation CSV is empty" : csv_message);
            result = fresh;
            return false;
        }
        int row_type_column = -1, scope_mode_column = -1, evaluated_beat_column = -1, beat_column = -1;
        int lead_column = -1, kind_column = -1, time_column = -1, confidence_column = -1;
        std::set<std::string> seen;
        for (std::size_t i = 0; i < header.size(); ++i)
        {
            const std::string name = trim_ascii(header[i]);
            if (name.empty()) add_message(fresh, delineation_io_range, "$.header[" + index_text(i) + "]", "CSV header cell must not be empty");
            else if (!seen.insert(name).second) add_message(fresh, delineation_io_duplicate, "$.header[" + index_text(i) + "]", "duplicate CSV column");
            else if (name == "row_type") row_type_column = static_cast<int>(i);
            else if (name == "scope_mode") scope_mode_column = static_cast<int>(i);
            else if (name == "evaluated_beat_index") evaluated_beat_column = static_cast<int>(i);
            else if (name == "beat_index") beat_column = static_cast<int>(i);
            else if (name == "lead") lead_column = static_cast<int>(i);
            else if (name == "kind") kind_column = static_cast<int>(i);
            else if (name == "time_seconds") time_column = static_cast<int>(i);
            else if (name == "confidence") confidence_column = static_cast<int>(i);
            else add_message(fresh, delineation_io_unknown_field, "$.header[" + index_text(i) + "]", "unsupported CSV column");
        }
        if (row_type_column < 0) add_message(fresh, delineation_io_missing_field, "$.header.row_type", "delineation CSV must contain row_type");
        if (scope_mode_column < 0) add_message(fresh, delineation_io_missing_field, "$.header.scope_mode", "delineation CSV must contain scope_mode");
        if (evaluated_beat_column < 0) add_message(fresh, delineation_io_missing_field, "$.header.evaluated_beat_index", "delineation CSV must contain evaluated_beat_index");
        if (beat_column < 0) add_message(fresh, delineation_io_missing_field, "$.header.beat_index", "delineation CSV must contain beat_index");
        if (lead_column < 0) add_message(fresh, delineation_io_missing_field, "$.header.lead", "delineation CSV must contain lead");
        if (kind_column < 0) add_message(fresh, delineation_io_missing_field, "$.header.kind", "delineation CSV must contain kind");
        if (time_column < 0) add_message(fresh, delineation_io_missing_field, "$.header.time_seconds", "delineation CSV must contain time_seconds");
        if (!fresh.messages.empty())
        {
            result = fresh;
            return false;
        }
        bool has_scope = false;
        std::set<std::string> scope_rows;
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
                add_message(fresh, delineation_io_range, "$.rows[" + index_text(physical_row) + "]", "CSV row has more cells than the header");
                continue;
            }
            cells.resize(header.size());
            const std::string path = "$.rows[" + index_text(physical_row) + "]";
            const std::string row_type = cells[static_cast<std::size_t>(row_type_column)];
            if (row_type == "scope")
            {
                delineation_scope_mode mode;
                if (!delineation_scope_mode_from_name(cells[static_cast<std::size_t>(scope_mode_column)], mode))
                {
                    add_message(fresh, delineation_io_range, path + ".scope_mode", "scope_mode must be all_beats or selected_beats");
                    continue;
                }
                if (has_scope && document.scope_mode != mode)
                    add_message(fresh, delineation_io_scope, path + ".scope_mode", "all scope rows must use the same mode");
                document.scope_mode = mode;
                has_scope = true;
                const std::string lead = cells[static_cast<std::size_t>(lead_column)];
                if (lead.empty())
                    add_message(fresh, delineation_io_missing_field, path + ".lead", "scope row requires lead");
                else if (std::find(document.leads.begin(), document.leads.end(), lead) == document.leads.end())
                    document.leads.push_back(lead);
                unsigned long long beat = 0;
                const std::string evaluated = cells[static_cast<std::size_t>(evaluated_beat_column)];
                if (mode == delineation_scope_all_beats && !evaluated.empty())
                    add_message(fresh, delineation_io_scope, path + ".evaluated_beat_index", "all_beats scope row must not contain evaluated_beat_index");
                if (mode == delineation_scope_selected_beats)
                {
                    if (!parse_uint64(evaluated, beat))
                        add_message(fresh, delineation_io_range, path + ".evaluated_beat_index", "selected scope row requires a canonical unsigned decimal beat index");
                    else if (std::find(document.beat_indices.begin(), document.beat_indices.end(), beat) == document.beat_indices.end())
                        document.beat_indices.push_back(beat);
                }
                if (!cells[static_cast<std::size_t>(beat_column)].empty() || !cells[static_cast<std::size_t>(kind_column)].empty() || !cells[static_cast<std::size_t>(time_column)].empty() || (confidence_column >= 0 && !cells[static_cast<std::size_t>(confidence_column)].empty()))
                    add_message(fresh, delineation_io_scope, path, "scope row contains event-only values");
                const std::string key = delineation_scope_mode_name(mode) + std::string("|") + evaluated + "|" + lead;
                if (!scope_rows.insert(key).second)
                    add_message(fresh, delineation_io_duplicate, path, "duplicate scope row");
            }
            else if (row_type == "event")
            {
                if (!cells[static_cast<std::size_t>(scope_mode_column)].empty() || !cells[static_cast<std::size_t>(evaluated_beat_column)].empty())
                    add_message(fresh, delineation_io_scope, path, "event row contains scope-only values");
                delineation_event event;
                event.original_index = event_index++;
                if (!parse_uint64(cells[static_cast<std::size_t>(beat_column)], event.beat_index))
                    add_message(fresh, delineation_io_range, path + ".beat_index", "event requires a canonical unsigned decimal beat index");
                event.lead = cells[static_cast<std::size_t>(lead_column)];
                if (!delineation_kind_from_name(cells[static_cast<std::size_t>(kind_column)], event.kind))
                    add_message(fresh, delineation_io_range, path + ".kind", "unsupported delineation kind");
                if (!parse_double(cells[static_cast<std::size_t>(time_column)], event.time_seconds))
                    add_message(fresh, delineation_io_range, path + ".time_seconds", "invalid time_seconds");
                if (confidence_column >= 0 && !cells[static_cast<std::size_t>(confidence_column)].empty())
                {
                    event.has_confidence = parse_double(cells[static_cast<std::size_t>(confidence_column)], event.confidence);
                    if (!event.has_confidence)
                        add_message(fresh, delineation_io_range, path + ".confidence", "invalid confidence");
                }
                document.events.push_back(event);
            }
            else
                add_message(fresh, delineation_io_range, path + ".row_type", "row_type must be scope or event");
        }
        if (!csv_message.empty())
            add_message(fresh, delineation_io_syntax, "$.rows", csv_message);
        if (!has_scope)
            add_message(fresh, delineation_io_missing_field, "$.rows", "delineation CSV requires at least one scope row");
        else
        {
            const std::size_t expected_scope_rows = document.scope_mode == delineation_scope_all_beats ? document.leads.size() : document.leads.size() * document.beat_indices.size();
            if (scope_rows.size() != expected_scope_rows)
                add_message(fresh, delineation_io_scope, "$.rows", "scope rows must enumerate every evaluated beat and lead combination exactly once");
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
}
