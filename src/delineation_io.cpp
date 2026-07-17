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

        bool fail(signal_synth::delineation_io_message_code value, const char* message)
        {
            code = value;
            error = message;
            return false;
        }

        void skip_ws()
        {
            while (offset < text.size() && (text[offset] == ' ' || text[offset] == '\t' || text[offset] == '\r' || text[offset] == '\n'))
                ++offset;
        }

        bool parse_string(std::string& output)
        {
            if (offset >= text.size() || text[offset] != '"')
                return fail(signal_synth::delineation_io_syntax, "expected string");
            ++offset;
            output.clear();
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
                    if (escaped == '"' || escaped == '\\' || escaped == '/') output.push_back(escaped);
                    else if (escaped == 'b') output.push_back('\b');
                    else if (escaped == 'f') output.push_back('\f');
                    else if (escaped == 'n') output.push_back('\n');
                    else if (escaped == 'r') output.push_back('\r');
                    else if (escaped == 't') output.push_back('\t');
                    else return fail(signal_synth::delineation_io_syntax, "unsupported escape");
                }
                else if (ch < 0x20)
                    return fail(signal_synth::delineation_io_syntax, "control character in string");
                else
                    output.push_back(static_cast<char>(ch));
            }
            return fail(signal_synth::delineation_io_syntax, "unterminated string");
        }

        static bool valid_number(const std::string& value)
        {
            if (value.empty()) return false;
            std::size_t index = value[0] == '-' ? 1u : 0u;
            if (index == value.size()) return false;
            if (value[index] == '0')
            {
                ++index;
                if (index < value.size() && value[index] >= '0' && value[index] <= '9') return false;
            }
            else if (value[index] >= '1' && value[index] <= '9')
                while (index < value.size() && value[index] >= '0' && value[index] <= '9') ++index;
            else return false;
            if (index < value.size() && value[index] == '.')
            {
                const std::size_t start = ++index;
                while (index < value.size() && value[index] >= '0' && value[index] <= '9') ++index;
                if (index == start) return false;
            }
            if (index < value.size() && (value[index] == 'e' || value[index] == 'E'))
            {
                ++index;
                if (index < value.size() && (value[index] == '+' || value[index] == '-')) ++index;
                const std::size_t start = index;
                while (index < value.size() && value[index] >= '0' && value[index] <= '9') ++index;
                if (index == start) return false;
            }
            return index == value.size();
        }

        bool parse_value(json_value& value)
        {
            skip_ws();
            if (offset >= text.size()) return fail(signal_synth::delineation_io_syntax, "unexpected end of input");
            if (text[offset] == '"') { value.type = json_value::string_kind; return parse_string(value.string); }
            if (text[offset] == '{') return parse_object(value);
            if (text[offset] == '[') return parse_array(value);
            const std::size_t start = offset;
            while (offset < text.size() && text[offset] != ',' && text[offset] != '}' && text[offset] != ']' && text[offset] != ' ' && text[offset] != '\t' && text[offset] != '\r' && text[offset] != '\n') ++offset;
            value.token = text.substr(start, offset - start);
            if (value.token == "null") { value.type = json_value::null_kind; return true; }
            if (value.token == "true" || value.token == "false") { value.type = json_value::bool_kind; return true; }
            if (!valid_number(value.token)) return fail(signal_synth::delineation_io_syntax, "invalid token");
            errno = 0;
            char* end = 0;
            value.number = std::strtod(value.token.c_str(), &end);
            if (!end || *end || errno == ERANGE || !std::isfinite(value.number)) return fail(signal_synth::delineation_io_syntax, "invalid number");
            value.type = json_value::number_kind;
            return true;
        }

        bool parse_array(json_value& value)
        {
            value.type = json_value::array_kind;
            ++offset;
            skip_ws();
            if (offset < text.size() && text[offset] == ']') { ++offset; return true; }
            while (offset < text.size())
            {
                json_value item;
                if (!parse_value(item)) return false;
                value.array.push_back(item);
                skip_ws();
                if (offset < text.size() && text[offset] == ',') { ++offset; continue; }
                if (offset < text.size() && text[offset] == ']') { ++offset; return true; }
                return fail(signal_synth::delineation_io_syntax, "expected comma or array end");
            }
            return fail(signal_synth::delineation_io_syntax, "unterminated array");
        }

        bool parse_object(json_value& value)
        {
            value.type = json_value::object_kind;
            ++offset;
            skip_ws();
            if (offset < text.size() && text[offset] == '}') { ++offset; return true; }
            std::set<std::string> keys;
            while (offset < text.size())
            {
                skip_ws();
                std::string key;
                if (!parse_string(key)) return false;
                if (!keys.insert(key).second) return fail(signal_synth::delineation_io_duplicate, "duplicate object key");
                skip_ws();
                if (offset >= text.size() || text[offset] != ':') return fail(signal_synth::delineation_io_syntax, "expected colon");
                ++offset;
                json_value item;
                if (!parse_value(item)) return false;
                value.object.push_back(std::make_pair(key, item));
                skip_ws();
                if (offset < text.size() && text[offset] == ',') { ++offset; continue; }
                if (offset < text.size() && text[offset] == '}') { ++offset; return true; }
                return fail(signal_synth::delineation_io_syntax, "expected comma or object end");
            }
            return fail(signal_synth::delineation_io_syntax, "unterminated object");
        }

        bool parse_root(json_value& value)
        {
            if (!parse_value(value)) return false;
            skip_ws();
            return offset == text.size() || fail(signal_synth::delineation_io_syntax, "trailing data");
        }
    };

    const json_value* member(const json_value& object, const char* name)
    {
        for (std::size_t i = 0; i < object.object.size(); ++i)
            if (object.object[i].first == name) return &object.object[i].second;
        return 0;
    }

    void message(signal_synth::delineation_io_result& result, signal_synth::delineation_io_message_code code, const std::string& path, const std::string& text)
    {
        signal_synth::delineation_io_message item;
        item.code = code;
        item.path = path;
        item.message = text;
        result.messages.push_back(item);
    }

    std::string index_text(std::size_t index)
    {
        std::ostringstream output;
        output << index;
        return output.str();
    }

    bool allowed_fields(const json_value& object, const char* const* names, std::size_t count, const std::string& path, signal_synth::delineation_io_result& result)
    {
        bool ok = true;
        for (std::size_t i = 0; i < object.object.size(); ++i)
        {
            bool found = false;
            for (std::size_t n = 0; n < count; ++n) found = found || object.object[i].first == names[n];
            if (!found) { message(result, signal_synth::delineation_io_unknown_field, path + "." + object.object[i].first, "unknown field"); ok = false; }
        }
        return ok;
    }

    const json_value* required(const json_value& object, const char* name, json_value::kind kind, const std::string& path, signal_synth::delineation_io_result& result)
    {
        const json_value* value = member(object, name);
        if (!value) message(result, signal_synth::delineation_io_missing_field, path + "." + name, "required field is missing");
        else if (value->type != kind) message(result, signal_synth::delineation_io_type, path + "." + name, "field has the wrong JSON type");
        else return value;
        return 0;
    }

    int lead_rank(const std::string& lead)
    {
        const char* names[] = {"I","II","III","aVR","aVL","aVF","V1","V2","V3","V4","V5","V6"};
        for (int i = 0; i < 12; ++i) if (lead == names[i]) return i;
        return -1;
    }

    bool event_less(const signal_synth::delineation_event& left, const signal_synth::delineation_event& right)
    {
        if (left.time_seconds != right.time_seconds) return left.time_seconds < right.time_seconds;
        if (left.lead != right.lead) return lead_rank(left.lead) < lead_rank(right.lead);
        if (left.kind != right.kind) return left.kind < right.kind;
        return left.original_index < right.original_index;
    }

    bool validate(signal_synth::delineation_output_document& document, signal_synth::delineation_io_result& result)
    {
        if (document.schema_version != 1u) message(result, signal_synth::delineation_io_range, "$.schema_version", "only schema version 1 is supported");
        std::set<std::string> duplicates;
        for (std::size_t i = 0; i < document.events.size(); ++i)
        {
            signal_synth::delineation_event& event = document.events[i];
            const std::string path = "$.events[" + index_text(i) + "]";
            if (lead_rank(event.lead) < 0) message(result, signal_synth::delineation_io_range, path + ".channel", "channel must be a standard ECG lead");
            if (event.kind < signal_synth::delineation_p_onset || event.kind >= signal_synth::delineation_kind_count) message(result, signal_synth::delineation_io_range, path + ".label", "label must be a supported delineation kind");
            if (!std::isfinite(event.time_seconds) || event.time_seconds < 0.0) message(result, signal_synth::delineation_io_range, path + ".time_seconds", "time_seconds must be finite and non-negative");
            if (event.has_confidence && (!std::isfinite(event.confidence) || event.confidence < 0.0 || event.confidence > 1.0)) message(result, signal_synth::delineation_io_range, path + ".confidence", "confidence must be in [0,1]");
            std::ostringstream key;
            key.imbue(std::locale::classic());
            key << std::setprecision(17) << event.time_seconds << '|' << event.lead << '|' << signal_synth::delineation_kind_name(event.kind);
            if (!duplicates.insert(key.str()).second) message(result, signal_synth::delineation_io_duplicate, path, "duplicate point event");
        }
        if (!result.messages.empty()) return false;
        std::stable_sort(document.events.begin(), document.events.end(), event_less);
        return true;
    }

    std::string json_string(const std::string& value)
    {
        std::ostringstream output;
        output << '"';
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            const unsigned char ch = static_cast<unsigned char>(value[i]);
            if (ch == '"') output << "\\\"";
            else if (ch == '\\') output << "\\\\";
            else if (ch == '\n') output << "\\n";
            else if (ch == '\r') output << "\\r";
            else if (ch == '\t') output << "\\t";
            else if (ch < 0x20) output << "\\u00" << "0123456789abcdef"[ch >> 4] << "0123456789abcdef"[ch & 15u];
            else output << static_cast<char>(ch);
        }
        return output.str() + '"';
    }

    std::string canonical_json(const signal_synth::delineation_output_document& document)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10) << "{\"schema_version\":1,\"events\":[";
        for (std::size_t i = 0; i < document.events.size(); ++i)
        {
            const signal_synth::delineation_event& event = document.events[i];
            output << (i ? "," : "") << "{\"time_seconds\":" << event.time_seconds;
            if (event.has_sample_index) output << ",\"sample_index\":" << event.sample_index;
            output << ",\"channel\":" << json_string(event.lead) << ",\"label\":" << json_string(signal_synth::delineation_kind_name(event.kind));
            if (event.has_confidence) output << ",\"confidence\":" << event.confidence;
            output << '}';
        }
        return output.str() + "]}";
    }

    std::string canonical_csv(const signal_synth::delineation_output_document& document)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10) << "time_seconds,sample_index,channel,label,confidence\n";
        for (std::size_t i = 0; i < document.events.size(); ++i)
        {
            const signal_synth::delineation_event& event = document.events[i];
            output << event.time_seconds << ',';
            if (event.has_sample_index) output << event.sample_index;
            output << ',' << event.lead << ',' << signal_synth::delineation_kind_name(event.kind) << ',';
            if (event.has_confidence) output << event.confidence;
            output << '\n';
        }
        return output.str();
    }

    std::string trim(const std::string& value)
    {
        std::size_t first = 0, last = value.size();
        while (first < last && (value[first] == ' ' || value[first] == '\t' || value[first] == '\r')) ++first;
        while (last > first && (value[last - 1] == ' ' || value[last - 1] == '\t' || value[last - 1] == '\r')) --last;
        return value.substr(first, last - first);
    }

    bool csv_record(const std::string& csv, std::size_t& offset, std::vector<std::string>& cells, std::string& error)
    {
        cells.clear();
        std::string cell;
        bool quoted = false, any = false;
        while (offset < csv.size())
        {
            any = true;
            const char ch = csv[offset++];
            if (quoted)
            {
                if (ch == '"')
                {
                    if (offset < csv.size() && csv[offset] == '"') { cell.push_back('"'); ++offset; }
                    else quoted = false;
                }
                else cell.push_back(ch);
            }
            else if (ch == '"')
            {
                if (!trim(cell).empty()) { error = "quote must start a CSV cell"; return false; }
                quoted = true; cell.clear();
            }
            else if (ch == ',') { cells.push_back(trim(cell)); cell.clear(); }
            else if (ch == '\n') { cells.push_back(trim(cell)); return true; }
            else cell.push_back(ch);
        }
        if (quoted) { error = "unterminated quoted CSV cell"; return false; }
        if (!any) return false;
        cells.push_back(trim(cell));
        return true;
    }

    bool parse_double(const std::string& text, double& output)
    {
        if (text.empty()) return false;
        errno = 0;
        char* end = 0;
        const double value = std::strtod(text.c_str(), &end);
        if (!end || *end || errno == ERANGE || !std::isfinite(value)) return false;
        output = value;
        return true;
    }

    bool parse_sample_index(const std::string& text, unsigned long long& output)
    {
        if (text.empty() || (text.size() > 1u && text[0] == '0')) return false;
        unsigned long long value = 0;
        const unsigned long long maximum = 9007199254740991ULL;
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            if (text[i] < '0' || text[i] > '9') return false;
            const unsigned int digit = static_cast<unsigned int>(text[i] - '0');
            if (value > (maximum - digit) / 10ULL) return false;
            value = value * 10ULL + digit;
        }
        output = value;
        return true;
    }
}

