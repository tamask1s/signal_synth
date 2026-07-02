#include "ecg_pack.h"
#include "signal_synth.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>

namespace
{
    using signal_synth::ecg_pack_json_message_code;

    struct json_value
    {
        enum kind
        {
            null_kind,
            bool_kind,
            number_kind,
            string_kind,
            array_kind,
            object_kind
        };

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
        explicit parser(const std::string& input) : text(input), offset(0), code(signal_synth::ecg_pack_json_syntax), failed(false) {}

        const std::string& text;
        std::size_t offset;
        signal_synth::ecg_pack_json_message_code code;
        std::string error;
        bool failed;

        void skip_ws()
        {
            while (offset < text.size() && (text[offset] == ' ' || text[offset] == '\t' || text[offset] == '\r' || text[offset] == '\n'))
                ++offset;
        }

        bool fail(signal_synth::ecg_pack_json_message_code failure_code, const char* message)
        {
            code = failure_code;
            error = message;
            failed = true;
            return false;
        }

        bool parse_string_token(std::string& value)
        {
            if (offset >= text.size() || text[offset] != '"')
                return fail(signal_synth::ecg_pack_json_syntax, "expected string");
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
                        return fail(signal_synth::ecg_pack_json_syntax, "truncated escape");
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
                    default: return fail(signal_synth::ecg_pack_json_syntax, "unsupported escape");
                    }
                }
                else if (ch < 0x20)
                    return fail(signal_synth::ecg_pack_json_syntax, "control character in string");
                else
                    value.push_back(static_cast<char>(ch));
            }
            return fail(signal_synth::ecg_pack_json_syntax, "unterminated string");
        }

        bool parse_value(json_value& value)
        {
            skip_ws();
            if (offset >= text.size())
                return fail(signal_synth::ecg_pack_json_syntax, "unexpected end of input");
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
            char* end = 0;
            value.number = std::strtod(value.token.c_str(), &end);
            if (end && *end == 0 && !value.token.empty())
            {
                value.type = json_value::number_kind;
                return true;
            }
            return fail(signal_synth::ecg_pack_json_syntax, "invalid token");
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
                return fail(signal_synth::ecg_pack_json_syntax, "expected comma or array end");
            }
            return fail(signal_synth::ecg_pack_json_syntax, "unterminated array");
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
                    return fail(signal_synth::ecg_pack_json_duplicate_id, "duplicate object key");
                skip_ws();
                if (offset >= text.size() || text[offset] != ':')
                    return fail(signal_synth::ecg_pack_json_syntax, "expected colon");
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
                return fail(signal_synth::ecg_pack_json_syntax, "expected comma or object end");
            }
            return fail(signal_synth::ecg_pack_json_syntax, "unterminated object");
        }

        bool parse_root(json_value& value)
        {
            if (!parse_value(value))
                return false;
            skip_ws();
            if (offset != text.size())
                return fail(signal_synth::ecg_pack_json_syntax, "trailing data");
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

    void add_message(signal_synth::ecg_pack_json_result& result, signal_synth::ecg_pack_json_message_code code, const std::string& path, const std::string& message)
    {
        signal_synth::ecg_pack_json_message item;
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

    void validate_target_list(signal_synth::ecg_pack_json_result& result, const std::string& path, const std::vector<std::string>& targets)
    {
        if (targets.empty())
        {
            add_message(result, signal_synth::ecg_pack_json_range, path, "at least one target is required");
            return;
        }
        std::set<std::string> unique_targets;
        for (std::size_t i = 0; i < targets.size(); ++i)
        {
            const std::string item_path = path + "[" + json_index(i) + "]";
            if (!safe_id(targets[i]))
                add_message(result, signal_synth::ecg_pack_json_range, item_path, "target must be a safe identifier");
            else if (!unique_targets.insert(targets[i]).second)
                add_message(result, signal_synth::ecg_pack_json_duplicate_id, item_path, "duplicate target");
        }
    }

    bool allowed_fields(const json_value& object, const char* const* names, std::size_t count, const std::string& path, signal_synth::ecg_pack_json_result& result)
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
                add_message(result, signal_synth::ecg_pack_json_unknown_field, path + "." + object.object[i].first, "unknown field");
                ok = false;
            }
        }
        return ok;
    }

    const json_value* required(const json_value& object, const char* name, json_value::kind kind, const std::string& path, signal_synth::ecg_pack_json_result& result)
    {
        const json_value* value = member(object, name);
        if (!value)
        {
            add_message(result, signal_synth::ecg_pack_json_missing_field, path + "." + name, "required field is missing");
            return 0;
        }
        if (value->type != kind)
        {
            add_message(result, signal_synth::ecg_pack_json_type, path + "." + name, "field has the wrong JSON type");
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

    std::string canonical(const signal_synth::ecg_pack_manifest& manifest)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << "{\"schema_version\":" << manifest.schema_version
               << ",\"pack_id\":" << escape_json(manifest.pack_id)
               << ",\"name\":" << escape_json(manifest.name)
               << ",\"version\":" << escape_json(manifest.version)
               << ",\"description\":" << escape_json(manifest.description)
               << ",\"targets\":[";
        for (std::size_t i = 0; i < manifest.targets.size(); ++i)
            output << (i ? "," : "") << escape_json(manifest.targets[i]);
        output << "],\"scenarios\":[";
        for (std::size_t i = 0; i < manifest.scenarios.size(); ++i)
        {
            const signal_synth::ecg_pack_scenario& scenario = manifest.scenarios[i];
            output << (i ? "," : "") << "{\"id\":" << escape_json(scenario.id)
                   << ",\"path\":" << escape_json(scenario.path)
                   << ",\"targets\":[";
            for (std::size_t target = 0; target < scenario.targets.size(); ++target)
                output << (target ? "," : "") << escape_json(scenario.targets[target]);
            output << "]}";
        }
        output << "]}";
        return output.str();
    }
}

