#include <cstdio>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <map>
#include <tuple>
#include <vector>
#include <locale>
#include <chrono>
#include <deque>
#include <algorithm>
#include <cmath>
#include <limits>
using namespace std;

#include "fileio2.h"
#include "stringutils.h"
#include "minvect.h"
#include "datastructures.h"
#include "math.h"
#include "ZaxJsonParser.h"
#include "../../../../Libs/3rdParty/LIB_RSPT/lib_rspt/rspt_types.h"
#include "../../../../Libs/3rdParty/LIB_RSPT/lib_rspt/filter.h"
#include "../../../../Libs/3rdParty/LIB_RSPT/lib_rspt/iir_filter_opt.h"
#include "../../../../Libs/3rdParty/LIB_RSPT/lib_rspt/peak_detector.h"
#include "../../../../Libs/3rdParty/LIB_RSPT/lib_rspt/ecg_analysis.h"
#include "ecg_model.h"
#include "clinical_ecg.h"
#include "ecg_scenario.h"
#include "ppg_model.h"
#include "ecg_scenario_json.h"
#include "ecg_render.h"
#include "signal_quality.h"
#include "wearable_timebase.h"

CVariable_List*    m_variable_list_ref = 0; /// reference for the application's variable list.
CSignalCodec_List* m_data_list_ref = 0;     /// reference for the application's data list.
NewChar_Proc       NewChar;                 /// function call for application's "new char[n]" constructor. Strings that are going to be deleted by other parts of the system, needs to be allocated with this.
NewCVariable_Proc  NewCVariable;            /// function call for application's "new CVariable" constructor. Variables that are going to be deleted by other parts of the system (added to m_variable_list_ref), needs to be allocated with this.
Call_Proc          Call;                    /// function implemented by the application. Executes a script in the current running script's context / sandbox.

template <size_t capacity>
static void copy_fixed_string(char (&destination)[capacity], const char* source)
{
    static_assert(capacity > 0, "Fixed string destination must not be empty.");
    const size_t source_length = source ? strlen(source) : 0;
    const size_t copy_length = source_length < capacity ? source_length : capacity - 1;
    if (copy_length)
        memcpy(destination, source, copy_length);
    destination[copy_length] = 0;
}

/** functions loaded by the application and passed to signal processor modules. */
FFT_PROC        FFT;
RFFT_PROC       RFFT;
FFTEXEC_PROC    FFTEXEC;
FFTDESTROY_PROC FFTDESTROY;


void convolve(double* res, const double* l, int n, double* r, int m)
{
    int res_size = n + m - 1;
    //res.resize(res_size, 0.0);

    for (int i = 0; i < res_size; ++i)
    {
        res[i] = 0.0;
        for (int j = 0; j < n; ++j)
            if (i - j >= 0 && i - j < m)
                res[i] += l[j] * r[i - j];
    }


//    for (int i = 0; i < n + m - 1; ++i)
//    {
//        res[i] = 0;
//        for (int j = 0; j <= i; ++j)
//            if (j < n && (i - j) < m)
//                res[i] += l[j] * r[i - j];
//    }
}

char* Convolve(char* a_dst_name, char* a_src_name, char* a_kernel_name)
{
    char* res = 0;
    if (!a_dst_name || !a_src_name)
        res = MakeString(NewChar, "ERROR: Copy: not enough arguments.");
    CVariable* src = m_variable_list_ref->variablemap_find(a_src_name);
    if (!res && !src)
        res = MakeString(NewChar, "ERROR: Copy ", a_src_name, " not found.");
    CVariable* kernel = m_variable_list_ref->variablemap_find(a_kernel_name);
    if (!res && !kernel)
        res = MakeString(NewChar, "ERROR: Copy ", a_kernel_name, " not found.");
    if (!res)
    {
        vector<unsigned int> out_samples(src->m_total_samples.m_size, 0.0);
        for (unsigned int ch = 0; ch < src->m_total_samples.m_size; ++ch)
            out_samples[ch] = src->m_total_samples.m_data[ch] + kernel->m_total_samples.m_data[ch] - 1;
        CVariable* dst = NewCVariable();
        dst->Rebuild(src->m_total_samples.m_size, out_samples.data());
        src->SCopyTo(dst);
        for (unsigned int ch = 0; ch < src->m_total_samples.m_size; ++ch)
            convolve(dst->m_data[ch], src->m_data[ch], src->m_total_samples.m_data[ch], kernel->m_data[ch], kernel->m_total_samples.m_data[ch]);

        copy_fixed_string(dst->m_varname, a_dst_name);
        m_variable_list_ref->Insert(dst->m_varname, dst);
    }
    return res;
}

char* RDetectionTest(char* a_ecg_signal_name, char* spike_signal_name, char* a_value)
{
    if (!a_ecg_signal_name || !spike_signal_name || !a_value)
        return MakeString(NewChar, "ERROR: RDetectionTest: not enough arguments!");

//    double l_value = atof(a_value);
//    if (ISignalCodec* ecg_data = m_data_list_ref->datamap_find(a_ecg_signal_name))
//    {
//    }
//    else
//        return MakeString(NewChar, "ERROR: RDetectionTest: Can't find data in datalist: '", a_ecg_signal_name, "'");
    return 0;
}

char* Delay(char* a_ecg_signal_name, char* a_value)
{
    if (!a_ecg_signal_name || !a_value)
        return MakeString(NewChar, "ERROR: RDetectionTest: not enough arguments!");

    double l_value = atof(a_value);
    if (CVariable* ecg_data = m_variable_list_ref->variablemap_find(a_ecg_signal_name))
    {
        for (unsigned int ch = 0; ch < ecg_data->m_total_samples.m_size; ++ch)
        {
            delay delay_(l_value);
            for (unsigned int i = 0; i < ecg_data->m_widths.m_data[ch] - l_value; ++i)
                ecg_data->m_data[ch][i] = delay_.get_delayed(ecg_data->m_data[ch][i]);
        }
    }
    else
        return MakeString(NewChar, "ERROR: RDetectionTest: Can't find data in datalist: '", a_ecg_signal_name, "'");
    return 0;
}

char* FilterRSPT(char* a_dataname, char* a_filter)
{
    char * l_result = nullptr;
    if (!a_dataname || !a_filter)
        l_result = MakeString(NewChar, "ERROR: FilterRSPT: Not enough argument!");
    else
    {
        CVariable* l_data = m_variable_list_ref->variablemap_find(a_dataname);
        if (!l_data)
            l_result = MakeString(NewChar, "ERROR: FilterRSPT: Can't find '", a_dataname, "' in variable list");
        CVariable* l_filter = m_variable_list_ref->variablemap_find(a_filter);
        double* l_denominator = nullptr;
        double* l_numerator = nullptr;
        if (!l_filter)
            l_result = MakeString(NewChar, "ERROR: FilterRSPT: Can't find '", a_filter, "' in variable list");
        else
        {
            l_denominator = l_filter->m_data[0];
            l_numerator = l_filter->m_data[1];
        }
        if (!l_result)
        {
            int order = l_filter->m_total_samples.m_data[0];
            i_filter* filter = i_filter::new_iir(l_numerator, l_denominator, order);
            for (unsigned int ch = 0; ch < l_data->m_total_samples.m_size; ++ch)
            {
                filter->init_history_values(l_data->m_data[ch][0], l_data->m_sample_rates.m_data[ch]);
                for (unsigned int i = 0; i < l_data->m_widths.m_data[ch]; ++i)
                    l_data->m_data[ch][i] = filter->filter_opt(l_data->m_data[ch][i]);
            }
            i_filter::delete_iir(filter);
        }
    }
    return l_result;
}

//            auto start = std::chrono::high_resolution_clock::now();
//            for (int iter = 0; iter < 10000; iter++)
//            std::cout << "ideje: " << (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start)).count() << " ms\n";

void double_from_str(double& var, const char* str)
{
    if (str && strlen(str))
        var = atof(str);
}

void int_from_str(int& var, const char* str)
{
    if (str && strlen(str))
        var = atoi(str);
}

void split_signal_around_indexes(double** slices, double* signal, unsigned int signal_len, unsigned int* peaks, unsigned int nr_peaks, unsigned int samples_before, unsigned int samples_after)
{
    for (unsigned int i = 0; i < nr_peaks; ++i)
    {
        if ((peaks[i] > samples_before) && (peaks[i] + samples_after < signal_len))
        {
            unsigned int si = 0;
            for (unsigned int j = peaks[i] - samples_before; j < peaks[i] + samples_after; ++j)
                slices[i][si++] = signal[j];
        }
    }
}

void average_signals(double** signals, unsigned int nr_signals, double* average, unsigned int signal_len)
{
    for (unsigned int i = 0; i < signal_len; ++i)
    {
        average[i] = 0;
        for (unsigned int j = 0; j < nr_signals; ++j)
            average[i] += signals[j][i];
        average[i] /= nr_signals;
    }
}

char* detect_spikes_rspt(char* a_dst_name, char* a_spike_signal, char* a_marker_val, char* a_previous_spike_reference_ratio, char* a_previous_spike_reference_attenuation)
{
    if (!a_dst_name || !a_spike_signal)
        return MakeString(NewChar, "ERROR: DetectSpikes: not enough arguments.");

    CVariable* spike_signal = m_variable_list_ref->variablemap_find(a_spike_signal);
    if (!spike_signal)
        return MakeString(NewChar, "ERROR: DetectSpikes ", a_spike_signal, " not found.");

    double marker_val = 1;
//    double previous_spike_reference_ratio = 0.5;
//    double previous_spike_reference_attenuation = 15;
//    const double threshold_ratio = 1.5;

    double_from_str(marker_val, a_marker_val);
//    double_from_str(previous_spike_reference_ratio, a_previous_spike_reference_ratio);
//    double_from_str(previous_spike_reference_attenuation, a_previous_spike_reference_attenuation);

    unsigned int nr_channels = spike_signal->m_total_samples.m_size;
    if (nr_channels > 12)
        nr_channels = 12;

    CVariable* dst = NewCVariable();
    unsigned int nr_samples[3];
    nr_samples[0] = spike_signal->m_total_samples.m_data[0];
    nr_samples[1] = spike_signal->m_total_samples.m_data[0];
    nr_samples[2] = spike_signal->m_total_samples.m_data[0];

    dst->Rebuild(3, nr_samples);
    dst->m_sample_rates.m_data[0] = spike_signal->m_sample_rates.m_data[0];
    dst->m_sample_rates.m_data[1] = spike_signal->m_sample_rates.m_data[0];
    dst->m_sample_rates.m_data[2] = spike_signal->m_sample_rates.m_data[0];

    copy_fixed_string(dst->m_varname, a_dst_name);
    m_variable_list_ref->Insert(dst->m_varname, dst);

//    double spike_sample = 0, threshold_sample = 0;
    std::vector<unsigned int> peak_indexes;
    for (unsigned int ch = 0; ch < nr_channels; ++ch)
    {
        peak_detector_offline peak_detector_(spike_signal->m_sample_rates.m_data[ch], marker_val);
        peak_detector_.detect(spike_signal->m_data[ch], nr_samples[0], dst->m_data[0], dst->m_data[1], dst->m_data[2], &peak_indexes);
//        peak_detector peak_detector_(spike_signal->m_sample_rates.m_data[ch], 10000);
//        for (unsigned int i = 0; i < dst->m_total_samples.m_data[ch]; ++i)
//        {
//            dst->m_data[ch][i] = peak_detector_.detect(spike_signal->m_data[ch][i], &spike_sample, &threshold_sample);
//            dst->m_data[1][i] = spike_sample;
//            dst->m_data[2][i] = threshold_sample;
//        }
    }

    unsigned int sizes[peak_indexes.size()];
    CVariable* dst2 = NewCVariable();
    copy_fixed_string(dst2->m_varname, "AVGs");
    m_variable_list_ref->Insert(dst2->m_varname, dst2);
    unsigned int triggercount = peak_indexes.size();
    unsigned int a_radius_left = dst->m_sample_rates.m_data[0] * 0.4;
    unsigned int a_radius_right = dst->m_sample_rates.m_data[0] * 0.6;
    for (unsigned int ch = 0; ch < triggercount; ++ch)
        sizes[ch] = a_radius_left + a_radius_right;
    dst2->Rebuild(triggercount, sizes);
    for (unsigned int ch = 0; ch < triggercount; ++ch)
        dst2->m_sample_rates.m_data[ch] = dst->m_sample_rates.m_data[0];

    split_signal_around_indexes(dst2->m_data, spike_signal->m_data[0], spike_signal->m_total_samples.m_data[0], peak_indexes.data(), triggercount, a_radius_left, a_radius_right);
    if (triggercount)
        average_signals(dst2->m_data, triggercount - 1, dst2->m_data[0], a_radius_left + a_radius_right);
    else
        cout << "detect_spikes_rspt: no spikes found?" << endl;

    return 0;
}

