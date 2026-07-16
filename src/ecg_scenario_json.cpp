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

    std::string json_index(std::size_t index)
    {
        std::ostringstream output;
        output << index;
        return output.str();
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
        if (value.type != json_value::number_kind || value.token.empty())
            return false;
        unsigned long long parsed = 0;
        for (std::size_t i = 0; i < value.token.size(); ++i)
        {
            const char c = value.token[i];
            if (c < '0' || c > '9')
                return false;
            const unsigned int digit = static_cast<unsigned int>(c - '0');
            if (parsed > (maximum - digit) / 10u)
                return false;
            parsed = parsed * 10u + digit;
        }
        output = parsed;
        return true;
    }

    bool safe_text(const std::string& value, std::size_t minimum, std::size_t maximum)
    {
        return value.size() >= minimum && value.size() <= maximum && value.find('\0') == std::string::npos && valid_utf8(value);
    }

    bool safe_identifier(const std::string& value)
    {
        if (value.empty() || value.size() > 128)
            return false;
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            const char c = value[i];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
                return false;
        }
        return true;
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

    const char* episode_type_name(signal_synth::ecg_episode_type value)
    {
        switch (value)
        {
        case signal_synth::ecg_episode_none: return "none";
        case signal_synth::ecg_episode_psvt: return "psvt";
        case signal_synth::ecg_episode_svarr: return "svarr";
        }
        return "";
    }

    const char* flutter_pattern_name(signal_synth::ecg_flutter_conduction_pattern value)
    {
        switch (value)
        {
        case signal_synth::ecg_flutter_fixed: return "fixed";
        case signal_synth::ecg_flutter_alternate_2_3: return "alternate_2_3";
        case signal_synth::ecg_flutter_cycle_2_3_4: return "cycle_2_3_4";
        }
        return "";
    }

    const char* pacing_mode_name(signal_synth::ecg_pacing_mode value)
    {
        switch (value)
        {
        case signal_synth::ecg_pacing_ventricular: return "ventricular";
        case signal_synth::ecg_pacing_atrial: return "atrial";
        case signal_synth::ecg_pacing_dual_chamber: return "dual_chamber";
        }
        return "";
    }

    bool artifact_type_from_name(const std::string& name, signal_synth::signal_quality_artifact_type& output)
    {
        if (name == "ecg_baseline_wander")
            output = signal_synth::signal_quality_ecg_baseline_wander;
        else if (name == "ecg_powerline")
            output = signal_synth::signal_quality_ecg_powerline;
        else if (name == "ecg_emg_noise")
            output = signal_synth::signal_quality_ecg_emg_noise;
        else if (name == "ecg_dropout")
            output = signal_synth::signal_quality_ecg_dropout;
        else if (name == "ecg_saturation")
            output = signal_synth::signal_quality_ecg_saturation;
        else if (name == "ppg_dropout")
            output = signal_synth::signal_quality_ppg_dropout;
        else if (name == "ecg_lead_reversal")
            output = signal_synth::signal_quality_ecg_lead_reversal;
        else if (name == "ecg_lead_swap")
            output = signal_synth::signal_quality_ecg_lead_swap;
        else if (name == "ecg_electrode_misplacement")
            output = signal_synth::signal_quality_ecg_electrode_misplacement;
        else if (name == "ecg_gain_mismatch")
            output = signal_synth::signal_quality_ecg_gain_mismatch;
        else if (name == "ecg_offset_drift")
            output = signal_synth::signal_quality_ecg_offset_drift;
        else if (name == "ecg_clock_drift")
            output = signal_synth::signal_quality_ecg_clock_drift;
        else if (name == "ecg_dropped_samples")
            output = signal_synth::signal_quality_ecg_dropped_samples;
        else if (name == "ecg_quantization")
            output = signal_synth::signal_quality_ecg_quantization;
        else if (name == "ecg_adc_clipping")
            output = signal_synth::signal_quality_ecg_adc_clipping;
        else if (name == "ppg_motion_periodic")
            output = signal_synth::signal_quality_ppg_motion_periodic;
        else if (name == "ppg_motion_burst")
            output = signal_synth::signal_quality_ppg_motion_burst;
        else if (name == "ppg_motion_broadband")
            output = signal_synth::signal_quality_ppg_motion_broadband;
        else if (name == "ppg_ambient_light")
            output = signal_synth::signal_quality_ppg_ambient_light;
        else if (name == "ppg_sensor_saturation")
            output = signal_synth::signal_quality_ppg_sensor_saturation;
        else
            return false;
        return true;
    }

    bool artifact_is_ecg(signal_synth::signal_quality_artifact_type type)
    {
        return !signal_synth::signal_quality_artifact_is_ppg(type);
    }

    const char* clinical_lead_json_name(unsigned int lead)
    {
        static const char* names[signal_synth::clinical_lead_count] = {"I","II","III","aVR","aVL","aVF","V1","V2","V3","V4","V5","V6"};
        return lead < signal_synth::clinical_lead_count ? names[lead] : "";
    }

    int clinical_lead_from_json_name(const std::string& name)
    {
        for (unsigned int lead = 0; lead < signal_synth::clinical_lead_count; ++lead)
            if (name == clinical_lead_json_name(lead))
                return static_cast<int>(lead);
        return -1;
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

    bool default_ppg_config(const signal_synth::ppg_config& config)
    {
        const signal_synth::ppg_config defaults;
        return config.enabled == defaults.enabled
            && config.pulse_delay_ms == defaults.pulse_delay_ms
            && config.rise_time_ms == defaults.rise_time_ms
            && config.decay_time_ms == defaults.decay_time_ms
            && config.amplitude_au == defaults.amplitude_au
            && config.baseline_au == defaults.baseline_au
            && config.dicrotic_delay_ms == defaults.dicrotic_delay_ms
            && config.dicrotic_width_ms == defaults.dicrotic_width_ms
            && config.dicrotic_amplitude_ratio == defaults.dicrotic_amplitude_ratio
            && config.pulse_delay_variation_ms == defaults.pulse_delay_variation_ms
            && config.pulse_delay_variation_hz == defaults.pulse_delay_variation_hz
            && config.missing_pulse_every_n_beats == defaults.missing_pulse_every_n_beats
            && config.clock_drift_ppm == defaults.clock_drift_ppm
            && config.pulse_delay_jitter_ms == defaults.pulse_delay_jitter_ms
            && config.low_frequency_amplitude_modulation_ratio == defaults.low_frequency_amplitude_modulation_ratio
            && config.low_frequency_amplitude_modulation_hz == defaults.low_frequency_amplitude_modulation_hz
            && config.rise_time_variation_ratio == defaults.rise_time_variation_ratio
            && config.decay_time_variation_ratio == defaults.decay_time_variation_ratio
            && config.pac_pulse_amplitude_scale == defaults.pac_pulse_amplitude_scale
            && config.pvc_pulse_amplitude_scale == defaults.pvc_pulse_amplitude_scale
            && config.paced_pulse_amplitude_scale == defaults.paced_pulse_amplitude_scale
            && config.seed == defaults.seed
            && config.perfusion_episodes.empty();
    }

    bool default_hrv_config(const signal_synth::hrv_scenario_config& config)
    {
        const signal_synth::hrv_scenario_config defaults;
        return config.enabled == defaults.enabled
            && config.target_mean_hr_bpm == defaults.target_mean_hr_bpm
            && config.target_sdnn_seconds == defaults.target_sdnn_seconds
            && config.lf_hf_ratio == defaults.lf_hf_ratio
            && config.lf_center_hz == defaults.lf_center_hz
            && config.lf_bandwidth_hz == defaults.lf_bandwidth_hz
            && config.hf_center_hz == defaults.hf_center_hz
            && config.hf_bandwidth_hz == defaults.hf_bandwidth_hz
            && config.respiratory_frequency_hz == defaults.respiratory_frequency_hz
            && config.respiratory_amplitude_seconds == defaults.respiratory_amplitude_seconds
            && config.minimum_rr_seconds == defaults.minimum_rr_seconds
            && config.maximum_rr_seconds == defaults.maximum_rr_seconds
            && config.seed == defaults.seed;
    }

    bool valid_hrv_config(const signal_synth::hrv_scenario_config& config, double duration_seconds)
    {
        if (!config.enabled)
            return default_hrv_config(config);
        if (!std::isfinite(config.target_mean_hr_bpm) || config.target_mean_hr_bpm < 30.0 || config.target_mean_hr_bpm > 220.0)
            return false;
        if (!std::isfinite(config.target_sdnn_seconds) || config.target_sdnn_seconds < 0.0 || config.target_sdnn_seconds > 2.0)
            return false;
        if (!std::isfinite(config.lf_hf_ratio) || config.lf_hf_ratio < 0.0 || config.lf_hf_ratio > 100.0)
            return false;
        if (!std::isfinite(config.lf_center_hz) || config.lf_center_hz <= 0.0 || config.lf_center_hz > 1.0)
            return false;
        if (!std::isfinite(config.lf_bandwidth_hz) || config.lf_bandwidth_hz <= 0.0 || config.lf_bandwidth_hz > 1.0)
            return false;
        if (!std::isfinite(config.hf_center_hz) || config.hf_center_hz <= 0.0 || config.hf_center_hz > 1.0)
            return false;
        if (!std::isfinite(config.hf_bandwidth_hz) || config.hf_bandwidth_hz <= 0.0 || config.hf_bandwidth_hz > 1.0)
            return false;
        if (!std::isfinite(config.respiratory_frequency_hz) || config.respiratory_frequency_hz <= 0.0 || config.respiratory_frequency_hz > 1.0)
            return false;
        if (!std::isfinite(config.respiratory_amplitude_seconds) || config.respiratory_amplitude_seconds < 0.0 || config.respiratory_amplitude_seconds > 2.0)
            return false;
        if (config.respiratory_amplitude_seconds > std::sqrt(2.0) * config.target_sdnn_seconds)
            return false;
        if (!std::isfinite(config.minimum_rr_seconds) || !std::isfinite(config.maximum_rr_seconds) || config.minimum_rr_seconds <= 0.0 || config.maximum_rr_seconds <= config.minimum_rr_seconds)
            return false;
        const double mean_rr_seconds = 60.0 / config.target_mean_hr_bpm;
        if (mean_rr_seconds <= config.minimum_rr_seconds || mean_rr_seconds >= config.maximum_rr_seconds)
            return false;
        return duration_seconds >= 300.0;
    }

    bool default_v3_config(const signal_synth::ecg_scenario_document& document)
    {
        const signal_synth::scenario_randomization_config randomization;
        const signal_synth::physiology_coupling_config physiology;
        const signal_synth::scenario_output_config output;
        const signal_synth::ppg_config ppg;
        return document.ppg.pulse_delay_variation_ms == ppg.pulse_delay_variation_ms
            && document.ppg.pulse_delay_variation_hz == ppg.pulse_delay_variation_hz
            && document.ppg.missing_pulse_every_n_beats == ppg.missing_pulse_every_n_beats
            && document.ppg.clock_drift_ppm == ppg.clock_drift_ppm
            && document.ppg.seed == ppg.seed
            && !document.ecg.has_morphology_controls()
            && document.randomization.enabled == randomization.enabled
            && document.randomization.seed == randomization.seed
            && document.randomization.envelopes.empty()
            && document.physiology.respiration_frequency_hz == physiology.respiration_frequency_hz
            && document.physiology.respiratory_rr_amplitude_seconds == physiology.respiratory_rr_amplitude_seconds
            && document.physiology.ecg_baseline_amplitude_mv == physiology.ecg_baseline_amplitude_mv
            && document.physiology.ppg_amplitude_modulation_ratio == physiology.ppg_amplitude_modulation_ratio
            && document.physiology.activity_start_seconds == physiology.activity_start_seconds
            && document.physiology.activity_duration_seconds == physiology.activity_duration_seconds
            && document.physiology.activity_intensity == physiology.activity_intensity
            && document.physiology.seed == physiology.seed
            && document.output.compact == output.compact
            && document.output.retain_source_channels == output.retain_source_channels
            && document.output.include_waveform_csv == output.include_waveform_csv
            && document.output.include_edf_bdf == output.include_edf_bdf;
    }

    bool randomization_parameter_bounds(const std::string& parameter, double& minimum, double& maximum)
    {
        if (parameter == "ecg.heart_rate_bpm") { minimum = 10.0; maximum = 400.0; return true; }
        if (parameter == "ecg.rr_variability_seconds") { minimum = 0.0; maximum = 2.0; return true; }
        const std::string morphology_prefix = "ecg.morphology.";
        if (parameter.size() > morphology_prefix.size() && parameter.compare(0, morphology_prefix.size(), morphology_prefix) == 0)
        {
            signal_synth::ecg_morphology_control control = signal_synth::ecg_morphology_control_count;
            return signal_synth::ecg_morphology_control_from_name(parameter.c_str() + morphology_prefix.size(), control)
                && signal_synth::ecg_morphology_control_bounds(control, minimum, maximum);
        }
        if (parameter == "ppg.pulse_delay_ms") { minimum = 0.0; maximum = 2000.0; return true; }
        if (parameter == "ppg.amplitude_au") { minimum = 0.000001; maximum = 100.0; return true; }
        if (parameter == "hrv.target_sdnn_seconds") { minimum = 0.0; maximum = 2.0; return true; }
        if (parameter == "hrv.lf_hf_ratio") { minimum = 0.0; maximum = 100.0; return true; }
        if (parameter == "physiology.activity_intensity") { minimum = 0.0; maximum = 1.0; return true; }
        return false;
    }

    bool default_v4_config(const signal_synth::ecg_scenario_document& document)
    {
        const signal_synth::ppg_config ppg;
        return document.ppg.pulse_delay_jitter_ms == ppg.pulse_delay_jitter_ms
            && document.ppg.low_frequency_amplitude_modulation_ratio == ppg.low_frequency_amplitude_modulation_ratio
            && document.ppg.low_frequency_amplitude_modulation_hz == ppg.low_frequency_amplitude_modulation_hz
            && document.ppg.rise_time_variation_ratio == ppg.rise_time_variation_ratio
            && document.ppg.decay_time_variation_ratio == ppg.decay_time_variation_ratio
            && document.ppg.pac_pulse_amplitude_scale == ppg.pac_pulse_amplitude_scale
            && document.ppg.pvc_pulse_amplitude_scale == ppg.pvc_pulse_amplitude_scale
            && document.ppg.paced_pulse_amplitude_scale == ppg.paced_pulse_amplitude_scale
            && document.ppg.perfusion_episodes.empty();
    }

    bool valid_v3_config(const signal_synth::ecg_scenario_document& document)
    {
        const signal_synth::scenario_randomization_config& randomization = document.randomization;
        if (!randomization.enabled && !randomization.envelopes.empty())
            return false;
        if (randomization.enabled && (randomization.envelopes.empty() || randomization.envelopes.size() > 32u))
            return false;
        std::set<std::string> parameters;
        for (std::size_t i = 0; i < randomization.envelopes.size(); ++i)
        {
            const signal_synth::scenario_randomization_envelope& envelope = randomization.envelopes[i];
            double minimum = 0.0, maximum = 0.0;
            if (!randomization_parameter_bounds(envelope.parameter, minimum, maximum) || !parameters.insert(envelope.parameter).second
                || !std::isfinite(envelope.minimum) || !std::isfinite(envelope.maximum) || envelope.minimum > envelope.maximum
                || envelope.minimum < minimum || envelope.maximum > maximum)
                return false;
        }
        if (document.hrv.enabled)
            for (std::size_t i = 0; i < randomization.envelopes.size(); ++i)
                if (randomization.envelopes[i].parameter == "ecg.heart_rate_bpm" || randomization.envelopes[i].parameter == "ecg.rr_variability_seconds")
                    return false;
        const signal_synth::physiology_coupling_config& physiology = document.physiology;
        const double activity_end = physiology.activity_start_seconds + physiology.activity_duration_seconds;
        if (!std::isfinite(physiology.respiration_frequency_hz) || physiology.respiration_frequency_hz <= 0.0 || physiology.respiration_frequency_hz > 1.0
            || !std::isfinite(physiology.respiratory_rr_amplitude_seconds) || physiology.respiratory_rr_amplitude_seconds < 0.0 || physiology.respiratory_rr_amplitude_seconds > 2.0
            || !std::isfinite(physiology.ecg_baseline_amplitude_mv) || physiology.ecg_baseline_amplitude_mv < 0.0 || physiology.ecg_baseline_amplitude_mv > 5.0
            || !std::isfinite(physiology.ppg_amplitude_modulation_ratio) || physiology.ppg_amplitude_modulation_ratio < 0.0 || physiology.ppg_amplitude_modulation_ratio > 1.0
            || !std::isfinite(physiology.activity_start_seconds) || physiology.activity_start_seconds < 0.0
            || !std::isfinite(physiology.activity_duration_seconds) || physiology.activity_duration_seconds < 0.0
            || !std::isfinite(physiology.activity_intensity) || physiology.activity_intensity < 0.0 || physiology.activity_intensity > 1.0
            || !std::isfinite(activity_end) || activity_end > document.duration_seconds)
            return false;
        if (physiology.activity_intensity > 0.0 && physiology.activity_duration_seconds <= 0.0)
            return false;
        if (physiology.activity_intensity > 0.0 && (document.ecg.has_condition(signal_synth::ecg_condition_afib) || document.ecg.has_condition(signal_synth::ecg_condition_aflt)
            || document.ecg.has_condition(signal_synth::ecg_condition_svtac) || document.ecg.has_condition(signal_synth::ecg_condition_pace)
            || document.ecg.has_condition(signal_synth::ecg_condition_psvt) || document.ecg.has_condition(signal_synth::ecg_condition_svarr)
            || document.ecg.has_condition(signal_synth::ecg_condition_1avb) || document.ecg.has_condition(signal_synth::ecg_condition_2avb) || document.ecg.has_condition(signal_synth::ecg_condition_3avb)))
            return false;
        if ((physiology.ppg_amplitude_modulation_ratio > 0.0 || document.ppg.pulse_delay_variation_ms > 0.0 || document.ppg.missing_pulse_every_n_beats > 0 || document.ppg.clock_drift_ppm != 0.0) && !document.ppg.enabled)
            return false;
        double minimum_pulse_delay_ms = document.ppg.pulse_delay_ms;
        for (std::size_t i = 0; i < randomization.envelopes.size(); ++i)
            if (randomization.envelopes[i].parameter == "ppg.pulse_delay_ms")
                minimum_pulse_delay_ms = randomization.envelopes[i].minimum;
        minimum_pulse_delay_ms -= document.ppg.pulse_delay_variation_ms;
        if (document.ppg.clock_drift_ppm < 0.0)
            minimum_pulse_delay_ms += document.duration_seconds * document.ppg.clock_drift_ppm * 0.001;
        if (minimum_pulse_delay_ms < 0.0)
            return false;
        if (document.output.compact && (document.output.retain_source_channels || document.output.include_waveform_csv || document.output.include_edf_bdf))
            return false;
        return true;
    }

    bool valid_v4_config(const signal_synth::ecg_scenario_document& document)
    {
        if (!valid_v3_config(document))
            return false;
        if ((!document.ppg.perfusion_episodes.empty() || document.ppg.pulse_delay_jitter_ms > 0.0
            || document.ppg.low_frequency_amplitude_modulation_ratio > 0.0 || document.ppg.rise_time_variation_ratio > 0.0
            || document.ppg.decay_time_variation_ratio > 0.0 || document.ppg.pac_pulse_amplitude_scale != 1.0
            || document.ppg.pvc_pulse_amplitude_scale != 1.0 || document.ppg.paced_pulse_amplitude_scale != 1.0) && !document.ppg.enabled)
            return false;
        double minimum_pulse_delay_ms = document.ppg.pulse_delay_ms;
        for (std::size_t i = 0; i < document.randomization.envelopes.size(); ++i)
            if (document.randomization.envelopes[i].parameter == "ppg.pulse_delay_ms")
                minimum_pulse_delay_ms = document.randomization.envelopes[i].minimum;
        minimum_pulse_delay_ms -= document.ppg.pulse_delay_variation_ms + document.ppg.pulse_delay_jitter_ms;
        if (document.ppg.clock_drift_ppm < 0.0)
            minimum_pulse_delay_ms += document.duration_seconds * document.ppg.clock_drift_ppm * 0.001;
        if (minimum_pulse_delay_ms < 0.0)
            return false;
        for (std::size_t i = 0; i < document.ppg.perfusion_episodes.size(); ++i)
        {
            const signal_synth::ppg_perfusion_episode_config& episode = document.ppg.perfusion_episodes[i];
            if (episode.start_seconds + episode.duration_seconds > document.duration_seconds)
                return false;
        }
        return true;
    }

    bool validate_document(const signal_synth::ecg_scenario_document& document, signal_synth::ecg_scenario_json_result& result, std::vector<std::string>& sorted_tags)
    {
        if (document.schema_version < 1 || document.schema_version > 4)
            add_message(result, signal_synth::ecg_json_schema_version, "$.schema_version", "only schema versions 1, 2, 3, and 4 are supported");
        if (document.schema_version == 1 && !default_ppg_config(document.ppg))
            add_message(result, signal_synth::ecg_json_semantic, "$.ppg", "schema version 1 cannot represent PPG configuration");
        if (document.schema_version == 1 && !default_hrv_config(document.hrv))
            add_message(result, signal_synth::ecg_json_semantic, "$.hrv", "schema version 1 cannot represent HRV configuration");
        if (document.schema_version >= 2 && !valid_hrv_config(document.hrv, document.duration_seconds))
            add_message(result, signal_synth::ecg_json_range, "$.hrv", "invalid HRV configuration or unsupported short HRV window");
        if (document.schema_version == 1 && !document.signal_quality.artifacts.empty())
            add_message(result, signal_synth::ecg_json_semantic, "$.artifacts", "schema version 1 cannot represent acquisition artifacts");
        if (document.schema_version >= 2 && !signal_synth::ppg_generator(document.ppg).valid())
            add_message(result, signal_synth::ecg_json_range, "$.ppg", "invalid PPG configuration");
        if (document.schema_version < 3 && !default_v3_config(document))
            add_message(result, signal_synth::ecg_json_semantic, "$", "randomization, physiology, output, and PPG stress controls require schema version 3");
        if (document.schema_version >= 3 && !valid_v3_config(document))
            add_message(result, signal_synth::ecg_json_range, "$", "invalid schema-v3 randomization, physiology, output, or PPG stress configuration");
        if (document.schema_version < 4 && !default_v4_config(document))
            add_message(result, signal_synth::ecg_json_semantic, "$.ppg", "PPG physiology-v2 and perfusion controls require schema version 4");
        if (document.schema_version == 4 && !valid_v4_config(document))
            add_message(result, signal_synth::ecg_json_range, "$.ppg", "invalid schema-v4 PPG physiology or perfusion configuration");
        if (!signal_synth::validate_signal_quality_config(document.signal_quality, document.duration_seconds, document.ecg.sampling_rate_hz(), document.ppg.enabled))
            add_message(result, signal_synth::ecg_json_range, "$.artifacts", "invalid artifact configuration");
        if (!safe_identifier(document.scenario_id))
            add_message(result, signal_synth::ecg_json_range, "$.scenario_id", "scenario_id must contain 1 to 128 ASCII letters, digits, dots, underscores, or hyphens");
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
        output << "{\"schema_version\":" << document.schema_version << ",\"scenario_id\":" << escape_json(document.scenario_id)
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
               << ",\"episode_type\":" << escape_json(episode_type_name(document.ecg.episode_type()))
               << ",\"episode_start_seconds\":" << format_double(document.ecg.episode_start_seconds())
               << ",\"episode_duration_seconds\":" << format_double(document.ecg.episode_duration_seconds())
               << ",\"episode_rate_bpm\":" << format_double(document.ecg.episode_rate_bpm())
               << ",\"flutter_conduction_pattern\":" << escape_json(flutter_pattern_name(document.ecg.flutter_conduction_pattern()))
               << ",\"pacing_mode\":" << escape_json(pacing_mode_name(document.ecg.pacing_mode()))
               << ",\"pacing_non_capture_every_n_beats\":" << document.ecg.pacing_non_capture_every_n_beats()
               << ",\"fidelity_policy\":" << escape_json(fidelity_name(document.ecg.fidelity_policy()))
               << (document.ecg.has_morphology_controls() ? ",\"morphology\":{" : "");
        if (document.ecg.has_morphology_controls())
        {
            bool first_morphology = true;
            for (unsigned int index = 0; index < signal_synth::ecg_morphology_control_count; ++index)
            {
                const signal_synth::ecg_morphology_control control = static_cast<signal_synth::ecg_morphology_control>(index);
                if (!document.ecg.morphology_control_enabled(control))
                    continue;
                output << (first_morphology ? "" : ",") << escape_json(signal_synth::ecg_morphology_control_name(control))
                       << ':' << format_double(document.ecg.morphology_control_value(control));
                first_morphology = false;
            }
            output << '}';
        }
        output << ",\"conditions\":[";
        for (unsigned int i = 0; i < document.ecg.condition_count(); ++i)
        {
            if (i)
                output << ',';
            const signal_synth::ecg_condition_info* info = signal_synth::find_ecg_condition(document.ecg.condition(i));
            output << "{\"code\":" << escape_json(info ? info->scp_code : "")
                   << ",\"severity\":" << format_double(document.ecg.condition_severity(i)) << '}';
        }
        output << "]}";
        if (document.hrv.enabled)
        {
            output << ",\"hrv\":{\"enabled\":true"
                   << ",\"target_mean_hr_bpm\":" << format_double(document.hrv.target_mean_hr_bpm)
                   << ",\"target_sdnn_seconds\":" << format_double(document.hrv.target_sdnn_seconds)
                   << ",\"lf_hf_ratio\":" << format_double(document.hrv.lf_hf_ratio)
                   << ",\"lf_center_hz\":" << format_double(document.hrv.lf_center_hz)
                   << ",\"lf_bandwidth_hz\":" << format_double(document.hrv.lf_bandwidth_hz)
                   << ",\"hf_center_hz\":" << format_double(document.hrv.hf_center_hz)
                   << ",\"hf_bandwidth_hz\":" << format_double(document.hrv.hf_bandwidth_hz)
                   << ",\"respiratory_frequency_hz\":" << format_double(document.hrv.respiratory_frequency_hz)
                   << ",\"respiratory_amplitude_seconds\":" << format_double(document.hrv.respiratory_amplitude_seconds)
                   << ",\"minimum_rr_seconds\":" << format_double(document.hrv.minimum_rr_seconds)
                   << ",\"maximum_rr_seconds\":" << format_double(document.hrv.maximum_rr_seconds)
                   << ",\"seed\":" << document.hrv.seed
                   << '}';
        }
        if (document.schema_version >= 2)
        {
            output << ",\"ppg\":{\"enabled\":" << (document.ppg.enabled ? "true" : "false")
                   << ",\"pulse_delay_ms\":" << format_double(document.ppg.pulse_delay_ms)
                   << ",\"rise_time_ms\":" << format_double(document.ppg.rise_time_ms)
                   << ",\"decay_time_ms\":" << format_double(document.ppg.decay_time_ms)
                   << ",\"amplitude_au\":" << format_double(document.ppg.amplitude_au)
                   << ",\"baseline_au\":" << format_double(document.ppg.baseline_au)
                   << ",\"dicrotic_delay_ms\":" << format_double(document.ppg.dicrotic_delay_ms)
                   << ",\"dicrotic_width_ms\":" << format_double(document.ppg.dicrotic_width_ms)
                   << ",\"dicrotic_amplitude_ratio\":" << format_double(document.ppg.dicrotic_amplitude_ratio);
            if (document.schema_version >= 3)
                output << ",\"pulse_delay_variation_ms\":" << format_double(document.ppg.pulse_delay_variation_ms)
                       << ",\"pulse_delay_variation_hz\":" << format_double(document.ppg.pulse_delay_variation_hz)
                       << ",\"missing_pulse_every_n_beats\":" << document.ppg.missing_pulse_every_n_beats
                       << ",\"clock_drift_ppm\":" << format_double(document.ppg.clock_drift_ppm)
                       << ",\"seed\":" << document.ppg.seed;
            if (document.schema_version >= 4)
            {
                std::vector<signal_synth::ppg_perfusion_episode_config> episodes = document.ppg.perfusion_episodes;
                std::sort(episodes.begin(), episodes.end(), [](const signal_synth::ppg_perfusion_episode_config& left, const signal_synth::ppg_perfusion_episode_config& right) { return left.start_seconds < right.start_seconds; });
                output << ",\"pulse_delay_jitter_ms\":" << format_double(document.ppg.pulse_delay_jitter_ms)
                       << ",\"low_frequency_amplitude_modulation_ratio\":" << format_double(document.ppg.low_frequency_amplitude_modulation_ratio)
                       << ",\"low_frequency_amplitude_modulation_hz\":" << format_double(document.ppg.low_frequency_amplitude_modulation_hz)
                       << ",\"rise_time_variation_ratio\":" << format_double(document.ppg.rise_time_variation_ratio)
                       << ",\"decay_time_variation_ratio\":" << format_double(document.ppg.decay_time_variation_ratio)
                       << ",\"pac_pulse_amplitude_scale\":" << format_double(document.ppg.pac_pulse_amplitude_scale)
                       << ",\"pvc_pulse_amplitude_scale\":" << format_double(document.ppg.pvc_pulse_amplitude_scale)
                       << ",\"paced_pulse_amplitude_scale\":" << format_double(document.ppg.paced_pulse_amplitude_scale)
                       << ",\"perfusion_episodes\":[";
                for (std::size_t i = 0; i < episodes.size(); ++i)
                    output << (i ? "," : "") << "{\"start_seconds\":" << format_double(episodes[i].start_seconds)
                           << ",\"duration_seconds\":" << format_double(episodes[i].duration_seconds)
                           << ",\"amplitude_scale\":" << format_double(episodes[i].amplitude_scale)
                           << ",\"rise_time_scale\":" << format_double(episodes[i].rise_time_scale)
                           << ",\"decay_time_scale\":" << format_double(episodes[i].decay_time_scale)
                           << ",\"weak_pulse_every_n_beats\":" << episodes[i].weak_pulse_every_n_beats
                           << ",\"weak_pulse_amplitude_scale\":" << format_double(episodes[i].weak_pulse_amplitude_scale)
                           << ",\"missing_pulse_every_n_beats\":" << episodes[i].missing_pulse_every_n_beats << '}';
                output << ']';
            }
            output << '}';
        }
        if (document.schema_version >= 3)
        {
            std::vector<signal_synth::scenario_randomization_envelope> envelopes = document.randomization.envelopes;
            std::sort(envelopes.begin(), envelopes.end(), [](const signal_synth::scenario_randomization_envelope& left, const signal_synth::scenario_randomization_envelope& right) { return left.parameter < right.parameter; });
            output << ",\"randomization\":{\"enabled\":" << (document.randomization.enabled ? "true" : "false")
                   << ",\"seed\":" << document.randomization.seed
                   << ",\"envelopes\":[";
            for (std::size_t i = 0; i < envelopes.size(); ++i)
                output << (i ? "," : "") << "{\"parameter\":" << escape_json(envelopes[i].parameter)
                       << ",\"minimum\":" << format_double(envelopes[i].minimum)
                       << ",\"maximum\":" << format_double(envelopes[i].maximum) << '}';
            output << "]},\"physiology\":{\"respiration_frequency_hz\":" << format_double(document.physiology.respiration_frequency_hz)
                   << ",\"respiratory_rr_amplitude_seconds\":" << format_double(document.physiology.respiratory_rr_amplitude_seconds)
                   << ",\"ecg_baseline_amplitude_mv\":" << format_double(document.physiology.ecg_baseline_amplitude_mv)
                   << ",\"ppg_amplitude_modulation_ratio\":" << format_double(document.physiology.ppg_amplitude_modulation_ratio)
                   << ",\"activity_start_seconds\":" << format_double(document.physiology.activity_start_seconds)
                   << ",\"activity_duration_seconds\":" << format_double(document.physiology.activity_duration_seconds)
                   << ",\"activity_intensity\":" << format_double(document.physiology.activity_intensity)
                   << ",\"seed\":" << document.physiology.seed
                   << "},\"output\":{\"compact\":" << (document.output.compact ? "true" : "false")
                   << ",\"retain_source_channels\":" << (document.output.retain_source_channels ? "true" : "false")
                   << ",\"include_waveform_csv\":" << (document.output.include_waveform_csv ? "true" : "false")
                   << ",\"include_edf_bdf\":" << (document.output.include_edf_bdf ? "true" : "false") << '}';
        }
        if (!document.signal_quality.artifacts.empty())
        {
            output << ",\"artifacts\":[";
            for (std::size_t i = 0; i < document.signal_quality.artifacts.size(); ++i)
            {
                if (i)
                    output << ',';
                const signal_synth::signal_quality_artifact_config& artifact = document.signal_quality.artifacts[i];
                output << "{\"type\":" << escape_json(signal_synth::signal_quality_artifact_type_name(artifact.type))
                       << ",\"start_seconds\":" << format_double(artifact.start_seconds)
                       << ",\"duration_seconds\":" << format_double(artifact.duration_seconds)
                       << ",\"severity\":" << format_double(artifact.severity)
                       << ",\"seed\":" << artifact.seed
                       << ",\"channels\":[";
                bool first_channel = true;
                for (unsigned int lead = 0; lead < signal_synth::clinical_lead_count; ++lead)
                {
                    if (artifact.ecg_leads[lead])
                    {
                        if (!first_channel)
                            output << ',';
                        output << escape_json(clinical_lead_json_name(lead));
                        first_channel = false;
                    }
                }
                if (artifact.ppg)
                {
                    if (!first_channel)
                        output << ',';
                    output << "\"ppg_green\"";
                }
                output << "]}";
            }
            output << ']';
        }
        output << '}';
        return output.str();
    }
}