namespace signal_synth
{
    delineation_event::delineation_event() : lead(), kind(delineation_p_onset), time_seconds(0.0), has_sample_index(false), sample_index(0), has_confidence(false), confidence(0.0), original_index(0) {}
    delineation_output_document::delineation_output_document() : schema_version(1), events() {}
    delineation_io_result::delineation_io_result() : success(false), messages(), canonical_json(), canonical_csv() {}

    const char* delineation_kind_name(delineation_kind kind)
    {
        const char* names[] = {"p_onset","p_peak","p_offset","qrs_onset","j_point","qrs_offset","t_onset","t_peak","t_offset"};
        return kind >= delineation_p_onset && kind < delineation_kind_count ? names[static_cast<int>(kind)] : "";
    }

    bool delineation_kind_from_name(const std::string& name, delineation_kind& kind)
    {
        for (int i = 0; i < static_cast<int>(delineation_kind_count); ++i)
            if (name == delineation_kind_name(static_cast<delineation_kind>(i))) { kind = static_cast<delineation_kind>(i); return true; }
        return false;
    }

    const char* delineation_io_message_code_name(delineation_io_message_code code)
    {
        const char* names[] = {"DELINEATION_IO_SYNTAX","DELINEATION_IO_TYPE","DELINEATION_IO_MISSING_FIELD","DELINEATION_IO_UNKNOWN_FIELD","DELINEATION_IO_RANGE","DELINEATION_IO_DUPLICATE"};
        return code >= delineation_io_syntax && code <= delineation_io_duplicate ? names[static_cast<int>(code)] : "DELINEATION_IO_UNKNOWN";
    }