char* create_filter_iir(char* a_outdataname, char* a_kind, char* a_type, char* a_order, char* a_sampling_rate, char* a_cutoff_low, char* a_cutoff_high)
{
    if (!a_outdataname || !a_kind || !a_type || !a_order || !a_sampling_rate || !a_cutoff_low)
        return MakeString(NewChar, "ERROR: create_filter_iir: not enough arguments.");

    filter_kind kind = kind_invalid;
    filter_type type = type_invalid;

    if (!strcmp(a_kind, "butter") || !strcmp(a_kind, "butt") || !strcmp(a_kind, "butterworth") || !strcmp(a_kind, "1"))
        kind = butterworth;
    if (!strcmp(a_kind, "chebyshev") || !strcmp(a_kind, "cheby") || !strcmp(a_kind, "2"))
        kind = chebyshev;
    if (!strcmp(a_kind, "bessel") || !strcmp(a_kind, "bess") || !strcmp(a_kind, "3"))
        kind = bessel;
    if (kind == kind_invalid)
        return MakeString(NewChar, "ERROR: create_filter_iir: Unknown filter kind: '", a_kind, "', HINT: Filter kind could be 'butt'. ");
    if (kind != butterworth)
        return MakeString(NewChar, "ERROR: create_filter_iir: Unsupported filter kind: '", a_kind, "'. Currently only Butterworth IIR design is supported.");

    if (!strcmp(a_type, "lowpass") || !strcmp(a_type, "lp") || !strcmp(a_type, "1"))
        type = low_pass;
    if (!strcmp(a_type, "bandpass") || !strcmp(a_type, "bp") || !strcmp(a_type, "2"))
        type = band_pass;
    if (!strcmp(a_type, "highpass") || !strcmp(a_type, "hp") || !strcmp(a_type, "3"))
        type = high_pass;
    if (!strcmp(a_type, "bandstop") || !strcmp(a_type, "bs") || !strcmp(a_type, "4"))
        type = band_stop;
    if (type == type_invalid)
        return MakeString(NewChar, "ERROR: create_filter_iir: Unknown filter type: '", a_type, "', HINT: filter type could be ('lowpass', 'bandpass' or 'highpass'). ");
    if (type == band_stop)
        return MakeString(NewChar, "ERROR: create_filter_iir: bandstop IIR design is not supported.");
    if (type == band_pass && !a_cutoff_high)
        return MakeString(NewChar, "ERROR: create_filter_iir: bandpass design requires cutoff_high.");

    double sampling_rate = atof(a_sampling_rate);
    double cutoff_low = atof(a_cutoff_low);
    double cutoff_high = a_cutoff_high ? atof(a_cutoff_high) : 0.0;
    int order = atoi(a_order);
    vector<double> n, d;
    if (!create_filter_iir(n, d, kind, type, order, sampling_rate, cutoff_low, cutoff_high))
        return MakeString(NewChar, "ERROR: create_filter_iir: bad filter parameters or unsupported order.");
    if (n.size() != d.size())
        return MakeString(NewChar, "ERROR: create_filter_iir: invalid coefficient layout.");

    UIntVec N;
    N.Rebuild(2);
    N.m_data[0] = d.size();
    N.m_data[1] = n.size();
    CVariable* output2 = NewCVariable();
    output2->Rebuild(N.m_size, N.m_data);

    for (unsigned int j = 0; j < output2->m_widths.m_data[0]; ++j)
        output2->m_data[0][j] = d[j];
    for (unsigned int j = 0; j < output2->m_widths.m_data[1]; ++j)
        output2->m_data[1][j] = n[j];
    copy_fixed_string(output2->m_varname, a_outdataname);
    m_variable_list_ref->Insert(a_outdataname, output2);
    return 0;
}

char* CreateFIRFilter(char* a_outdataname, char* a_type, char* a_sampling_rate, char* a_kernel_size, char* a_cutoff_low, char* a_cutoff_high)
{
    if (!a_outdataname || !a_type || !a_sampling_rate || !a_kernel_size || !a_cutoff_low)
        return MakeString(NewChar, "ERROR: CreateFIRFilter: not enough arguments.");

    filter_type type = type_invalid;
    if (!strcmp(a_type, "lowpass") || !strcmp(a_type, "lp") || !strcmp(a_type, "1"))
        type = low_pass;
    if (!strcmp(a_type, "bandpass") || !strcmp(a_type, "bp") || !strcmp(a_type, "2"))
        type = band_pass;
    if (!strcmp(a_type, "highpass") || !strcmp(a_type, "hp") || !strcmp(a_type, "3"))
        type = high_pass;
    if (!strcmp(a_type, "bandstop") || !strcmp(a_type, "bs") || !strcmp(a_type, "4"))
        type = band_stop;
    if (type == type_invalid)
        return MakeString(NewChar, "ERROR: CreateFIRFilter: Unknown filter type: '", a_type, "', HINT: filter type could be ('lowpass', 'bandpass', 'highpass' or 'bandstop'). ");
    if ((type == band_pass || type == band_stop) && !a_cutoff_high)
        return MakeString(NewChar, "ERROR: CreateFIRFilter: bandpass and bandstop filters require cutoff2.");

    int kernel_size = atoi(a_kernel_size);
    double sampling_rate = atof(a_sampling_rate);
    double cutoff_low = atof(a_cutoff_low);
    double cutoff_high = a_cutoff_high ? atof(a_cutoff_high) : 0.0;
    vector<double> kernel;
    if (!create_filter_fir(kernel, type, kernel_size, sampling_rate, cutoff_low, cutoff_high))
        return MakeString(NewChar, "ERROR: CreateFIRFilter: bad filter parameters. Cutoffs must be between 0 and Nyquist; high-pass and band-stop FIR filters require an odd kernel size.");

    unsigned int number_of_samples[1] = {static_cast<unsigned int>(kernel.size())};
    CVariable* output = NewCVariable();
    output->Rebuild(1, number_of_samples);
    output->m_sample_rates.m_data[0] = sampling_rate;
    for (unsigned int i = 0; i < number_of_samples[0]; ++i)
        output->m_data[0][i] = kernel[i];

    copy_fixed_string(output->m_varname, a_outdataname);
    m_variable_list_ref->Insert(a_outdataname, output);
    return 0;
}

void put_marker_interval(
    CVariable* var,
    double start,
    double len,
    const char* label = nullptr,
    int channel = -1)
{
    char fixed_label[32];
    copy_fixed_string(fixed_label, label);
    var->m_marker_list.AddTMarker(
        start,
        len,
        -1,
        0,
        GetSomeColor((start + len) * 100),
        fixed_label,
        channel);
    var->m_marker_list.m_data[var->m_marker_list.m_size - 1].m_movable = true;
    var->m_marker_list.m_data[var->m_marker_list.m_size - 1].m_sizable_left = true;
    var->m_marker_list.m_data[var->m_marker_list.m_size - 1].m_sizable_right = true;
}

static void set_sample_value(CVariable* dst, unsigned int channel, int32_t sample, unsigned int sample_count, double value)
{
    if (!dst || channel >= dst->m_total_samples.m_size || sample < 0)
        return;
    if (static_cast<unsigned int>(sample) >= sample_count)
        return;
    dst->m_data[channel][sample] = value;
}

static void put_marker_interval_if_valid(
    CVariable* dst,
    int32_t start_sample,
    int32_t end_sample,
    double sampling_rate,
    const char* label = nullptr,
    int channel = -1)
{
    if (!dst || start_sample < 0 || end_sample <= start_sample || sampling_rate <= 0.0)
        return;
    put_marker_interval(
        dst,
        start_sample / sampling_rate,
        (end_sample - start_sample) / sampling_rate,
        label,
        channel);
}

char* analyse_ecg_detect_peaks(char* data_name,  char* annotations_signal_name, char* chindx, char* a_analysis_peak_indx)
//char* detect_spikes_rspt(char* a_dst_name, char* a_spike_signal, char* a_marker_val, char* a_previous_spike_reference_ratio, char* a_previous_spike_reference_attenuation)
{
    if (!data_name || !annotations_signal_name)
        return MakeString(NewChar, "ERROR: analyse_ecg_detect_peaks: not enough arguments.");

    CVariable* data = m_variable_list_ref->variablemap_find(data_name);
    if (!data)
        return MakeString(NewChar, "ERROR: analyse_ecg_detect_peaks ", data_name, " not found.");

    int ch_indx = 0;
    int analysis_peak_indx = -1;
    int_from_str(ch_indx, chindx);
    int_from_str(analysis_peak_indx, a_analysis_peak_indx);

    unsigned int nr_channels = data->m_total_samples.m_size;
    unsigned int nr_samples_per_ch = data->m_total_samples.m_data[0];
    double sampling_rate = data->m_sample_rates.m_data[0];
    if (nr_channels > 12)
        nr_channels = 12;
    if (ch_indx < 0)
        ch_indx = 0;
    if (static_cast<unsigned int>(ch_indx) >= nr_channels)
        ch_indx = nr_channels ? nr_channels - 1 : 0;

    CVariable* dst = NewCVariable();
    unsigned int nr_samples[3];
    nr_samples[0] = nr_samples_per_ch;
    nr_samples[1] = nr_samples_per_ch;
    nr_samples[2] = nr_samples_per_ch;

    dst->Rebuild(3, nr_samples);
    dst->m_sample_rates.m_data[0] = sampling_rate;
    dst->m_sample_rates.m_data[1] = sampling_rate;
    dst->m_sample_rates.m_data[2] = sampling_rate;
    copy_fixed_string(dst->m_labels.m_data[0].s, "ECG");
    copy_fixed_string(dst->m_labels.m_data[1].s, "R peaks");
    copy_fixed_string(dst->m_labels.m_data[2].s, "RSPT PQRST fiducials");

    copy_fixed_string(dst->m_varname, annotations_signal_name);
    m_variable_list_ref->Insert(dst->m_varname, dst);

    std::vector<uint32_t> beat_indexes_to_analyze;
    if (analysis_peak_indx >= 0)
        beat_indexes_to_analyze.push_back(static_cast<uint32_t>(analysis_peak_indx));

    std::vector<rspt_ecg_beat_result> beat_results;
    rspt_ecg_summary_result summary = {};
    std::vector<uint32_t> peak_indxs;
    int32_t status = rspt::analyze_ecg((const double**)data->m_data, nr_channels, nr_samples_per_ch, sampling_rate, beat_results, summary, ch_indx, nullptr, &peak_indxs, RSPT_MODE_DEFAULT, beat_indexes_to_analyze.empty() ? nullptr : &beat_indexes_to_analyze);

    if (status != RSPT_STATUS_OK)
        return MakeString(NewChar, "ERROR: analyse_ecg_detect_peaks: ", rspt::status_message(status));

    double scale = 0.0500;
    for (const rspt_ecg_beat_result& beat : beat_results)
    {
        const rspt_pqrst_annotation& annot = beat.annotation;

        set_sample_value(dst, 2, annot.p1_onset_sample, nr_samples_per_ch, scale * 0.25);
        set_sample_value(dst, 2, annot.p1_peak_sample, nr_samples_per_ch, scale * 1.0);
        set_sample_value(dst, 2, annot.p1_offset_sample, nr_samples_per_ch, scale * 0.5);
        set_sample_value(dst, 2, annot.p2_onset_sample, nr_samples_per_ch, scale * 0.25);
        set_sample_value(dst, 2, annot.p2_peak_sample, nr_samples_per_ch, scale * 1.0);
        set_sample_value(dst, 2, annot.p2_offset_sample, nr_samples_per_ch, scale * 0.5);
        set_sample_value(dst, 2, annot.qrs_onset_sample, nr_samples_per_ch, scale * 0.25);
        set_sample_value(dst, 2, annot.r_peak_sample, nr_samples_per_ch, scale * 1.0);
        set_sample_value(dst, 2, annot.qrs_offset_sample, nr_samples_per_ch, scale * 0.5);
        set_sample_value(dst, 2, annot.t_onset_sample, nr_samples_per_ch, scale * 0.25);
        set_sample_value(dst, 2, annot.t_peak_sample, nr_samples_per_ch, scale * 1.0);
        set_sample_value(dst, 2, annot.t_offset_sample, nr_samples_per_ch, scale * 0.5);

        const int32_t p_offset_sample = annot.p2_offset_sample >= 0 ? annot.p2_offset_sample : annot.p1_offset_sample;
        put_marker_interval_if_valid(
            dst,
            annot.p1_onset_sample,
            p_offset_sample,
            sampling_rate,
            "RSPT P wave",
            0);
        put_marker_interval_if_valid(
            dst,
            annot.qrs_onset_sample,
            annot.qrs_offset_sample,
            sampling_rate,
            "RSPT QRS complex",
            0);
        put_marker_interval_if_valid(
            dst,
            annot.t_onset_sample,
            annot.t_offset_sample,
            sampling_rate,
            "RSPT T wave",
            0);
        if (annot.j_point_sample >= 0)
            put_marker_interval(
                dst,
                annot.j_point_sample / sampling_rate,
                5.0 / sampling_rate,
                "RSPT J point",
                0);
    }
//    for (unsigned int i = 0; i < nr_samples_per_ch; ++i)
//        dst->m_data[1][i] = peak_indxs[i];
    for (auto peak : peak_indxs)
        set_sample_value(dst, 1, peak, nr_samples_per_ch, scale * 1.0);

    memcpy(dst->m_data[0], data->m_data[ch_indx], nr_samples_per_ch * sizeof(double));
    //memcpy(dst->m_data[1], data->m_data[1], nr_samples_per_ch * sizeof(double));

    return 0;
}