namespace signal_synth
{
    hrv_scenario_config::hrv_scenario_config()
        : enabled(false), target_mean_hr_bpm(60.0), target_sdnn_seconds(0.0), lf_hf_ratio(1.0), lf_center_hz(0.10), lf_bandwidth_hz(0.04), hf_center_hz(0.25), hf_bandwidth_hz(0.12), respiratory_frequency_hz(0.25), respiratory_amplitude_seconds(0.0), minimum_rr_seconds(0.25), maximum_rr_seconds(3.0), seed(0x4852565343454e31ULL)
    {
    }

    scenario_randomization_config::scenario_randomization_config()
        : enabled(false), seed(0x52414e444f4d5631ULL), envelopes()
    {
    }

    physiology_coupling_config::physiology_coupling_config()
        : respiration_frequency_hz(0.25), respiratory_rr_amplitude_seconds(0.0), ecg_baseline_amplitude_mv(0.0), ppg_amplitude_modulation_ratio(0.0), activity_start_seconds(0.0), activity_duration_seconds(0.0), activity_intensity(0.0), seed(0x50485953494f5631ULL)
    {
    }

    scenario_output_config::scenario_output_config()
        : compact(false), retain_source_channels(true), include_waveform_csv(true), include_edf_bdf(true)
    {
    }

