import json
import os
import shutil
import subprocess
import tempfile

from .detections import DetectionDocument
from .delineation import DelineationDocument
from .intervals import IntervalDocument


class _TemporaryDirectory(object):
    def __init__(self):
        self.name = tempfile.mkdtemp(prefix="synsigra_score_")

    def cleanup(self):
        if self.name and os.path.exists(self.name):
            shutil.rmtree(self.name)
        self.name = None


class ScoreReport(object):
    def __init__(self, output_dir, summary, json_report=None, csv_report="", html_report="", tempdir=None):
        self.output_dir = output_dir
        self.summary = summary
        self.json = json_report or {}
        self.csv = csv_report
        self.html = html_report
        self._tempdir = tempdir

    def write(self, output_dir):
        if os.path.exists(output_dir):
            raise OSError("destination already exists: %s" % output_dir)
        shutil.copytree(self.output_dir, output_dir)
        return output_dir

    def close(self):
        if self._tempdir is not None:
            self._tempdir.cleanup()
            self._tempdir = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.close()


def compare_rpeaks(case, detections, out_dir=None, cli_path=None, tolerance_ms=None):
    return _compare("r_peak", case, detections, out_dir, cli_path, tolerance_ms)


def compare_ppg_peaks(case, detections, out_dir=None, cli_path=None, tolerance_ms=None):
    return _compare("ppg_systolic_peak", case, detections, out_dir, cli_path, tolerance_ms)


def compare_ppg_onsets(case, detections, out_dir=None, cli_path=None, tolerance_ms=None):
    return _compare("ppg_pulse_onset", case, detections, out_dir, cli_path, tolerance_ms)


def compare_beat_classes(case, detections, out_dir=None, cli_path=None, tolerance_ms=None):
    return _compare("ecg_beat_classification", case, detections, out_dir, cli_path, tolerance_ms)


def score_hrv(case, user_output, out_dir=None, cli_path=None):
    case.package.ensure_integrity_verified()
    tempdir = None
    if out_dir is None or isinstance(user_output, dict):
        tempdir = _TemporaryDirectory()
    if out_dir is None:
        out_dir = os.path.join(tempdir.name, "hrv_score")
    if isinstance(user_output, dict):
        user_output_path = os.path.join(tempdir.name, "hrv_output.json")
        with open(user_output_path, "w") as handle:
            json.dump(user_output, handle, sort_keys=True, separators=(",", ":"))
    elif isinstance(user_output, str):
        user_output_path = user_output
    else:
        if tempdir is not None:
            tempdir.cleanup()
        raise TypeError("user_output must be a JSON file path or dict")
    summary = _run([_cli(cli_path), "hrv", "score", case.scenario_path, user_output_path, "--out", out_dir])
    return ScoreReport(
        out_dir,
        summary,
        _read_json(os.path.join(out_dir, "hrv_score.json")),
        _read_text(os.path.join(out_dir, "hrv_score.csv")),
        _read_text(os.path.join(out_dir, "hrv_score_report.html")),
        tempdir,
    )


def score_intervals(case, intervals, out_dir=None, cli_path=None, minimum_iou=None):
    if not isinstance(intervals, IntervalDocument):
        raise TypeError("intervals must be an IntervalDocument")
    if intervals.target not in ("rhythm_episode", "signal_quality"):
        raise ValueError("interval target must be rhythm_episode or signal_quality")
    case.package.ensure_integrity_verified()
    tempdir = None
    if out_dir is None:
        tempdir = _TemporaryDirectory()
        out_dir = os.path.join(tempdir.name, "interval_score")
    command = [_cli(cli_path), "interval", "score", intervals.target, case.scenario_path, intervals.path, "--out", out_dir]
    if minimum_iou is not None:
        command.extend(["--minimum-iou", str(minimum_iou)])
    summary = _run(command)
    return ScoreReport(
        out_dir,
        summary,
        _read_json(os.path.join(out_dir, "interval_score.json")),
        _read_text(os.path.join(out_dir, "interval_score.csv")),
        _read_text(os.path.join(out_dir, "interval_score_report.html")),
        tempdir,
    )