struct zax_ecg_model_config: public signal_synth::ecg_model_config
{
    ZAX_JSON_SERIALIZABLE(
        zax_ecg_model_config,
        JSON_PROPERTY(heart_rate_bpm),
        JSON_PROPERTY(baseline_amplitude_mv),
        JSON_PROPERTY(respiration_frequency_hz),
        JSON_PROPERTY_NAME(
            waves[signal_synth::ecg_wave_p].phase_radians,
            "p_phase_radians"),
        JSON_PROPERTY_NAME(
            waves[signal_synth::ecg_wave_p].amplitude,
            "p_amplitude"),
        JSON_PROPERTY_NAME(
            waves[signal_synth::ecg_wave_p].width_radians,
            "p_width_radians"),
        JSON_PROPERTY_NAME(
            waves[signal_synth::ecg_wave_q].phase_radians,
            "q_phase_radians"),
        JSON_PROPERTY_NAME(
            waves[signal_synth::ecg_wave_q].amplitude,
            "q_amplitude"),
        JSON_PROPERTY_NAME(
            waves[signal_synth::ecg_wave_q].width_radians,
            "q_width_radians"),
        JSON_PROPERTY_NAME(
            waves[signal_synth::ecg_wave_r].phase_radians,
            "r_phase_radians"),
        JSON_PROPERTY_NAME(
            waves[signal_synth::ecg_wave_r].amplitude,
            "r_amplitude"),
        JSON_PROPERTY_NAME(
            waves[signal_synth::ecg_wave_r].width_radians,
            "r_width_radians"),
        JSON_PROPERTY_NAME(
            waves[signal_synth::ecg_wave_s].phase_radians,
            "s_phase_radians"),
        JSON_PROPERTY_NAME(
            waves[signal_synth::ecg_wave_s].amplitude,
            "s_amplitude"),
        JSON_PROPERTY_NAME(
            waves[signal_synth::ecg_wave_s].width_radians,
            "s_width_radians"),
        JSON_PROPERTY_NAME(
            waves[signal_synth::ecg_wave_t].phase_radians,
            "t_phase_radians"),
        JSON_PROPERTY_NAME(
            waves[signal_synth::ecg_wave_t].amplitude,
            "t_amplitude"),
        JSON_PROPERTY_NAME(
            waves[signal_synth::ecg_wave_t].width_radians,
            "t_width_radians"),
        JSON_PROPERTY_NAME(hrv.enabled, "hrv_enabled"),
        JSON_PROPERTY_NAME(
            hrv.rr_standard_deviation_seconds,
            "rr_standard_deviation_seconds"),
        JSON_PROPERTY_NAME(hrv.lf_hf_ratio, "lf_hf_ratio"),
        JSON_PROPERTY_NAME(
            hrv.lf_center_frequency_hz,
            "lf_center_frequency_hz"),
        JSON_PROPERTY_NAME(
            hrv.hf_center_frequency_hz,
            "hf_center_frequency_hz"),
        JSON_PROPERTY_NAME(
            hrv.lf_bandwidth_hz,
            "lf_bandwidth_hz"),
        JSON_PROPERTY_NAME(
            hrv.hf_bandwidth_hz,
            "hf_bandwidth_hz"),
        JSON_PROPERTY_NAME(
            hrv.minimum_rr_seconds,
            "minimum_rr_seconds"),
        JSON_PROPERTY_NAME(
            hrv.maximum_rr_seconds,
            "maximum_rr_seconds"),
        JSON_PROPERTY_NAME(hrv.seed, "hrv_seed"),
        JSON_PROPERTY_NAME(
            scenario.premature_every_n_beats,
            "premature_every_n_beats"),
        JSON_PROPERTY_NAME(
            scenario.premature_probability,
            "premature_probability"),
        JSON_PROPERTY_NAME(
            scenario.premature_rr_ratio,
            "premature_rr_ratio"),
        JSON_PROPERTY_NAME(
            scenario.compensatory_pause_ratio,
            "compensatory_pause_ratio"),
        JSON_PROPERTY_NAME(
            scenario.premature_p_amplitude_scale,
            "premature_p_amplitude_scale"),
        JSON_PROPERTY_NAME(
            scenario.premature_qrs_amplitude_scale,
            "premature_qrs_amplitude_scale"),
        JSON_PROPERTY_NAME(
            scenario.premature_qrs_width_scale,
            "premature_qrs_width_scale"),
        JSON_PROPERTY_NAME(
            scenario.premature_t_amplitude_scale,
            "premature_t_amplitude_scale"),
        JSON_PROPERTY_NAME(scenario.seed, "scenario_seed"))
};

enum clinical_annotation_output
{
    clinical_annotations_markers = 1,
    clinical_annotations_channels = 2,
    clinical_annotations_none = 3
};

bool parse_clinical_annotation_output(const char* text, clinical_annotation_output& output)
{
    output = clinical_annotations_markers;
    if (!text || !text[0])
        return true;
    char* end = nullptr;
    const long value = strtol(text, &end, 10);
    if (!end || end == text || end[0] || value < clinical_annotations_markers || value > clinical_annotations_none)
        return false;
    output = static_cast<clinical_annotation_output>(value);
    return true;
}

const char* ecg_model_wave_label(signal_synth::ecg_wave wave)
{
    const char* labels[signal_synth::ecg_wave_count] = {"GT P", "GT Q", "GT R", "GT S", "GT T"};
    return wave >= signal_synth::ecg_wave_p && wave < signal_synth::ecg_wave_count ? labels[wave] : "GT ECG event";
}

char* GenerateSyntheticECG(
    char* output_name,
    char* number_of_samples_text,
    char* sampling_rate_text,
    char* parameters_json,
    char* annotation_output_text)
{
    if (!output_name || !number_of_samples_text ||
        !sampling_rate_text || !parameters_json)
    {
        return MakeString(
            NewChar,
            "ERROR: GenerateSyntheticECG: not enough arguments.");
    }

    const unsigned long parsed_samples =
        strtoul(number_of_samples_text, nullptr, 10);
    const unsigned long parsed_sampling_rate =
        strtoul(sampling_rate_text, nullptr, 10);
    if (parsed_samples == 0 ||
        parsed_samples > std::numeric_limits<unsigned int>::max() ||
        parsed_sampling_rate == 0 ||
        parsed_sampling_rate >
            std::numeric_limits<unsigned int>::max())
    {
        return MakeString(
            NewChar,
            "ERROR: GenerateSyntheticECG: invalid sample count or sampling rate.");
    }
    clinical_annotation_output annotation_output;
    if (!parse_clinical_annotation_output(annotation_output_text, annotation_output))
        return MakeString(NewChar, "ERROR: GenerateSyntheticECG: annotation output must be 1 (markers), 2 (channels), or 3 (none).");

    zax_ecg_model_config config = parameters_json;
    config.sampling_rate_hz =
        static_cast<unsigned int>(parsed_sampling_rate);

    const unsigned int sample_count =
        static_cast<unsigned int>(parsed_samples);
    signal_synth::ecg_validation_package package;
    if (!package.generate(config, sample_count))
    {
        return MakeString(
            NewChar,
            "ERROR: GenerateSyntheticECG: invalid parameters or generation failed.");
    }

    const unsigned int channel_count = annotation_output == clinical_annotations_channels ? signal_synth::ecg_validation_channel_count : 1U;
    vector<unsigned int> channel_sizes(channel_count, sample_count);
    CVariable* output = NewCVariable();
    output->Rebuild(channel_count, channel_sizes.data());
    const char* channel_labels[
        signal_synth::ecg_validation_channel_count] = {
        "ECG",
        "Model PQRST events",
        "Measured PQRST and P/T boundaries",
        "RR interval",
        "Beat type"
    };
    for (unsigned int channel = 0; channel < channel_count; ++channel)
    {
        output->m_sample_rates.m_data[channel] =
            config.sampling_rate_hz;
        copy_fixed_string(
            output->m_labels.m_data[channel].s,
            channel_labels[channel]);
        memcpy(
            output->m_data[channel],
            package.channel(
                static_cast<signal_synth::ecg_validation_channel>(
                    channel)),
            sample_count * sizeof(double));
    }
    if (annotation_output == clinical_annotations_channels)
        copy_fixed_string(output->m_vertical_units.m_data[signal_synth::ecg_validation_rr_intervals].s, "s");

    const signal_synth::ecg_model_annotation* annotations = package.model_annotations();
    if (annotation_output == clinical_annotations_markers)
    {
        for (unsigned int i = 0; i < package.model_annotation_count(); ++i)
        {
            const signal_synth::ecg_model_annotation& annotation = annotations[i];
            if (!annotation.present)
                continue;
            put_marker_interval(output, annotation.time_seconds, 1.0 / config.sampling_rate_hz, ecg_model_wave_label(annotation.wave), 0);
            if (annotation.wave == signal_synth::ecg_wave_r && annotation.beat_kind != signal_synth::ecg_beat_sinus)
            {
                const char* label = annotation.beat_kind == signal_synth::ecg_beat_premature ? "Premature coupling interval" : "Compensatory pause";
                put_marker_interval(output, std::max(0.0, annotation.time_seconds - annotation.rr_interval_seconds), annotation.rr_interval_seconds, label, 0);
            }
        }
    }

    copy_fixed_string(output->m_varname, output_name);
    m_variable_list_ref->Insert(output_name, output);
    return 0;
}

struct zax_clinical_source_config: public signal_synth::clinical_source_config
{
    void set_from(const signal_synth::clinical_source_config& config)
    {
        static_cast<signal_synth::clinical_source_config&>(*this) = config;
    }

    void apply_to(signal_synth::clinical_source_config& config) const
    {
        config = static_cast<const signal_synth::clinical_source_config&>(*this);
    }

    ZAX_JSON_SERIALIZABLE(zax_clinical_source_config, JSON_PROPERTY(gain), JSON_PROPERTY(axis_offset_degrees), JSON_PROPERTY(elevation_offset_degrees))
};

struct zax_clinical_enum_config
{
    int rhythm_type = signal_synth::clinical_rhythm_sinus;
    int av_conduction_type = signal_synth::clinical_av_normal;
    int intraventricular_conduction_type = signal_synth::clinical_iv_normal;
    int preexcitation_type = signal_synth::clinical_preexcitation_none;
    int episode_kind_type = signal_synth::clinical_episode_none;
    int qt_correction_type = signal_synth::clinical_qt_fridericia;
    int premature_origin_type = signal_synth::clinical_origin_pvc;

    void set_from(const signal_synth::clinical_ecg_config& config)
    {
        rhythm_type = config.rhythm.rhythm;
        av_conduction_type = config.rhythm.av_conduction;
        intraventricular_conduction_type = config.rhythm.intraventricular_conduction;
        preexcitation_type = config.rhythm.preexcitation;
        episode_kind_type = config.scenario.episode_kind;
        qt_correction_type = config.timing.qt_correction;
        premature_origin_type = config.scenario.premature_origin;
    }