    const char* ecg_scenario_json_message_code_name(ecg_scenario_json_message_code code)
    {
        switch (code)
        {
        case ecg_json_syntax: return "SCENARIO_JSON_SYNTAX";
        case ecg_json_duplicate_key: return "SCENARIO_JSON_DUPLICATE_KEY";
        case ecg_json_unknown_field: return "SCENARIO_JSON_UNKNOWN_FIELD";
        case ecg_json_missing_field: return "SCENARIO_JSON_MISSING_FIELD";
        case ecg_json_type: return "SCENARIO_JSON_TYPE";
        case ecg_json_range: return "SCENARIO_JSON_RANGE";
        case ecg_json_schema_version: return "SCENARIO_JSON_SCHEMA_VERSION";
        case ecg_json_duplicate_condition: return "SCENARIO_JSON_DUPLICATE_CONDITION";
        case ecg_json_duplicate_tag: return "SCENARIO_JSON_DUPLICATE_TAG";
        case ecg_json_semantic: return "SCENARIO_JSON_SEMANTIC";
        case ecg_json_internal: return "SCENARIO_JSON_INTERNAL";
        }
        return "SCENARIO_JSON_INTERNAL";
    }

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

        const char* top_fields[] = {"schema_version","scenario_id","name","description","author","tags","duration_seconds","sample_rate_hz","seed","ecg","hrv","ppg","randomization","physiology","output","artifacts"};
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
        if (!integral_number(*schema, 4, integer) || integer < 1 || integer > 4)
            add_message(fresh_result, ecg_json_schema_version, "$.schema_version", "only schema versions 1, 2, 3, and 4 are supported");
        ecg_scenario_document document;
        document.schema_version = static_cast<unsigned int>(integer);
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
        if (!integral_number(*seed, std::numeric_limits<unsigned long long>::max(), integer) || !document.ecg.set_seed(integer))
            add_message(fresh_result, ecg_json_range, "$.seed", "seed must be an unsigned 64-bit decimal integer");

