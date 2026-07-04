#pragma once

#include "ecg_export.h"

#include <string>
#include <vector>

namespace signal_synth
{
    struct edf_bdf_export_artifact
    {
        std::string name;
        std::string media_type;
        std::string content;
    };

    struct edf_bdf_export_bundle
    {
        std::vector<edf_bdf_export_artifact> artifacts;
    };

    bool build_edf_bdf_export_bundle(const ecg_render_bundle& render, const std::string& record_name, edf_bdf_export_bundle& output, ecg_export_result& result);
}