namespace signal_synth
{
    ecg_pack_manifest::ecg_pack_manifest() : schema_version(1) {}

    ecg_pack_json_result::ecg_pack_json_result() : success(false) {}

    const char* ecg_pack_json_message_code_name(ecg_pack_json_message_code code)
    {
        switch (code)
        {
        case ecg_pack_json_syntax: return "PACK_JSON_SYNTAX";
        case ecg_pack_json_type: return "PACK_JSON_TYPE";
        case ecg_pack_json_missing_field: return "PACK_JSON_MISSING_FIELD";
        case ecg_pack_json_unknown_field: return "PACK_JSON_UNKNOWN_FIELD";
        case ecg_pack_json_range: return "PACK_JSON_RANGE";
        case ecg_pack_json_duplicate_id: return "PACK_JSON_DUPLICATE_ID";
        }
        return "PACK_JSON_UNKNOWN";
    }

    bool write_ecg_pack_json(const ecg_pack_manifest& manifest, ecg_pack_json_result& result)
    {
        ecg_pack_json_result fresh;
        if (manifest.schema_version != 1)
            add_message(fresh, ecg_pack_json_range, "$.schema_version", "only schema version 1 is supported");
        if (!safe_id(manifest.pack_id))
            add_message(fresh, ecg_pack_json_range, "$.pack_id", "pack_id must be a safe identifier");
        if (manifest.name.empty() || manifest.name.size() > 256)
            add_message(fresh, ecg_pack_json_range, "$.name", "name must contain 1 to 256 characters");
        if (manifest.version.empty() || manifest.version.size() > 64)
            add_message(fresh, ecg_pack_json_range, "$.version", "version must contain 1 to 64 characters");
        validate_target_list(fresh, "$.targets", manifest.targets);
        if (manifest.scenarios.empty())
            add_message(fresh, ecg_pack_json_range, "$.scenarios", "at least one scenario is required");
        std::set<std::string> ids;
        for (std::size_t i = 0; i < manifest.scenarios.size(); ++i)
        {
            const ecg_pack_scenario& scenario = manifest.scenarios[i];
            const std::string path = "$.scenarios[" + json_index(i) + "]";
            if (!safe_id(scenario.id))
                add_message(fresh, ecg_pack_json_range, path + ".id", "scenario id must be a safe identifier");
            else if (!ids.insert(scenario.id).second)
                add_message(fresh, ecg_pack_json_duplicate_id, path + ".id", "duplicate scenario id");
            if (scenario.path.empty())
                add_message(fresh, ecg_pack_json_range, path + ".path", "scenario path is required");
            validate_target_list(fresh, path + ".targets", scenario.targets);
        }
        if (!fresh.messages.empty())
        {
            result = fresh;
            return false;
        }
        fresh.canonical_json = canonical(manifest);
        fresh.pack_fingerprint = sha256(fresh.canonical_json);
        fresh.success = true;
        result = fresh;
        return true;
    }