        const char* ecg_fields[] = {"heart_rate_bpm","rr_variability_seconds","ectopic_every_n_beats","second_degree_av_pattern","q_wave_territory","episode_type","episode_start_seconds","episode_duration_seconds","episode_rate_bpm","flutter_conduction_pattern","pacing_mode","pacing_non_capture_every_n_beats","fidelity_policy","morphology","conditions"};
        allowed_fields(*ecg, ecg_fields, sizeof(ecg_fields) / sizeof(ecg_fields[0]), "$.ecg", fresh_result);
        const json_value* heart_rate = required(*ecg, "heart_rate_bpm", json_value::number_kind, "$.ecg", fresh_result);
        const json_value* rr_variability = required(*ecg, "rr_variability_seconds", json_value::number_kind, "$.ecg", fresh_result);
        const json_value* ectopic = required(*ecg, "ectopic_every_n_beats", json_value::number_kind, "$.ecg", fresh_result);
        const json_value* av_pattern = required(*ecg, "second_degree_av_pattern", json_value::string_kind, "$.ecg", fresh_result);
        const json_value* territory = required(*ecg, "q_wave_territory", json_value::string_kind, "$.ecg", fresh_result);
        const json_value* episode_type = member(*ecg, "episode_type");
        const json_value* episode_start = member(*ecg, "episode_start_seconds");
        const json_value* episode_duration = member(*ecg, "episode_duration_seconds");
        const json_value* episode_rate = member(*ecg, "episode_rate_bpm");
        const json_value* flutter_pattern = member(*ecg, "flutter_conduction_pattern");
        const json_value* pacing_mode = member(*ecg, "pacing_mode");
        const json_value* pacing_non_capture = member(*ecg, "pacing_non_capture_every_n_beats");
        const json_value* fidelity = required(*ecg, "fidelity_policy", json_value::string_kind, "$.ecg", fresh_result);
        const json_value* morphology = member(*ecg, "morphology");
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
        if (episode_type)
        {
            if (episode_type->type != json_value::string_kind)
                add_message(fresh_result, ecg_json_type, "$.ecg.episode_type", "field has the wrong JSON type");
            else
            {
                ecg_episode_type value = ecg_episode_none;
                if (episode_type->string == "psvt")
                    value = ecg_episode_psvt;
                else if (episode_type->string == "svarr")
                    value = ecg_episode_svarr;
                else if (episode_type->string != "none")
                    add_message(fresh_result, ecg_json_range, "$.ecg.episode_type", "unknown episode type");
                document.ecg.set_episode_type(value);
            }
        }
        if (episode_start)
        {
            if (episode_start->type != json_value::number_kind)
                add_message(fresh_result, ecg_json_type, "$.ecg.episode_start_seconds", "field has the wrong JSON type");
            else if (!document.ecg.set_episode_start_seconds(episode_start->number))
                add_message(fresh_result, ecg_json_range, "$.ecg.episode_start_seconds", "invalid episode start");
        }
        if (episode_duration)
        {
            if (episode_duration->type != json_value::number_kind)
                add_message(fresh_result, ecg_json_type, "$.ecg.episode_duration_seconds", "field has the wrong JSON type");
            else if (!document.ecg.set_episode_duration_seconds(episode_duration->number))
                add_message(fresh_result, ecg_json_range, "$.ecg.episode_duration_seconds", "invalid episode duration");
        }
        if (episode_rate)
        {
            if (episode_rate->type != json_value::number_kind)
                add_message(fresh_result, ecg_json_type, "$.ecg.episode_rate_bpm", "field has the wrong JSON type");
            else if (!document.ecg.set_episode_rate_bpm(episode_rate->number))
                add_message(fresh_result, ecg_json_range, "$.ecg.episode_rate_bpm", "invalid episode rate");
        }
        if (flutter_pattern)
        {
            if (flutter_pattern->type != json_value::string_kind)
                add_message(fresh_result, ecg_json_type, "$.ecg.flutter_conduction_pattern", "field has the wrong JSON type");
            else
            {
                ecg_flutter_conduction_pattern value = ecg_flutter_fixed;
                if (flutter_pattern->string == "alternate_2_3")
                    value = ecg_flutter_alternate_2_3;
                else if (flutter_pattern->string == "cycle_2_3_4")
                    value = ecg_flutter_cycle_2_3_4;
                else if (flutter_pattern->string != "fixed")
                    add_message(fresh_result, ecg_json_range, "$.ecg.flutter_conduction_pattern", "unknown flutter conduction pattern");
                document.ecg.set_flutter_conduction_pattern(value);
            }
        }
        if (pacing_mode)
        {
            if (pacing_mode->type != json_value::string_kind)
                add_message(fresh_result, ecg_json_type, "$.ecg.pacing_mode", "field has the wrong JSON type");
            else
            {
                ecg_pacing_mode value = ecg_pacing_ventricular;
                if (pacing_mode->string == "atrial")
                    value = ecg_pacing_atrial;
                else if (pacing_mode->string == "dual_chamber")
                    value = ecg_pacing_dual_chamber;
                else if (pacing_mode->string != "ventricular")
                    add_message(fresh_result, ecg_json_range, "$.ecg.pacing_mode", "unknown pacing mode");
                document.ecg.set_pacing_mode(value);
            }
        }
        if (pacing_non_capture)
        {
            if (pacing_non_capture->type != json_value::number_kind)
                add_message(fresh_result, ecg_json_type, "$.ecg.pacing_non_capture_every_n_beats", "field has the wrong JSON type");
            else if (!integral_number(*pacing_non_capture, std::numeric_limits<unsigned int>::max(), integer) || !document.ecg.set_pacing_non_capture_every_n_beats(static_cast<unsigned int>(integer)))
                add_message(fresh_result, ecg_json_range, "$.ecg.pacing_non_capture_every_n_beats", "invalid pacing non-capture cadence");
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
        if (morphology)
        {
            if (document.schema_version < 3)
                add_message(fresh_result, ecg_json_unknown_field, "$.ecg.morphology", "morphology controls require schema version 3");
            else if (morphology->type != json_value::object_kind)
                add_message(fresh_result, ecg_json_type, "$.ecg.morphology", "field has the wrong JSON type");
            else
            {
                const char* morphology_fields[ecg_morphology_control_count];
                for (unsigned int control_index = 0; control_index < ecg_morphology_control_count; ++control_index)
                    morphology_fields[control_index] = ecg_morphology_control_name(static_cast<ecg_morphology_control>(control_index));
                allowed_fields(*morphology, morphology_fields, ecg_morphology_control_count, "$.ecg.morphology", fresh_result);
                for (unsigned int control_index = 0; control_index < ecg_morphology_control_count; ++control_index)
                {
                    const ecg_morphology_control control = static_cast<ecg_morphology_control>(control_index);
                    const char* name = ecg_morphology_control_name(control);
                    const json_value* value = member(*morphology, name);
                    if (!value)
                        continue;
                    const std::string path = std::string("$.ecg.morphology.") + name;
                    if (value->type != json_value::number_kind)
                        add_message(fresh_result, ecg_json_type, path, "field has the wrong JSON type");
                    else if (!document.ecg.set_morphology_control(control, value->number))
                        add_message(fresh_result, ecg_json_range, path, "invalid morphology control value");
                }
            }
        }

        const json_value* hrv = member(root, "hrv");
        if (document.schema_version == 1 && hrv)
            add_message(fresh_result, ecg_json_unknown_field, "$.hrv", "HRV requires schema version 2");
        else if (hrv)
        {
            if (hrv->type != json_value::object_kind)
                add_message(fresh_result, ecg_json_type, "$.hrv", "field has the wrong JSON type");
            else
            {
                const char* hrv_fields[] = {"enabled","target_mean_hr_bpm","target_sdnn_seconds","lf_hf_ratio","lf_center_hz","lf_bandwidth_hz","hf_center_hz","hf_bandwidth_hz","respiratory_frequency_hz","respiratory_amplitude_seconds","minimum_rr_seconds","maximum_rr_seconds","seed"};
                allowed_fields(*hrv, hrv_fields, sizeof(hrv_fields) / sizeof(hrv_fields[0]), "$.hrv", fresh_result);
                const json_value* enabled = required(*hrv, "enabled", json_value::bool_kind, "$.hrv", fresh_result);
                const json_value* target_mean_hr = required(*hrv, "target_mean_hr_bpm", json_value::number_kind, "$.hrv", fresh_result);
                const json_value* target_sdnn = required(*hrv, "target_sdnn_seconds", json_value::number_kind, "$.hrv", fresh_result);
                const json_value* lf_hf = required(*hrv, "lf_hf_ratio", json_value::number_kind, "$.hrv", fresh_result);
                const json_value* lf_center = required(*hrv, "lf_center_hz", json_value::number_kind, "$.hrv", fresh_result);
                const json_value* lf_bandwidth = required(*hrv, "lf_bandwidth_hz", json_value::number_kind, "$.hrv", fresh_result);
                const json_value* hf_center = required(*hrv, "hf_center_hz", json_value::number_kind, "$.hrv", fresh_result);
                const json_value* hf_bandwidth = required(*hrv, "hf_bandwidth_hz", json_value::number_kind, "$.hrv", fresh_result);
                const json_value* respiratory_frequency = required(*hrv, "respiratory_frequency_hz", json_value::number_kind, "$.hrv", fresh_result);
                const json_value* respiratory_amplitude = required(*hrv, "respiratory_amplitude_seconds", json_value::number_kind, "$.hrv", fresh_result);
                const json_value* minimum_rr = required(*hrv, "minimum_rr_seconds", json_value::number_kind, "$.hrv", fresh_result);
                const json_value* maximum_rr = required(*hrv, "maximum_rr_seconds", json_value::number_kind, "$.hrv", fresh_result);
                const json_value* hrv_seed = required(*hrv, "seed", json_value::number_kind, "$.hrv", fresh_result);
                if (enabled) document.hrv.enabled = enabled->boolean;
                if (target_mean_hr) document.hrv.target_mean_hr_bpm = target_mean_hr->number;
                if (target_sdnn) document.hrv.target_sdnn_seconds = target_sdnn->number;
                if (lf_hf) document.hrv.lf_hf_ratio = lf_hf->number;
                if (lf_center) document.hrv.lf_center_hz = lf_center->number;
                if (lf_bandwidth) document.hrv.lf_bandwidth_hz = lf_bandwidth->number;
                if (hf_center) document.hrv.hf_center_hz = hf_center->number;
                if (hf_bandwidth) document.hrv.hf_bandwidth_hz = hf_bandwidth->number;
                if (respiratory_frequency) document.hrv.respiratory_frequency_hz = respiratory_frequency->number;
                if (respiratory_amplitude) document.hrv.respiratory_amplitude_seconds = respiratory_amplitude->number;
                if (minimum_rr) document.hrv.minimum_rr_seconds = minimum_rr->number;
                if (maximum_rr) document.hrv.maximum_rr_seconds = maximum_rr->number;
                if (hrv_seed)
                {
                    if (!integral_number(*hrv_seed, std::numeric_limits<unsigned long long>::max(), integer))
                        add_message(fresh_result, ecg_json_range, "$.hrv.seed", "seed must be an unsigned 64-bit decimal integer");
                    else
                        document.hrv.seed = integer;
                }
                if (document.hrv.enabled)
                {
                    if (!document.ecg.set_heart_rate_bpm(document.hrv.target_mean_hr_bpm))
                        add_message(fresh_result, ecg_json_range, "$.hrv.target_mean_hr_bpm", "invalid target mean HR");
                    if (!document.ecg.set_rr_variability_seconds(document.hrv.target_sdnn_seconds))
                        add_message(fresh_result, ecg_json_range, "$.hrv.target_sdnn_seconds", "invalid target SDNN");
                    if (!document.ecg.set_minimum_rr_seconds(document.hrv.minimum_rr_seconds))
                        add_message(fresh_result, ecg_json_range, "$.hrv.minimum_rr_seconds", "invalid minimum RR");
                    if (!document.ecg.set_maximum_rr_seconds(document.hrv.maximum_rr_seconds))
                        add_message(fresh_result, ecg_json_range, "$.hrv.maximum_rr_seconds", "invalid maximum RR");
                    if (!document.ecg.set_hrv_modulation(document.hrv.lf_hf_ratio, document.hrv.lf_center_hz, document.hrv.lf_bandwidth_hz, document.hrv.hf_center_hz, document.hrv.hf_bandwidth_hz, document.hrv.respiratory_frequency_hz, document.hrv.respiratory_amplitude_seconds))
                        add_message(fresh_result, ecg_json_range, "$.hrv", "invalid HRV modulation parameters");
                    document.ecg.set_seed(document.hrv.seed);
                }
            }
        }

        std::set<int> seen_conditions;
        if (conditions)
        {
            for (std::size_t i = 0; i < conditions->array.size(); ++i)
            {
                const std::string path = "$.ecg.conditions[" + json_index(i) + "]";
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

        const json_value* ppg = member(root, "ppg");
        if (document.schema_version == 1 && ppg)
            add_message(fresh_result, ecg_json_unknown_field, "$.ppg", "PPG requires schema version 2");
        if (document.schema_version >= 2)
        {
            if (!ppg)
                add_message(fresh_result, ecg_json_missing_field, "$.ppg", "required field is missing");
            else if (ppg->type != json_value::object_kind)
                add_message(fresh_result, ecg_json_type, "$.ppg", "field has the wrong JSON type");
            else
            {
                const char* ppg_fields[] = {"enabled","pulse_delay_ms","rise_time_ms","decay_time_ms","amplitude_au","baseline_au","dicrotic_delay_ms","dicrotic_width_ms","dicrotic_amplitude_ratio","pulse_delay_variation_ms","pulse_delay_variation_hz","missing_pulse_every_n_beats","clock_drift_ppm","seed","pulse_delay_jitter_ms","low_frequency_amplitude_modulation_ratio","low_frequency_amplitude_modulation_hz","rise_time_variation_ratio","decay_time_variation_ratio","pac_pulse_amplitude_scale","pvc_pulse_amplitude_scale","paced_pulse_amplitude_scale","perfusion_episodes"};
                const std::size_t ppg_field_count = document.schema_version >= 4 ? sizeof(ppg_fields) / sizeof(ppg_fields[0]) : document.schema_version >= 3 ? 14u : 9u;
                allowed_fields(*ppg, ppg_fields, ppg_field_count, "$.ppg", fresh_result);
                const json_value* enabled = required(*ppg, "enabled", json_value::bool_kind, "$.ppg", fresh_result);
                const json_value* pulse_delay = required(*ppg, "pulse_delay_ms", json_value::number_kind, "$.ppg", fresh_result);
                const json_value* rise = required(*ppg, "rise_time_ms", json_value::number_kind, "$.ppg", fresh_result);
                const json_value* decay = required(*ppg, "decay_time_ms", json_value::number_kind, "$.ppg", fresh_result);
                const json_value* amplitude = required(*ppg, "amplitude_au", json_value::number_kind, "$.ppg", fresh_result);
                const json_value* baseline = required(*ppg, "baseline_au", json_value::number_kind, "$.ppg", fresh_result);
                const json_value* dicrotic_delay = required(*ppg, "dicrotic_delay_ms", json_value::number_kind, "$.ppg", fresh_result);
                const json_value* dicrotic_width = required(*ppg, "dicrotic_width_ms", json_value::number_kind, "$.ppg", fresh_result);
                const json_value* dicrotic_amplitude = required(*ppg, "dicrotic_amplitude_ratio", json_value::number_kind, "$.ppg", fresh_result);
                const json_value* delay_variation = member(*ppg, "pulse_delay_variation_ms");
                const json_value* delay_frequency = member(*ppg, "pulse_delay_variation_hz");
                const json_value* missing_pulse = member(*ppg, "missing_pulse_every_n_beats");
                const json_value* clock_drift = member(*ppg, "clock_drift_ppm");
                const json_value* ppg_seed = member(*ppg, "seed");
                const json_value* delay_jitter = member(*ppg, "pulse_delay_jitter_ms");
                const json_value* lf_amplitude = member(*ppg, "low_frequency_amplitude_modulation_ratio");
                const json_value* lf_frequency = member(*ppg, "low_frequency_amplitude_modulation_hz");
                const json_value* rise_variation = member(*ppg, "rise_time_variation_ratio");
                const json_value* decay_variation = member(*ppg, "decay_time_variation_ratio");
                const json_value* pac_scale = member(*ppg, "pac_pulse_amplitude_scale");
                const json_value* pvc_scale = member(*ppg, "pvc_pulse_amplitude_scale");
                const json_value* paced_scale = member(*ppg, "paced_pulse_amplitude_scale");
                const json_value* perfusion_episodes = member(*ppg, "perfusion_episodes");
                if (document.schema_version >= 3)
                {
                    delay_variation = required(*ppg, "pulse_delay_variation_ms", json_value::number_kind, "$.ppg", fresh_result);
                    delay_frequency = required(*ppg, "pulse_delay_variation_hz", json_value::number_kind, "$.ppg", fresh_result);
                    missing_pulse = required(*ppg, "missing_pulse_every_n_beats", json_value::number_kind, "$.ppg", fresh_result);
                    clock_drift = required(*ppg, "clock_drift_ppm", json_value::number_kind, "$.ppg", fresh_result);
                    ppg_seed = required(*ppg, "seed", json_value::number_kind, "$.ppg", fresh_result);
                }
                if (document.schema_version >= 4)
                {
                    delay_jitter = required(*ppg, "pulse_delay_jitter_ms", json_value::number_kind, "$.ppg", fresh_result);
                    lf_amplitude = required(*ppg, "low_frequency_amplitude_modulation_ratio", json_value::number_kind, "$.ppg", fresh_result);
                    lf_frequency = required(*ppg, "low_frequency_amplitude_modulation_hz", json_value::number_kind, "$.ppg", fresh_result);
                    rise_variation = required(*ppg, "rise_time_variation_ratio", json_value::number_kind, "$.ppg", fresh_result);
                    decay_variation = required(*ppg, "decay_time_variation_ratio", json_value::number_kind, "$.ppg", fresh_result);
                    perfusion_episodes = required(*ppg, "perfusion_episodes", json_value::array_kind, "$.ppg", fresh_result);
                }
                if (enabled) document.ppg.enabled = enabled->boolean;
                if (pulse_delay) document.ppg.pulse_delay_ms = pulse_delay->number;
                if (rise) document.ppg.rise_time_ms = rise->number;
                if (decay) document.ppg.decay_time_ms = decay->number;
                if (amplitude) document.ppg.amplitude_au = amplitude->number;
                if (baseline) document.ppg.baseline_au = baseline->number;
                if (dicrotic_delay) document.ppg.dicrotic_delay_ms = dicrotic_delay->number;
                if (dicrotic_width) document.ppg.dicrotic_width_ms = dicrotic_width->number;
                if (dicrotic_amplitude) document.ppg.dicrotic_amplitude_ratio = dicrotic_amplitude->number;
                if (delay_variation)
                {
                    if (delay_variation->type != json_value::number_kind)
                        add_message(fresh_result, ecg_json_type, "$.ppg.pulse_delay_variation_ms", "field has the wrong JSON type");
                    else
                        document.ppg.pulse_delay_variation_ms = delay_variation->number;
                }
                if (delay_frequency)
                {
                    if (delay_frequency->type != json_value::number_kind)
                        add_message(fresh_result, ecg_json_type, "$.ppg.pulse_delay_variation_hz", "field has the wrong JSON type");
                    else
                        document.ppg.pulse_delay_variation_hz = delay_frequency->number;
                }
                if (missing_pulse)
                {
                    if (missing_pulse->type != json_value::number_kind || !integral_number(*missing_pulse, std::numeric_limits<unsigned int>::max(), integer))
                        add_message(fresh_result, ecg_json_range, "$.ppg.missing_pulse_every_n_beats", "field must be an unsigned integer");
                    else
                        document.ppg.missing_pulse_every_n_beats = static_cast<unsigned int>(integer);
                }
                if (clock_drift)
                {
                    if (clock_drift->type != json_value::number_kind)
                        add_message(fresh_result, ecg_json_type, "$.ppg.clock_drift_ppm", "field has the wrong JSON type");
                    else
                        document.ppg.clock_drift_ppm = clock_drift->number;
                }
                if (ppg_seed)
                {
                    if (ppg_seed->type != json_value::number_kind || !integral_number(*ppg_seed, std::numeric_limits<unsigned long long>::max(), integer))
                        add_message(fresh_result, ecg_json_range, "$.ppg.seed", "seed must be an unsigned 64-bit decimal integer");
                    else
                        document.ppg.seed = integer;
                }
                if (delay_jitter && delay_jitter->type == json_value::number_kind) document.ppg.pulse_delay_jitter_ms = delay_jitter->number;
                if (lf_amplitude && lf_amplitude->type == json_value::number_kind) document.ppg.low_frequency_amplitude_modulation_ratio = lf_amplitude->number;
                if (lf_frequency && lf_frequency->type == json_value::number_kind) document.ppg.low_frequency_amplitude_modulation_hz = lf_frequency->number;
                if (rise_variation && rise_variation->type == json_value::number_kind) document.ppg.rise_time_variation_ratio = rise_variation->number;
                if (decay_variation && decay_variation->type == json_value::number_kind) document.ppg.decay_time_variation_ratio = decay_variation->number;
                if (pac_scale)
                {
                    if (pac_scale->type != json_value::number_kind)
                        add_message(fresh_result, ecg_json_type, "$.ppg.pac_pulse_amplitude_scale", "field has the wrong JSON type");
                    else
                        document.ppg.pac_pulse_amplitude_scale = pac_scale->number;
                }
                if (pvc_scale)
                {
                    if (pvc_scale->type != json_value::number_kind)
                        add_message(fresh_result, ecg_json_type, "$.ppg.pvc_pulse_amplitude_scale", "field has the wrong JSON type");
                    else
                        document.ppg.pvc_pulse_amplitude_scale = pvc_scale->number;
                }
                if (paced_scale)
                {
                    if (paced_scale->type != json_value::number_kind)
                        add_message(fresh_result, ecg_json_type, "$.ppg.paced_pulse_amplitude_scale", "field has the wrong JSON type");
                    else
                        document.ppg.paced_pulse_amplitude_scale = paced_scale->number;
                }
                if (perfusion_episodes && perfusion_episodes->type == json_value::array_kind)
                {
                    for (std::size_t i = 0; i < perfusion_episodes->array.size(); ++i)
                    {
                        const std::string path = "$.ppg.perfusion_episodes[" + json_index(i) + "]";
                        const json_value& item = perfusion_episodes->array[i];
                        if (item.type != json_value::object_kind)
                        {
                            add_message(fresh_result, ecg_json_type, path, "perfusion episode must be an object");
                            continue;
                        }
                        const char* fields[] = {"start_seconds","duration_seconds","amplitude_scale","rise_time_scale","decay_time_scale","weak_pulse_every_n_beats","weak_pulse_amplitude_scale","missing_pulse_every_n_beats"};
                        allowed_fields(item, fields, 8u, path, fresh_result);
                        const json_value* start = required(item, fields[0], json_value::number_kind, path, fresh_result);
                        const json_value* episode_duration = required(item, fields[1], json_value::number_kind, path, fresh_result);
                        const json_value* amplitude_scale = required(item, fields[2], json_value::number_kind, path, fresh_result);
                        const json_value* rise_scale = required(item, fields[3], json_value::number_kind, path, fresh_result);
                        const json_value* decay_scale = required(item, fields[4], json_value::number_kind, path, fresh_result);
                        const json_value* weak_every = required(item, fields[5], json_value::number_kind, path, fresh_result);
                        const json_value* weak_scale = required(item, fields[6], json_value::number_kind, path, fresh_result);
                        const json_value* missing_every = required(item, fields[7], json_value::number_kind, path, fresh_result);
                        unsigned long long weak_integer = 0, missing_integer = 0;
                        if (weak_every && !integral_number(*weak_every, std::numeric_limits<unsigned int>::max(), weak_integer))
                            add_message(fresh_result, ecg_json_range, path + ".weak_pulse_every_n_beats", "field must be an unsigned integer");
                        if (missing_every && !integral_number(*missing_every, std::numeric_limits<unsigned int>::max(), missing_integer))
                            add_message(fresh_result, ecg_json_range, path + ".missing_pulse_every_n_beats", "field must be an unsigned integer");
                        if (start && episode_duration && amplitude_scale && rise_scale && decay_scale && weak_every && weak_scale && missing_every)
                        {
                            ppg_perfusion_episode_config episode;
                            episode.start_seconds = start->number;
                            episode.duration_seconds = episode_duration->number;
                            episode.amplitude_scale = amplitude_scale->number;
                            episode.rise_time_scale = rise_scale->number;
                            episode.decay_time_scale = decay_scale->number;
                            episode.weak_pulse_every_n_beats = static_cast<unsigned int>(weak_integer);
                            episode.weak_pulse_amplitude_scale = weak_scale->number;
                            episode.missing_pulse_every_n_beats = static_cast<unsigned int>(missing_integer);
                            document.ppg.perfusion_episodes.push_back(episode);
                        }
                    }
                }
                if (!ppg_generator(document.ppg).valid())
                    add_message(fresh_result, ecg_json_range, "$.ppg", "invalid PPG configuration");
            }
        }

        const json_value* randomization = member(root, "randomization");
        const json_value* physiology = member(root, "physiology");
        const json_value* output_config = member(root, "output");
        if (document.schema_version < 3 && (randomization || physiology || output_config))
            add_message(fresh_result, ecg_json_unknown_field, "$", "randomization, physiology, and output require schema version 3");
        if (document.schema_version >= 3)
        {
            if (!randomization || !physiology || !output_config)
                add_message(fresh_result, ecg_json_missing_field, "$", "schema version 3 requires randomization, physiology, and output");
            if (randomization)
            {
                if (randomization->type != json_value::object_kind)
                    add_message(fresh_result, ecg_json_type, "$.randomization", "field has the wrong JSON type");
                else
                {
                    const char* fields[] = {"enabled","seed","envelopes"};
                    allowed_fields(*randomization, fields, 3, "$.randomization", fresh_result);
                    const json_value* enabled = required(*randomization, "enabled", json_value::bool_kind, "$.randomization", fresh_result);
                    const json_value* random_seed = required(*randomization, "seed", json_value::number_kind, "$.randomization", fresh_result);
                    const json_value* envelopes = required(*randomization, "envelopes", json_value::array_kind, "$.randomization", fresh_result);
                    if (enabled) document.randomization.enabled = enabled->boolean;
                    if (random_seed)
                    {
                        if (!integral_number(*random_seed, std::numeric_limits<unsigned long long>::max(), integer))
                            add_message(fresh_result, ecg_json_range, "$.randomization.seed", "seed must be an unsigned 64-bit decimal integer");
                        else
                            document.randomization.seed = integer;
                    }
                    if (envelopes)
                    {
                        for (std::size_t i = 0; i < envelopes->array.size(); ++i)
                        {
                            const std::string path = "$.randomization.envelopes[" + json_index(i) + "]";
                            const json_value& item = envelopes->array[i];
                            if (item.type != json_value::object_kind)
                            {
                                add_message(fresh_result, ecg_json_type, path, "envelope must be an object");
                                continue;
                            }
                            const char* envelope_fields[] = {"parameter","minimum","maximum"};
                            allowed_fields(item, envelope_fields, 3, path, fresh_result);
                            const json_value* parameter = required(item, "parameter", json_value::string_kind, path, fresh_result);
                            const json_value* minimum = required(item, "minimum", json_value::number_kind, path, fresh_result);
                            const json_value* maximum = required(item, "maximum", json_value::number_kind, path, fresh_result);
                            if (parameter && minimum && maximum)
                            {
                                scenario_randomization_envelope envelope;
                                envelope.parameter = parameter->string;
                                envelope.minimum = minimum->number;
                                envelope.maximum = maximum->number;
                                document.randomization.envelopes.push_back(envelope);
                            }
                        }
                    }
                }
            }
            if (physiology)
            {
                if (physiology->type != json_value::object_kind)
                    add_message(fresh_result, ecg_json_type, "$.physiology", "field has the wrong JSON type");
                else
                {
                    const char* fields[] = {"respiration_frequency_hz","respiratory_rr_amplitude_seconds","ecg_baseline_amplitude_mv","ppg_amplitude_modulation_ratio","activity_start_seconds","activity_duration_seconds","activity_intensity","seed"};
                    allowed_fields(*physiology, fields, 8, "$.physiology", fresh_result);
                    const json_value* values[7] = {
                        required(*physiology, fields[0], json_value::number_kind, "$.physiology", fresh_result),
                        required(*physiology, fields[1], json_value::number_kind, "$.physiology", fresh_result),
                        required(*physiology, fields[2], json_value::number_kind, "$.physiology", fresh_result),
                        required(*physiology, fields[3], json_value::number_kind, "$.physiology", fresh_result),
                        required(*physiology, fields[4], json_value::number_kind, "$.physiology", fresh_result),
                        required(*physiology, fields[5], json_value::number_kind, "$.physiology", fresh_result),
                        required(*physiology, fields[6], json_value::number_kind, "$.physiology", fresh_result)};
                    const json_value* physiology_seed = required(*physiology, fields[7], json_value::number_kind, "$.physiology", fresh_result);
                    if (values[0]) document.physiology.respiration_frequency_hz = values[0]->number;
                    if (values[1]) document.physiology.respiratory_rr_amplitude_seconds = values[1]->number;
                    if (values[2]) document.physiology.ecg_baseline_amplitude_mv = values[2]->number;
                    if (values[3]) document.physiology.ppg_amplitude_modulation_ratio = values[3]->number;
                    if (values[4]) document.physiology.activity_start_seconds = values[4]->number;
                    if (values[5]) document.physiology.activity_duration_seconds = values[5]->number;
                    if (values[6]) document.physiology.activity_intensity = values[6]->number;
                    if (physiology_seed)
                    {
                        if (!integral_number(*physiology_seed, std::numeric_limits<unsigned long long>::max(), integer))
                            add_message(fresh_result, ecg_json_range, "$.physiology.seed", "seed must be an unsigned 64-bit decimal integer");
                        else
                            document.physiology.seed = integer;
                    }
                }
            }
            if (output_config)
            {
                if (output_config->type != json_value::object_kind)
                    add_message(fresh_result, ecg_json_type, "$.output", "field has the wrong JSON type");
                else
                {
                    const char* fields[] = {"compact","retain_source_channels","include_waveform_csv","include_edf_bdf"};
                    allowed_fields(*output_config, fields, 4, "$.output", fresh_result);
                    const json_value* compact = required(*output_config, fields[0], json_value::bool_kind, "$.output", fresh_result);
                    const json_value* retain = required(*output_config, fields[1], json_value::bool_kind, "$.output", fresh_result);
                    const json_value* csv = required(*output_config, fields[2], json_value::bool_kind, "$.output", fresh_result);
                    const json_value* edf = required(*output_config, fields[3], json_value::bool_kind, "$.output", fresh_result);
                    if (compact) document.output.compact = compact->boolean;
                    if (retain) document.output.retain_source_channels = retain->boolean;
                    if (csv) document.output.include_waveform_csv = csv->boolean;
                    if (edf) document.output.include_edf_bdf = edf->boolean;
                }
            }
        }

        const json_value* artifacts = member(root, "artifacts");
        if (document.schema_version == 1 && artifacts)
            add_message(fresh_result, ecg_json_unknown_field, "$.artifacts", "artifacts require schema version 2");
        if (document.schema_version >= 2 && artifacts)
        {
            if (artifacts->type != json_value::array_kind)
                add_message(fresh_result, ecg_json_type, "$.artifacts", "field has the wrong JSON type");
            else
            {
                for (std::size_t i = 0; i < artifacts->array.size(); ++i)
                {
                    const std::string path = "$.artifacts[" + json_index(i) + "]";
                    const json_value& item = artifacts->array[i];
                    if (item.type != json_value::object_kind)
                    {
                        add_message(fresh_result, ecg_json_type, path, "artifact must be an object");
                        continue;
                    }
                    const char* artifact_fields[] = {"type","start_seconds","duration_seconds","severity","seed","channels"};
                    if (!allowed_fields(item, artifact_fields, sizeof(artifact_fields) / sizeof(artifact_fields[0]), path, fresh_result))
                        continue;
                    const json_value* type = required(item, "type", json_value::string_kind, path, fresh_result);
                    const json_value* start = required(item, "start_seconds", json_value::number_kind, path, fresh_result);
                    const json_value* duration_value = required(item, "duration_seconds", json_value::number_kind, path, fresh_result);
                    const json_value* severity = required(item, "severity", json_value::number_kind, path, fresh_result);
                    const json_value* artifact_seed = required(item, "seed", json_value::number_kind, path, fresh_result);
                    const json_value* channels = required(item, "channels", json_value::array_kind, path, fresh_result);
                    if (!type || !start || !duration_value || !severity || !artifact_seed || !channels)
                        continue;

                    signal_quality_artifact_config artifact;
                    if (!artifact_type_from_name(type->string, artifact.type))
                    {
                        add_message(fresh_result, ecg_json_range, path + ".type", "unknown artifact type");
                        continue;
                    }
                    artifact.start_seconds = start->number;
                    artifact.duration_seconds = duration_value->number;
                    artifact.severity = severity->number;
                    if (!integral_number(*artifact_seed, std::numeric_limits<unsigned long long>::max(), integer))
                    {
                        add_message(fresh_result, ecg_json_range, path + ".seed", "seed must be an unsigned 64-bit decimal integer");
                        continue;
                    }
                    artifact.seed = integer;

                    std::set<std::string> seen_channels;
                    for (std::size_t channel_index = 0; channel_index < channels->array.size(); ++channel_index)
                    {
                        const std::string channel_path = path + ".channels[" + json_index(channel_index) + "]";
                        const json_value& channel = channels->array[channel_index];
                        if (channel.type != json_value::string_kind)
                        {
                            add_message(fresh_result, ecg_json_type, channel_path, "channel must be a string");
                            continue;
                        }
                        if (!seen_channels.insert(channel.string).second)
                        {
                            add_message(fresh_result, ecg_json_duplicate_tag, channel_path, "duplicate artifact channel");
                            continue;
                        }
                        if (artifact_is_ecg(artifact.type))
                        {
                            if (channel.string == "all" || channel.string == "all_ecg")
                            {
                                if (artifact.affects_ecg())
                                {
                                    add_message(fresh_result, ecg_json_duplicate_tag, channel_path, "duplicate artifact channel");
                                    continue;
                                }
                                for (unsigned int lead = 0; lead < clinical_lead_count; ++lead)
                                    artifact.ecg_leads[lead] = true;
                            }
                            else
                            {
                                const int lead = clinical_lead_from_json_name(channel.string);
                                if (lead < 0)
                                    add_message(fresh_result, ecg_json_range, channel_path, "unknown ECG artifact channel");
                                else if (artifact.ecg_leads[lead])
                                    add_message(fresh_result, ecg_json_duplicate_tag, channel_path, "duplicate artifact channel");
                                else
                                    artifact.ecg_leads[lead] = true;
                            }
                        }
                        else
                        {
                            if (channel.string == "all" || channel.string == "ppg_green")
                            {
                                if (artifact.ppg)
                                {
                                    add_message(fresh_result, ecg_json_duplicate_tag, channel_path, "duplicate artifact channel");
                                    continue;
                                }
                                artifact.ppg = true;
                            }
                            else
                                add_message(fresh_result, ecg_json_range, channel_path, "unknown PPG artifact channel");
                        }
                    }
                    document.signal_quality.artifacts.push_back(artifact);
                }
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