    void apply_to(signal_synth::clinical_ecg_config& config) const
    {
        config.rhythm.rhythm = static_cast<signal_synth::clinical_rhythm>(rhythm_type);
        config.rhythm.av_conduction = static_cast<signal_synth::clinical_av_conduction>(av_conduction_type);
        config.rhythm.intraventricular_conduction = static_cast<signal_synth::clinical_intraventricular_conduction>(intraventricular_conduction_type);
        config.rhythm.preexcitation = static_cast<signal_synth::clinical_preexcitation>(preexcitation_type);
        config.scenario.episode_kind = static_cast<signal_synth::clinical_episode_kind>(episode_kind_type);
        config.timing.qt_correction = static_cast<signal_synth::clinical_qt_correction>(qt_correction_type);
        config.scenario.premature_origin = static_cast<signal_synth::clinical_ventricular_origin>(premature_origin_type);
    }

    ZAX_JSON_SERIALIZABLE(
        zax_clinical_enum_config,
        JSON_PROPERTY(rhythm_type),
        JSON_PROPERTY(av_conduction_type),
        JSON_PROPERTY(intraventricular_conduction_type),
        JSON_PROPERTY(preexcitation_type),
        JSON_PROPERTY(episode_kind_type),
        JSON_PROPERTY(qt_correction_type),
        JSON_PROPERTY(premature_origin_type))
};

struct zax_clinical_timing_config: public signal_synth::clinical_timing_config
{
    void set_from(const signal_synth::clinical_timing_config& config)
    {
        static_cast<signal_synth::clinical_timing_config&>(*this) = config;
    }

    void apply_to(signal_synth::clinical_timing_config& config) const
    {
        config = static_cast<const signal_synth::clinical_timing_config&>(*this);
    }

    ZAX_JSON_SERIALIZABLE(
        zax_clinical_timing_config,
        JSON_PROPERTY_NAME(p_duration_ms, "p_duration_ms"),
        JSON_PROPERTY_NAME(pr_interval_ms, "pr_interval_ms"),
        JSON_PROPERTY_NAME(qrs_duration_ms, "qrs_duration_ms"),
        JSON_PROPERTY_NAME(qrs_q_fraction, "qrs_q_fraction"),
        JSON_PROPERTY_NAME(qrs_r_fraction, "qrs_r_fraction"),
        JSON_PROPERTY_NAME(qrs_s_fraction, "qrs_s_fraction"),
        JSON_PROPERTY_NAME(t_duration_ms, "t_duration_ms"),
        JSON_PROPERTY_NAME(t_peak_fraction, "t_peak_fraction"),
        JSON_PROPERTY_NAME(qt_interval_ms, "qt_interval_ms"),
        JSON_PROPERTY_NAME(qtc_ms, "qtc_ms"))
};

struct zax_clinical_morphology_config: public signal_synth::clinical_morphology_config
{
    void set_from(const signal_synth::clinical_morphology_config& config)
    {
        static_cast<signal_synth::clinical_morphology_config&>(*this) = config;
    }

    void apply_to(signal_synth::clinical_morphology_config& config) const
    {
        config = static_cast<const signal_synth::clinical_morphology_config&>(*this);
    }

    ZAX_JSON_SERIALIZABLE(
        zax_clinical_morphology_config,
        JSON_PROPERTY_NAME(p_amplitude_mv, "p_amplitude_mv"),
        JSON_PROPERTY_NAME(q_amplitude_mv, "q_amplitude_mv"),
        JSON_PROPERTY_NAME(r_amplitude_mv, "r_amplitude_mv"),
        JSON_PROPERTY_NAME(s_amplitude_mv, "s_amplitude_mv"),
        JSON_PROPERTY_NAME(t_amplitude_mv, "t_amplitude_mv"),
        JSON_PROPERTY_NAME(st_j_amplitude_mv, "st_j_amplitude_mv"),
        JSON_PROPERTY_NAME(st_slope_mv_per_second, "st_slope_mv_per_second"),
        JSON_PROPERTY_NAME(p_axis_degrees, "p_axis_degrees"),
        JSON_PROPERTY_NAME(qrs_axis_degrees, "qrs_axis_degrees"),
        JSON_PROPERTY_NAME(t_axis_degrees, "t_axis_degrees"),
        JSON_PROPERTY_NAME(p_elevation_degrees, "p_elevation_degrees"),
        JSON_PROPERTY_NAME(qrs_elevation_degrees, "qrs_elevation_degrees"),
        JSON_PROPERTY_NAME(t_elevation_degrees, "t_elevation_degrees"),
        JSON_PROPERTY_NAME(presence_threshold_mv, "presence_threshold_mv"))
};

struct zax_clinical_rhythm_config: public signal_synth::clinical_rhythm_config
{
    void set_from(const signal_synth::clinical_rhythm_config& config)
    {
        static_cast<signal_synth::clinical_rhythm_config&>(*this) = config;
    }

    void apply_to(signal_synth::clinical_rhythm_config& config) const
    {
        config = static_cast<const signal_synth::clinical_rhythm_config&>(*this);
    }

    ZAX_JSON_SERIALIZABLE(
        zax_clinical_rhythm_config,
        JSON_PROPERTY_NAME(heart_rate_bpm, "heart_rate_bpm"),
        JSON_PROPERTY_NAME(atrial_rate_bpm, "atrial_rate_bpm"),
        JSON_PROPERTY_NAME(ventricular_escape_rate_bpm, "ventricular_escape_rate_bpm"),
        JSON_PROPERTY_NAME(rr_variability_seconds, "rr_variability_seconds"),
        JSON_PROPERTY_NAME(minimum_rr_seconds, "minimum_rr_seconds"),
        JSON_PROPERTY_NAME(maximum_rr_seconds, "maximum_rr_seconds"),
        JSON_PROPERTY_NAME(first_degree_pr_ms, "first_degree_pr_ms"),
        JSON_PROPERTY_NAME(mobitz_cycle_length, "mobitz_cycle_length"),
        JSON_PROPERTY_NAME(wenckebach_pr_increment_ms, "wenckebach_pr_increment_ms"),
        JSON_PROPERTY_NAME(flutter_conduction_ratio, "flutter_conduction_ratio"),
        JSON_PROPERTY_NAME(seed, "seed"))
};

struct zax_clinical_scenario_config: public signal_synth::clinical_scenario_config
{
    void set_from(const signal_synth::clinical_scenario_config& config)
    {
        static_cast<signal_synth::clinical_scenario_config&>(*this) = config;
    }

    void apply_to(signal_synth::clinical_scenario_config& config) const
    {
        config = static_cast<const signal_synth::clinical_scenario_config&>(*this);
    }

    ZAX_JSON_SERIALIZABLE(
        zax_clinical_scenario_config,
        JSON_PROPERTY_NAME(premature_every_n_beats, "premature_every_n_beats"),
        JSON_PROPERTY_NAME(premature_coupling_ratio, "premature_coupling_ratio"),
        JSON_PROPERTY_NAME(compensatory_pause_ratio, "compensatory_pause_ratio"),
        JSON_PROPERTY_NAME(sinus_pause_every_n_beats, "sinus_pause_every_n_beats"),
        JSON_PROPERTY_NAME(sinus_pause_ratio, "sinus_pause_ratio"),
        JSON_PROPERTY_NAME(episode_start_seconds, "episode_start_seconds"),
        JSON_PROPERTY_NAME(episode_duration_seconds, "episode_duration_seconds"),
        JSON_PROPERTY_NAME(episode_rate_bpm, "episode_rate_bpm"))
};

struct zax_clinical_lead_config: public signal_synth::clinical_lead_config
{
    void set_from(const signal_synth::clinical_lead_config& config)
    {
        static_cast<signal_synth::clinical_lead_config&>(*this) = config;
    }

    void apply_to(signal_synth::clinical_lead_config& config) const
    {
        config = static_cast<const signal_synth::clinical_lead_config&>(*this);
    }

    ZAX_JSON_SERIALIZABLE(
        zax_clinical_lead_config,
        JSON_PROPERTY_NAME(yaw_degrees, "yaw_degrees"),
        JSON_PROPERTY_NAME(pitch_degrees, "pitch_degrees"),
        JSON_PROPERTY_NAME(roll_degrees, "roll_degrees"),
        JSON_PROPERTY_NAME(lead_gain[signal_synth::clinical_lead_i], "lead_i_gain"),
        JSON_PROPERTY_NAME(lead_gain[signal_synth::clinical_lead_ii], "lead_ii_gain"),
        JSON_PROPERTY_NAME(lead_gain[signal_synth::clinical_lead_iii], "lead_iii_gain"),
        JSON_PROPERTY_NAME(lead_gain[signal_synth::clinical_lead_avr], "lead_avr_gain"),
        JSON_PROPERTY_NAME(lead_gain[signal_synth::clinical_lead_avl], "lead_avl_gain"),
        JSON_PROPERTY_NAME(lead_gain[signal_synth::clinical_lead_avf], "lead_avf_gain"),
        JSON_PROPERTY_NAME(lead_gain[signal_synth::clinical_lead_v1], "lead_v1_gain"),
        JSON_PROPERTY_NAME(lead_gain[signal_synth::clinical_lead_v2], "lead_v2_gain"),
        JSON_PROPERTY_NAME(lead_gain[signal_synth::clinical_lead_v3], "lead_v3_gain"),
        JSON_PROPERTY_NAME(lead_gain[signal_synth::clinical_lead_v4], "lead_v4_gain"),
        JSON_PROPERTY_NAME(lead_gain[signal_synth::clinical_lead_v5], "lead_v5_gain"),
        JSON_PROPERTY_NAME(lead_gain[signal_synth::clinical_lead_v6], "lead_v6_gain"))
};

struct zax_clinical_source_section
{
    zax_clinical_source_config source_model;

    void set_from(const signal_synth::clinical_source_config& config)
    {
        source_model.set_from(config);
    }

    void apply_to(signal_synth::clinical_source_config& config) const
    {
        source_model.apply_to(config);
    }

    ZAX_JSON_SERIALIZABLE(zax_clinical_source_section, JSON_PROPERTY_NAME(source_model, "sources"))
};

static std::string zax_json_object_body(const std::string& json)
{
    const std::string::size_type first = json.find('{');
    const std::string::size_type last = json.rfind('}');
    if (first == std::string::npos || last == std::string::npos || last <= first)
        return std::string();
    return json.substr(first + 1, last - first - 1);
}

static void append_zax_json_object_body(std::string& output, const std::string& json)
{
    const std::string body = zax_json_object_body(json);
    if (body.empty())
        return;
    if (!output.empty())
        output += ", ";
    output += body;
}

struct zax_clinical_ecg_config: public signal_synth::clinical_ecg_config
{
    zax_clinical_enum_config enum_model;
    zax_clinical_timing_config timing_model;
    zax_clinical_morphology_config morphology_model;
    zax_clinical_rhythm_config rhythm_model;
    zax_clinical_scenario_config scenario_model;
    zax_clinical_lead_config lead_model;
    zax_clinical_source_section source_section;

    zax_clinical_ecg_config()
    {
        sync_from_config();
    }

    zax_clinical_ecg_config(const char* a_json)
    {
        sync_from_config();
        *this = a_json;
    }

    zax_clinical_ecg_config(const std::string& a_json)
    {
        sync_from_config();
        *this = a_json;
    }

    virtual void zax_from_json(const char* a_json, std::vector<std::string>* a_err_stream = 0)
    {
        sync_from_config();
        enum_model.zax_from_json(a_json, a_err_stream);
        timing_model.zax_from_json(a_json, a_err_stream);
        morphology_model.zax_from_json(a_json, a_err_stream);
        rhythm_model.zax_from_json(a_json, a_err_stream);
        scenario_model.zax_from_json(a_json, a_err_stream);
        lead_model.zax_from_json(a_json, a_err_stream);
        source_section.zax_from_json(a_json, a_err_stream);
        apply_enum_properties();
    }

    virtual int zax_to_json(char* a_json, int a_alloc_size, int a_deep = 0) const
    {
        const std::string json = zax_to_json(a_deep);
        if (!a_json || a_alloc_size <= static_cast<int>(json.size()))
            return 0;
        memcpy(a_json, json.c_str(), json.size() + 1);
        return static_cast<int>(json.size());
    }

