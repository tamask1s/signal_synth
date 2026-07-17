#include "challenge_package.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
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
        explicit parser(const std::string& input) : text(input), offset(0), code(signal_synth::challenge_package_json_syntax), failed(false) {}
        const std::string& text;
        std::size_t offset;
        signal_synth::challenge_package_json_message_code code;
        std::string error;
        bool failed;

        void skip_ws()
        {
            while (offset < text.size() && (text[offset] == ' ' || text[offset] == '\t' || text[offset] == '\r' || text[offset] == '\n'))
                ++offset;
        }

        bool fail(signal_synth::challenge_package_json_message_code failure_code, const char* message)
        {
            code = failure_code;
            error = message;
            failed = true;
            return false;
        }

        bool parse_string_token(std::string& value)
        {
            if (offset >= text.size() || text[offset] != '"')
                return fail(signal_synth::challenge_package_json_syntax, "expected string");
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
                        return fail(signal_synth::challenge_package_json_syntax, "truncated escape");
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
                    default: return fail(signal_synth::challenge_package_json_syntax, "unsupported escape");
                    }
                }
                else if (ch < 0x20)
                    return fail(signal_synth::challenge_package_json_syntax, "control character in string");
                else
                    value.push_back(static_cast<char>(ch));
            }
            return fail(signal_synth::challenge_package_json_syntax, "unterminated string");
        }

        bool parse_value(json_value& value)
        {
            skip_ws();
            if (offset >= text.size())
                return fail(signal_synth::challenge_package_json_syntax, "unexpected end of input");
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
                return fail(signal_synth::challenge_package_json_syntax, "invalid token");
            errno = 0;
            char* end = 0;
            value.number = std::strtod(value.token.c_str(), &end);
            if (end && *end == 0 && errno != ERANGE && std::isfinite(value.number))
            {
                value.type = json_value::number_kind;
                return true;
            }
            return fail(signal_synth::challenge_package_json_syntax, "invalid token");
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
                return fail(signal_synth::challenge_package_json_syntax, "expected comma or array end");
            }
            return fail(signal_synth::challenge_package_json_syntax, "unterminated array");
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
                    return fail(signal_synth::challenge_package_json_duplicate_id, "duplicate object key");
                skip_ws();
                if (offset >= text.size() || text[offset] != ':')
                    return fail(signal_synth::challenge_package_json_syntax, "expected colon");
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
                return fail(signal_synth::challenge_package_json_syntax, "expected comma or object end");
            }
            return fail(signal_synth::challenge_package_json_syntax, "unterminated object");
        }

        bool parse_root(json_value& value)
        {
            if (!parse_value(value))
                return false;
            skip_ws();
            if (offset != text.size())
                return fail(signal_synth::challenge_package_json_syntax, "trailing data");
            return true;
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
                const std::size_t fraction_start = offset;
                while (offset < token.size() && token[offset] >= '0' && token[offset] <= '9')
                    ++offset;
                if (offset == fraction_start)
                    return false;
            }
            if (offset < token.size() && (token[offset] == 'e' || token[offset] == 'E'))
            {
                ++offset;
                if (offset < token.size() && (token[offset] == '+' || token[offset] == '-'))
                    ++offset;
                const std::size_t exponent_start = offset;
                while (offset < token.size() && token[offset] >= '0' && token[offset] <= '9')
                    ++offset;
                if (offset == exponent_start)
                    return false;
            }
            return offset == token.size();
        }
    };

    const json_value* member(const json_value& object, const char* name)
    {
        for (std::size_t i = 0; i < object.object.size(); ++i)
            if (object.object[i].first == name)
                return &object.object[i].second;
        return 0;
    }

    void add_message(signal_synth::challenge_package_json_result& result, signal_synth::challenge_package_json_message_code code, const std::string& path, const std::string& message)
    {
        signal_synth::challenge_package_json_message item;
        item.code = code;
        item.path = path;
        item.message = message;
        result.messages.push_back(item);
    }

    std::string json_index(std::size_t index)
    {
        std::ostringstream output;
        output << index;
        return output.str();
    }

    bool safe_id(const std::string& value)
    {
        if (value.empty() || value.size() > 128)
            return false;
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            const char ch = value[i];
            if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.'))
                return false;
        }
        return true;
    }

    bool safe_relative_path(const std::string& value)
    {
        if (value.empty() || value.size() > 512 || value[0] == '/' || value[0] == '\\' || value.find("..") != std::string::npos)
            return false;
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            const char ch = value[i];
            if (ch < 0x20 || ch == ':')
                return false;
        }
        return true;
    }

    bool valid_sha256(const std::string& value)
    {
        if (value.size() != 71 || value.substr(0, 7) != "sha256:")
            return false;
        for (std::size_t i = 7; i < value.size(); ++i)
            if (!((value[i] >= '0' && value[i] <= '9') || (value[i] >= 'a' && value[i] <= 'f')))
                return false;
        return true;
    }

    bool valid_media_type(const std::string& value)
    {
        return !value.empty() && value.size() <= 128 && value.find('/') != std::string::npos && value.find(' ') == std::string::npos;
    }

    bool bool_value(const json_value& value)
    {
        return value.token == "true";
    }

    bool integer_number(const json_value& value, unsigned long long& output)
    {
        if (value.type != json_value::number_kind || value.number < 0.0 || value.number > 18446744073709551615.0)
            return false;
        const unsigned long long integer = static_cast<unsigned long long>(value.number);
        if (static_cast<double>(integer) != value.number)
            return false;
        output = integer;
        return true;
    }

    bool allowed_fields(const json_value& object, const char* const* names, std::size_t count, const std::string& path, signal_synth::challenge_package_json_result& result)
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
                add_message(result, signal_synth::challenge_package_json_unknown_field, path + "." + object.object[i].first, "unknown field");
                ok = false;
            }
        }
        return ok;
    }

    const json_value* required(const json_value& object, const char* name, json_value::kind kind, const std::string& path, signal_synth::challenge_package_json_result& result)
    {
        const json_value* value = member(object, name);
        if (!value)
        {
            add_message(result, signal_synth::challenge_package_json_missing_field, path + "." + name, "required field is missing");
            return 0;
        }
        if (value->type != kind)
        {
            add_message(result, signal_synth::challenge_package_json_type, path + "." + name, "field has the wrong JSON type");
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

    uint32_t rotate_right(uint32_t value, unsigned int count)
    {
        return (value >> count) | (value << (32u - count));
    }

    std::string sha256(const std::string& input)
    {
        static const uint32_t constants[64] = {
            0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
            0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
            0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
            0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
            0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
            0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
            0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
            0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};
        std::vector<unsigned char> data(input.begin(), input.end());
        const uint64_t bit_length = static_cast<uint64_t>(data.size()) * 8u;
        data.push_back(0x80u);
        while ((data.size() % 64u) != 56u)
            data.push_back(0u);
        for (int shift = 56; shift >= 0; shift -= 8)
            data.push_back(static_cast<unsigned char>((bit_length >> shift) & 0xffu));
        uint32_t hash[8] = {0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
        for (std::size_t offset = 0; offset < data.size(); offset += 64)
        {
            uint32_t words[64];
            for (unsigned int i = 0; i < 16; ++i)
                words[i] = (static_cast<uint32_t>(data[offset + i * 4]) << 24) | (static_cast<uint32_t>(data[offset + i * 4 + 1]) << 16) | (static_cast<uint32_t>(data[offset + i * 4 + 2]) << 8) | data[offset + i * 4 + 3];
            for (unsigned int i = 16; i < 64; ++i)
            {
                const uint32_t s0 = rotate_right(words[i - 15], 7) ^ rotate_right(words[i - 15], 18) ^ (words[i - 15] >> 3);
                const uint32_t s1 = rotate_right(words[i - 2], 17) ^ rotate_right(words[i - 2], 19) ^ (words[i - 2] >> 10);
                words[i] = words[i - 16] + s0 + words[i - 7] + s1;
            }
            uint32_t a = hash[0], b = hash[1], c = hash[2], d = hash[3], e = hash[4], f = hash[5], g = hash[6], h = hash[7];
            for (unsigned int i = 0; i < 64; ++i)
            {
                const uint32_t s1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25);
                const uint32_t choice = (e & f) ^ ((~e) & g);
                const uint32_t temporary1 = h + s1 + choice + constants[i] + words[i];
                const uint32_t s0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22);
                const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
                const uint32_t temporary2 = s0 + majority;
                h = g; g = f; f = e; e = d + temporary1; d = c; c = b; b = a; a = temporary1 + temporary2;
            }
            hash[0] += a; hash[1] += b; hash[2] += c; hash[3] += d;
            hash[4] += e; hash[5] += f; hash[6] += g; hash[7] += h;
        }
        std::ostringstream output;
        output << "sha256:" << std::hex << std::setfill('0');
        for (unsigned int i = 0; i < 8; ++i)
            output << std::setw(8) << hash[i];
        return output.str();
    }

    void write_string_array(std::ostringstream& output, const std::vector<std::string>& values)
    {
        output << '[';
        for (std::size_t i = 0; i < values.size(); ++i)
            output << (i ? "," : "") << escape_json(values[i]);
        output << ']';
    }

    std::string canonical(const signal_synth::challenge_package_manifest& manifest)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << "{\"schema_version\":" << manifest.schema_version
               << ",\"package_id\":" << escape_json(manifest.package_id)
               << ",\"name\":" << escape_json(manifest.name)
               << ",\"version\":" << escape_json(manifest.version)
               << ",\"description\":" << escape_json(manifest.description)
               << ",\"package_type\":" << escape_json(signal_synth::challenge_package_type_name(manifest.package_type))
               << ",\"ground_truth_included\":" << (manifest.ground_truth_included ? "true" : "false")
               << ",\"waveform_formats\":";
        write_string_array(output, manifest.waveform_formats);
        output << ",\"generator_version\":" << escape_json(manifest.generator_version)
               << ",\"usage_restrictions\":" << escape_json(manifest.usage_restrictions)
               << ",\"not_for\":" << escape_json(manifest.not_for)
               << ",\"files\":[";
        for (std::size_t i = 0; i < manifest.files.size(); ++i)
        {
            const signal_synth::challenge_package_file& file = manifest.files[i];
            output << (i ? "," : "") << "{\"path\":" << escape_json(file.path)
                   << ",\"role\":" << escape_json(signal_synth::challenge_file_role_name(file.role))
                   << ",\"media_type\":" << escape_json(file.media_type)
                   << ",\"sha256\":" << escape_json(file.sha256)
                   << ",\"size_bytes\":" << file.size_bytes
                   << ",\"required\":" << (file.required ? "true" : "false") << "}";
        }
        output << "],\"cases\":[";
        for (std::size_t i = 0; i < manifest.cases.size(); ++i)
        {
            const signal_synth::challenge_package_case& item = manifest.cases[i];
            output << (i ? "," : "") << "{\"id\":" << escape_json(item.id)
                   << ",\"scenario_id\":" << escape_json(item.scenario_id)
                   << ",\"scenario_path\":" << escape_json(item.scenario_path)
                   << ",\"document_fingerprint\":" << escape_json(item.document_fingerprint)
                   << ",\"render_identity\":" << escape_json(item.render_identity)
                   << ",\"files\":";
            write_string_array(output, item.files);
            output << "}";
        }
        output << "]}";
        return output.str();
    }
}

