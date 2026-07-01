#include "ecg_scenario_json.h"
#include "ecg_export.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <locale>
#include <new>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace
{
    const std::size_t maximum_input_size = 16u * 1024u * 1024u;

    bool read_stream(std::istream& input, std::string& output)
    {
        output.clear();
        char buffer[16384];
        while (input)
        {
            input.read(buffer, sizeof(buffer));
            const std::streamsize count = input.gcount();
            if (count > 0)
            {
                if (output.size() > maximum_input_size - static_cast<std::size_t>(count))
                    return false;
                output.append(buffer, static_cast<std::size_t>(count));
            }
        }
        return input.eof();
    }

    bool read_input(const std::string& path, std::string& output)
    {
        if (path == "-")
            return read_stream(std::cin, output);
        std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
        return file && read_stream(file, output);
    }

    void print_usage()
    {
        std::cerr << "usage: signal-synth <validate|fingerprint> <scenario.json|->\n"
                  << "       signal-synth render <scenario.json|-> --out <new-directory>\n";
    }

    void print_errors(const signal_synth::ecg_scenario_json_result& result)
    {
        for (std::size_t i = 0; i < result.messages.size(); ++i)
        {
            const signal_synth::ecg_scenario_json_message& message = result.messages[i];
            std::cerr << "error=" << signal_synth::ecg_scenario_json_message_code_name(message.code)
                      << " path=" << message.path << " message=" << message.message << '\n';
        }
    }

    bool create_directory(const std::string& path)
    {
#ifdef _WIN32
        return _mkdir(path.c_str()) == 0;
#else
        return mkdir(path.c_str(), 0777) == 0;
#endif
    }

    bool remove_directory(const std::string& path)
    {
#ifdef _WIN32
        return _rmdir(path.c_str()) == 0;
#else
        return rmdir(path.c_str()) == 0;
#endif
    }

    bool write_export_directory(const std::string& path, const signal_synth::ecg_export_bundle& bundle)
    {
        if (!create_directory(path))
            return false;
        std::vector<std::string> temporary;
        std::vector<std::string> completed;
        bool success = true;
        for (std::size_t i = 0; i < bundle.artifacts.size() && success; ++i)
        {
            const std::string final_name = path + "/" + bundle.artifacts[i].name;
            const std::string temporary_name = final_name + ".tmp";
            temporary.push_back(temporary_name);
            std::ofstream file(temporary_name.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
            file.write(bundle.artifacts[i].content.data(), static_cast<std::streamsize>(bundle.artifacts[i].content.size()));
            success = file.good();
            file.close();
            if (success)
            {
                success = std::rename(temporary_name.c_str(), final_name.c_str()) == 0;
                if (success)
                    completed.push_back(final_name);
            }
        }
        if (!success)
        {
            for (std::size_t i = 0; i < temporary.size(); ++i)
                std::remove(temporary[i].c_str());
            for (std::size_t i = 0; i < completed.size(); ++i)
                std::remove(completed[i].c_str());
            remove_directory(path);
        }
        return success;
    }
}

int main(int argc, char** argv)
{
    std::cout.imbue(std::locale::classic());
    std::cerr.imbue(std::locale::classic());
    if (argc < 2)
    {
        print_usage();
        return 2;
    }
    const std::string command(argv[1]);
    const bool render_command = command == "render";
    if ((render_command && (argc != 5 || std::string(argv[3]) != "--out")) || (!render_command && (argc != 3 || (command != "validate" && command != "fingerprint"))))
    {
        print_usage();
        return 2;
    }

    try
    {
        std::string json;
        if (!read_input(argv[2], json))
        {
            std::cerr << "error=INPUT_READ_FAILED path=" << argv[2] << " message=unable to read input or input exceeds 16 MiB\n";
            return 3;
        }
        signal_synth::ecg_scenario_document document;
        signal_synth::ecg_scenario_json_result result;
        if (!signal_synth::parse_ecg_scenario_json(json, document, result))
        {
            print_errors(result);
            return 4;
        }
        if (render_command)
        {
            signal_synth::ecg_render_bundle render;
            signal_synth::ecg_export_bundle export_bundle;
            signal_synth::ecg_export_result export_result;
            if (!signal_synth::render_ecg_document(document, render, export_result) || !signal_synth::build_ecg_export_bundle(render, export_bundle, export_result))
            {
                std::cerr << "error=RENDER_FAILED path=$ message=" << (export_result.messages.empty() ? "render or export failed" : export_result.messages[0]) << '\n';
                return 4;
            }
            if (!write_export_directory(argv[4], export_bundle))
            {
                std::cerr << "error=OUTPUT_WRITE_FAILED path=" << argv[4] << " message=output directory must be new and writable\n";
                return 3;
            }
            std::cout << "status=rendered\n"
                      << "output_directory=" << argv[4] << '\n'
                      << "artifact_count=" << export_bundle.artifacts.size() << '\n'
                      << "document_fingerprint=" << result.document_fingerprint << '\n'
                      << "generation_fingerprint=" << result.generation_fingerprint << '\n'
                      << "render_identity=" << render.render_identity << '\n'
                      << "ecg_run_fingerprint=" << render.scenario_report.run_fingerprint() << '\n';
            return 0;
        }
        if (command == "validate")
        {
            std::cout << "status=valid\n"
                      << "schema_version=" << document.schema_version << '\n'
                      << "scenario_id=" << document.scenario_id << '\n'
                      << "sample_count=" << document.sample_count() << '\n';
        }
        std::cout << "document_fingerprint=" << result.document_fingerprint << '\n'
                  << "generation_fingerprint=" << result.generation_fingerprint << '\n';
        return 0;
    }
    catch (const std::bad_alloc&)
    {
        std::cerr << "error=INTERNAL_ERROR path=$ message=memory allocation failed\n";
    }
    catch (...)
    {
        std::cerr << "error=INTERNAL_ERROR path=$ message=unexpected failure\n";
    }
    return 5;
}
