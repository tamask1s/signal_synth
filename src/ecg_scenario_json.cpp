#include "ecg_scenario_json.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <utility>

namespace
{
    using signal_synth::ecg_scenario_json_message;
    using signal_synth::ecg_scenario_json_message_code;

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
        bool boolean;
        double number;
        std::string token;
        std::string string;
        std::vector<json_value> array;
        std::vector<std::pair<std::string, json_value> > object;

        json_value() : type(null_kind), boolean(false), number(0.0) {}
    };

    bool continuation(unsigned char c)
    {
        return (c & 0xc0u) == 0x80u;
    }

    bool valid_utf8_sequence(const std::string& input, std::size_t position, std::size_t& length)
    {
        const unsigned char c0 = static_cast<unsigned char>(input[position]);
        length = 0;
        if (c0 < 0x80u)
        {
            length = 1;
            return true;
        }
        if (c0 >= 0xc2u && c0 <= 0xdfu)
        {
            length = 2;
            return position + 1 < input.size() && continuation(static_cast<unsigned char>(input[position + 1]));
        }
        if (c0 >= 0xe0u && c0 <= 0xefu)
        {
            length = 3;
            if (position + 2 >= input.size())
                return false;
            const unsigned char c1 = static_cast<unsigned char>(input[position + 1]);
            const unsigned char c2 = static_cast<unsigned char>(input[position + 2]);
            if (!continuation(c1) || !continuation(c2))
                return false;
            if (c0 == 0xe0u && c1 < 0xa0u)
                return false;
            if (c0 == 0xedu && c1 >= 0xa0u)
                return false;
            return true;
        }
        if (c0 >= 0xf0u && c0 <= 0xf4u)
        {
            length = 4;
            if (position + 3 >= input.size())
                return false;
            const unsigned char c1 = static_cast<unsigned char>(input[position + 1]);
            if (!continuation(c1) || !continuation(static_cast<unsigned char>(input[position + 2])) || !continuation(static_cast<unsigned char>(input[position + 3])))
                return false;
            if (c0 == 0xf0u && c1 < 0x90u)
                return false;
            if (c0 == 0xf4u && c1 >= 0x90u)
                return false;
            return true;
        }
        return false;
    }

    bool valid_utf8(const std::string& value)
    {
        for (std::size_t i = 0; i < value.size();)
        {
            std::size_t length = 0;
            if (!valid_utf8_sequence(value, i, length))
                return false;
            i += length;
        }
        return true;
    }

    void append_utf8(std::string& output, unsigned int codepoint)
    {
        if (codepoint <= 0x7fu)
            output.push_back(static_cast<char>(codepoint));
        else if (codepoint <= 0x7ffu)
        {
            output.push_back(static_cast<char>(0xc0u | (codepoint >> 6)));
            output.push_back(static_cast<char>(0x80u | (codepoint & 0x3fu)));
        }
        else if (codepoint <= 0xffffu)
        {
            output.push_back(static_cast<char>(0xe0u | (codepoint >> 12)));
            output.push_back(static_cast<char>(0x80u | ((codepoint >> 6) & 0x3fu)));
            output.push_back(static_cast<char>(0x80u | (codepoint & 0x3fu)));
        }
        else
        {
            output.push_back(static_cast<char>(0xf0u | (codepoint >> 18)));
            output.push_back(static_cast<char>(0x80u | ((codepoint >> 12) & 0x3fu)));
            output.push_back(static_cast<char>(0x80u | ((codepoint >> 6) & 0x3fu)));
            output.push_back(static_cast<char>(0x80u | (codepoint & 0x3fu)));
        }
    }

    class json_parser
    {
    public:
        explicit json_parser(const std::string& input) : input_(input), position_(0), code_(signal_synth::ecg_json_syntax) {}

        bool parse(json_value& output)
        {
            skip_space();
            if (!parse_value(output, 0))
                return false;
            skip_space();
            if (position_ != input_.size())
                return fail(signal_synth::ecg_json_syntax, "trailing data");
            return true;
        }

        ecg_scenario_json_message message() const
        {
            std::ostringstream text;
            text << message_ << " at byte " << position_;
            ecg_scenario_json_message result;
            result.code = code_;
            result.path = "$";
            result.message = text.str();
            return result;
        }

    private:
        const std::string& input_;
        std::size_t position_;
        ecg_scenario_json_message_code code_;
        std::string message_;

        bool fail(ecg_scenario_json_message_code code, const char* message)
        {
            code_ = code;
            message_ = message;
            return false;
        }

        void skip_space()
        {
            while (position_ < input_.size())
            {
                const char c = input_[position_];
                if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
                    break;
                ++position_;
            }
        }

        bool consume(char expected)
        {
            if (position_ >= input_.size() || input_[position_] != expected)
                return false;
            ++position_;
            return true;
        }

        bool parse_value(json_value& output, unsigned int depth)
        {
            if (depth > 64)
                return fail(signal_synth::ecg_json_syntax, "maximum nesting depth exceeded");
            skip_space();
            if (position_ >= input_.size())
                return fail(signal_synth::ecg_json_syntax, "unexpected end of input");
            const char c = input_[position_];
            if (c == '{')
                return parse_object(output, depth + 1);
            if (c == '[')
                return parse_array(output, depth + 1);
            if (c == '"')
            {
                output.type = json_value::string_kind;
                return parse_string(output.string);
            }
            if (c == 't' && input_.compare(position_, 4, "true") == 0)
            {
                position_ += 4;
                output.type = json_value::bool_kind;
                output.boolean = true;
                return true;
            }
            if (c == 'f' && input_.compare(position_, 5, "false") == 0)
            {
                position_ += 5;
                output.type = json_value::bool_kind;
                output.boolean = false;
                return true;
            }
            if (c == 'n' && input_.compare(position_, 4, "null") == 0)
            {
                position_ += 4;
                output.type = json_value::null_kind;
                return true;
            }
            if (c == '-' || (c >= '0' && c <= '9'))
                return parse_number(output);
            return fail(signal_synth::ecg_json_syntax, "invalid value");
        }

        bool parse_object(json_value& output, unsigned int depth)
        {
            consume('{');
            output.type = json_value::object_kind;
            skip_space();
            if (consume('}'))
                return true;
            while (true)
            {
                skip_space();
                std::string key;
                if (!parse_string(key))
                    return false;
                for (std::size_t i = 0; i < output.object.size(); ++i)
                    if (output.object[i].first == key)
                        return fail(signal_synth::ecg_json_duplicate_key, "duplicate object key");
                skip_space();
                if (!consume(':'))
                    return fail(signal_synth::ecg_json_syntax, "expected colon");
                json_value value;
                if (!parse_value(value, depth))
                    return false;
                output.object.push_back(std::make_pair(key, value));
                skip_space();
                if (consume('}'))
                    return true;
                if (!consume(','))
                    return fail(signal_synth::ecg_json_syntax, "expected comma or object end");
            }
        }

        bool parse_array(json_value& output, unsigned int depth)
        {
            consume('[');
            output.type = json_value::array_kind;
            skip_space();
            if (consume(']'))
                return true;
            while (true)
            {
                json_value value;
                if (!parse_value(value, depth))
                    return false;
                output.array.push_back(value);
                skip_space();
                if (consume(']'))
                    return true;
                if (!consume(','))
                    return fail(signal_synth::ecg_json_syntax, "expected comma or array end");
            }
        }

        static int hex_value(char c)
        {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
            if (c >= 'A' && c <= 'F')
                return c - 'A' + 10;
            return -1;
        }

        bool parse_hex4(unsigned int& value)
        {
            if (position_ + 4 > input_.size())
                return fail(signal_synth::ecg_json_syntax, "incomplete Unicode escape");
            value = 0;
            for (unsigned int i = 0; i < 4; ++i)
            {
                const int digit = hex_value(input_[position_++]);
                if (digit < 0)
                    return fail(signal_synth::ecg_json_syntax, "invalid Unicode escape");
                value = (value << 4) | static_cast<unsigned int>(digit);
            }
            return true;
        }

        bool parse_string(std::string& output)
        {
            if (!consume('"'))
                return fail(signal_synth::ecg_json_syntax, "expected string");
            output.clear();
            while (position_ < input_.size())
            {
                const unsigned char c = static_cast<unsigned char>(input_[position_++]);
                if (c == '"')
                    return true;
                if (c < 0x20u)
                    return fail(signal_synth::ecg_json_syntax, "unescaped control character");
                if (c == '\\')
                {
                    if (position_ >= input_.size())
                        return fail(signal_synth::ecg_json_syntax, "incomplete escape");
                    const char escaped = input_[position_++];
                    switch (escaped)
                    {
                    case '"': output.push_back('"'); break;
                    case '\\': output.push_back('\\'); break;
                    case '/': output.push_back('/'); break;
                    case 'b': output.push_back('\b'); break;
                    case 'f': output.push_back('\f'); break;
                    case 'n': output.push_back('\n'); break;
                    case 'r': output.push_back('\r'); break;
                    case 't': output.push_back('\t'); break;
                    case 'u':
                    {
                        unsigned int first = 0;
                        if (!parse_hex4(first))
                            return false;
                        unsigned int codepoint = first;
                        if (first >= 0xd800u && first <= 0xdbffu)
                        {
                            if (position_ + 2 > input_.size() || input_[position_] != '\\' || input_[position_ + 1] != 'u')
                                return fail(signal_synth::ecg_json_syntax, "missing low surrogate");
                            position_ += 2;
                            unsigned int second = 0;
                            if (!parse_hex4(second))
                                return false;
                            if (second < 0xdc00u || second > 0xdfffu)
                                return fail(signal_synth::ecg_json_syntax, "invalid low surrogate");
                            codepoint = 0x10000u + ((first - 0xd800u) << 10) + (second - 0xdc00u);
                        }
                        else if (first >= 0xdc00u && first <= 0xdfffu)
                            return fail(signal_synth::ecg_json_syntax, "unexpected low surrogate");
                        append_utf8(output, codepoint);
                        break;
                    }
                    default:
                        return fail(signal_synth::ecg_json_syntax, "invalid escape");
                    }
                }
                else if (c < 0x80u)
                    output.push_back(static_cast<char>(c));
                else
                {
                    --position_;
                    std::size_t length = 0;
                    if (!valid_utf8_sequence(input_, position_, length))
                        return fail(signal_synth::ecg_json_syntax, "invalid UTF-8");
                    output.append(input_, position_, length);
                    position_ += length;
                }
            }
            return fail(signal_synth::ecg_json_syntax, "unterminated string");
        }

        bool parse_number(json_value& output)
        {
            const std::size_t start = position_;
            if (input_[position_] == '-')
                ++position_;
            if (position_ >= input_.size())
                return fail(signal_synth::ecg_json_syntax, "incomplete number");
            if (input_[position_] == '0')
                ++position_;
            else
            {
                if (input_[position_] < '1' || input_[position_] > '9')
                    return fail(signal_synth::ecg_json_syntax, "invalid integer part");
                while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9')
                    ++position_;
            }
            if (position_ < input_.size() && input_[position_] == '.')
            {
                ++position_;
                const std::size_t fraction = position_;
                while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9')
                    ++position_;
                if (position_ == fraction)
                    return fail(signal_synth::ecg_json_syntax, "missing fraction digits");
            }
            if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E'))
            {
                ++position_;
                if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-'))
                    ++position_;
                const std::size_t exponent = position_;
                while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9')
                    ++position_;
                if (position_ == exponent)
                    return fail(signal_synth::ecg_json_syntax, "missing exponent digits");
            }
            output.token = input_.substr(start, position_ - start);
            char* end = 0;
            output.number = std::strtod(output.token.c_str(), &end);
            if (!end || *end != '\0' || !std::isfinite(output.number))
                return fail(signal_synth::ecg_json_range, "number is outside finite range");
            output.type = json_value::number_kind;
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

    void add_message(signal_synth::ecg_scenario_json_result& result, ecg_scenario_json_message_code code, const std::string& path, const std::string& message)
    {
        ecg_scenario_json_message item;
        item.code = code;
        item.path = path;
        item.message = message;
        result.messages.push_back(item);
    }

    bool allowed_fields(const json_value& object, const char* const* names, std::size_t count, const std::string& path, signal_synth::ecg_scenario_json_result& result)
    {
        for (std::size_t i = 0; i < object.object.size(); ++i)
        {
            bool allowed = false;
            for (std::size_t j = 0; j < count; ++j)
                allowed = allowed || object.object[i].first == names[j];
            if (!allowed)
            {
                add_message(result, signal_synth::ecg_json_unknown_field, path + "." + object.object[i].first, "unknown field");
                return false;
            }
        }
        return true;
    }

    const json_value* required(const json_value& object, const char* name, json_value::kind kind, const std::string& path, signal_synth::ecg_scenario_json_result& result)
    {
        const json_value* value = member(object, name);
        if (!value)
            add_message(result, signal_synth::ecg_json_missing_field, path + "." + name, "required field is missing");
        else if (value->type != kind)
            add_message(result, signal_synth::ecg_json_type, path + "." + name, "field has the wrong JSON type");
        else
            return value;
        return 0;
    }

    bool integral_number(const json_value& value, unsigned long long maximum, unsigned long long& output)
    {
        if (value.type != json_value::number_kind || value.number < 0.0 || value.number > static_cast<double>(maximum) || std::floor(value.number) != value.number)
            return false;
        if (value.number > 9007199254740991.0)
            return false;
        output = static_cast<unsigned long long>(value.number);
        return true;
    }

    bool safe_text(const std::string& value, std::size_t minimum, std::size_t maximum)
    {
        return value.size() >= minimum && value.size() <= maximum && value.find('\0') == std::string::npos && valid_utf8(value);
    }

    std::string escape_json(const std::string& value)
    {
        static const char hex[] = "0123456789abcdef";
        std::string output;
        output.push_back('"');
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            const unsigned char c = static_cast<unsigned char>(value[i]);
            switch (c)
            {
            case '"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (c < 0x20u)
                {
                    output += "\\u00";
                    output.push_back(hex[c >> 4]);
                    output.push_back(hex[c & 0x0fu]);
                }
                else
                    output.push_back(static_cast<char>(c));
            }
        }
        output.push_back('"');
        return output;
    }

    std::string format_double(double value)
    {
        if (value == 0.0)
            return "0";
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
        return output.str();
    }

    const char* av_pattern_name(signal_synth::ecg_second_degree_av_pattern value)
    {
        switch (value)
        {
        case signal_synth::ecg_second_degree_unspecified: return "unspecified";
        case signal_synth::ecg_second_degree_mobitz_i: return "mobitz_i";
        case signal_synth::ecg_second_degree_mobitz_ii: return "mobitz_ii";
        }
        return "";
    }

    const char* q_territory_name(signal_synth::ecg_q_wave_territory value)
    {
        switch (value)
        {
        case signal_synth::ecg_q_wave_unspecified: return "unspecified";
        case signal_synth::ecg_q_wave_inferior: return "inferior";
        case signal_synth::ecg_q_wave_anterior: return "anterior";
        case signal_synth::ecg_q_wave_lateral: return "lateral";
        }
        return "";
    }

    const char* fidelity_name(signal_synth::ecg_scenario_fidelity_policy value)
    {
        switch (value)
        {
        case signal_synth::ecg_fidelity_native_only: return "native_only";
        case signal_synth::ecg_fidelity_allow_parameterized: return "allow_parameterized";
        }
        return "";
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

    bool validate_document(const signal_synth::ecg_scenario_document& document, signal_synth::ecg_scenario_json_result& result, std::vector<std::string>& sorted_tags)
    {
        if (document.schema_version != 1)
            add_message(result, signal_synth::ecg_json_schema_version, "$.schema_version", "only schema version 1 is supported");
        if (!safe_text(document.scenario_id, 1, 128))
            add_message(result, signal_synth::ecg_json_range, "$.scenario_id", "scenario_id must contain 1 to 128 valid UTF-8 bytes");
        if (!safe_text(document.name, 1, 256))
            add_message(result, signal_synth::ecg_json_range, "$.name", "name must contain 1 to 256 valid UTF-8 bytes");
        if (!safe_text(document.description, 0, 4096))
            add_message(result, signal_synth::ecg_json_range, "$.description", "description is invalid or too long");
        if (!safe_text(document.author, 0, 256))
            add_message(result, signal_synth::ecg_json_range, "$.author", "author is invalid or too long");
        if (document.sample_count() == 0)
            add_message(result, signal_synth::ecg_json_range, "$.duration_seconds", "duration and sample rate must produce a positive integral sample count");
        if (document.tags.size() > 64)
            add_message(result, signal_synth::ecg_json_range, "$.tags", "at most 64 tags are allowed");

        sorted_tags = document.tags;
        std::sort(sorted_tags.begin(), sorted_tags.end());
        for (std::size_t i = 0; i < sorted_tags.size(); ++i)
        {
            if (!safe_text(sorted_tags[i], 1, 64))
                add_message(result, signal_synth::ecg_json_range, "$.tags", "tags must contain 1 to 64 valid UTF-8 bytes");
            if (i && sorted_tags[i] == sorted_tags[i - 1])
                add_message(result, signal_synth::ecg_json_duplicate_tag, "$.tags", "duplicate tag");
        }
        if (document.ecg.condition_count() == 0)
            add_message(result, signal_synth::ecg_json_semantic, "$.ecg.conditions", "at least one condition is required");

        signal_synth::ecg_scenario_report semantic;
        if (!signal_synth::ecg_scenario_engine().validate(document.ecg, semantic))
        {
            for (unsigned int i = 0; i < semantic.issue_count(); ++i)
                if (semantic.issue_severity(i) == signal_synth::ecg_issue_error)
                    add_message(result, signal_synth::ecg_json_semantic, "$.ecg", semantic.issue_message(i));
        }
        return result.messages.empty();
    }

    std::string canonical_document(const signal_synth::ecg_scenario_document& document, const std::vector<std::string>& tags)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << "{\"schema_version\":1,\"scenario_id\":" << escape_json(document.scenario_id)
               << ",\"name\":" << escape_json(document.name)
               << ",\"description\":" << escape_json(document.description)
               << ",\"author\":" << escape_json(document.author)
               << ",\"tags\":[";
        for (std::size_t i = 0; i < tags.size(); ++i)
        {
            if (i)
                output << ',';
            output << escape_json(tags[i]);
        }
        output << "],\"duration_seconds\":" << format_double(document.duration_seconds)
               << ",\"sample_rate_hz\":" << document.ecg.sampling_rate_hz()
               << ",\"seed\":" << document.ecg.seed()
               << ",\"ecg\":{\"heart_rate_bpm\":" << format_double(document.ecg.heart_rate_bpm())
               << ",\"rr_variability_seconds\":" << format_double(document.ecg.rr_variability_seconds())
               << ",\"ectopic_every_n_beats\":" << document.ecg.ectopic_every_n_beats()
               << ",\"second_degree_av_pattern\":" << escape_json(av_pattern_name(document.ecg.second_degree_av_pattern()))
               << ",\"q_wave_territory\":" << escape_json(q_territory_name(document.ecg.q_wave_territory()))
               << ",\"fidelity_policy\":" << escape_json(fidelity_name(document.ecg.fidelity_policy()))
               << ",\"conditions\":[";
        for (unsigned int i = 0; i < document.ecg.condition_count(); ++i)
        {
            if (i)
                output << ',';
            const signal_synth::ecg_condition_info* info = signal_synth::find_ecg_condition(document.ecg.condition(i));
            output << "{\"code\":" << escape_json(info ? info->scp_code : "")
                   << ",\"severity\":" << format_double(document.ecg.condition_severity(i)) << '}';
        }
        output << "]}}";
        return output.str();
    }
}

