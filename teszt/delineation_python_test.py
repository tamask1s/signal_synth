import json
import os
import shutil
import subprocess

import synsigra as ss
from synsigra.delineation import DelineationEvent, DelineationScope, delineation_truth_from_annotations, load_delineations, score_delineation_events


def run(command):
    process = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    if process.returncode != 0:
        raise RuntimeError("command failed: %s\n%s\n%s" % (" ".join(command), process.stdout, process.stderr))
    assert process.stderr == ""
    return process.stdout


def event_json(item):
    return {"time_seconds": item.time_seconds, "channel": item.lead, "label": item.kind}


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

duration = 10.0
scope = DelineationScope(["II", "V2"])
truth = delineation_truth_from_annotations(annotations, scope, duration)
assert truth
assert any(item.anchor_type == "atrial_event" and item.kind == "p_peak" for item in truth)
assert any(item.anchor_type == "ventricular_beat" and item.kind == "qrs_onset" for item in truth)
record_edge = next(item for item in truth if item.status == "not_evaluable" and item.reason == "record_boundary")
assert record_edge.evaluation_end_seconds > record_edge.evaluation_start_seconds
prediction_path = os.path.join(work_dir, "delineations.json")
with open(prediction_path, "w") as handle:
    json.dump({"schema_version": 1, "events": [event_json(item) for item in truth if item.status == "present"]}, handle, sort_keys=True, separators=(",", ":"))

document = load_delineations(prediction_path)
python_score = score_delineation_events(truth, document.events, duration, scope, 0.040, 0.200)
assert python_score["overall"]["f1_score"] == 1.0
score_dir = os.path.join(work_dir, "score")
stdout = run([cli, "delineation", "score", scenario, prediction_path, "--out", score_dir])
assert stdout.startswith("status=delineation-scored\n")
with open(os.path.join(score_dir, "delineation_score.json"), "r") as handle:
    cpp_score = json.load(handle)
assert cpp_score["schema_version"] == 2
assert cpp_score["overall"] == python_score["overall"]
assert cpp_score["by_kind"] == python_score["by_kind"]
assert cpp_score["by_lead"] == python_score["by_lead"]
assert len(cpp_score["by_kind_lead"]) == 36
assert any(item["anchor_type"] == "atrial_event" for item in cpp_score["truth"])

edge_prediction = list(document.events) + [DelineationEvent(record_edge.lead, record_edge.kind, 0.5 * (record_edge.evaluation_start_seconds + record_edge.evaluation_end_seconds), original_index=len(document.events))]
edge_score = score_delineation_events(truth, edge_prediction, duration, scope, 0.040, 0.200)
assert edge_score["overall"]["excluded_prediction_count"] == 1
assert edge_score["overall"]["false_positive_count"] == 0

shifted = list(document.events)
shifted[0].time_seconds += 0.050
shifted_score = score_delineation_events(truth, shifted, duration, scope, 0.040, 0.200)
assert shifted_score["overall"]["out_of_tolerance_count"] == 1
assert shifted_score["overall"]["false_negative_count"] == 1
assert shifted_score["overall"]["false_positive_count"] == 1

malformed_path = os.path.join(work_dir, "malformed.json")
with open(malformed_path, "w") as handle:
    handle.write('{"schema_version":1,"events":[{"time_seconds":1,"channel":"II","label":"p_peak","beat_index":"1"}]}')
try:
    load_delineations(malformed_path)
    raise AssertionError("generator identity was accepted in customer output")
except ValueError:
    pass

incomplete_csv = os.path.join(work_dir, "incomplete.csv")
with open(incomplete_csv, "w") as handle:
    handle.write("time_seconds,sample_index,channel,label,confidence\n1,,,p_peak,\n")
try:
    load_delineations(incomplete_csv)
    raise AssertionError("missing delineation channel was accepted")
except ValueError:
    pass

flutter_render = os.path.join(work_dir, "flutter_render")
run([cli, "render", os.path.join(source_dir, "examples", "scenarios", "catalog", "rhythm_flutter_variable.json"), "--out", flutter_render])
with open(os.path.join(flutter_render, "annotations.json"), "r") as handle:
    flutter_annotations = json.load(handle)