namespace signal_synth
{
    challenge_package_file::challenge_package_file() : role(challenge_file_other), size_bytes(0), required(true) {}

    challenge_package_manifest::challenge_package_manifest() : schema_version(1), package_type(challenge_package_single_scenario), ground_truth_included(true) {}

    challenge_package_json_result::challenge_package_json_result() : success(false) {}

    const char* challenge_package_json_message_code_name(challenge_package_json_message_code code)
    {
        switch (code)
        {
        case challenge_package_json_syntax: return "CHALLENGE_PACKAGE_JSON_SYNTAX";
        case challenge_package_json_type: return "CHALLENGE_PACKAGE_JSON_TYPE";
        case challenge_package_json_missing_field: return "CHALLENGE_PACKAGE_JSON_MISSING_FIELD";
        case challenge_package_json_unknown_field: return "CHALLENGE_PACKAGE_JSON_UNKNOWN_FIELD";
        case challenge_package_json_range: return "CHALLENGE_PACKAGE_JSON_RANGE";
        case challenge_package_json_duplicate_id: return "CHALLENGE_PACKAGE_JSON_DUPLICATE_ID";
        }
        return "CHALLENGE_PACKAGE_JSON_UNKNOWN";
    }

    const char* challenge_package_type_name(challenge_package_type type)
    {
        return type == challenge_package_scenario_pack ? "scenario_pack" : "single_scenario";
    }

