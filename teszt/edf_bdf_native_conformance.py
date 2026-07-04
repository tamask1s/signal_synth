#!/usr/bin/env python3
import argparse
import os
import shutil
import subprocess
import sys


def run_render(cli, scenario, out_dir):
    if os.path.exists(out_dir):
        shutil.rmtree(out_dir)
    completed = subprocess.run([cli, "render", scenario, "--out", out_dir], stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    if completed.returncode != 0 or completed.stderr:
        raise RuntimeError("render failed for {}: {}".format(scenario, completed.stderr))


def validate_file(pyedflib, path, expect_ppg):
    reader = pyedflib.EdfReader(path)
    try:
        labels = list(reader.getSignalLabels())
        expected_waveform_count = 13 if expect_ppg else 12
        expected_labels = ["I", "II", "III", "aVR", "aVL", "aVF", "V1", "V2", "V3", "V4", "V5", "V6"]
        if labels[:12] != expected_labels:
            raise AssertionError("{} labels start with {}".format(path, labels[:12]))
        if expect_ppg and "ppg_green" not in labels:
            raise AssertionError("{} missing ppg_green".format(path))
        if reader.signals_in_file != expected_waveform_count:
            raise AssertionError("{} signal count {} != {}".format(path, reader.signals_in_file, expected_waveform_count))
        samples = list(reader.getNSamples())
        frequencies = list(reader.getSampleFrequencies())
        if len(samples) != expected_waveform_count or len(frequencies) != expected_waveform_count:
            raise AssertionError("{} sample metadata length mismatch".format(path))
        if not all(sample == samples[0] for sample in samples):
            raise AssertionError("{} channel sample counts differ".format(path))
        if not all(abs(float(freq) - float(frequencies[0])) < 1e-9 for freq in frequencies):
            raise AssertionError("{} channel sample rates differ".format(path))
        annotations = reader.readAnnotations()
        annotation_text = " ".join(str(item) for item in annotations)
        if "beat:normal" not in annotation_text:
            raise AssertionError("{} missing beat annotation".format(path))
        if expect_ppg and "ppg_systolic_peak" not in annotation_text:
            raise AssertionError("{} missing PPG annotation".format(path))
    finally:
        if hasattr(reader, "close"):
            reader.close()
        elif hasattr(reader, "_close"):
            reader._close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cli", required=True)
    parser.add_argument("--ecg-example", required=True)
    parser.add_argument("--ppg-example", required=True)
    parser.add_argument("--work-dir", required=True)
    args = parser.parse_args()

    try:
        import pyedflib
    except Exception:
        print("Skipping native EDF/BDF conformance: pyedflib not available")
        return 0

    if not os.path.exists(args.work_dir):
        os.makedirs(args.work_dir)
    cases = [("ecg_only", args.ecg_example, False), ("ecg_ppg", args.ppg_example, True)]
    for case_id, scenario, expect_ppg in cases:
        out_dir = os.path.join(args.work_dir, case_id)
        run_render(args.cli, scenario, out_dir)
        validate_file(pyedflib, os.path.join(out_dir, "synsigra.edf"), expect_ppg)
        validate_file(pyedflib, os.path.join(out_dir, "synsigra.bdf"), expect_ppg)
    return 0


if __name__ == "__main__":
    sys.exit(main())