namespace signal_synth
{
    ecg_scenario_document::ecg_scenario_document()
        : schema_version(1), scenario_id("ecg_scenario"), name("ECG scenario"), description(""), author(""), duration_seconds(10.0)
    {
        ecg.add_condition(ecg_condition_norm);
    }

    unsigned int ecg_scenario_document::sample_count() const
    {
        const double samples = duration_seconds * static_cast<double>(ecg.sampling_rate_hz());
        if (!std::isfinite(samples) || samples < 1.0 || samples > static_cast<double>(std::numeric_limits<unsigned int>::max()) || std::floor(samples) != samples)
            return 0;
        return static_cast<unsigned int>(samples);
    }

    ecg_scenario_json_result::ecg_scenario_json_result()
        : success(false), generation_fingerprint(0)
    {
    }

    bool write_ecg_scenario_json(const ecg_scenario_document& document, ecg_scenario_json_result& result)
    {
        ecg_scenario_json_result fresh;
        std::vector<std::string> tags;
        if (!validate_document(document, fresh, tags))
        {
            result = fresh;
            return false;
        }
        fresh.canonical_json = canonical_document(document, tags);
        fresh.document_fingerprint = sha256(fresh.canonical_json);
        fresh.generation_fingerprint = document.ecg.fingerprint();
        fresh.success = true;
        result = fresh;
        return true;
    }