flutter_truth = delineation_truth_from_annotations(flutter_annotations, DelineationScope(["II"]), 30.0)
flutter_p = [item for item in flutter_truth if item.kind == "p_peak" and item.status == "present"]
assert len(flutter_p) > len(flutter_annotations["beats"])
assert len(set(item.anchor_index for item in flutter_p)) == len(flutter_p)

with open(os.path.join(source_dir, "examples", "scenarios", "packs", "morph_population_seed_101.json"), "r") as handle:
    subthreshold_document = json.load(handle)
subthreshold_document["scenario_id"] = "delineation_subthreshold_p"
subthreshold_document["duration_seconds"] = 10
subthreshold_document["randomization"]["envelopes"] = [{"parameter": "ecg.morphology.p_amplitude_mv", "minimum": 0.0001, "maximum": 0.0001}]
subthreshold_path = os.path.join(work_dir, "subthreshold_p.json")
with open(subthreshold_path, "w") as handle:
    json.dump(subthreshold_document, handle, sort_keys=True, separators=(",", ":"))
subthreshold_render = os.path.join(work_dir, "subthreshold_render")
run([cli, "render", subthreshold_path, "--out", subthreshold_render])
with open(os.path.join(subthreshold_render, "annotations.json"), "r") as handle:
    subthreshold_annotations = json.load(handle)
subthreshold_truth = delineation_truth_from_annotations(subthreshold_annotations, DelineationScope(["II", "V2"]), 10.0)
assert any(item.kind == "p_peak" and item.status == "not_evaluable" and item.reason == "below_lead_threshold" for item in subthreshold_truth)

pack_challenge = os.path.join(work_dir, "pack_challenge")
run([cli, "pack", "challenge", os.path.join(source_dir, "examples", "packs", "ecg_delineation_v2.json"), "--out", pack_challenge])
pack_submission = os.path.join(work_dir, "pack_submission")
shutil.copytree(os.path.join(pack_challenge, "user-output-template"), pack_submission)
with open(os.path.join(pack_submission, "submission.json"), "r") as handle:
    submission_manifest = json.load(handle)
submission_manifest["algorithm"] = {"name": "perfect-delineator", "version": "2"}
with open(os.path.join(pack_submission, "submission.json"), "w") as handle:
    json.dump(submission_manifest, handle, sort_keys=True, separators=(",", ":"))
pack_package = ss.load_challenge(pack_challenge)
for output in submission_manifest["outputs"]:
    case = pack_package.case(output["case_id"])
    case_annotations = case.annotations()
    case_duration = case.case_summary()["render"]["duration_seconds"]
    case_truth = delineation_truth_from_annotations(case_annotations, DelineationScope(["II", "V2"]), case_duration)
    with open(os.path.join(pack_submission, *output["path"].split("/")), "w") as handle:
        json.dump({"schema_version": 1, "events": [event_json(item) for item in case_truth if item.status == "present"]}, handle, sort_keys=True, separators=(",", ":"))
pack_result_dir = os.path.join(work_dir, "pack_result")
pack_result = ss.verify_package(pack_challenge, pack_submission, pack_result_dir)
assert pack_result.summary["success"]
assert pack_result.summary["case_target_count"] == 8
mobitz_report = json.load(open(os.path.join(pack_result_dir, "verification", "mobitz_ii_nonconducted_p", "comparison.json"), "r"))
mobitz_annotations = pack_package.case("mobitz_ii_nonconducted_p").annotations()
linked_atrials = set(int(item["linked_atrial_index"]) for item in mobitz_annotations["beats"] if int(item["linked_atrial_index"]) >= 0)
assert any(item["anchor_type"] == "atrial_event" and item["kind"] == "p_peak" and item["status"] == "present" and int(item["anchor_index"]) not in linked_atrials for item in mobitz_report["truth"])
pack_package.close()

print("delineation_python_test=passed")
