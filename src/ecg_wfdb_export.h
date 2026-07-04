#pragma once

#include "ecg_export.h"

#include <string>
#include <vector>

namespace signal_synth
{
    struct wfdb_export_artifact
    {
        std::string name;
        std::string media_type;
        std::string content;
    };

    struct wfdb_export_bundle
    {
        std::vector<wfdb_export_artifact> artifacts;
    };

    bool build_wfdb_export_bundle(const ecg_render_bundle& render, const std::string& record_name, wfdb_export_bundle& output, ecg_export_result& result);
}
