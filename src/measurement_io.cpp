#include "measurement_io.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <utility>

namespace
{
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
                    default: return fail("unsupported string escape");
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
                if (digits == offset)
                    return fail("invalid number fraction");
            }
            if (offset < text.size() && (text[offset] == 'e' || text[offset] == 'E'))
            {
                ++offset;
                if (offset < text.size() && (text[offset] == '+' || text[offset] == '-'))
                    ++offset;
                const std::size_t digits = offset;
                while (offset < text.size() && text[offset] >= '0' && text[offset] <= '9')
                    ++offset;
                if (digits == offset)
                    return fail("invalid number exponent");
            }
            const std::string token = text.substr(start, offset - start);
            errno = 0;
            char* end = 0;
            value.number = std::strtod(token.c_str(), &end);
            if (!end || *end || errno == ERANGE || !std::isfinite(value.number))
                return fail("number must be finite");
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
            if (text.compare(offset, 4, "true") == 0 || text.compare(offset, 5, "false") == 0)
            {
                offset += text[offset] == 't' ? 4 : 5;
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

    const json_value* member(const json_value& value, const char* name)
    {
        for (std::size_t i = 0; i < value.object.size(); ++i)
            if (value.object[i].first == name)
                return &value.object[i].second;
        return 0;
    }

    void add_message(signal_synth::measurement_io_result& result, signal_synth::measurement_io_message_code code, const std::string& path, const std::string& message)
    {
        signal_synth::measurement_io_message item;
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

    bool parse_u64_string(const std::string& text, unsigned long long& output)
    {
        if (text.empty() || (text.size() > 1u && text[0] == '0'))
            return false;
        unsigned long long value = 0;
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            if (text[i] < '0' || text[i] > '9')
                return false;
            const unsigned int digit = static_cast<unsigned int>(text[i] - '0');
            if (value > (std::numeric_limits<unsigned long long>::max() - digit) / 10u)
                return false;
            value = value * 10u + digit;
        }
        output = value;
        return true;
    }

    bool safe_name(const std::string& value)
    {
        if (value.empty() || value.size() > 128u || value[0] < 'a' || value[0] > 'z')
            return false;
        for (std::size_t i = 0; i < value.size(); ++i)
            if (!((value[i] >= 'a' && value[i] <= 'z') || (value[i] >= '0' && value[i] <= '9') || value[i] == '_' || value[i] == '.'))
                return false;
        return true;
    }

    bool safe_text(const std::string& value, std::size_t maximum)
    {
        if (value.size() > maximum)
            return false;
        for (std::size_t i = 0; i < value.size(); ++i)
            if (static_cast<unsigned char>(value[i]) < 0x20u)
                return false;
        return true;
    }

    bool supported_unit(const std::string& unit)
    {
        static const char* units[] = {"s", "s2", "mV", "mV/s", "deg", "count", "ratio", "%", "bpm", "a.u.", "bool"};
        for (std::size_t i = 0; i < sizeof(units) / sizeof(units[0]); ++i)
            if (unit == units[i])
                return true;
        return false;
    }

    bool supported_qt_formula(const std::string& formula)
    {
        return formula == "fixed" || formula == "bazett" || formula == "fridericia" || formula == "framingham" || formula == "hodges";
    }

    bool validate_measurement(signal_synth::measurement_value& item, const std::string& path, signal_synth::measurement_io_result& result)
    {
        bool valid = true;
        if (!safe_name(item.name))
        {
            add_message(result, signal_synth::measurement_io_invalid_value, path + ".name", "name must be lower-case ASCII dotted snake case and at most 128 characters");
            valid = false;
        }
        if (!supported_unit(item.unit))
        {
            add_message(result, signal_synth::measurement_io_invalid_value, path + ".unit", "unsupported measurement unit");
            valid = false;
        }
        if (item.status == signal_synth::measurement_valid && !item.has_value)
        {
            add_message(result, signal_synth::measurement_io_missing_field, path + ".value", "valid measurement requires a finite value");
            valid = false;
        }
        if (item.status != signal_synth::measurement_valid && item.has_value)
        {
            add_message(result, signal_synth::measurement_io_invalid_value, path + ".value", "non-valid measurement must not contain value");
            valid = false;
        }
        if (item.unit == "bool" && item.has_value && item.value != 0.0 && item.value != 1.0)
        {
            add_message(result, signal_synth::measurement_io_invalid_value, path + ".value", "bool measurement value must be zero or one");
            valid = false;
        }
        if (item.has_time_seconds && item.time_seconds < 0.0)
        {
            add_message(result, signal_synth::measurement_io_invalid_value, path + ".time_seconds", "time_seconds must be non-negative");
            valid = false;
        }
        if (item.has_confidence && (item.confidence < 0.0 || item.confidence > 1.0))
        {
            add_message(result, signal_synth::measurement_io_invalid_value, path + ".confidence", "confidence must be in [0,1]");
            valid = false;
        }
        if (!safe_text(item.channel, 128u) || !safe_text(item.formula, 64u))
        {
            add_message(result, signal_synth::measurement_io_invalid_value, path, "channel or formula is too long or contains control characters");
            valid = false;
        }
        const bool beat_like = item.scope == signal_synth::measurement_beat || item.scope == signal_synth::measurement_beat_lead || item.scope == signal_synth::measurement_paired_signal;
        const bool channel_scope = item.scope == signal_synth::measurement_lead || item.scope == signal_synth::measurement_beat_lead || item.scope == signal_synth::measurement_paired_signal;
        if (beat_like && !item.has_time_seconds && !item.has_beat_index)
        {
            add_message(result, signal_synth::measurement_io_missing_field, path, "beat-like scope requires time_seconds or beat_index");
            valid = false;
        }
        if (!beat_like && (item.has_time_seconds || item.has_beat_index))
        {
            add_message(result, signal_synth::measurement_io_invalid_value, path, "record and lead scopes must not contain a temporal or beat anchor");
            valid = false;
        }
        if (channel_scope && item.channel.empty())
        {
            add_message(result, signal_synth::measurement_io_missing_field, path + ".channel", "scope requires a channel or signal-pair name");
            valid = false;
        }
        if (!channel_scope && !item.channel.empty())
        {
            add_message(result, signal_synth::measurement_io_invalid_value, path + ".channel", "scope must not contain channel");
            valid = false;
        }
        if (item.name == "qtc_interval" && !supported_qt_formula(item.formula))
        {
            add_message(result, signal_synth::measurement_io_invalid_value, path + ".formula", "qtc_interval requires fixed, bazett, fridericia, framingham, or hodges formula");
            valid = false;
        }
        return valid;
    }

    bool allowed_fields(const json_value& object, const std::string& path, signal_synth::measurement_io_result& result)
    {
        static const char* allowed[] = {"name", "value", "unit", "status", "scope", "time_seconds", "beat_index", "channel", "formula", "confidence"};
        bool valid = true;
        for (std::size_t i = 0; i < object.object.size(); ++i)
        {
            bool found = false;
            for (std::size_t field = 0; field < sizeof(allowed) / sizeof(allowed[0]); ++field)
                found = found || object.object[i].first == allowed[field];
            if (!found)
            {
                add_message(result, signal_synth::measurement_io_unknown_field, path + "." + object.object[i].first, "unknown measurement field");
                valid = false;
            }
        }
        return valid;
    }

    const json_value* required(const json_value& object, const char* name, json_value::kind kind, const std::string& path, signal_synth::measurement_io_result& result)
    {
        const json_value* value = member(object, name);
        if (!value)
        {
            add_message(result, signal_synth::measurement_io_missing_field, path + "." + name, "missing required field");
            return 0;
        }
        if (value->type != kind)
        {
            add_message(result, signal_synth::measurement_io_invalid_value, path + "." + name, "wrong JSON type");
            return 0;
        }
        return value;
    }

    bool measurement_from_json(const json_value& value, std::size_t index, signal_synth::measurement_value& output, signal_synth::measurement_io_result& result)
    {
        const std::string path = "$.measurements[" + index_text(index) + "]";
        if (value.type != json_value::object_kind)
        {
            add_message(result, signal_synth::measurement_io_invalid_value, path, "measurement must be an object");
            return false;
        }
        bool valid = allowed_fields(value, path, result);
        const json_value* name = required(value, "name", json_value::string_kind, path, result);
        const json_value* unit = required(value, "unit", json_value::string_kind, path, result);
        const json_value* status = required(value, "status", json_value::string_kind, path, result);
        const json_value* scope = required(value, "scope", json_value::string_kind, path, result);
        if (!name || !unit || !status || !scope)
            valid = false;
        signal_synth::measurement_value item;
        item.original_index = static_cast<unsigned int>(index);
        if (name) item.name = name->string;
        if (unit) item.unit = unit->string;
        if (status && !signal_synth::measurement_status_from_name(status->string, item.status))
        {
            add_message(result, signal_synth::measurement_io_invalid_value, path + ".status", "unknown measurement status");
            valid = false;
        }
        if (scope && !signal_synth::measurement_scope_from_name(scope->string, item.scope))
        {
            add_message(result, signal_synth::measurement_io_invalid_value, path + ".scope", "unknown measurement scope");
            valid = false;
        }
        const json_value* numeric = member(value, "value");
        if (numeric)
        {
            if (numeric->type != json_value::number_kind)
            {
                add_message(result, signal_synth::measurement_io_invalid_value, path + ".value", "value must be a finite number");
                valid = false;
            }
            else
            {
                item.value = numeric->number;
                item.has_value = true;
            }
        }
        const json_value* time = member(value, "time_seconds");
        if (time)
        {
            if (time->type != json_value::number_kind)
            {
                add_message(result, signal_synth::measurement_io_invalid_value, path + ".time_seconds", "time_seconds must be a finite number");
                valid = false;
            }
            else
            {
                item.time_seconds = time->number;
                item.has_time_seconds = true;
            }
        }
        const json_value* beat = member(value, "beat_index");
        if (beat)
        {
            if (beat->type != json_value::string_kind || !parse_u64_string(beat->string, item.beat_index))
            {
                add_message(result, signal_synth::measurement_io_invalid_value, path + ".beat_index", "beat_index must be a canonical unsigned decimal string");
                valid = false;
            }
            else
                item.has_beat_index = true;
        }
        const json_value* channel = member(value, "channel");
        if (channel)
        {
            if (channel->type != json_value::string_kind)
            {
                add_message(result, signal_synth::measurement_io_invalid_value, path + ".channel", "channel must be a string");
                valid = false;
            }
            else item.channel = channel->string;
        }
        const json_value* formula = member(value, "formula");
        if (formula)
        {
            if (formula->type != json_value::string_kind)
            {
                add_message(result, signal_synth::measurement_io_invalid_value, path + ".formula", "formula must be a string");
                valid = false;
            }
            else item.formula = formula->string;
        }
        const json_value* confidence = member(value, "confidence");
        if (confidence)
        {
            if (confidence->type != json_value::number_kind)
            {
                add_message(result, signal_synth::measurement_io_invalid_value, path + ".confidence", "confidence must be a finite number");
                valid = false;
            }
            else
            {
                item.confidence = confidence->number;
                item.has_confidence = true;
            }
        }
        valid = validate_measurement(item, path, result) && valid;
        output = item;
        return valid;
    }

    std::string identity(const signal_synth::measurement_value& value)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10)
               << value.name << '\n' << value.unit << '\n' << signal_synth::measurement_scope_name(value.scope) << '\n'
               << value.channel << '\n' << value.formula << '\n';
        if (value.has_beat_index)
            output << 'b' << value.beat_index;
        else if (value.has_time_seconds)
            output << 't' << value.time_seconds;
        else
            output << 'r';
        return output.str();
    }

    bool unique_measurements(const std::vector<signal_synth::measurement_value>& values, signal_synth::measurement_io_result& result)
    {
        std::set<std::string> identities;
        bool valid = true;
        for (std::size_t i = 0; i < values.size(); ++i)
        {
            if (!identities.insert(identity(values[i])).second)
            {
                add_message(result, signal_synth::measurement_io_duplicate, "$.measurements[" + index_text(i) + "]", "duplicate measurement identity");
                valid = false;
            }
        }
        return valid;
    }

    std::string json_string(const std::string& value)
    {
        static const char hex[] = "0123456789abcdef";
        std::string output("\"");
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            const unsigned char ch = static_cast<unsigned char>(value[i]);
            switch (ch)
            {
            case '"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (ch < 0x20u)
                {
                    output += "\\u00";
                    output.push_back(hex[ch >> 4]);
                    output.push_back(hex[ch & 0x0fu]);
                }
                else output.push_back(static_cast<char>(ch));
            }
        }
        output.push_back('"');
        return output;
    }

    std::string canonical_json(const signal_synth::measurement_output_document& document)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10)
               << "{\"schema_version\":1,\"measurements\":[";
        for (std::size_t i = 0; i < document.measurements.size(); ++i)
        {
            const signal_synth::measurement_value& item = document.measurements[i];
            output << (i ? "," : "") << "{\"name\":" << json_string(item.name);
            if (item.has_value) output << ",\"value\":" << item.value;
            output << ",\"unit\":" << json_string(item.unit)
                   << ",\"status\":" << json_string(signal_synth::measurement_status_name(item.status))
                   << ",\"scope\":" << json_string(signal_synth::measurement_scope_name(item.scope));
            if (item.has_time_seconds) output << ",\"time_seconds\":" << item.time_seconds;
            if (item.has_beat_index) output << ",\"beat_index\":\"" << item.beat_index << '"';
            if (!item.channel.empty()) output << ",\"channel\":" << json_string(item.channel);
            if (!item.formula.empty()) output << ",\"formula\":" << json_string(item.formula);
            if (item.has_confidence) output << ",\"confidence\":" << item.confidence;
            output << '}';
        }
        output << "]}";
        return output.str();
    }

    std::string csv_cell(const std::string& value)
    {
        bool quote = false;
        for (std::size_t i = 0; i < value.size(); ++i)
            quote = quote || value[i] == ',' || value[i] == '"' || value[i] == '\r' || value[i] == '\n';
        if (!quote)
            return value;
        std::string output("\"");
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            if (value[i] == '"') output += "\"\"";
            else output.push_back(value[i]);
        }
        output.push_back('"');
        return output;
    }

    std::string number_text(double value)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
        return output.str();
    }

    std::string canonical_csv(const signal_synth::measurement_output_document& document)
    {
        std::ostringstream output;
        output << "name,value,unit,status,scope,time_seconds,beat_index,channel,formula,confidence\n";
        for (std::size_t i = 0; i < document.measurements.size(); ++i)
        {
            const signal_synth::measurement_value& item = document.measurements[i];
            output << csv_cell(item.name) << ',' << (item.has_value ? number_text(item.value) : std::string()) << ',' << csv_cell(item.unit) << ','
                   << signal_synth::measurement_status_name(item.status) << ',' << signal_synth::measurement_scope_name(item.scope) << ','
                   << (item.has_time_seconds ? number_text(item.time_seconds) : std::string()) << ',';
            if (item.has_beat_index) output << item.beat_index;
            output << ',' << csv_cell(item.channel) << ',' << csv_cell(item.formula) << ',';
            if (item.has_confidence) output << number_text(item.confidence);
            output << '\n';
        }
        return output.str();
    }

    bool parse_finite(const std::string& text, double& value)
    {
        if (text.empty()) return false;
        errno = 0;
        char* end = 0;
        value = std::strtod(text.c_str(), &end);
        return end && *end == 0 && errno != ERANGE && std::isfinite(value);
    }

    bool parse_csv_rows(const std::string& input, std::vector<std::vector<std::string> >& rows, std::string& error)
    {
        rows.clear();
        std::vector<std::string> row;
        std::string cell;
        bool quoted = false;
        bool quote_closed = false;
        for (std::size_t i = 0; i <= input.size(); ++i)
        {
            const char ch = i < input.size() ? input[i] : '\n';
            if (quoted)
            {
                if (ch == '"')
                {
                    if (i + 1u < input.size() && input[i + 1u] == '"')
                    {
                        cell.push_back('"');
                        ++i;
                    }
                    else { quoted = false; quote_closed = true; }
                }
                else cell.push_back(ch);
                continue;
            }
            if (quote_closed && ch != ',' && ch != '\r' && ch != '\n')
            {
                error = "characters after closing CSV quote";
                return false;
            }
            if (ch == '"' && cell.empty() && !quote_closed)
            {
                quoted = true;
                continue;
            }
            if (ch == '"')
            {
                error = "quote inside unquoted CSV field";
                return false;
            }
            if (ch == ',')
            {
                row.push_back(cell);
                cell.clear();
                quote_closed = false;
                continue;
            }
            if (ch == '\r' || ch == '\n')
            {
                if (ch == '\r' && i + 1u < input.size() && input[i + 1u] == '\n') ++i;
                row.push_back(cell);
                cell.clear();
                quote_closed = false;
                if (!(row.size() == 1u && row[0].empty())) rows.push_back(row);
                row.clear();
                continue;
            }
            cell.push_back(ch);
        }
        if (quoted)
        {
            error = "unterminated quoted CSV field";
            return false;
        }
        return true;
    }

    bool measurement_from_csv(const std::vector<std::string>& row, std::size_t index, signal_synth::measurement_value& output, signal_synth::measurement_io_result& result)
    {
        const std::string path = "$.measurements[" + index_text(index) + "]";
        signal_synth::measurement_value item;
        item.original_index = static_cast<unsigned int>(index);
        item.name = row[0];
        item.unit = row[2];
        bool valid = true;
        if (!signal_synth::measurement_status_from_name(row[3], item.status))
        {
            add_message(result, signal_synth::measurement_io_invalid_value, path + ".status", "unknown measurement status");
            valid = false;
        }
        if (!signal_synth::measurement_scope_from_name(row[4], item.scope))
        {
            add_message(result, signal_synth::measurement_io_invalid_value, path + ".scope", "unknown measurement scope");
            valid = false;
        }
        if (!row[1].empty())
        {
            item.has_value = parse_finite(row[1], item.value);
            if (!item.has_value)
            {
                add_message(result, signal_synth::measurement_io_invalid_value, path + ".value", "value must be a finite number");
                valid = false;
            }
        }
        if (!row[5].empty())
        {
            item.has_time_seconds = parse_finite(row[5], item.time_seconds);
            if (!item.has_time_seconds)
            {
                add_message(result, signal_synth::measurement_io_invalid_value, path + ".time_seconds", "time_seconds must be a finite number");
                valid = false;
            }
        }
        if (!row[6].empty())
        {
            item.has_beat_index = parse_u64_string(row[6], item.beat_index);
            if (!item.has_beat_index)
            {
                add_message(result, signal_synth::measurement_io_invalid_value, path + ".beat_index", "beat_index must be a canonical unsigned decimal string");
                valid = false;
            }
        }
        item.channel = row[7];
        item.formula = row[8];
        if (!row[9].empty())
        {
            item.has_confidence = parse_finite(row[9], item.confidence);
            if (!item.has_confidence)
            {
                add_message(result, signal_synth::measurement_io_invalid_value, path + ".confidence", "confidence must be a finite number");
                valid = false;
            }
        }
        valid = validate_measurement(item, path, result) && valid;
        output = item;
        return valid;
    }

    void finalize(const signal_synth::measurement_output_document& document, signal_synth::measurement_io_result& result)
    {
        result.success = result.messages.empty();
        if (result.success)
        {
            result.canonical_json = canonical_json(document);
            result.canonical_csv = canonical_csv(document);
        }
    }
}