    virtual std::string zax_to_json(int a_deep = 0) const
    {
        zax_clinical_enum_config enums;
        zax_clinical_timing_config timing_values;
        zax_clinical_morphology_config morphology_values;
        zax_clinical_rhythm_config rhythm_values;
        zax_clinical_scenario_config scenario_values;
        zax_clinical_lead_config lead_values;
        zax_clinical_source_section source_values;

        enums.set_from(*this);
        timing_values.set_from(timing);
        morphology_values.set_from(morphology);
        rhythm_values.set_from(rhythm);
        scenario_values.set_from(scenario);
        lead_values.set_from(leads);
        source_values.set_from(sources);

        std::string body;
        append_zax_json_object_body(body, enums.zax_to_json(a_deep));
        append_zax_json_object_body(body, timing_values.zax_to_json(a_deep));
        append_zax_json_object_body(body, morphology_values.zax_to_json(a_deep));
        append_zax_json_object_body(body, rhythm_values.zax_to_json(a_deep));
        append_zax_json_object_body(body, scenario_values.zax_to_json(a_deep));
        append_zax_json_object_body(body, lead_values.zax_to_json(a_deep));
        append_zax_json_object_body(body, source_values.zax_to_json(a_deep));
        return std::string("{") + body + "}";
    }

    virtual void operator = (const char* a_json)
    {
        zax_from_json(a_json);
    }

    virtual void operator = (const std::string& a_json)
    {
        zax_from_json(a_json.c_str());
    }

    template <typename T> operator T() const
    {
        return zax_to_json();
    }

    friend std::ostream& operator<<(std::ostream& os, const zax_clinical_ecg_config& a_obj)
    {
        std::string s = a_obj;
        return os << s;
    }

    void apply_enum_properties()
    {
        timing_model.apply_to(timing);
        morphology_model.apply_to(morphology);
        rhythm_model.apply_to(rhythm);
        scenario_model.apply_to(scenario);
        lead_model.apply_to(leads);
        source_section.apply_to(sources);
        enum_model.apply_to(*this);
    }

private:
    void sync_from_config()
    {
        enum_model.set_from(*this);
        timing_model.set_from(timing);
        morphology_model.set_from(morphology);
        rhythm_model.set_from(rhythm);
        scenario_model.set_from(scenario);
        lead_model.set_from(leads);
        source_section.set_from(sources);
    }
};

const char* clinical_qrs_label(signal_synth::clinical_ventricular_origin origin)
{
    switch (origin)
    {
    case signal_synth::clinical_origin_pac:
        return "GT PAC QRS";
    case signal_synth::clinical_origin_pvc:
        return "GT PVC QRS";
    case signal_synth::clinical_origin_junctional_escape:
        return "GT junctional escape QRS";
    case signal_synth::clinical_origin_ventricular_escape:
        return "GT ventricular escape QRS";
    case signal_synth::clinical_origin_paced:
        return "GT paced QRS";
    case signal_synth::clinical_origin_vt:
        return "GT VT QRS";
    case signal_synth::clinical_origin_conducted:
    default:
        return "GT QRS complex";
    }
}

const char* clinical_episode_label(signal_synth::clinical_episode_kind kind)
{
    switch (kind)
    {
    case signal_synth::clinical_episode_psvt:
        return "GT PSVT episode";
    case signal_synth::clinical_episode_svarr:
        return "GT SVARR episode";
    case signal_synth::clinical_episode_repolarization:
        return "GT dynamic repolarization";
    case signal_synth::clinical_episode_none:
    default:
        return "GT episode";
    }
}

const char* clinical_pacing_label(const signal_synth::clinical_pacing_event& event)
{
    if (event.kind == signal_synth::clinical_pacing_event_atrial)
        return event.captured ? "GT atrial pace captured" : "GT atrial pace no capture";
    return event.captured ? "GT ventricular pace captured" : "GT ventricular pace no capture";
}

const char* clinical_dynamic_annotation_label(signal_synth::clinical_dynamic_annotation_kind kind)
{
    switch (kind)
    {
    case signal_synth::clinical_dynamic_repolarization_severity: return "GT repolarization severity";
    case signal_synth::clinical_dynamic_qt_interval_ms: return "GT QT interval ms";
    case signal_synth::clinical_dynamic_qtc_ms: return "GT QTc ms";
    case signal_synth::clinical_dynamic_st_j_amplitude_mv: return "GT ST-J amplitude mV";
    case signal_synth::clinical_dynamic_st_slope_mv_per_second: return "GT ST slope mV/s";
    case signal_synth::clinical_dynamic_t_amplitude_mv: return "GT T amplitude mV";
    }
    return "GT dynamic annotation";
}

void add_clinical_markers(CVariable* output, const signal_synth::clinical_ecg_record& record)
{
    const int marker_lead = signal_synth::clinical_lead_ii;
    const double duration_seconds = static_cast<double>(record.sample_count()) / record.sampling_rate_hz();
    for (unsigned int i = 0; i < record.atrial_event_count(); ++i)
    {
        const signal_synth::clinical_atrial_event& atrial = record.atrial_events()[i];
        if (atrial.visible && atrial.onset_time_seconds >= 0.0 && atrial.offset_time_seconds < duration_seconds)
            put_marker_interval(output, atrial.onset_time_seconds, atrial.offset_time_seconds - atrial.onset_time_seconds, atrial.conducted ? "GT P wave" : "GT non-conducted P wave", marker_lead);
    }
    for (unsigned int i = 0; i < record.beat_count(); ++i)
    {
        const signal_synth::clinical_beat_annotation& beat = record.beats()[i];
        if (beat.qrs_present && beat.qrs_onset_time_seconds >= 0.0 && beat.qrs_offset_time_seconds < duration_seconds)
            put_marker_interval(output, beat.qrs_onset_time_seconds, beat.qrs_offset_time_seconds - beat.qrs_onset_time_seconds, clinical_qrs_label(beat.origin), marker_lead);
        if (beat.qrs_present && beat.j_point_time_seconds >= 0.0 && beat.j_point_time_seconds < duration_seconds)
            put_marker_interval(output, beat.j_point_time_seconds, 5.0 / record.sampling_rate_hz(), "GT J point", marker_lead);
        if (beat.t_present && beat.t_onset_time_seconds >= 0.0 && beat.t_offset_time_seconds < duration_seconds)
            put_marker_interval(output, beat.t_onset_time_seconds, beat.t_offset_time_seconds - beat.t_onset_time_seconds, "GT T wave", marker_lead);
    }
    for (unsigned int i = 0; i < record.episode_count(); ++i)
    {
        const signal_synth::clinical_episode_annotation& episode = record.episodes()[i];
        if (episode.present && episode.start_time_seconds >= 0.0 && episode.end_time_seconds <= duration_seconds)
            put_marker_interval(output, episode.start_time_seconds, episode.end_time_seconds - episode.start_time_seconds, clinical_episode_label(episode.kind), marker_lead);
    }
    for (unsigned int i = 0; i < record.pacing_event_count(); ++i)
    {
        const signal_synth::clinical_pacing_event& event = record.pacing_events()[i];
        if (event.time_seconds >= 0.0 && event.time_seconds < duration_seconds)
            put_marker_interval(output, event.time_seconds, 1.0 / record.sampling_rate_hz(), clinical_pacing_label(event), marker_lead);
    }
    for (unsigned int i = 0; i < record.dynamic_annotation_count(); ++i)
    {
        const signal_synth::clinical_dynamic_annotation& annotation = record.dynamic_annotations()[i];
        if (annotation.present && annotation.time_seconds >= 0.0 && annotation.time_seconds < duration_seconds)
            put_marker_interval(output, annotation.time_seconds, 1.0 / record.sampling_rate_hz(), clinical_dynamic_annotation_label(annotation.kind), marker_lead);
    }
}

unsigned int clinical_annotation_sample(double time_seconds, const signal_synth::clinical_ecg_record& record)
{
    if (time_seconds <= 0.0)
        return 0;
    const unsigned long long sample = static_cast<unsigned long long>(std::llround(time_seconds * record.sampling_rate_hz()));
    return static_cast<unsigned int>(std::min<unsigned long long>(sample, record.sample_count() - 1));
}

void fill_clinical_annotation_interval(CVariable* output, unsigned int channel, double onset, double offset, double value, const signal_synth::clinical_ecg_record& record)
{
    if (onset < 0.0 || offset < onset || onset >= static_cast<double>(record.sample_count()) / record.sampling_rate_hz())
        return;
    const unsigned int first = clinical_annotation_sample(onset, record);
    const unsigned int last = clinical_annotation_sample(offset, record);
    std::fill(output->m_data[channel] + first, output->m_data[channel] + last + 1, value);
}

void add_clinical_annotation_channels_at(CVariable* output, const signal_synth::clinical_ecg_record& record, unsigned int first_channel)
{
    const unsigned int p_channel = first_channel;
    const unsigned int qrs_channel = p_channel + 1;
    const unsigned int j_channel = p_channel + 2;
    const unsigned int t_channel = p_channel + 3;
    const unsigned int episode_channel = p_channel + 4;
    const unsigned int pacing_channel = p_channel + 5;
    const unsigned int dynamic_first_channel = p_channel + 6;
    const unsigned int dynamic_channel_count = 6;
    const unsigned int annotation_channel_count = 6 + dynamic_channel_count;
    const char* labels[] = {"GT P waves (+conducted, -blocked)", "GT QRS origin code", "GT J points", "GT T waves", "GT rhythm episodes", "GT pacing event code", "GT repolarization severity", "GT QT interval ms", "GT QTc ms", "GT ST-J amplitude mV", "GT ST slope mV/s", "GT T amplitude mV"};
    const char* units[] = {"code", "code", "event", "event", "code", "code", "ratio", "ms", "ms", "mV", "mV/s", "mV"};
    for (unsigned int channel = p_channel; channel < p_channel + annotation_channel_count; ++channel)
    {
        output->m_sample_rates.m_data[channel] = record.sampling_rate_hz();
        copy_fixed_string(output->m_labels.m_data[channel].s, labels[channel - p_channel]);
        copy_fixed_string(output->m_vertical_units.m_data[channel].s, units[channel - p_channel]);
        std::fill(output->m_data[channel], output->m_data[channel] + record.sample_count(), 0.0);
    }
    for (unsigned int i = 0; i < record.atrial_event_count(); ++i)
    {
        const signal_synth::clinical_atrial_event& atrial = record.atrial_events()[i];
        if (atrial.visible)
            fill_clinical_annotation_interval(output, p_channel, atrial.onset_time_seconds, atrial.offset_time_seconds, atrial.conducted ? 1.0 : -1.0, record);
    }
    for (unsigned int i = 0; i < record.beat_count(); ++i)
    {
        const signal_synth::clinical_beat_annotation& beat = record.beats()[i];
        if (beat.qrs_present)
        {
            fill_clinical_annotation_interval(output, qrs_channel, beat.qrs_onset_time_seconds, beat.qrs_offset_time_seconds, static_cast<int>(beat.origin) + 1.0, record);
            output->m_data[j_channel][clinical_annotation_sample(beat.j_point_time_seconds, record)] = 1.0;
        }
        if (beat.t_present)
            fill_clinical_annotation_interval(output, t_channel, beat.t_onset_time_seconds, beat.t_offset_time_seconds, 1.0, record);
    }
    for (unsigned int i = 0; i < record.episode_count(); ++i)
    {
        const signal_synth::clinical_episode_annotation& episode = record.episodes()[i];
        if (episode.present)
            fill_clinical_annotation_interval(output, episode_channel, episode.start_time_seconds, episode.end_time_seconds, static_cast<int>(episode.kind), record);
    }
    for (unsigned int i = 0; i < record.pacing_event_count(); ++i)
    {
        const signal_synth::clinical_pacing_event& event = record.pacing_events()[i];
        const double value = (static_cast<int>(event.kind) + 1.0) * (event.captured ? 1.0 : -1.0);
        output->m_data[pacing_channel][clinical_annotation_sample(event.time_seconds, record)] = value;
    }
    for (unsigned int i = 0; i < record.dynamic_annotation_count(); ++i)
    {
        const signal_synth::clinical_dynamic_annotation& annotation = record.dynamic_annotations()[i];
        const unsigned int kind = static_cast<unsigned int>(annotation.kind);
        if (annotation.present && kind < dynamic_channel_count)
        {
            const unsigned int first = clinical_annotation_sample(annotation.time_seconds, record);
            unsigned int last = record.sample_count();
            for (unsigned int next = i + 1; next < record.dynamic_annotation_count(); ++next)
            {
                if (record.dynamic_annotations()[next].kind == annotation.kind)
                {
                    last = clinical_annotation_sample(record.dynamic_annotations()[next].time_seconds, record);
                    break;
                }
            }
            if (first < last)
                std::fill(output->m_data[dynamic_first_channel + kind] + first, output->m_data[dynamic_first_channel + kind] + last, annotation.value);
        }
    }
}

void add_clinical_annotation_channels(CVariable* output, const signal_synth::clinical_ecg_record& record)
{
    add_clinical_annotation_channels_at(output, record, signal_synth::clinical_lead_count);
}

