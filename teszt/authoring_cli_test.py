import json
import os
import subprocess
import sys


def run_json(command, stdin_text=None):
    process = subprocess.Popen(command, stdin=subprocess.PIPE if stdin_text is not None else None, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    payload = stdin_text.encode("utf-8") if stdin_text is not None else None
    stdout, stderr = process.communicate(payload)
    if process.returncode != 0:
        raise RuntimeError("command failed: %s\n%s\n%s" % (" ".join(command), stdout.decode("utf-8"), stderr.decode("utf-8")))
    if stderr:
        raise RuntimeError("stderr was not empty: %s" % stderr.decode("utf-8"))
    return json.loads(stdout.decode("utf-8"))


def run(command, stdin_text=None):
    process = subprocess.Popen(command, stdin=subprocess.PIPE if stdin_text is not None else None, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    payload = stdin_text.encode("utf-8") if stdin_text is not None else None
    stdout, stderr = process.communicate(payload)
    if process.returncode != 0:
        raise RuntimeError("command failed: %s\n%s\n%s" % (" ".join(command), stdout.decode("utf-8"), stderr.decode("utf-8")))
    if stderr:
        raise RuntimeError("stderr was not empty: %s" % stderr.decode("utf-8"))
    return stdout.decode("utf-8")


def read_json(path):
    with open(path, "r") as handle:
        return json.load(handle)


def main():
    cli = os.environ["SIGNAL_SYNTH_CLI"]
    source_dir = os.environ["SIGNAL_SYNTH_SOURCE_DIR"]
    schema = run_json([cli, "authoring", "schema"])
    assert schema["schema_version"] == 1
    assert schema["scenario_schema_version"] == 2
    assert len(schema["fields"]) >= 40
    assert len(schema["conditions"]) == 71
    assert len(schema["artifacts"]) == 15
    assert len(schema["targets"]) >= 7
    paths = [item["path"] for item in schema["fields"]]
    assert len(paths) == len(set(paths))
    for field in schema["fields"]:
        assert field["control"] and field["value_type"] and field["group"]
        if "minimum" in field and "maximum" in field:
            assert field["minimum"] <= field["maximum"]
    condition_codes = [item["code"] for item in schema["conditions"]]
    assert len(condition_codes) == len(set(condition_codes))
    assert any(item["code"] == "AFIB" and item["support"] == "native" for item in schema["conditions"])
    assert any(item["code"] == "CLBBB" and item["support"] == "parameterized" for item in schema["conditions"])
    target_support = dict((item["name"], item["support"]) for item in schema["targets"])

    templates = run_json([cli, "authoring", "templates"])
    assert templates["difficulty_values"] == ["smoke", "regression", "stress", "benchmark"]
    assert len(templates["templates"]) >= 6
    template_ids = set()
    for item in templates["templates"]:
        assert item["template_id"] not in template_ids
        template_ids.add(item["template_id"])
        assert item["difficulty"] in templates["difficulty_values"]
        assert item["feature_tags"] and item["targets"] and item["editable_paths"]
        for target in item["targets"]:
            assert target in target_support
        validation = run([cli, "validate", "-"], json.dumps(item["scenario"], sort_keys=True, separators=(",", ":")))
        assert validation.startswith("status=valid\n")

    catalog_path = os.path.join(source_dir, "examples", "catalog", "verification_packs_v1.json")
    catalog = read_json(catalog_path)
    for entry in catalog["packs"]:
        pack_path = os.path.normpath(os.path.join(os.path.dirname(catalog_path), entry["path"]))
        analysis = run_json([cli, "pack", "analyze", pack_path])
        assert analysis["success"]
        assert analysis["pack_id"] == entry["pack_id"]
        assert analysis["summary"]["case_count"] > 0
        assert analysis["summary"]["total_duration_seconds"] > 0
        assert analysis["summary"]["estimated_package_bytes"] > 0
        for target in analysis["targets"]:
            assert target["support"] in ("local_scoring", "reference_only")

    print("authoring_cli_test=passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