    const char* challenge_file_role_name(challenge_file_role role)
    {
        switch (role)
        {
        case challenge_file_scenario_json: return "scenario_json";
        case challenge_file_pack_json: return "pack_json";
        case challenge_file_metadata_json: return "metadata_json";
        case challenge_file_waveform_csv: return "waveform_csv";
        case challenge_file_annotations_json: return "annotations_json";
        case challenge_file_ground_truth_metrics_json: return "ground_truth_metrics_json";
        case challenge_file_report_html: return "report_html";
        case challenge_file_readme: return "readme";
        case challenge_file_wfdb_header: return "wfdb_header";
        case challenge_file_wfdb_signal: return "wfdb_signal";
        case challenge_file_wfdb_annotation: return "wfdb_annotation";
        case challenge_file_edf: return "edf";
        case challenge_file_bdf: return "bdf";
        case challenge_file_measurement_truth_json: return "measurement_truth_json";
        case challenge_file_wearable_samples_csv: return "wearable_samples_csv";
        case challenge_file_wearable_timestamp_truth_csv: return "wearable_timestamp_truth_csv";
        case challenge_file_wearable_timebase_truth_json: return "wearable_timebase_truth_json";
        case challenge_file_wearable_alignment_truth_json: return "wearable_alignment_truth_json";
        case challenge_file_realism_metrics_json: return "realism_metrics_json";
        case challenge_file_realism_metrics_csv: return "realism_metrics_csv";
        case challenge_file_realism_report_html: return "realism_report_html";
        case challenge_file_realism_population_json: return "realism_population_json";
        case challenge_file_other: return "other";
        }
        return "other";
    }