CVariable* create_clinical_ecg_variable(const char* output_name, const signal_synth::clinical_ecg_record& record, clinical_annotation_output annotation_output)
{
    const unsigned int annotation_channel_count = annotation_output == clinical_annotations_channels ? 12U : 0U;
    vector<unsigned int> channel_sizes(signal_synth::clinical_lead_count + annotation_channel_count, record.sample_count());
    CVariable* output = NewCVariable();
    output->Rebuild(channel_sizes.size(), channel_sizes.data());
    for (int lead = 0; lead < signal_synth::clinical_lead_count; ++lead)
    {
        output->m_sample_rates.m_data[lead] = record.sampling_rate_hz();
        copy_fixed_string(output->m_labels.m_data[lead].s, record.lead_name(lead));
        copy_fixed_string(output->m_vertical_units.m_data[lead].s, "mV");
        memcpy(output->m_data[lead], record.lead_data(lead), record.sample_count() * sizeof(double));
    }
    if (annotation_output == clinical_annotations_markers)
        add_clinical_markers(output, record);
    else if (annotation_output == clinical_annotations_channels)
        add_clinical_annotation_channels(output, record);
    copy_fixed_string(output->m_varname, output_name);
    m_variable_list_ref->Insert(output_name, output);
    return output;
}

bool render_has_artifact_ecg(const signal_synth::ecg_render_bundle& render, unsigned int lead)
{
    return render.signal_quality.ecg_leads.size() == signal_synth::clinical_lead_count && lead < render.signal_quality.ecg_leads.size() && render.signal_quality.ecg_leads[lead].size() == render.record.sample_count();
}

const double* rendered_ecg_lead_data(const signal_synth::ecg_render_bundle& render, unsigned int lead)
{
    return render_has_artifact_ecg(render, lead) ? render.signal_quality.ecg_leads[lead].data() : render.record.lead_data(lead);
}

unsigned int rendered_ppg_channel_count(const signal_synth::ecg_render_bundle& render)
{
    return render.ppg.sample_count() == render.record.sample_count() && render.ppg.sampling_rate_hz() == render.record.sampling_rate_hz() ? render.ppg.channel_count() : 0U;
}

const double* rendered_ppg_channel_data(const signal_synth::ecg_render_bundle& render, unsigned int channel)
{
    if (channel < render.signal_quality.ppg_channels.size() && render.signal_quality.ppg_channels[channel].size() == render.record.sample_count())
        return render.signal_quality.ppg_channels[channel].data();
    return render.ppg.channel_samples(channel);
}

string artifact_marker_label(const signal_synth::signal_quality_artifact_interval& artifact)
{
    return string("GT artifact ") + signal_synth::signal_quality_artifact_type_name(artifact.type);
}

void add_artifact_markers(CVariable* output, const signal_synth::ecg_render_bundle& render, unsigned int ppg_channel_count, unsigned int first_ppg_channel)
{
    for (unsigned int i = 0; i < render.signal_quality.artifacts.size(); ++i)
    {
        const signal_synth::signal_quality_artifact_interval& artifact = render.signal_quality.artifacts[i];
        const string label = artifact_marker_label(artifact);
        const double duration = artifact.end_seconds - artifact.start_seconds;
        for (unsigned int lead = 0; lead < signal_synth::clinical_lead_count; ++lead)
            if (artifact.ecg_leads[lead])
                put_marker_interval(output, artifact.start_seconds, duration, label.c_str(), lead);
        if (artifact.ppg)
            for (unsigned int channel = 0; channel < ppg_channel_count; ++channel)
                put_marker_interval(output, artifact.start_seconds, duration, label.c_str(), first_ppg_channel + channel);
    }
}

void add_artifact_annotation_channel(CVariable* output, const signal_synth::ecg_render_bundle& render, unsigned int channel)
{
    output->m_sample_rates.m_data[channel] = render.record.sampling_rate_hz();
    copy_fixed_string(output->m_labels.m_data[channel].s, "GT acquisition artifact code");
    copy_fixed_string(output->m_vertical_units.m_data[channel].s, "code");
    std::fill(output->m_data[channel], output->m_data[channel] + render.record.sample_count(), 0.0);
    for (unsigned int i = 0; i < render.signal_quality.artifacts.size(); ++i)
    {
        const signal_synth::signal_quality_artifact_interval& artifact = render.signal_quality.artifacts[i];
        const double value = static_cast<int>(artifact.type) + 1.0;
        const unsigned int first = static_cast<unsigned int>(std::min<unsigned long long>(artifact.start_sample_index, render.record.sample_count() - 1));
        const unsigned int last = static_cast<unsigned int>(std::min<unsigned long long>(artifact.end_sample_index, render.record.sample_count() - 1));
        if (first <= last)
            std::fill(output->m_data[channel] + first, output->m_data[channel] + last + 1, value);
    }
}

const char* ppg_annotation_label(const signal_synth::ppg_annotation& annotation);

void add_ppg_annotation_channels(CVariable* output, const signal_synth::ppg_record& ppg, unsigned int first_channel)
{
    for (unsigned int ppg_channel = 0; ppg_channel < ppg.channel_count(); ++ppg_channel)
    {
        const string construction_label = string("GT ") + ppg.channel_name(ppg_channel) + " construction";
        const string measurement_label = string("Measured ") + ppg.channel_name(ppg_channel) + " peaks";
        for (unsigned int source = 0; source < 2; ++source)
        {
            const unsigned int output_channel = first_channel + 2U * ppg_channel + source;
            output->m_sample_rates.m_data[output_channel] = ppg.sampling_rate_hz();
            copy_fixed_string(output->m_labels.m_data[output_channel].s, source == 0 ? construction_label.c_str() : measurement_label.c_str());
            copy_fixed_string(output->m_vertical_units.m_data[output_channel].s, "event");
            std::fill(output->m_data[output_channel], output->m_data[output_channel] + ppg.sample_count(), 0.0);
        }
        for (unsigned int i = 0; i < ppg.channel_annotation_count(ppg_channel); ++i)
        {
            const signal_synth::ppg_annotation& annotation = ppg.channel_annotations(ppg_channel)[i];
            const unsigned int source = annotation.source == signal_synth::ppg_fiducial_construction ? 0U : 1U;
            const unsigned int output_channel = first_channel + 2U * ppg_channel + source;
            output->m_data[output_channel][annotation.sample_index] = source == 0 ? static_cast<int>(annotation.kind) + 1.0 : 1.0;
        }
    }
}

void add_ppg_markers(CVariable* output, const signal_synth::ppg_record& ppg, unsigned int first_ppg_channel)
{
    for (unsigned int ppg_channel = 0; ppg_channel < ppg.channel_count(); ++ppg_channel)
        for (unsigned int i = 0; i < ppg.channel_annotation_count(ppg_channel); ++i)
            put_marker_interval(output, ppg.channel_annotations(ppg_channel)[i].time_seconds, 1.0 / ppg.sampling_rate_hz(), ppg_annotation_label(ppg.channel_annotations(ppg_channel)[i]), first_ppg_channel + ppg_channel);
}

CVariable* create_rendered_ecg_variable(const char* output_name, const signal_synth::ecg_render_bundle& render, clinical_annotation_output annotation_output)
{
    const signal_synth::clinical_ecg_record& record = render.record;
    const unsigned int ppg_channel_count = rendered_ppg_channel_count(render);
    const unsigned int annotation_channel_count = annotation_output == clinical_annotations_channels ? 13U + 2U * ppg_channel_count : 0U;
    const unsigned int first_ppg_annotation_channel = signal_synth::clinical_lead_count + 13U;
    const unsigned int first_ppg_channel = signal_synth::clinical_lead_count + annotation_channel_count;
    vector<unsigned int> channel_sizes(signal_synth::clinical_lead_count + annotation_channel_count + ppg_channel_count, record.sample_count());
    CVariable* output = NewCVariable();
    output->Rebuild(channel_sizes.size(), channel_sizes.data());

    for (unsigned int lead = 0; lead < signal_synth::clinical_lead_count; ++lead)
    {
        output->m_sample_rates.m_data[lead] = record.sampling_rate_hz();
        copy_fixed_string(output->m_labels.m_data[lead].s, record.lead_name(lead));
        copy_fixed_string(output->m_vertical_units.m_data[lead].s, "mV");
        memcpy(output->m_data[lead], rendered_ecg_lead_data(render, lead), record.sample_count() * sizeof(double));
    }
    if (annotation_output == clinical_annotations_channels)
    {
        add_clinical_annotation_channels_at(output, record, signal_synth::clinical_lead_count);
        add_artifact_annotation_channel(output, render, signal_synth::clinical_lead_count + 12U);
        add_ppg_annotation_channels(output, render.ppg, first_ppg_annotation_channel);
    }
    for (unsigned int ppg_channel = 0; ppg_channel < ppg_channel_count; ++ppg_channel)
    {
        const unsigned int output_channel = first_ppg_channel + ppg_channel;
        output->m_sample_rates.m_data[output_channel] = record.sampling_rate_hz();
        copy_fixed_string(output->m_labels.m_data[output_channel].s, render.ppg.channel_name(ppg_channel));
        copy_fixed_string(output->m_vertical_units.m_data[output_channel].s, render.ppg.channel_unit(ppg_channel));
        memcpy(output->m_data[output_channel], rendered_ppg_channel_data(render, ppg_channel), record.sample_count() * sizeof(double));
    }
    if (annotation_output == clinical_annotations_markers)
    {
        add_clinical_markers(output, record);
        add_artifact_markers(output, render, ppg_channel_count, first_ppg_channel);
        add_ppg_markers(output, render.ppg, first_ppg_channel);
    }

    copy_fixed_string(output->m_varname, output_name);
    m_variable_list_ref->Insert(output_name, output);
    return output;
}

char* GenerateClinicalECG12(char* output_name, char* number_of_samples_text, char* sampling_rate_text, char* parameters_json, char* annotation_output_text)
{
    if (!output_name || !number_of_samples_text || !sampling_rate_text || !parameters_json)
        return MakeString(NewChar, "ERROR: GenerateClinicalECG12: not enough arguments.");

    const unsigned long parsed_samples = strtoul(number_of_samples_text, nullptr, 10);
    const unsigned long parsed_sampling_rate = strtoul(sampling_rate_text, nullptr, 10);
    if (parsed_samples == 0 || parsed_samples > std::numeric_limits<unsigned int>::max() || parsed_sampling_rate == 0 || parsed_sampling_rate > std::numeric_limits<unsigned int>::max())
        return MakeString(NewChar, "ERROR: GenerateClinicalECG12: invalid sample count or sampling rate.");
    clinical_annotation_output annotation_output;
    if (!parse_clinical_annotation_output(annotation_output_text, annotation_output))
        return MakeString(NewChar, "ERROR: GenerateClinicalECG12: annotation output must be 1 (markers), 2 (channels), or 3 (none).");

    zax_clinical_ecg_config config = parameters_json;
    config.apply_enum_properties();
    config.sampling_rate_hz = static_cast<unsigned int>(parsed_sampling_rate);
    signal_synth::clinical_ecg_generator generator(config);
    signal_synth::clinical_ecg_record record;
    const unsigned int sample_count = static_cast<unsigned int>(parsed_samples);
    if (!generator.valid() || !generator.generate(sample_count, record))
        return MakeString(NewChar, "ERROR: GenerateClinicalECG12: invalid parameters or generation failed.");

    create_clinical_ecg_variable(output_name, record, annotation_output);
    return 0;
}

const char* ppg_annotation_label(const signal_synth::ppg_annotation& annotation)
{
    if (annotation.source == signal_synth::ppg_fiducial_measurement)
        return "Measured PPG systolic peak";
    switch (annotation.kind)
    {
    case signal_synth::ppg_pulse_onset: return "GT PPG pulse onset";
    case signal_synth::ppg_systolic_peak: return "GT PPG systolic peak";
    case signal_synth::ppg_dicrotic_feature: return "GT PPG dicrotic feature";
    case signal_synth::ppg_pulse_offset: return "GT PPG pulse offset";
    }
    return "GT PPG fiducial";
}

struct zax_ecg_qa_scenario
{
    vector<string> conditions;
    vector<double> severities;
    double heart_rate_bpm = 0.0;
    double rr_variability_seconds = 0.0;
    unsigned long long seed = 0x5343454e4152494fULL;
    unsigned int ectopic_every_n_beats = 0;
    int second_degree_pattern = signal_synth::ecg_second_degree_unspecified;
    int q_wave_territory = signal_synth::ecg_q_wave_unspecified;
    int episode_type = signal_synth::ecg_episode_none;
    double episode_start_seconds = 2.0;
    double episode_duration_seconds = 4.0;
    double episode_rate_bpm = 170.0;
    int fidelity_policy = signal_synth::ecg_fidelity_allow_parameterized;