namespace signal_synth
{
    measurement_value::measurement_value()
        : name(), value(0.0), has_value(false), unit(), status(measurement_valid), scope(measurement_record), time_seconds(0.0), has_time_seconds(false), beat_index(0), has_beat_index(false), channel(), formula(), confidence(0.0), has_confidence(false), original_index(0) {}

    measurement_output_document::measurement_output_document() : schema_version(1), measurements() {}
    measurement_io_result::measurement_io_result() : success(false), messages(), canonical_json(), canonical_csv() {}

    const char* measurement_status_name(measurement_status status)
    {
        switch (status)
        {
        case measurement_valid: return "valid";
        case measurement_undefined: return "undefined";
        case measurement_absent: return "absent";
        case measurement_not_evaluable: return "not_evaluable";
        }
        return "undefined";
    }

    bool measurement_status_from_name(const std::string& name, measurement_status& status)
    {
        if (name == "valid") status = measurement_valid;
        else if (name == "undefined") status = measurement_undefined;
        else if (name == "absent") status = measurement_absent;
        else if (name == "not_evaluable") status = measurement_not_evaluable;
        else return false;
        return true;
    }

    const char* measurement_scope_name(measurement_scope scope)
    {
        switch (scope)
        {
        case measurement_record: return "record";
        case measurement_lead: return "lead";
        case measurement_beat: return "beat";
        case measurement_beat_lead: return "beat_lead";
        case measurement_paired_signal: return "paired_signal";
        }
        return "record";
    }