def score_rhythm_episodes(case, intervals, out_dir=None, cli_path=None, minimum_iou=None):
    if not isinstance(intervals, IntervalDocument) or intervals.target != "rhythm_episode":
        raise TypeError("intervals must be a rhythm_episode IntervalDocument")
    return score_intervals(case, intervals, out_dir, cli_path, minimum_iou)


def score_signal_quality(case, intervals, out_dir=None, cli_path=None, minimum_iou=None):
    if not isinstance(intervals, IntervalDocument) or intervals.target != "signal_quality":
        raise TypeError("intervals must be a signal_quality IntervalDocument")
    return score_intervals(case, intervals, out_dir, cli_path, minimum_iou)


def score_delineation(case, delineations, out_dir=None, cli_path=None, tolerance_ms=None):
    if not isinstance(delineations, DelineationDocument):
        raise TypeError("delineations must be a DelineationDocument")
    case.package.ensure_integrity_verified()
    tempdir = None
    if out_dir is None:
        tempdir = _TemporaryDirectory()
        out_dir = os.path.join(tempdir.name, "delineation_score")
    command = [_cli(cli_path), "delineation", "score", case.scenario_path, delineations.path, "--out", out_dir]
    if tolerance_ms is not None:
        command.extend(["--tolerance-ms", str(tolerance_ms)])
    summary = _run(command)
    return ScoreReport(
        out_dir,
        summary,
        _read_json(os.path.join(out_dir, "delineation_score.json")),
        _read_text(os.path.join(out_dir, "delineation_score.csv")),
        _read_text(os.path.join(out_dir, "delineation_score_report.html")),
        tempdir,
    )


def score_pack(pack_json, detections_dir, out_dir=None, cli_path=None):
    tempdir = None
    if out_dir is None:
        tempdir = _TemporaryDirectory()
        out_dir = os.path.join(tempdir.name, "pack_score")
    command = [_cli(cli_path), "pack", "score", pack_json, detections_dir, "--out", out_dir]
    summary = _run(command)
    return ScoreReport(
        out_dir,
        summary,
        _read_json(os.path.join(out_dir, "pack_score_summary.json")),
        _read_text(os.path.join(out_dir, "pack_score_summary.csv")),
        _read_text(os.path.join(out_dir, "pack_score_report.html")),
        tempdir,
    )


def _compare(target, case, detections, out_dir, cli_path, tolerance_ms):
    if not isinstance(detections, DetectionDocument):
        raise TypeError("detections must be a DetectionDocument")
    case.package.ensure_integrity_verified()
    tempdir = None
    if out_dir is None:
        tempdir = _TemporaryDirectory()
        out_dir = os.path.join(tempdir.name, "comparison")
    command = [_cli(cli_path), "compare", target, case.scenario_path, detections.path, "--out", out_dir]
    if tolerance_ms is not None:
        command.extend(["--tolerance-ms", str(tolerance_ms)])
    summary = _run(command)
    return ScoreReport(
        out_dir,
        summary,
        _read_json(os.path.join(out_dir, "comparison.json")),
        _read_text(os.path.join(out_dir, "comparison.csv")),
        _read_text(os.path.join(out_dir, "comparison_report.html")),
        tempdir,
    )


def _cli(cli_path):
    return cli_path or os.environ.get("SYNSIGRA_CLI") or os.environ.get("SIGNAL_SYNTH_CLI") or "signal-synth"


def _run(command):
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()
    stdout_text = stdout.decode("utf-8")
    stderr_text = stderr.decode("utf-8")
    if process.returncode != 0:
        raise RuntimeError("command failed with exit code %s: %s\n%s" % (process.returncode, " ".join(command), stderr_text))
    if stderr_text:
        raise RuntimeError("command wrote stderr: %s" % stderr_text)
    return _parse_key_value_stdout(stdout_text)


def _parse_key_value_stdout(text):
    result = {}
    for line in text.splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            result[key] = _coerce(value)
    return result


def _coerce(value):
    try:
        if "." not in value and "e" not in value.lower():
            return int(value)
        return float(value)
    except ValueError:
        return value


def _read_text(path):
    with open(path, "r") as handle:
        return handle.read()


def _read_json(path):
    with open(path, "r") as handle:
        return json.load(handle)