    bool challenge_package_type_from_name(const std::string& name, challenge_package_type& output)
    {
        if (name == "single_scenario")
        {
            output = challenge_package_single_scenario;
            return true;
        }
        if (name == "scenario_pack")
        {
            output = challenge_package_scenario_pack;
            return true;
        }
        return false;
    }

    bool challenge_file_role_from_name(const std::string& name, challenge_file_role& output)
    {
        for (int value = challenge_file_scenario_json; value <= challenge_file_other; ++value)
        {
            const challenge_file_role role = static_cast<challenge_file_role>(value);
            if (name == challenge_file_role_name(role))
            {
                output = role;
                return true;
            }
        }
        return false;
    }

    std::string challenge_package_content_sha256(const std::string& content)
    {
        return sha256(content);
    }

    bool write_challenge_package_json(const challenge_package_manifest& manifest, challenge_package_json_result& result)
    {
        challenge_package_json_result fresh;
        if (manifest.schema_version != 1)
            add_message(fresh, challenge_package_json_range, "$.schema_version", "only schema version 1 is supported");
        if (!safe_id(manifest.package_id))
            add_message(fresh, challenge_package_json_range, "$.package_id", "package_id must be a safe identifier");
        if (manifest.name.empty() || manifest.name.size() > 256)
            add_message(fresh, challenge_package_json_range, "$.name", "name must contain 1 to 256 characters");
        if (manifest.version.empty() || manifest.version.size() > 64)
            add_message(fresh, challenge_package_json_range, "$.version", "version must contain 1 to 64 characters");
        if (manifest.description.empty())
            add_message(fresh, challenge_package_json_range, "$.description", "description is required");
        if (manifest.generator_version.empty())
            add_message(fresh, challenge_package_json_range, "$.generator_version", "generator_version is required");
        if (!manifest.ground_truth_included)
            add_message(fresh, challenge_package_json_range, "$.ground_truth_included", "offline challenge packages must include ground truth");
        if (manifest.usage_restrictions.empty())
            add_message(fresh, challenge_package_json_range, "$.usage_restrictions", "usage restrictions are required");
        if (manifest.not_for.find("diagnosis") == std::string::npos || manifest.not_for.find("clinical validation") == std::string::npos)
            add_message(fresh, challenge_package_json_range, "$.not_for", "not_for must include diagnosis and clinical validation limitations");
        if (manifest.waveform_formats.empty())
            add_message(fresh, challenge_package_json_range, "$.waveform_formats", "at least one waveform format is required");
        for (std::size_t i = 0; i < manifest.waveform_formats.size(); ++i)
            if (!safe_id(manifest.waveform_formats[i]))
                add_message(fresh, challenge_package_json_range, "$.waveform_formats[" + json_index(i) + "]", "waveform format must be a safe identifier");
        if (manifest.files.empty())
            add_message(fresh, challenge_package_json_range, "$.files", "at least one file is required");
        if (manifest.cases.empty())
            add_message(fresh, challenge_package_json_range, "$.cases", "at least one case is required");

        bool has_ground_truth = false;
        std::set<std::string> file_paths;
        for (std::size_t i = 0; i < manifest.files.size(); ++i)
        {
            const challenge_package_file& file = manifest.files[i];
            const std::string path = "$.files[" + json_index(i) + "]";
            if (!safe_relative_path(file.path))
                add_message(fresh, challenge_package_json_range, path + ".path", "file path must be a safe relative path");
            else if (!file_paths.insert(file.path).second)
                add_message(fresh, challenge_package_json_duplicate_id, path + ".path", "duplicate file path");
            if (!valid_media_type(file.media_type))
                add_message(fresh, challenge_package_json_range, path + ".media_type", "media_type must be a compact type/subtype value");
            if (!valid_sha256(file.sha256))
                add_message(fresh, challenge_package_json_range, path + ".sha256", "sha256 must be sha256:<64 lowercase hex characters>");
            if (file.role == challenge_file_annotations_json || file.role == challenge_file_ground_truth_metrics_json || file.role == challenge_file_measurement_truth_json)
                has_ground_truth = true;
        }
        if (!has_ground_truth)
            add_message(fresh, challenge_package_json_range, "$.files", "at least one ground-truth file role is required");

        std::set<std::string> case_ids;
        for (std::size_t i = 0; i < manifest.cases.size(); ++i)
        {
            const challenge_package_case& item = manifest.cases[i];
            const std::string path = "$.cases[" + json_index(i) + "]";
            if (!safe_id(item.id))
                add_message(fresh, challenge_package_json_range, path + ".id", "case id must be a safe identifier");
            else if (!case_ids.insert(item.id).second)
                add_message(fresh, challenge_package_json_duplicate_id, path + ".id", "duplicate case id");
            if (item.scenario_id.empty())
                add_message(fresh, challenge_package_json_range, path + ".scenario_id", "scenario_id is required");
            if (!safe_relative_path(item.scenario_path))
                add_message(fresh, challenge_package_json_range, path + ".scenario_path", "scenario_path must be a safe relative path");
            if (!valid_sha256(item.document_fingerprint))
                add_message(fresh, challenge_package_json_range, path + ".document_fingerprint", "document_fingerprint must be sha256:<64 lowercase hex characters>");
            if (item.render_identity.empty())
                add_message(fresh, challenge_package_json_range, path + ".render_identity", "render_identity is required");
            if (item.files.empty())
                add_message(fresh, challenge_package_json_range, path + ".files", "case must reference at least one file");
            for (std::size_t file = 0; file < item.files.size(); ++file)
            {
                if (!safe_relative_path(item.files[file]))
                    add_message(fresh, challenge_package_json_range, path + ".files[" + json_index(file) + "]", "case file must be a safe relative path");
                else if (file_paths.find(item.files[file]) == file_paths.end())
                    add_message(fresh, challenge_package_json_range, path + ".files[" + json_index(file) + "]", "case references a file not listed in $.files");
            }
        }
        if (!fresh.messages.empty())
        {
            result = fresh;
            return false;
        }
        fresh.canonical_json = canonical(manifest);
        fresh.package_fingerprint = sha256(fresh.canonical_json);
        fresh.success = true;
        result = fresh;
        return true;
    }