    ZAX_JSON_SERIALIZABLE(zax_ecg_qa_scenario, JSON_PROPERTY(conditions), JSON_PROPERTY(severities), JSON_PROPERTY(heart_rate_bpm), JSON_PROPERTY(rr_variability_seconds), JSON_PROPERTY(seed), JSON_PROPERTY(ectopic_every_n_beats), JSON_PROPERTY(second_degree_pattern), JSON_PROPERTY(q_wave_territory), JSON_PROPERTY(episode_type), JSON_PROPERTY(episode_start_seconds), JSON_PROPERTY(episode_duration_seconds), JSON_PROPERTY(episode_rate_bpm), JSON_PROPERTY(fidelity_policy))
};

CVariable* create_clinical_source_variable(const char* output_name, const signal_synth::clinical_ecg_record& record)
{
    const unsigned int channel_count = signal_synth::clinical_axis_count * (signal_synth::clinical_source_count + 1);
    vector<unsigned int> channel_sizes(channel_count, record.sample_count());
    CVariable* output = NewCVariable();
    output->Rebuild(channel_count, channel_sizes.data());
    const char* axis_names[signal_synth::clinical_axis_count] = {"X", "Y", "Z"};
    unsigned int channel = 0;
    for (int axis = 0; axis < signal_synth::clinical_axis_count; ++axis, ++channel)
    {
        const string label = string("VCG ") + axis_names[axis];
        output->m_sample_rates.m_data[channel] = record.sampling_rate_hz();
        copy_fixed_string(output->m_labels.m_data[channel].s, label.c_str());
        copy_fixed_string(output->m_vertical_units.m_data[channel].s, "mV");
        memcpy(output->m_data[channel], record.vcg_data(axis), record.sample_count() * sizeof(double));
    }
    for (unsigned int source = 0; source < record.source_count(); ++source)
    {
        for (int axis = 0; axis < signal_synth::clinical_axis_count; ++axis, ++channel)
        {
            const string label = string(record.source_name(source)) + " " + axis_names[axis];
            output->m_sample_rates.m_data[channel] = record.sampling_rate_hz();
            copy_fixed_string(output->m_labels.m_data[channel].s, label.c_str());
            copy_fixed_string(output->m_vertical_units.m_data[channel].s, "mV");
            memcpy(output->m_data[channel], record.source_data(source, axis), record.sample_count() * sizeof(double));
        }
    }
    copy_fixed_string(output->m_varname, output_name);
    m_variable_list_ref->Insert(output_name, output);
    return output;
}

CVariable* create_ecg_assertion_variable(const char* output_name, const signal_synth::clinical_ecg_record& record, const signal_synth::ecg_scenario_report& report)
{
    vector<unsigned int> channel_sizes(report.assertion_count(), record.sample_count());
    CVariable* output = NewCVariable();
    output->Rebuild(report.assertion_count(), channel_sizes.data());
    for (unsigned int assertion = 0; assertion < report.assertion_count(); ++assertion)
    {
        const bool passed = report.assertion_status(assertion) == signal_synth::ecg_assertion_passed;
        const string label = string(passed ? "PASS " : "FAIL ") + report.assertion_name(assertion);
        output->m_sample_rates.m_data[assertion] = record.sampling_rate_hz();
        copy_fixed_string(output->m_labels.m_data[assertion].s, label.c_str());
        copy_fixed_string(output->m_vertical_units.m_data[assertion].s, report.assertion_unit(assertion));
        std::fill(output->m_data[assertion], output->m_data[assertion] + record.sample_count(), report.assertion_measured_value(assertion));
    }
    copy_fixed_string(output->m_varname, output_name);
    m_variable_list_ref->Insert(output_name, output);
    return output;
}

char* GenerateECGQAScenario(char* ecg_output_name, char* source_output_name, char* assertion_output_name, char* number_of_samples_text, char* sampling_rate_text, char* parameters_json, char* annotation_output_text)
{
    if (!ecg_output_name || !source_output_name || !assertion_output_name || !number_of_samples_text || !sampling_rate_text || !parameters_json)
        return MakeString(NewChar, "ERROR: GenerateECGQAScenario: not enough arguments.");
    const unsigned long parsed_samples = strtoul(number_of_samples_text, nullptr, 10);
    const unsigned long parsed_sampling_rate = strtoul(sampling_rate_text, nullptr, 10);
    if (parsed_samples == 0 || parsed_samples > std::numeric_limits<unsigned int>::max() || parsed_sampling_rate < 100 || parsed_sampling_rate > std::numeric_limits<unsigned int>::max())
        return MakeString(NewChar, "ERROR: GenerateECGQAScenario: invalid sample count or sampling rate.");
    clinical_annotation_output annotation_output;
    if (!parse_clinical_annotation_output(annotation_output_text, annotation_output))
        return MakeString(NewChar, "ERROR: GenerateECGQAScenario: annotation output must be 1 (markers), 2 (channels), or 3 (none).");

    zax_ecg_qa_scenario parameters = parameters_json;
    if (parameters.conditions.empty() || (!parameters.severities.empty() && parameters.severities.size() != parameters.conditions.size()))
        return MakeString(NewChar, "ERROR: GenerateECGQAScenario: conditions are required and severities must be empty or match their count.");
    signal_synth::ecg_qa_scenario scenario;
    if (!scenario.set_sampling_rate_hz(static_cast<unsigned int>(parsed_sampling_rate)) || !scenario.set_heart_rate_bpm(parameters.heart_rate_bpm) || !scenario.set_rr_variability_seconds(parameters.rr_variability_seconds) || !scenario.set_ectopic_every_n_beats(parameters.ectopic_every_n_beats) || !scenario.set_second_degree_av_pattern(static_cast<signal_synth::ecg_second_degree_av_pattern>(parameters.second_degree_pattern)) || !scenario.set_q_wave_territory(static_cast<signal_synth::ecg_q_wave_territory>(parameters.q_wave_territory)) || !scenario.set_episode_type(static_cast<signal_synth::ecg_episode_type>(parameters.episode_type)) || !scenario.set_episode_start_seconds(parameters.episode_start_seconds) || !scenario.set_episode_duration_seconds(parameters.episode_duration_seconds) || !scenario.set_episode_rate_bpm(parameters.episode_rate_bpm) || !scenario.set_fidelity_policy(static_cast<signal_synth::ecg_scenario_fidelity_policy>(parameters.fidelity_policy)))
        return MakeString(NewChar, "ERROR: GenerateECGQAScenario: invalid scenario parameters.");
    scenario.set_seed(parameters.seed);
    for (unsigned int index = 0; index < parameters.conditions.size(); ++index)
    {
        const signal_synth::ecg_condition_info* condition = signal_synth::find_ecg_condition(parameters.conditions[index].c_str());
        const double severity = parameters.severities.empty() ? 1.0 : parameters.severities[index];
        if (!condition || !scenario.add_condition(condition->code, severity))
            return MakeString(NewChar, "ERROR: GenerateECGQAScenario: unknown condition or invalid severity.");
    }

    signal_synth::clinical_ecg_record record;
    signal_synth::ecg_scenario_report report;
    signal_synth::ecg_scenario_engine engine;
    if (!engine.generate(scenario, static_cast<unsigned int>(parsed_samples), record, report))
        return MakeString(NewChar, "ERROR: GenerateECGQAScenario: ", report.issue_count() ? report.issue_message(0) : "generation failed.");
    create_clinical_ecg_variable(ecg_output_name, record, annotation_output);
    create_clinical_source_variable(source_output_name, record);
    create_ecg_assertion_variable(assertion_output_name, record, report);
    return 0;
}

bool render_scenario_json(const char* scenario_json, signal_synth::ecg_render_bundle& render, string& error)
{
    signal_synth::ecg_scenario_document document;
    signal_synth::ecg_scenario_json_result json_result;
    if (!signal_synth::parse_ecg_scenario_json(scenario_json, document, json_result))
    {
        error = json_result.messages.empty() ? "scenario JSON parsing failed" : json_result.messages[0].path + ": " + json_result.messages[0].message;
        return false;
    }
    signal_synth::ecg_document_render_result render_result;
    if (!signal_synth::render_ecg_document(document, render, render_result))
    {
        error = render_result.messages.empty() ? "render failed" : render_result.messages[0];
        return false;
    }
    return true;
}

char* GenerateECGScenarioJSON(char* output_name, char* scenario_json, char* annotation_output_text)
{
    if (!output_name || !scenario_json)
        return MakeString(NewChar, "ERROR: GenerateECGScenarioJSON: not enough arguments.");
    clinical_annotation_output annotation_output;
    if (!parse_clinical_annotation_output(annotation_output_text, annotation_output))
        return MakeString(NewChar, "ERROR: GenerateECGScenarioJSON: annotation output must be 1 (markers), 2 (channels), or 3 (none).");

    signal_synth::ecg_render_bundle render;
    string error;
    if (!render_scenario_json(scenario_json, render, error))
        return MakeString(NewChar, "ERROR: GenerateECGScenarioJSON: ", error.c_str());

    create_rendered_ecg_variable(output_name, render, annotation_output);
    return 0;
}

double optical_truth_value(const signal_synth::ppg_optical_pulse_state& state, unsigned int channel)
{
    if (channel == 0U) return state.spo2_percent;
    if (channel == 1U) return state.ratio_of_ratios;
    if (channel == 2U) return state.red_perfusion_index_percent;
    return state.infrared_perfusion_index_percent;
}

CVariable* create_ppg_optical_truth_variable(const signal_synth::ecg_render_bundle& render)
{
    const signal_synth::ppg_optical_pulse_state* states = render.ppg.optical_states();
    const unsigned int state_count = render.ppg.optical_state_count();
    if (!states || !state_count || !render.record.sample_count()) return 0;
    const unsigned int channel_count = 5U;
    vector<unsigned int> sizes(channel_count, render.record.sample_count());
    CVariable* output = NewCVariable();
    output->Rebuild(channel_count, sizes.data());
    const char* labels[] = {"GT SpO2 target", "GT red/IR ratio R", "GT red perfusion index", "GT infrared perfusion index", "GT optical measurement valid"};
    const char* units[] = {"%", "ratio", "%", "%", "bool"};
    for (unsigned int channel = 0; channel < channel_count; ++channel)
    {
        output->m_sample_rates.m_data[channel] = render.record.sampling_rate_hz();
        copy_fixed_string(output->m_labels.m_data[channel].s, labels[channel]);
        copy_fixed_string(output->m_vertical_units.m_data[channel].s, units[channel]);
    }
    unsigned int right = 0U;
    for (unsigned int sample = 0; sample < render.record.sample_count(); ++sample)
    {
        const double time = static_cast<double>(sample) / render.record.sampling_rate_hz();
        while (right < state_count && states[right].time_seconds < time) ++right;
        const unsigned int left = right == 0U ? 0U : right - 1U;
        const unsigned int bounded_right = right < state_count ? right : state_count - 1U;
        double weight = 0.0;
        if (bounded_right != left && states[bounded_right].time_seconds > states[left].time_seconds)
            weight = (time - states[left].time_seconds) / (states[bounded_right].time_seconds - states[left].time_seconds);
        weight = std::max(0.0, std::min(1.0, weight));
        for (unsigned int channel = 0; channel < 4U; ++channel)
            output->m_data[channel][sample] = optical_truth_value(states[left], channel) + weight * (optical_truth_value(states[bounded_right], channel) - optical_truth_value(states[left], channel));
        output->m_data[4][sample] = states[left].valid_for_measurement && states[bounded_right].valid_for_measurement ? 1.0 : 0.0;
    }
    return output;
}