    bool parse_ecg_scenario_json(const std::string& json, ecg_scenario_document& output, ecg_scenario_json_result& result)
    {
        ecg_scenario_json_result fresh_result;
        json_value root;
        json_parser parser(json);
        if (!parser.parse(root))
        {
            fresh_result.messages.push_back(parser.message());
            result = fresh_result;
            return false;
        }
        if (root.type != json_value::object_kind)
        {
            add_message(fresh_result, ecg_json_type, "$", "scenario document must be an object");
            result = fresh_result;
            return false;
        }

        const char* top_fields[] = {"schema_version","scenario_id","name","description","author","tags","duration_seconds","sample_rate_hz","seed","ecg"};
        if (!allowed_fields(root, top_fields, sizeof(top_fields) / sizeof(top_fields[0]), "$", fresh_result))
        {
            result = fresh_result;
            return false;
        }

        const json_value* schema = required(root, "schema_version", json_value::number_kind, "$", fresh_result);
        const json_value* scenario_id = required(root, "scenario_id", json_value::string_kind, "$", fresh_result);
        const json_value* name = required(root, "name", json_value::string_kind, "$", fresh_result);
        const json_value* description = required(root, "description", json_value::string_kind, "$", fresh_result);
        const json_value* author = required(root, "author", json_value::string_kind, "$", fresh_result);
        const json_value* tags = required(root, "tags", json_value::array_kind, "$", fresh_result);
        const json_value* duration = required(root, "duration_seconds", json_value::number_kind, "$", fresh_result);
        const json_value* sample_rate = required(root, "sample_rate_hz", json_value::number_kind, "$", fresh_result);
        const json_value* seed = required(root, "seed", json_value::number_kind, "$", fresh_result);
        const json_value* ecg = required(root, "ecg", json_value::object_kind, "$", fresh_result);
        if (!fresh_result.messages.empty())
        {
            result = fresh_result;
            return false;
        }

        unsigned long long integer = 0;
        if (!integral_number(*schema, 1, integer) || integer != 1)
            add_message(fresh_result, ecg_json_schema_version, "$.schema_version", "only schema version 1 is supported");
        ecg_scenario_document document;
        document.ecg.clear_conditions();
        document.scenario_id = scenario_id->string;
        document.name = name->string;
        document.description = description->string;
        document.author = author->string;
        document.duration_seconds = duration->number;

        std::set<std::string> seen_tags;
        for (std::size_t i = 0; i < tags->array.size(); ++i)
        {
            if (tags->array[i].type != json_value::string_kind)
            {
                add_message(fresh_result, ecg_json_type, "$.tags", "every tag must be a string");
                break;
            }
            if (!seen_tags.insert(tags->array[i].string).second)
            {
                add_message(fresh_result, ecg_json_duplicate_tag, "$.tags", "duplicate tag");
                break;
            }
            document.tags.push_back(tags->array[i].string);
        }
        if (!integral_number(*sample_rate, std::numeric_limits<unsigned int>::max(), integer) || !document.ecg.set_sampling_rate_hz(static_cast<unsigned int>(integer)))
            add_message(fresh_result, ecg_json_range, "$.sample_rate_hz", "invalid sampling rate");
        if (!integral_number(*seed, 9007199254740991ull, integer) || !document.ecg.set_seed(integer))
            add_message(fresh_result, ecg_json_range, "$.seed", "seed must be an exactly representable non-negative integer");

        const char* ecg_fields[] = {"heart_rate_bpm","rr_variability_seconds","ectopic_every_n_beats","second_degree_av_pattern","q_wave_territory","fidelity_policy","conditions"};
        allowed_fields(*ecg, ecg_fields, sizeof(ecg_fields) / sizeof(ecg_fields[0]), "$.ecg", fresh_result);
        const json_value* heart_rate = required(*ecg, "heart_rate_bpm", json_value::number_kind, "$.ecg", fresh_result);
        const json_value* rr_variability = required(*ecg, "rr_variability_seconds", json_value::number_kind, "$.ecg", fresh_result);
        const json_value* ectopic = required(*ecg, "ectopic_every_n_beats", json_value::number_kind, "$.ecg", fresh_result);
        const json_value* av_pattern = required(*ecg, "second_degree_av_pattern", json_value::string_kind, "$.ecg", fresh_result);
        const json_value* territory = required(*ecg, "q_wave_territory", json_value::string_kind, "$.ecg", fresh_result);
        const json_value* fidelity = required(*ecg, "fidelity_policy", json_value::string_kind, "$.ecg", fresh_result);
        const json_value* conditions = required(*ecg, "conditions", json_value::array_kind, "$.ecg", fresh_result);

        if (heart_rate && !document.ecg.set_heart_rate_bpm(heart_rate->number))
            add_message(fresh_result, ecg_json_range, "$.ecg.heart_rate_bpm", "invalid heart rate");
        if (rr_variability && !document.ecg.set_rr_variability_seconds(rr_variability->number))
            add_message(fresh_result, ecg_json_range, "$.ecg.rr_variability_seconds", "invalid RR variability");
        if (ectopic && (!integral_number(*ectopic, std::numeric_limits<unsigned int>::max(), integer) || !document.ecg.set_ectopic_every_n_beats(static_cast<unsigned int>(integer))))
            add_message(fresh_result, ecg_json_range, "$.ecg.ectopic_every_n_beats", "invalid ectopic cadence");

        if (av_pattern)
        {
            ecg_second_degree_av_pattern value = ecg_second_degree_unspecified;
            if (av_pattern->string == "mobitz_i")
                value = ecg_second_degree_mobitz_i;
            else if (av_pattern->string == "mobitz_ii")
                value = ecg_second_degree_mobitz_ii;
            else if (av_pattern->string != "unspecified")
                add_message(fresh_result, ecg_json_range, "$.ecg.second_degree_av_pattern", "unknown AV pattern");
            document.ecg.set_second_degree_av_pattern(value);
        }
        if (territory)
        {
            ecg_q_wave_territory value = ecg_q_wave_unspecified;
            if (territory->string == "inferior")
                value = ecg_q_wave_inferior;
            else if (territory->string == "anterior")
                value = ecg_q_wave_anterior;
            else if (territory->string == "lateral")
                value = ecg_q_wave_lateral;
            else if (territory->string != "unspecified")
                add_message(fresh_result, ecg_json_range, "$.ecg.q_wave_territory", "unknown Q-wave territory");
            document.ecg.set_q_wave_territory(value);
        }
        if (fidelity)
        {
            ecg_scenario_fidelity_policy value = ecg_fidelity_allow_parameterized;
            if (fidelity->string == "native_only")
                value = ecg_fidelity_native_only;
            else if (fidelity->string != "allow_parameterized")
                add_message(fresh_result, ecg_json_range, "$.ecg.fidelity_policy", "unknown fidelity policy");
            document.ecg.set_fidelity_policy(value);
        }

        std::set<int> seen_conditions;
        if (conditions)
        {
            for (std::size_t i = 0; i < conditions->array.size(); ++i)
            {
                const std::string path = "$.ecg.conditions[" + std::to_string(i) + "]";
                const json_value& item = conditions->array[i];
                if (item.type != json_value::object_kind)
                {
                    add_message(fresh_result, ecg_json_type, path, "condition must be an object");
                    continue;
                }
                const char* fields[] = {"code","severity"};
                if (!allowed_fields(item, fields, 2, path, fresh_result))
                    continue;
                const json_value* code = required(item, "code", json_value::string_kind, path, fresh_result);
                const json_value* severity = required(item, "severity", json_value::number_kind, path, fresh_result);
                if (!code || !severity)
                    continue;
                const ecg_condition_info* info = find_ecg_condition(code->string.c_str());
                if (!info)
                {
                    add_message(fresh_result, ecg_json_range, path + ".code", "unknown condition code");
                    continue;
                }
                if (!seen_conditions.insert(static_cast<int>(info->code)).second)
                {
                    add_message(fresh_result, ecg_json_duplicate_condition, path + ".code", "duplicate condition");
                    continue;
                }
                if (!document.ecg.add_condition(info->code, severity->number))
                    add_message(fresh_result, ecg_json_range, path + ".severity", "invalid condition severity");
            }
        }

        if (!fresh_result.messages.empty())
        {
            result = fresh_result;
            return false;
        }
        if (!write_ecg_scenario_json(document, fresh_result))
        {
            result = fresh_result;
            return false;
        }
        output = document;
        result = fresh_result;
        return true;
    }
}