    bool write_delineation_point_events(const delineation_output_document& document, delineation_io_result& result)
    {
        delineation_io_result fresh;
        delineation_output_document normalized = document;
        if (!validate(normalized, fresh)) { result = fresh; return false; }
        fresh.canonical_json = canonical_json(normalized);
        fresh.canonical_csv = canonical_csv(normalized);
        fresh.success = true;
        result = fresh;
        return true;
    }

    bool parse_delineation_point_events_json_v1(const std::string& json, delineation_output_document& output, delineation_io_result& result)
    {
        delineation_io_result fresh;
        json_value root;
        parser input(json);
        if (!input.parse_root(root)) { message(fresh, input.code, "$", input.error); result = fresh; return false; }
        if (root.type != json_value::object_kind) { message(fresh, delineation_io_type, "$", "root must be an object"); result = fresh; return false; }
        const char* top_fields[] = {"schema_version","events"};
        allowed_fields(root, top_fields, 2u, "$", fresh);
        const json_value* schema = required(root, "schema_version", json_value::number_kind, "$", fresh);
        const json_value* events = required(root, "events", json_value::array_kind, "$", fresh);
        if (!schema || !events) { result = fresh; return false; }
        delineation_output_document document;
        if (schema->number != 1.0) message(fresh, delineation_io_range, "$.schema_version", "schema_version must be 1");
        for (std::size_t i = 0; i < events->array.size(); ++i)
        {
            const std::string path = "$.events[" + index_text(i) + "]";
            const json_value& item = events->array[i];
            if (item.type != json_value::object_kind) { message(fresh, delineation_io_type, path, "event must be an object"); continue; }
            const char* fields[] = {"time_seconds","sample_index","channel","label","confidence"};
            allowed_fields(item, fields, 5u, path, fresh);
            const json_value* time = required(item, "time_seconds", json_value::number_kind, path, fresh);
            const json_value* channel = required(item, "channel", json_value::string_kind, path, fresh);
            const json_value* label = required(item, "label", json_value::string_kind, path, fresh);
            delineation_event event;
            event.original_index = static_cast<unsigned int>(i);
            if (time) event.time_seconds = time->number;
            if (channel) event.lead = channel->string;
            if (label && !delineation_kind_from_name(label->string, event.kind)) message(fresh, delineation_io_range, path + ".label", "unsupported delineation kind");
            const json_value* sample = member(item, "sample_index");
            if (sample && (sample->type != json_value::number_kind || sample->number < 0.0 || sample->number > 9007199254740991.0 || std::floor(sample->number) != sample->number)) message(fresh, delineation_io_range, path + ".sample_index", "sample_index must be an exact non-negative JSON integer");
            else if (sample) { event.has_sample_index = true; event.sample_index = static_cast<unsigned long long>(sample->number); }
            const json_value* confidence = member(item, "confidence");
            if (confidence && confidence->type != json_value::number_kind) message(fresh, delineation_io_type, path + ".confidence", "confidence must be a number");
            else if (confidence) { event.has_confidence = true; event.confidence = confidence->number; }
            document.events.push_back(event);
        }
        if (!validate(document, fresh)) { result = fresh; return false; }
        fresh.canonical_json = canonical_json(document);
        fresh.canonical_csv = canonical_csv(document);
        fresh.success = true;
        output = document;
        result = fresh;
        return true;
    }