    bool parse_ecg_pack_json(const std::string& json, ecg_pack_manifest& output, ecg_pack_json_result& result)
    {
        ecg_pack_json_result fresh_result;
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
            add_message(fresh_result, ecg_pack_json_type, "$", "root must be an object");
            result = fresh_result;
            return false;
        }
        const char* top_fields[] = {"schema_version","pack_id","name","version","description","targets","scenarios"};
        allowed_fields(root, top_fields, sizeof(top_fields) / sizeof(top_fields[0]), "$", fresh_result);
        const json_value* schema = required(root, "schema_version", json_value::number_kind, "$", fresh_result);
        const json_value* pack_id = required(root, "pack_id", json_value::string_kind, "$", fresh_result);
        const json_value* name = required(root, "name", json_value::string_kind, "$", fresh_result);
        const json_value* version = required(root, "version", json_value::string_kind, "$", fresh_result);
        const json_value* description = required(root, "description", json_value::string_kind, "$", fresh_result);
        const json_value* targets = required(root, "targets", json_value::array_kind, "$", fresh_result);
        const json_value* scenarios = required(root, "scenarios", json_value::array_kind, "$", fresh_result);
        if (!fresh_result.messages.empty())
        {
            result = fresh_result;
            return false;
        }

        ecg_pack_manifest manifest;
        if (schema->number != 1.0)
            add_message(fresh_result, ecg_pack_json_range, "$.schema_version", "only schema version 1 is supported");
        manifest.schema_version = 1;
        manifest.pack_id = pack_id->string;
        manifest.name = name->string;
        manifest.version = version->string;
        manifest.description = description->string;
        for (std::size_t i = 0; i < targets->array.size(); ++i)
        {
            if (targets->array[i].type != json_value::string_kind)
                add_message(fresh_result, ecg_pack_json_type, "$.targets[" + json_index(i) + "]", "target must be a string");
            else
                manifest.targets.push_back(targets->array[i].string);
        }
        for (std::size_t i = 0; i < scenarios->array.size(); ++i)
        {
            const std::string item_path = "$.scenarios[" + json_index(i) + "]";
            const json_value& item = scenarios->array[i];
            if (item.type != json_value::object_kind)
            {
                add_message(fresh_result, ecg_pack_json_type, item_path, "scenario must be an object");
                continue;
            }
            const char* fields[] = {"id","path","targets"};
            allowed_fields(item, fields, sizeof(fields) / sizeof(fields[0]), item_path, fresh_result);
            const json_value* scenario_id = required(item, "id", json_value::string_kind, item_path, fresh_result);
            const json_value* scenario_path = required(item, "path", json_value::string_kind, item_path, fresh_result);
            const json_value* scenario_targets = required(item, "targets", json_value::array_kind, item_path, fresh_result);
            if (!scenario_id || !scenario_path || !scenario_targets)
                continue;
            ecg_pack_scenario scenario;
            scenario.id = scenario_id->string;
            scenario.path = scenario_path->string;
            for (std::size_t target = 0; target < scenario_targets->array.size(); ++target)
            {
                const json_value& value = scenario_targets->array[target];
                if (value.type != json_value::string_kind)
                    add_message(fresh_result, ecg_pack_json_type, item_path + ".targets[" + json_index(target) + "]", "target must be a string");
                else
                    scenario.targets.push_back(value.string);
            }
            manifest.scenarios.push_back(scenario);
        }
        if (!fresh_result.messages.empty())
        {
            result = fresh_result;
            return false;
        }
        if (!write_ecg_pack_json(manifest, fresh_result))
        {
            result = fresh_result;
            return false;
        }
        output = manifest;
        result = fresh_result;
        return true;
    }
}