char* GeneratePPGOpticalScenarioJSON(char* signal_output_name, char* truth_output_name, char* scenario_json, char* annotation_output_text)
{
    if (!signal_output_name || !signal_output_name[0] || !truth_output_name || !truth_output_name[0] || !scenario_json)
        return MakeString(NewChar, "ERROR: GeneratePPGOpticalScenarioJSON: not enough arguments.");
    if (strcmp(signal_output_name, truth_output_name) == 0)
        return MakeString(NewChar, "ERROR: GeneratePPGOpticalScenarioJSON: output names must differ.");
    clinical_annotation_output annotation_output;
    if (!parse_clinical_annotation_output(annotation_output_text, annotation_output))
        return MakeString(NewChar, "ERROR: GeneratePPGOpticalScenarioJSON: annotation output must be 1 (markers), 2 (channels), or 3 (none).");
    signal_synth::ecg_render_bundle render;
    string error;
    if (!render_scenario_json(scenario_json, render, error))
        return MakeString(NewChar, "ERROR: GeneratePPGOpticalScenarioJSON: ", error.c_str());
    if (!render.ppg.optical_enabled())
        return MakeString(NewChar, "ERROR: GeneratePPGOpticalScenarioJSON: ppg.optical must be enabled.");
    CVariable* truth = create_ppg_optical_truth_variable(render);
    if (!truth)
        return MakeString(NewChar, "ERROR: GeneratePPGOpticalScenarioJSON: output variable creation failed.");
    CVariable* signals = create_rendered_ecg_variable(signal_output_name, render, annotation_output);
    if (!signals)
        return MakeString(NewChar, "ERROR: GeneratePPGOpticalScenarioJSON: output variable creation failed.");
    copy_fixed_string(truth->m_varname, truth_output_name);
    m_variable_list_ref->Insert(truth_output_name, truth);
    return 0;
}

struct wearable_display_channel
{
    const signal_synth::wearable_stream_record* stream;
    unsigned int source_channel;
};

void add_wearable_packet_markers(CVariable* output, unsigned int output_channel, const signal_synth::wearable_stream_record& stream)
{
    for (unsigned int i = 0; i < stream.packets.size(); ++i)
    {
        const signal_synth::wearable_packet_annotation& packet = stream.packets[i];
        if (packet.dropped)
            put_marker_interval(output, static_cast<double>(packet.first_sample_index) / stream.config.sample_rate_hz, static_cast<double>(packet.sample_count) / stream.config.sample_rate_hz, "GT dropped device packet", output_channel);
    }
}

CVariable* create_wearable_signal_variable(const signal_synth::wearable_timebase_record& wearable)
{
    vector<wearable_display_channel> channels;
    const signal_synth::wearable_stream_record* ecg = wearable.stream(signal_synth::wearable_stream_ecg);
    const signal_synth::wearable_stream_record* ppg = wearable.stream(signal_synth::wearable_stream_ppg);
    const signal_synth::wearable_stream_record* accelerometer = wearable.stream(signal_synth::wearable_stream_accelerometer);
    if (ecg && ecg->channel_count() > signal_synth::clinical_lead_ii)
        channels.push_back(wearable_display_channel{ecg, signal_synth::clinical_lead_ii});
    if (ppg)
        for (unsigned int channel = 0; channel < ppg->channel_count(); ++channel)
            channels.push_back(wearable_display_channel{ppg, channel});
    if (accelerometer && accelerometer->channel_count())
        channels.push_back(wearable_display_channel{accelerometer, 0U});
    if (channels.empty())
        return 0;
    vector<unsigned int> sizes(channels.size(), 0U);
    for (unsigned int channel = 0; channel < channels.size(); ++channel)
        sizes[channel] = channels[channel].stream->sample_count();
    CVariable* output = NewCVariable();
    output->Rebuild(channels.size(), sizes.data());
    for (unsigned int channel = 0; channel < channels.size(); ++channel)
    {
        const signal_synth::wearable_stream_record& stream = *channels[channel].stream;
        output->m_sample_rates.m_data[channel] = stream.config.sample_rate_hz;
        const string label = string("Wearable ") + signal_synth::wearable_stream_kind_name(stream.kind) + " " + stream.channel_names[channels[channel].source_channel];
        copy_fixed_string(output->m_labels.m_data[channel].s, label.c_str());
        copy_fixed_string(output->m_vertical_units.m_data[channel].s, stream.channel_units[channels[channel].source_channel].c_str());
        for (unsigned int sample = 0; sample < stream.sample_count(); ++sample)
            output->m_data[channel][sample] = stream.samples[sample].received ? stream.channel_samples[channels[channel].source_channel][sample] : 0.0;
        add_wearable_packet_markers(output, channel, stream);
    }
    return output;
}

CVariable* create_wearable_timing_variable(const signal_synth::wearable_timebase_record& wearable)
{
    if (wearable.streams.empty())
        return 0;
    vector<unsigned int> sizes(wearable.streams.size() * 2U, 0U);
    for (unsigned int stream = 0; stream < wearable.streams.size(); ++stream)
        sizes[2U * stream] = sizes[2U * stream + 1U] = wearable.streams[stream].sample_count();
    CVariable* output = NewCVariable();
    output->Rebuild(sizes.size(), sizes.data());
    for (unsigned int stream_index = 0; stream_index < wearable.streams.size(); ++stream_index)
    {
        const signal_synth::wearable_stream_record& stream = wearable.streams[stream_index];
        const unsigned int error_channel = 2U * stream_index;
        const unsigned int availability_channel = error_channel + 1U;
        output->m_sample_rates.m_data[error_channel] = stream.config.sample_rate_hz;
        output->m_sample_rates.m_data[availability_channel] = stream.config.sample_rate_hz;
        const string prefix = string("Wearable ") + signal_synth::wearable_stream_kind_name(stream.kind);
        copy_fixed_string(output->m_labels.m_data[error_channel].s, (prefix + " timestamp error").c_str());
        copy_fixed_string(output->m_labels.m_data[availability_channel].s, (prefix + " packet availability").c_str());
        copy_fixed_string(output->m_vertical_units.m_data[error_channel].s, "ms");
        copy_fixed_string(output->m_vertical_units.m_data[availability_channel].s, "bool");
        for (unsigned int sample = 0; sample < stream.sample_count(); ++sample)
        {
            output->m_data[error_channel][sample] = 1000.0 * (stream.samples[sample].reported_device_time_seconds - stream.samples[sample].latent_time_seconds);
            output->m_data[availability_channel][sample] = stream.samples[sample].received ? 1.0 : 0.0;
        }
        add_wearable_packet_markers(output, availability_channel, stream);
    }
    return output;
}

char* GenerateWearableScenarioJSON(char* signal_output_name, char* timing_output_name, char* scenario_json)
{
    if (!signal_output_name || !signal_output_name[0] || !timing_output_name || !timing_output_name[0] || !scenario_json)
        return MakeString(NewChar, "ERROR: GenerateWearableScenarioJSON: not enough arguments.");
    if (strcmp(signal_output_name, timing_output_name) == 0)
        return MakeString(NewChar, "ERROR: GenerateWearableScenarioJSON: output names must differ.");
    signal_synth::ecg_render_bundle render;
    string error;
    if (!render_scenario_json(scenario_json, render, error))
        return MakeString(NewChar, "ERROR: GenerateWearableScenarioJSON: ", error.c_str());
    if (render.wearable.streams.empty())
        return MakeString(NewChar, "ERROR: GenerateWearableScenarioJSON: wearable render failed or no wearable streams were configured.");
    CVariable* signals = create_wearable_signal_variable(render.wearable);
    CVariable* timing = create_wearable_timing_variable(render.wearable);
    if (!signals || !timing)
        return MakeString(NewChar, "ERROR: GenerateWearableScenarioJSON: output variable creation failed.");
    copy_fixed_string(signals->m_varname, signal_output_name);
    copy_fixed_string(timing->m_varname, timing_output_name);
    m_variable_list_ref->Insert(signal_output_name, signals);
    m_variable_list_ref->Insert(timing_output_name, timing);
    return 0;
}

extern "C"
{
    char* __declspec (dllexport) Procedure(int a_procindx, char* a_statement_body, char* a_param1, char* a_param2, char* a_param3, char* a_param4, char* a_param5, char* a_param6, char* a_param7, char* a_param8, char* a_param9, char* a_param10, char* a_param11, char* a_param12)
    {
        switch (a_procindx)
        {
        case 0:
            return RDetectionTest(a_param1, a_param2, a_param3);
        case 1:
            return FilterRSPT(a_param1, a_param2);
        case 2:
            return Delay(a_param1, a_param2);
        case 3:
            return create_filter_iir(a_param1, a_param2, a_param3, a_param4, a_param5, a_param6, a_param7);
        case 4:
            return Convolve(a_param1, a_param2, a_param3);
        case 5:
            return detect_spikes_rspt(a_param1, a_param2, a_param3, a_param4, a_param5);
        case 6:
            return analyse_ecg_detect_peaks(a_param1, a_param2, a_param3, a_param4);
        case 7:
            return CreateFIRFilter(a_param1, a_param2, a_param3, a_param4, a_param5, a_param6);
        case 8:
            return GenerateSyntheticECG(a_param1, a_param2, a_param3, a_param4, a_param5);
        case 9:
            return GenerateClinicalECG12(a_param1, a_param2, a_param3, a_param4, a_param5);
        case 10:
            return GenerateECGQAScenario(a_param1, a_param2, a_param3, a_param4, a_param5, a_param6, a_param7);
        case 11:
            return GenerateECGScenarioJSON(a_param1, a_param2, a_param3);
        case 12:
            return GenerateWearableScenarioJSON(a_param1, a_param2, a_param3);
        case 13:
            return GeneratePPGOpticalScenarioJSON(a_param1, a_param2, a_param3, a_param4);
        }
        return 0;
    }

    void __declspec (dllexport) InitLib(CSignalCodec_List* a_datalist, CVariable_List* a_variablelist, NewCVariable_Proc a_newCVariable, NewChar_Proc a_newChar, Call_Proc a_inpCall, FFT_PROC a_inpFFT, RFFT_PROC a_inpRFFT, FFTEXEC_PROC a_inpFFTEXEC, FFTDESTROY_PROC a_inpFFTDESTROY)
    {
        m_variable_list_ref = a_variablelist;
        m_data_list_ref     = a_datalist;
        NewChar             = a_newChar;
        NewCVariable        = a_newCVariable;
        FFT                 = a_inpFFT;
        FFTEXEC             = a_inpFFTEXEC;
        RFFT                = a_inpRFFT;
        FFTDESTROY          = a_inpFFTDESTROY;
        Call                = a_inpCall;
    }

    int __declspec (dllexport) GetProcedureList(TFunctionLibrary* a_functionlibrary_reference)
    {
        CStringVec FunctionList;
        FunctionList.AddElement("RDetectionTest(data, value)");
        FunctionList.AddElement("FilterRSPT(data, filetr)");
        FunctionList.AddElement("Delay(a_ecg_signal_name, a_value)");
        FunctionList.AddElement("create_filter_iir(a_outdataname, a_kind, a_type, a_order, a_sampling_rate, a_cutoff_low, a_cutoff_high)");
        FunctionList.AddElement("Convolve(a_dst_name, a_src_name, a_kernel_name)");
        FunctionList.AddElement("detect_spikes_rspt(a_dst_name, a_spike_signal, a_marker_val, a_previous_spike_reference_ratio, a_previous_spike_reference_attenuation)");
        FunctionList.AddElement("analyse_ecg_detect_peaks(data_name,  annotations_signal_name, chindx, analysis_peak_indx)");
        FunctionList.AddElement("CreateFIRFilter(outdataname, type, sampling_rate, kernel_size, cutoff1, cutoff2)");
        FunctionList.AddElement("GenerateSyntheticECG(outdataname, number_of_samples, sampling_rate, parameters, annotation_output)");
        FunctionList.AddElement("GenerateClinicalECG12(outdataname, number_of_samples, sampling_rate, parameters, annotation_output)");
        FunctionList.AddElement("GenerateECGQAScenario(ecg_outdataname, source_outdataname, assertion_outdataname, number_of_samples, sampling_rate, parameters, annotation_output)");
        FunctionList.AddElement("GenerateECGScenarioJSON(outdataname, scenario_json, annotation_output)");
        FunctionList.AddElement("GenerateWearableScenarioJSON(signal_outdataname, timing_outdataname, scenario_json)");
        FunctionList.AddElement("GeneratePPGOpticalScenarioJSON(signal_outdataname, truth_outdataname, scenario_json, annotation_output)");
        a_functionlibrary_reference->ParseFunctionList(&FunctionList);
        return FunctionList.m_size;
    }

    bool __declspec (dllexport) CopyrightInfo(CStringMx* a_copyrightinfo)
    {
        a_copyrightinfo->RebuildPreserve(a_copyrightinfo->m_size + 1);
        a_copyrightinfo->m_data[a_copyrightinfo->m_size - 1]->AddElement("This library is a part of the DB's basic signal processing libraries.");
        return 0;
    }

    bool __stdcall DllMain(int hInst, int reason, int reserved)
    {
        return true;
    }
}