    bool parse_challenge_package_json(const std::string& json, challenge_package_manifest& output, challenge_package_json_result& result)
    {
        challenge_package_json_result fresh_result;
        json_value root;
        parser p(json);
        if (!p.parse_root(root))
        {
            add_message(fresh_result, p.code, "$", p.error);
            result = fresh_result;
            return false;
        }
        if (root.type != json_value::object_kind)
        {
            add_message(fresh_result, challenge_package_json_type, "$", "root must be an object");
            result = fresh_result;
            return false;
        }
        const char* top_fields[] = {"schema_version","package_id","name","version","description","package_type","ground_truth_included","waveform_formats","generator_version","usage_restrictions","not_for","files","cases"};
        allowed_fields(root, top_fields, sizeof(top_fields) / sizeof(top_fields[0]), "$", fresh_result);
        const json_value* schema = required(root, "schema_version", json_value::number_kind, "$", fresh_result);
        const json_value* package_id = required(root, "package_id", json_value::string_kind, "$", fresh_result);
        const json_value* name = required(root, "name", json_value::string_kind, "$", fresh_result);
        const json_value* version = required(root, "version", json_value::string_kind, "$", fresh_result);
        const json_value* description = required(root, "description", json_value::string_kind, "$", fresh_result);
        const json_value* package_type = required(root, "package_type", json_value::string_kind, "$", fresh_result);
        const json_value* ground_truth = required(root, "ground_truth_included", json_value::bool_kind, "$", fresh_result);
        const json_value* waveform_formats = required(root, "waveform_formats", json_value::array_kind, "$", fresh_result);
        const json_value* generator_version = required(root, "generator_version", json_value::string_kind, "$", fresh_result);
        const json_value* usage = required(root, "usage_restrictions", json_value::string_kind, "$", fresh_result);
        const json_value* not_for = required(root, "not_for", json_value::string_kind, "$", fresh_result);
        const json_value* files = required(root, "files", json_value::array_kind, "$", fresh_result);
        const json_value* cases = required(root, "cases", json_value::array_kind, "$", fresh_result);
        if (!fresh_result.messages.empty())
        {
            result = fresh_result;
            return false;
        }

        challenge_package_manifest manifest;
        if (schema->number != 1.0)
            add_message(fresh_result, challenge_package_json_range, "$.schema_version", "only schema version 1 is supported");
        manifest.schema_version = 1;
        manifest.package_id = package_id->string;
        manifest.name = name->string;
        manifest.version = version->string;
        manifest.description = description->string;
        if (!challenge_package_type_from_name(package_type->string, manifest.package_type))
            add_message(fresh_result, challenge_package_json_range, "$.package_type", "unsupported package_type");
        manifest.ground_truth_included = bool_value(*ground_truth);
        manifest.generator_version = generator_version->string;
        manifest.usage_restrictions = usage->string;
        manifest.not_for = not_for->string;
        for (std::size_t i = 0; i < waveform_formats->array.size(); ++i)
        {
            if (waveform_formats->array[i].type != json_value::string_kind)
                add_message(fresh_result, challenge_package_json_type, "$.waveform_formats[" + json_index(i) + "]", "waveform format must be a string");
            else
                manifest.waveform_formats.push_back(waveform_formats->array[i].string);
        }
        for (std::size_t i = 0; i < files->array.size(); ++i)
        {
            const std::string item_path = "$.files[" + json_index(i) + "]";
            const json_value& item = files->array[i];
            if (item.type != json_value::object_kind)
            {
                add_message(fresh_result, challenge_package_json_type, item_path, "file must be an object");
                continue;
            }
            const char* file_fields[] = {"path","role","media_type","sha256","size_bytes","required"};
            allowed_fields(item, file_fields, sizeof(file_fields) / sizeof(file_fields[0]), item_path, fresh_result);
            const json_value* path = required(item, "path", json_value::string_kind, item_path, fresh_result);
            const json_value* role = required(item, "role", json_value::string_kind, item_path, fresh_result);
            const json_value* media_type = required(item, "media_type", json_value::string_kind, item_path, fresh_result);
            const json_value* sha = required(item, "sha256", json_value::string_kind, item_path, fresh_result);
            const json_value* size = required(item, "size_bytes", json_value::number_kind, item_path, fresh_result);
            const json_value* required_file = required(item, "required", json_value::bool_kind, item_path, fresh_result);
            if (!path || !role || !media_type || !sha || !size || !required_file)
                continue;
            challenge_package_file file;
            file.path = path->string;
            if (!challenge_file_role_from_name(role->string, file.role))
                add_message(fresh_result, challenge_package_json_range, item_path + ".role", "unsupported file role");
            file.media_type = media_type->string;
            file.sha256 = sha->string;
            if (!integer_number(*size, file.size_bytes))
                add_message(fresh_result, challenge_package_json_range, item_path + ".size_bytes", "size_bytes must be a non-negative integer");
            file.required = bool_value(*required_file);
            manifest.files.push_back(file);
        }
        for (std::size_t i = 0; i < cases->array.size(); ++i)
        {
            const std::string item_path = "$.cases[" + json_index(i) + "]";
            const json_value& item = cases->array[i];
            if (item.type != json_value::object_kind)
            {
                add_message(fresh_result, challenge_package_json_type, item_path, "case must be an object");
                continue;
            }
            const char* case_fields[] = {"id","scenario_id","scenario_path","document_fingerprint","render_identity","files"};
            allowed_fields(item, case_fields, sizeof(case_fields) / sizeof(case_fields[0]), item_path, fresh_result);
            const json_value* id = required(item, "id", json_value::string_kind, item_path, fresh_result);
            const json_value* scenario_id = required(item, "scenario_id", json_value::string_kind, item_path, fresh_result);
            const json_value* scenario_path = required(item, "scenario_path", json_value::string_kind, item_path, fresh_result);
            const json_value* document_fingerprint = required(item, "document_fingerprint", json_value::string_kind, item_path, fresh_result);
            const json_value* render_identity = required(item, "render_identity", json_value::string_kind, item_path, fresh_result);
            const json_value* case_files = required(item, "files", json_value::array_kind, item_path, fresh_result);
            if (!id || !scenario_id || !scenario_path || !document_fingerprint || !render_identity || !case_files)
                continue;
            challenge_package_case challenge_case;
            challenge_case.id = id->string;
            challenge_case.scenario_id = scenario_id->string;
            challenge_case.scenario_path = scenario_path->string;
            challenge_case.document_fingerprint = document_fingerprint->string;
            challenge_case.render_identity = render_identity->string;
            for (std::size_t file = 0; file < case_files->array.size(); ++file)
            {
                if (case_files->array[file].type != json_value::string_kind)
                    add_message(fresh_result, challenge_package_json_type, item_path + ".files[" + json_index(file) + "]", "case file must be a string");
                else
                    challenge_case.files.push_back(case_files->array[file].string);
            }
            manifest.cases.push_back(challenge_case);
        }
        if (!fresh_result.messages.empty())
        {
            result = fresh_result;
            return false;
        }
        if (!write_challenge_package_json(manifest, fresh_result))
        {
            result = fresh_result;
            return false;
        }
        output = manifest;
        result = fresh_result;
        return true;
    }
}
