import json
import os
import shutil
import subprocess

from synsigra.delineation import DelineationDocument, delineation_truth_from_annotations, load_delineations, score_delineation_events


def run(command):
    process = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    if process.returncode != 0:
        raise RuntimeError("command failed: %s\n%s\n%s" % (" ".join(command), process.stdout, process.stderr))
    assert process.stderr == ""
    return process.stdout


def event_json(item):
    return {"beat_index": str(item.beat_index), "lead": item.lead, "kind": item.kind, "time_seconds": item.time_seconds}


cli = os.environ["SIGNAL_SYNTH_CLI"]
source_dir = os.environ["SIGNAL_SYNTH_SOURCE_DIR"]
work_dir = os.environ["SIGNAL_SYNTH_DELINEATION_WORK_DIR"]
scenario = os.path.join(source_dir, "examples", "scenarios", "ecg_clean.json")
shutil.rmtree(work_dir, ignore_errors=True)
os.makedirs(work_dir)
render_dir = os.path.join(work_dir, "render")
run([cli, "render", scenario, "--out", render_dir])
with open(os.path.join(render_dir, "annotations.json"), "r") as handle:
    annotations = json.load(handle)

scope = DelineationDocument("", "all_beats", ["II", "V2"], [], algorithm_name="perfect", algorithm_version="1")
truth = delineation_truth_from_annotations(annotations, scope)
assert truth
prediction_path = os.path.join(work_dir, "delineations.json")
with open(prediction_path, "w") as handle:
    json.dump({
        "schema_version": 1,
        "algorithm": {"name": "perfect", "version": "1"},
        "target": "ecg_delineation",
        "scope": {"mode": "all_beats", "leads": ["II", "V2"]},
        "events": [event_json(item) for item in truth],
    }, handle, sort_keys=True, separators=(",", ":"))

document = load_delineations(prediction_path)
duration = 10.0
python_score = score_delineation_events(truth, document.events, duration, 0.040, document.leads)
assert python_score["overall"]["f1_score"] == 1.0
score_dir = os.path.join(work_dir, "score")
stdout = run([cli, "delineation", "score", scenario, prediction_path, "--out", score_dir])
assert stdout.startswith("status=delineation-scored\n")
with open(os.path.join(score_dir, "delineation_score.json"), "r") as handle:
    cpp_score = json.load(handle)
assert cpp_score["overall"] == python_score["overall"]
assert cpp_score["by_kind"] == python_score["by_kind"]
assert cpp_score["by_lead"] == python_score["by_lead"]
assert len(cpp_score["by_kind_lead"]) == 18

shifted = list(document.events)
shifted[0].time_seconds += 0.050
shifted_score = score_delineation_events(truth, shifted, duration, 0.040, document.leads)
assert shifted_score["overall"]["out_of_tolerance_count"] == 1
assert shifted_score["overall"]["false_negative_count"] == 1
assert shifted_score["overall"]["false_positive_count"] == 1

malformed_path = os.path.join(work_dir, "malformed.json")
with open(malformed_path, "w") as handle:
    handle.write('{"schema_version":1,"algorithm":{"name":"x","version":"1"},"target":"ecg_delineation","scope":{"mode":"selected_beats","beat_indices":["01"],"leads":["II"]},"events":[]}')
try:
    load_delineations(malformed_path)
    raise AssertionError("noncanonical beat index was accepted")
except ValueError:
    pass

incomplete_csv = os.path.join(work_dir, "incomplete.csv")
with open(incomplete_csv, "w") as handle:
    handle.write("row_type,scope_mode,evaluated_beat_index,beat_index,lead,kind,time_seconds\nscope,selected_beats,1,,II,,\nscope,selected_beats,2,,V2,,\n")
try:
    load_delineations(incomplete_csv)
    raise AssertionError("incomplete CSV scope grid was accepted")
except ValueError:
    pass

print("delineation_python_test=passed")