    bool measurement_scope_from_name(const std::string& name, measurement_scope& scope)
    {
        if (name == "record") scope = measurement_record;
        else if (name == "lead") scope = measurement_lead;
        else if (name == "beat") scope = measurement_beat;
        else if (name == "beat_lead") scope = measurement_beat_lead;
        else if (name == "paired_signal") scope = measurement_paired_signal;
        else return false;
        return true;
    }

    const char* measurement_io_message_code_name(measurement_io_message_code code)
    {
        switch (code)
        {
        case measurement_io_syntax: return "MEASUREMENT_SYNTAX";
        case measurement_io_schema: return "MEASUREMENT_SCHEMA";
        case measurement_io_missing_field: return "MEASUREMENT_MISSING_FIELD";
        case measurement_io_unknown_field: return "MEASUREMENT_UNKNOWN_FIELD";
        case measurement_io_invalid_value: return "MEASUREMENT_INVALID_VALUE";
        case measurement_io_duplicate: return "MEASUREMENT_DUPLICATE";
        }
        return "MEASUREMENT_ERROR";
    }

    bool parse_measurement_values_json_v1(const std::string& input, measurement_output_document& output, measurement_io_result& result)
    {
        measurement_io_result fresh;
        measurement_output_document document;
        json_value root;
        json_parser parser(input);
        if (!parser.parse(root))
        {
            add_message(fresh, measurement_io_syntax, "$", parser.error);
            result = fresh;
            return false;
        }
        if (root.type != json_value::object_kind)
            add_message(fresh, measurement_io_schema, "$", "measurement JSON root must be an object");
        else
        {
            for (std::size_t i = 0; i < root.object.size(); ++i)
                if (root.object[i].first != "schema_version" && root.object[i].first != "measurements")
                    add_message(fresh, measurement_io_unknown_field, "$." + root.object[i].first, "unknown root field");
            const json_value* version = member(root, "schema_version");
            const json_value* measurements = member(root, "measurements");
            if (!version)
                add_message(fresh, measurement_io_missing_field, "$.schema_version", "missing schema_version");
            else if (version->type != json_value::number_kind || version->number != 1.0)
                add_message(fresh, measurement_io_schema, "$.schema_version", "measurement JSON requires schema_version 1");
            if (!measurements)
                add_message(fresh, measurement_io_missing_field, "$.measurements", "missing measurements array");
            else if (measurements->type != json_value::array_kind)
                add_message(fresh, measurement_io_schema, "$.measurements", "measurements must be an array");
            else
            {
                for (std::size_t i = 0; i < measurements->array.size(); ++i)
                {
                    measurement_value item;
                    measurement_from_json(measurements->array[i], i, item, fresh);
                    document.measurements.push_back(item);
                }
                unique_measurements(document.measurements, fresh);
            }
        }
        finalize(document, fresh);
        if (fresh.success) output = document;
        result = fresh;
        return fresh.success;
    }