    bool parse_delineation_point_events_csv_v1(const std::string& csv, delineation_output_document& output, delineation_io_result& result)
    {
        delineation_io_result fresh;
        std::size_t offset = 0;
        std::vector<std::string> header;
        std::string error;
        if (!csv_record(csv, offset, header, error) || !error.empty()) { message(fresh, delineation_io_syntax, "$", error.empty() ? "point-event CSV is empty" : error); result = fresh; return false; }
        int time = -1, sample = -1, channel = -1, label = -1, confidence = -1;
        std::set<std::string> fields;
        for (std::size_t i = 0; i < header.size(); ++i)
        {
            if (!fields.insert(header[i]).second) message(fresh, delineation_io_duplicate, "$.header[" + index_text(i) + "]", "duplicate CSV column");
            else if (header[i] == "time_seconds") time = static_cast<int>(i);
            else if (header[i] == "sample_index") sample = static_cast<int>(i);
            else if (header[i] == "channel") channel = static_cast<int>(i);
            else if (header[i] == "label") label = static_cast<int>(i);
            else if (header[i] == "confidence") confidence = static_cast<int>(i);
            else message(fresh, delineation_io_unknown_field, "$.header[" + index_text(i) + "]", "unsupported CSV column");
        }
        if (time < 0 || channel < 0 || label < 0) message(fresh, delineation_io_missing_field, "$.header", "CSV requires time_seconds, channel, and label columns");
        if (!fresh.messages.empty()) { result = fresh; return false; }
        delineation_output_document document;
        std::vector<std::string> cells;
        unsigned int row = 1u;
        while (csv_record(csv, offset, cells, error))
        {
            ++row;
            bool empty = true;
            for (std::size_t i = 0; i < cells.size(); ++i) empty = empty && cells[i].empty();
            if (empty) continue;
            if (cells.size() > header.size()) { message(fresh, delineation_io_range, "$.rows[" + index_text(row) + "]", "CSV row has more cells than the header"); continue; }
            delineation_event event;
            event.original_index = static_cast<unsigned int>(document.events.size());
            if (static_cast<std::size_t>(time) >= cells.size() || !parse_double(cells[time], event.time_seconds)) message(fresh, delineation_io_range, "$.rows[" + index_text(row) + "].time_seconds", "invalid time_seconds");
            if (sample >= 0 && static_cast<std::size_t>(sample) < cells.size() && !cells[sample].empty())
            {
                event.has_sample_index = parse_sample_index(cells[sample], event.sample_index);
                if (!event.has_sample_index) message(fresh, delineation_io_range, "$.rows[" + index_text(row) + "].sample_index", "invalid sample_index");
            }
            if (static_cast<std::size_t>(channel) < cells.size()) event.lead = cells[channel];
            if (static_cast<std::size_t>(label) >= cells.size() || !delineation_kind_from_name(cells[label], event.kind)) message(fresh, delineation_io_range, "$.rows[" + index_text(row) + "].label", "unsupported delineation kind");
            if (confidence >= 0 && static_cast<std::size_t>(confidence) < cells.size() && !cells[confidence].empty())
            {
                event.has_confidence = parse_double(cells[confidence], event.confidence);
                if (!event.has_confidence) message(fresh, delineation_io_range, "$.rows[" + index_text(row) + "].confidence", "invalid confidence");
            }
            document.events.push_back(event);
        }
        if (!error.empty()) message(fresh, delineation_io_syntax, "$.rows", error);
        if (!validate(document, fresh)) { result = fresh; return false; }
        fresh.canonical_json = canonical_json(document);
        fresh.canonical_csv = canonical_csv(document);
        fresh.success = true;
        output = document;
        result = fresh;
        return true;
    }
}
