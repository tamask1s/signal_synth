import json
import os


SUBMISSION_CONTRACT = "synsigra_submission_v1"


class SubmissionError(ValueError):
    pass


class SubmissionOutput(object):
    def __init__(self, case_id, target, format_name, relative_path):
        self.case_id = case_id
        self.target = target
        self.format = format_name
        self.relative_path = relative_path


class Submission(object):
    def __init__(self, root, algorithm, challenge_identity, outputs):
        self.root = os.path.abspath(root)
        self.algorithm = dict(algorithm)
        self.challenge = dict(challenge_identity)
        self.outputs = list(outputs)
        self._by_key = dict(((item.case_id, item.target), item) for item in outputs)

    def output(self, case_id, target):
        return self._by_key.get((case_id, target))

    def path(self, output):
        return os.path.join(self.root, *output.relative_path.split("/"))


def load_submission(path, package, scoring_manifest):
    if not os.path.isdir(path):
        raise SubmissionError("submission directory does not exist: %s" % path)
    manifest_path = os.path.join(path, "submission.json")
    if not os.path.isfile(manifest_path):
        raise SubmissionError("submission.json is missing from submission directory")
    with open(manifest_path, "r") as handle:
        raw = json.load(handle, object_pairs_hook=_unique_object_pairs)
    _require_exact_fields(raw, set(["schema_version", "contract", "challenge", "algorithm", "outputs"]), "submission")
    if isinstance(raw["schema_version"], bool) or raw["schema_version"] != 1 or raw["contract"] != SUBMISSION_CONTRACT:
        raise SubmissionError("submission requires schema_version 1 and contract %s" % SUBMISSION_CONTRACT)
    challenge = raw["challenge"]
    _require_exact_fields(challenge, set(["package_id", "pack_version", "pack_fingerprint"]), "submission challenge")
    expected_challenge = {
        "package_id": scoring_manifest.get("package_id", package.package_id),
        "pack_version": scoring_manifest.get("pack_version", package.version),
        "pack_fingerprint": scoring_manifest.get("pack_fingerprint", ""),
    }
    if challenge != expected_challenge:
        raise SubmissionError("submission challenge identity does not match the loaded challenge package")
    algorithm = raw["algorithm"]
    _require_exact_fields(algorithm, set(["name", "version"]), "submission algorithm")
    if not _short_text(algorithm["name"]) or not _short_text(algorithm["version"]):
        raise SubmissionError("submission algorithm name and version must be non-empty strings of at most 128 characters")
    if algorithm["name"].startswith("REPLACE_WITH_") or algorithm["version"].startswith("REPLACE_WITH_"):
        raise SubmissionError("submission algorithm placeholders must be replaced")
    expected = _expected_outputs(scoring_manifest)
    raw_outputs = raw["outputs"]
    if not isinstance(raw_outputs, list):
        raise SubmissionError("submission outputs must be an array")
    outputs = []
    keys = set()
    paths = set()
    for index, item in enumerate(raw_outputs):
        _require_exact_fields(item, set(["case_id", "target", "format", "path"]), "submission outputs[%s]" % index)
        if any(not isinstance(item[name], str) or not item[name] for name in ("case_id", "target", "format", "path")):
            raise SubmissionError("submission outputs[%s] fields must be non-empty strings" % index)
        key = (item["case_id"], item["target"])
        if key in keys:
            raise SubmissionError("duplicate submission output for %s/%s" % key)
        if key not in expected:
            raise SubmissionError("unexpected submission output for %s/%s" % key)
        if item["format"] not in expected[key]:
            raise SubmissionError("unsupported submission format for %s/%s: %s" % (key[0], key[1], item["format"]))
        if not _safe_relative_path(item["path"]):
            raise SubmissionError("submission output path must be a safe relative path: %s" % item["path"])
        comparable_path = os.path.normcase(item["path"])
        if comparable_path in paths:
            raise SubmissionError("duplicate submission output path: %s" % item["path"])
        keys.add(key)
        paths.add(comparable_path)
        outputs.append(SubmissionOutput(key[0], key[1], item["format"], item["path"]))
    missing = sorted(set(expected.keys()) - keys)
    if missing:
        raise SubmissionError("submission output is missing for %s/%s" % missing[0])
    _reject_unexpected_files(path, outputs)
    return Submission(path, algorithm, challenge, outputs)


def _expected_outputs(scoring_manifest):
    output = {}
    for case in scoring_manifest.get("cases", []):
        case_id = case.get("case_id", "")
        for entry in case.get("scoring", []):
            if not entry.get("supported", False):
                continue
            formats = entry.get("accepted_formats", [])
            if not isinstance(formats, list) or not formats:
                raise SubmissionError("challenge scoring entry has no accepted formats for %s/%s" % (case_id, entry.get("target", "")))
            output[(case_id, entry.get("target", ""))] = set(formats)
    return output


def _unique_object_pairs(pairs):
    output = {}
    for key, value in pairs:
        if key in output:
            raise SubmissionError("duplicate JSON object key: %s" % key)
        output[key] = value
    return output


def _require_exact_fields(value, fields, name):
    if not isinstance(value, dict) or set(value.keys()) != fields:
        raise SubmissionError("%s has missing or unknown fields" % name)


def _short_text(value):
    return isinstance(value, str) and 0 < len(value) <= 128


def _safe_relative_path(value):
    if not isinstance(value, str) or not value or len(value) > 512 or "\\" in value or value.startswith("/"):
        return False
    parts = value.split("/")
    return all(part not in ("", ".", "..") and ":" not in part and all(ord(ch) >= 0x20 for ch in part) for part in parts)


def _reject_unexpected_files(root, outputs):
    allowed = set(["submission.json", "formats.json"] + [item.relative_path for item in outputs])
    for directory, names, files in os.walk(root):
        for name in names:
            full = os.path.join(directory, name)
            if os.path.islink(full):
                raise SubmissionError("submission must not contain symbolic links: %s" % os.path.relpath(full, root).replace(os.sep, "/"))
        for name in files:
            full = os.path.join(directory, name)
            relative = os.path.relpath(full, root).replace(os.sep, "/")
            if os.path.islink(full):
                raise SubmissionError("submission must not contain symbolic links: %s" % relative)
            if relative not in allowed:
                raise SubmissionError("unexpected file in submission directory: %s" % relative)