    bool parse_measurement_values_csv_v1(const std::string& input, measurement_output_document& output, measurement_io_result& result)
    {
        measurement_io_result fresh;
        measurement_output_document document;
        std::vector<std::vector<std::string> > rows;
        std::string error;
        if (!parse_csv_rows(input, rows, error))
            add_message(fresh, measurement_io_syntax, "$", error);
        static const char* header[] = {"name", "value", "unit", "status", "scope", "time_seconds", "beat_index", "channel", "formula", "confidence"};
        if (rows.empty())
            add_message(fresh, measurement_io_schema, "$", "measurement CSV requires a header");
        else
        {
            bool header_valid = rows[0].size() == sizeof(header) / sizeof(header[0]);
            for (std::size_t i = 0; header_valid && i < rows[0].size(); ++i)
                header_valid = rows[0][i] == header[i];
            if (!header_valid)
                add_message(fresh, measurement_io_schema, "$", "measurement CSV header must exactly match the v1 column order");
            else
            {
                for (std::size_t row = 1; row < rows.size(); ++row)
                {
                    if (rows[row].size() != sizeof(header) / sizeof(header[0]))
                    {
                        add_message(fresh, measurement_io_schema, "$.measurements[" + index_text(row - 1u) + "]", "CSV row has the wrong number of columns");
                        continue;
                    }
                    measurement_value item;
                    measurement_from_csv(rows[row], row - 1u, item, fresh);
                    document.measurements.push_back(item);
                }
                unique_measurements(document.measurements, fresh);
            }
        }
        finalize(document, fresh);
        if (fresh.success) output = document;
        result = fresh;
        return fresh.success;
    }
}
