import csv
import hashlib
import json
import os
import re
import shutil
import stat
import tempfile
import zipfile

from .profiles import ThresholdProfileError, load_threshold_profile


_MANIFEST_FIELDS = set(["schema_version", "contract", "package_id", "name", "version", "description", "package_type", "ground_truth_included", "waveform_formats", "generator_version", "usage_restrictions", "not_for", "files", "cases"])
_FILE_FIELDS = set(["path", "role", "media_type", "sha256", "size_bytes"])
_CASE_FIELDS = set(["id", "scenario_id", "scenario_path", "document_fingerprint", "render_identity", "files"])
_FILE_ROLES = set([
    "scenario_json", "pack_json", "metadata_json", "waveform_csv", "annotations_json",
    "ground_truth_metrics_json", "report_html", "readme", "wfdb_header", "wfdb_signal",
    "wfdb_annotation", "edf", "bdf", "measurement_truth_json", "wearable_samples_csv",
    "wearable_timestamp_truth_csv", "wearable_timebase_truth_json", "wearable_alignment_truth_json",
    "realism_metrics_json", "realism_metrics_csv", "realism_report_html", "realism_population_json",
    "ppg_optical_latent_csv", "ppg_optical_truth_json", "cardiorespiratory_truth_json",
    "prv_tachogram_csv", "respiration_reference_csv", "scoring_manifest_json",
    "submission_manifest_json", "submission_formats_json", "verification_protocol_json", "other",
])
_SINGLETON_ROLES = set(["pack_json", "scoring_manifest_json", "submission_manifest_json", "submission_formats_json", "verification_protocol_json", "realism_population_json"])
_GROUND_TRUTH_ROLES = set(["annotations_json", "ground_truth_metrics_json", "measurement_truth_json"])
_SAFE_ID = re.compile(r"^[A-Za-z0-9_.-]{1,128}$")
_SHA256 = re.compile(r"^sha256:[0-9a-f]{64}$")
_MEDIA_TYPE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9!#$&^_.+-]*/[A-Za-z0-9][A-Za-z0-9!#$&^_.+-]*$")
_MAX_ARCHIVE_MEMBERS = 100000
_MAX_ARCHIVE_MEMBER_BYTES = 32 * 1024 * 1024 * 1024
_MAX_ARCHIVE_TOTAL_BYTES = 64 * 1024 * 1024 * 1024


class _TemporaryDirectory(object):
    def __init__(self):
        self.name = tempfile.mkdtemp(prefix="synsigra_challenge_")

    def cleanup(self):
        if self.name and os.path.exists(self.name):
            shutil.rmtree(self.name)
        self.name = None


class WaveformTable(object):
    def __init__(self, columns, rows):
        self.columns = columns
        self.rows = rows

    def column(self, name):
        if name not in self.columns:
            raise KeyError(name)
        index = self.columns.index(name)
        return [row[index] for row in self.rows]

    def __len__(self):
        return len(self.rows)


class ChallengeIntegrityError(ValueError):
    pass


class ChallengeFormatError(ValueError):
    pass


class ChallengeCase(object):
    def __init__(self, package, data):
        self.package = package
        self.data = data
        self.id = data.get("id", "")
        self.scenario_id = data.get("scenario_id", "")
        self.scenario_path = package.resolve(data.get("scenario_path", ""))
        self.document_fingerprint = data.get("document_fingerprint", "")
        self.render_identity = data.get("render_identity", "")
        self.files = list(data.get("files", []))

    def file_path(self, role):
        for item in self.package.files:
            if item.get("role") == role and item.get("path") in self.files:
                return self.package.resolve(item.get("path", ""))
        raise KeyError(role)

    def waveform(self):
        return read_waveform_csv(self.file_path("waveform_csv"))

    def _case_file_path(self, name):
        relative_path = "cases/%s/%s" % (self.id, name)
        if relative_path not in self.files:
            raise KeyError(name)
        return self.package.resolve(relative_path)

    def wearable_samples(self, stream):
        if stream not in ("ecg", "ppg", "accelerometer"):
            raise ValueError("stream must be 'ecg', 'ppg', or 'accelerometer'")
        return read_waveform_csv(self._case_file_path("wearable_%s_samples.csv" % stream))

    def wearable_timestamp_truth(self):
        return read_waveform_csv(self._case_file_path("wearable_timestamp_truth.csv"))

    def wearable_timebase_truth(self):
        return _read_json_strict(self._case_file_path("wearable_timebase_truth.json"))

    def wearable_alignment_truth(self):
        return _read_json_strict(self._case_file_path("wearable_alignment_truth.json"))

    def realism_metrics(self):
        return _read_json_strict(self.file_path("realism_metrics_json"))

    def realism_table(self):
        return read_waveform_csv(self.file_path("realism_metrics_csv"))

    def realism_report_path(self):
        return self.file_path("realism_report_html")

    def ppg_optical_latent(self):
        return read_waveform_csv(self.file_path("ppg_optical_latent_csv"))

    def ppg_optical_truth(self):
        return _read_json_strict(self.file_path("ppg_optical_truth_json"))

    def annotations(self):
        return _read_json_strict(self.file_path("annotations_json"))

    def case_summary(self):
        return self.package.read_json("cases/%s/case_summary.json" % self.id)

    def metadata(self):
        return self.package.read_json("cases/%s/metadata.json" % self.id)

    def ground_truth_metrics(self):
        return self.package.read_json("cases/%s/ground_truth_metrics.json" % self.id)

    def hrv_metrics(self):
        return self.package.read_json("cases/%s/hrv_metrics.json" % self.id)

    def cardiorespiratory_truth(self):
        return self.package.read_json("cases/%s/cardiorespiratory_truth.json" % self.id)

    def prv_tachogram(self):
        return read_waveform_csv(self._case_file_path("prv_tachogram.csv"))

    def respiration_reference(self):
        return read_waveform_csv(self._case_file_path("respiration_reference.csv"))

    def measurement_truth(self, target=None):
        document = self.package.read_json("cases/%s/measurement_truth.json" % self.id)
        if target is None:
            return document
        matches = [item for item in document.get("targets", []) if item.get("target") == target]
        if len(matches) != 1:
            raise KeyError(target)
        return list(matches[0].get("measurements", []))

    def warnings(self):
        return self.package.read_json("cases/%s/warnings.json" % self.id)


class ChallengePackage(object):
    def __init__(self, root, manifest, tempdir=None):
        self.root = root
        self.manifest = manifest
        self._tempdir = tempdir
        self.package_id = manifest.get("package_id", "")
        self.name = manifest.get("name", "")
        self.version = manifest.get("version", "")
        self.files = list(manifest.get("files", []))
        self.cases = [ChallengeCase(self, item) for item in manifest.get("cases", [])]
        self._case_map = dict((item.id, item) for item in self.cases)
        self._integrity_report = None

    def close(self):
        if self._tempdir is not None:
            self._tempdir.cleanup()
            self._tempdir = None

    def resolve(self, relative_path):
        normalized = _safe_relative_path(relative_path, "challenge package path")
        path = os.path.join(self.root, *normalized.split("/"))
        if os.path.islink(path) or not _inside_root(self.root, path):
            raise ChallengeIntegrityError("challenge package path escapes through a symlink: %s" % relative_path)
        return path

    def case(self, case_id):
        return self._case_map[case_id]

    def case_ids(self):
        return [item.id for item in self.cases]

    def file_by_role(self, role):
        return self.resolve(self.file_entry_by_role(role).get("path", ""))

    def file_entry_by_role(self, role):
        for item in self.files:
            if item.get("role") == role:
                return dict(item)
        raise KeyError(role)

    def read_json(self, relative_path):
        return _read_json_strict(self.resolve(relative_path))

    def read_json_by_role(self, role):
        return _read_json_strict(self.file_by_role(role))

    def scoring_manifest(self):
        return self.read_json_by_role("scoring_manifest_json")

    def submission_manifest(self):
        return self.read_json_by_role("submission_manifest_json")

    def submission_formats(self):
        return self.read_json_by_role("submission_formats_json")

    def verification_protocol(self):
        document = self.read_json_by_role("verification_protocol_json")
        _validate_verification_protocol(document, self.package_id)
        return document

    def verification_protocol_identity(self):
        entry = self.file_entry_by_role("verification_protocol_json")
        protocol = self.verification_protocol()
        return {
            "protocol_id": protocol["protocol_id"],
            "contract": protocol["contract"],
            "path": entry["path"],
            "size_bytes": entry["size_bytes"],
            "sha256": entry["sha256"],
        }

    def realism_population(self):
        return self.read_json_by_role("realism_population_json")

    def verify_integrity(self):
        errors = []
        checked_files = 0
        total_bytes = 0
        try:
            _validate_layout(self.root, self.manifest)
        except ChallengeIntegrityError as error:
            errors.append(str(error))
        for item in self.files:
            relative_path = item.get("path", "")
            try:
                path = self.resolve(relative_path)
            except (ValueError, ChallengeIntegrityError) as error:
                errors.append(str(error))
                continue
            if not os.path.isfile(path):
                errors.append("missing file: %s" % relative_path)
                continue
            size = os.path.getsize(path)
            expected_size = item.get("size_bytes")
            if expected_size is not None and size != expected_size:
                errors.append("size mismatch for %s: expected %s, got %s" % (relative_path, expected_size, size))
            digest = _sha256_file(path)
            expected_sha = item.get("sha256", "")
            if expected_sha and digest != expected_sha:
                errors.append("sha256 mismatch for %s: expected %s, got %s" % (relative_path, expected_sha, digest))
            checked_files += 1
            total_bytes += size
        report = {"checked_file_count": checked_files, "total_bytes": total_bytes, "errors": errors, "ok": not errors}
        self._integrity_report = report
        if errors:
            raise ChallengeIntegrityError("; ".join(errors))
        return report

    def ensure_integrity_verified(self):
        if self._integrity_report is None:
            return self.verify_integrity()
        if not self._integrity_report.get("ok"):
            raise ChallengeIntegrityError("; ".join(self._integrity_report.get("errors", [])))
        return self._integrity_report

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.close()


def load_challenge(path):
    path = os.fspath(path)
    root = path
    tempdir = None
    try:
        if not os.path.isdir(path):
            tempdir = _TemporaryDirectory()
            with zipfile.ZipFile(path, "r") as archive:
                _extract_archive_safely(archive, tempdir.name)
            root = tempdir.name
        manifest_path = _validate_root_manifest_path(root)
        manifest = _read_json_strict(manifest_path)
        _validate_manifest(manifest)
        _validate_layout(root, manifest)
        package = ChallengePackage(root, manifest, tempdir)
        if any(item["role"] == "verification_protocol_json" for item in package.files):
            package.verification_protocol()
        return package
    except Exception:
        if tempdir is not None:
            tempdir.cleanup()
        raise


def _extract_archive_safely(archive, destination):
    members = archive.infolist()
    if len(members) > _MAX_ARCHIVE_MEMBERS:
        raise ChallengeFormatError("challenge archive contains too many members")
    validated = []
    seen = set()
    files = set()
    all_paths = set()
    total_bytes = 0
    for member in members:
        if member.flag_bits & 1:
            raise ChallengeFormatError("encrypted challenge archive members are not supported: %s" % member.filename)
        unix_mode = (member.external_attr >> 16) & 0xffff
        if unix_mode and stat.S_ISLNK(unix_mode):
            raise ChallengeFormatError("challenge archive symlinks are not supported: %s" % member.filename)
        is_directory = member.is_dir()
        raw_name = member.filename
        if "\\" in raw_name:
            raise ChallengeFormatError("challenge archive path must use forward slashes: %s" % raw_name)
        name = raw_name[:-1] if is_directory and raw_name.endswith("/") else raw_name
        name = _safe_relative_path(name, "challenge archive path")
        if raw_name != name + ("/" if is_directory else ""):
            raise ChallengeFormatError("challenge archive path is not canonical: %s" % raw_name)
        folded = name.lower()
        if folded in seen:
            raise ChallengeFormatError("duplicate or case-colliding challenge archive path: %s" % name)
        seen.add(folded)
        all_paths.add(folded)
        if not is_directory:
            files.add(folded)
            if member.file_size > _MAX_ARCHIVE_MEMBER_BYTES:
                raise ChallengeFormatError("challenge archive member is too large: %s" % name)
            total_bytes += member.file_size
            if total_bytes > _MAX_ARCHIVE_TOTAL_BYTES:
                raise ChallengeFormatError("challenge archive expands beyond the supported size limit")
        validated.append((member, name, is_directory))
    for file_path in files:
        for other in all_paths:
            if other != file_path and other.startswith(file_path + "/"):
                raise ChallengeFormatError("challenge archive file/directory path conflict: %s" % file_path)
    for member, name, is_directory in validated:
        target = os.path.join(destination, name)
        if is_directory:
            if not os.path.exists(target):
                os.makedirs(target)
            continue
        parent = os.path.dirname(target)
        if parent and not os.path.exists(parent):
            os.makedirs(parent)
        with archive.open(member, "r") as source:
            with open(target, "wb") as handle:
                shutil.copyfileobj(source, handle)


def _read_json_strict(path):
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return json.load(handle, object_pairs_hook=_unique_json_object)
    except ChallengeFormatError:
        raise
    except (OSError, ValueError) as error:
        raise ChallengeFormatError("invalid JSON document %s: %s" % (path, error))


def _unique_json_object(pairs):
    output = {}
    for key, value in pairs:
        if key in output:
            raise ChallengeFormatError("duplicate JSON object key: %s" % key)
        output[key] = value
    return output


def _safe_relative_path(value, label):
    if not isinstance(value, str) or not value or len(value) > 512:
        raise ChallengeFormatError("%s must contain 1 to 512 characters" % label)
    if value.startswith("/") or "\\" in value or ":" in value:
        raise ChallengeFormatError("%s is not a portable relative path: %s" % (label, value))
    if any(ord(character) < 0x20 or ord(character) > 0x7e for character in value):
        raise ChallengeFormatError("%s must contain printable ASCII only: %s" % (label, value))
    components = value.split("/")
    if any(component in ("", ".", "..") for component in components):
        raise ChallengeFormatError("%s is not canonical: %s" % (label, value))
    return value


def _inside_root(root, path):
    root_real = os.path.realpath(root)
    path_real = os.path.realpath(path)
    try:
        return os.path.commonpath([root_real, path_real]) == root_real
    except ValueError:
        return False


def _validate_root_manifest_path(root):
    if not os.path.isdir(root) or os.path.islink(root):
        raise ChallengeIntegrityError("challenge package root must be a real directory")
    manifest_path = os.path.join(root, "manifest.json")
    if os.path.islink(manifest_path) or not _inside_root(root, manifest_path) or not os.path.isfile(manifest_path):
        raise ChallengeIntegrityError("challenge package manifest must be a real file inside the package root")
    return manifest_path


def _exact_fields(document, expected, path):
    if not isinstance(document, dict):
        raise ChallengeFormatError("%s must be an object" % path)
    actual = set(document)
    if actual != expected:
        missing = sorted(expected - actual)
        unknown = sorted(actual - expected)
        details = []
        if missing:
            details.append("missing %s" % ", ".join(missing))
        if unknown:
            details.append("unknown %s" % ", ".join(unknown))
        raise ChallengeFormatError("%s has invalid fields: %s" % (path, "; ".join(details)))


def _string(value, path, maximum=None):
    if not isinstance(value, str) or not value or (maximum is not None and len(value) > maximum):
        raise ChallengeFormatError("%s must be a non-empty string%s" % (path, " of at most %d characters" % maximum if maximum else ""))
    return value


def _safe_id(value, path):
    if not isinstance(value, str) or not _SAFE_ID.match(value):
        raise ChallengeFormatError("%s must be a safe identifier" % path)
    return value


def _validate_manifest(manifest):
    _exact_fields(manifest, _MANIFEST_FIELDS, "manifest")
    if type(manifest["schema_version"]) is not int or manifest["schema_version"] != 1:
        raise ChallengeFormatError("manifest.schema_version must be 1")
    if manifest["contract"] != "synsigra_challenge_package_v3":
        raise ChallengeFormatError("manifest.contract is unsupported")
    _safe_id(manifest["package_id"], "manifest.package_id")
    _string(manifest["name"], "manifest.name", 256)
    _string(manifest["version"], "manifest.version", 64)
    _string(manifest["description"], "manifest.description")
    if manifest["package_type"] not in ("single_scenario", "scenario_pack"):
        raise ChallengeFormatError("manifest.package_type is unsupported")
    if manifest["ground_truth_included"] is not True:
        raise ChallengeFormatError("manifest.ground_truth_included must be true")
    formats = manifest["waveform_formats"]
    if not isinstance(formats, list) or not formats:
        raise ChallengeFormatError("manifest.waveform_formats must be a non-empty array")
    for index, value in enumerate(formats):
        _safe_id(value, "manifest.waveform_formats[%d]" % index)
    if len(set(formats)) != len(formats):
        raise ChallengeFormatError("manifest.waveform_formats contains duplicates")
    _string(manifest["generator_version"], "manifest.generator_version")
    _string(manifest["usage_restrictions"], "manifest.usage_restrictions")
    not_for = _string(manifest["not_for"], "manifest.not_for")
    if "diagnosis" not in not_for or "clinical validation" not in not_for:
        raise ChallengeFormatError("manifest.not_for must include diagnosis and clinical validation limitations")

    files = manifest["files"]
    if not isinstance(files, list) or not files:
        raise ChallengeFormatError("manifest.files must be a non-empty array")
    file_paths = {}
    portable_paths = set()
    singleton_roles = set()
    has_ground_truth = False
    for index, item in enumerate(files):
        path = "manifest.files[%d]" % index
        _exact_fields(item, _FILE_FIELDS, path)
        relative_path = _safe_relative_path(item["path"], path + ".path")
        folded = relative_path.lower()
        if folded in portable_paths:
            raise ChallengeFormatError("%s duplicates or case-collides with another file path" % (path + ".path"))
        for previous in portable_paths:
            if _path_prefix_conflict(previous, folded):
                raise ChallengeFormatError("%s conflicts with a file/directory prefix" % (path + ".path"))
        portable_paths.add(folded)
        role = item["role"]
        if not isinstance(role, str) or role not in _FILE_ROLES:
            raise ChallengeFormatError("%s.role is unsupported" % path)
        if role in _SINGLETON_ROLES:
            if role in singleton_roles:
                raise ChallengeFormatError("%s.role duplicates singleton role %s" % (path, role))
            singleton_roles.add(role)
        has_ground_truth = has_ground_truth or role in _GROUND_TRUTH_ROLES
        if not isinstance(item["media_type"], str) or not _MEDIA_TYPE.match(item["media_type"]):
            raise ChallengeFormatError("%s.media_type is invalid" % path)
        if not isinstance(item["sha256"], str) or not _SHA256.match(item["sha256"]):
            raise ChallengeFormatError("%s.sha256 is invalid" % path)
        if type(item["size_bytes"]) is not int or item["size_bytes"] < 0:
            raise ChallengeFormatError("%s.size_bytes must be a non-negative integer" % path)
        file_paths[relative_path] = role
    if not has_ground_truth:
        raise ChallengeFormatError("manifest.files must include ground truth")

    cases = manifest["cases"]
    if not isinstance(cases, list) or not cases:
        raise ChallengeFormatError("manifest.cases must be a non-empty array")
    case_ids = set()
    for index, item in enumerate(cases):
        path = "manifest.cases[%d]" % index
        _exact_fields(item, _CASE_FIELDS, path)
        case_id = _safe_id(item["id"], path + ".id")
        if case_id.lower() in case_ids:
            raise ChallengeFormatError("%s.id duplicates or case-collides with another case" % path)
        case_ids.add(case_id.lower())
        _string(item["scenario_id"], path + ".scenario_id")
        scenario_path = _safe_relative_path(item["scenario_path"], path + ".scenario_path")
        if not isinstance(item["document_fingerprint"], str) or not _SHA256.match(item["document_fingerprint"]):
            raise ChallengeFormatError("%s.document_fingerprint is invalid" % path)
        _string(item["render_identity"], path + ".render_identity")
        case_files = item["files"]
        if not isinstance(case_files, list) or not case_files:
            raise ChallengeFormatError("%s.files must be a non-empty array" % path)
        normalized_case_files = []
        for file_index, relative_path in enumerate(case_files):
            relative_path = _safe_relative_path(relative_path, "%s.files[%d]" % (path, file_index))
            normalized_case_files.append(relative_path)
            if relative_path not in file_paths:
                raise ChallengeFormatError("%s.files references an unlisted path: %s" % (path, relative_path))
        if len(set(normalized_case_files)) != len(normalized_case_files):
            raise ChallengeFormatError("%s.files contains duplicates" % path)
        if scenario_path not in case_files or file_paths.get(scenario_path) != "scenario_json":
            raise ChallengeFormatError("%s.scenario_path must reference the case scenario_json file" % path)


def _path_prefix_conflict(left, right):
    return left.startswith(right + "/") or right.startswith(left + "/")


def _validate_layout(root, manifest):
    if not os.path.isdir(root) or os.path.islink(root):
        raise ChallengeIntegrityError("challenge package root must be a real directory")
    actual = set()
    errors = []
    for current, directories, files in os.walk(root, followlinks=False):
        for name in directories:
            path = os.path.join(current, name)
            if os.path.islink(path):
                errors.append("symlink directory is not allowed: %s" % os.path.relpath(path, root))
        for name in files:
            path = os.path.join(current, name)
            relative_path = os.path.relpath(path, root).replace(os.sep, "/")
            if os.path.islink(path) or not _inside_root(root, path):
                errors.append("symlink file is not allowed: %s" % relative_path)
            else:
                actual.add(relative_path)
    expected = set(["manifest.json"] + [item["path"] for item in manifest["files"]])
    missing = sorted(expected - actual)
    unlisted = sorted(actual - expected)
    if missing:
        errors.append("missing package files: %s" % ", ".join(missing))
    if unlisted:
        errors.append("unlisted package files: %s" % ", ".join(unlisted))
    if errors:
        raise ChallengeIntegrityError("; ".join(errors))


def _validate_verification_protocol(document, package_id):
    required = set(["schema_version", "contract", "protocol_id", "pack_id", "context_of_use", "scoring_contract", "required_case_targets", "stress_strata", "truth_policy", "evidence_boundary"])
    optional = set(["acceptance_profile", "acceptance_strata", "verdict_scope"])
    if not isinstance(document, dict):
        raise ChallengeFormatError("verification_protocol must be an object")
    actual = set(document)
    missing = sorted(required - actual)
    unknown = sorted(actual - required - optional)
    if missing or unknown:
        details = []
        if missing:
            details.append("missing %s" % ", ".join(missing))
        if unknown:
            details.append("unknown %s" % ", ".join(unknown))
        raise ChallengeFormatError("verification_protocol has invalid fields: %s" % "; ".join(details))
    if type(document["schema_version"]) is not int or document["schema_version"] != 2:
        raise ChallengeFormatError("verification protocol schema_version must be 2")
    if document["contract"] != "synsigra_verification_protocol_v2":
        raise ChallengeFormatError("verification protocol contract is unsupported")
    _safe_id(document["protocol_id"], "verification_protocol.protocol_id")
    if document["pack_id"] != package_id:
        raise ChallengeFormatError("verification protocol pack_id does not match the challenge package")
    _string(document["context_of_use"], "verification_protocol.context_of_use")
    if document["scoring_contract"] != "synsigra_local_verification_v3":
        raise ChallengeFormatError("verification protocol scoring_contract is unsupported")
    verdict_scope = document.get("verdict_scope", "aggregate")
    if verdict_scope not in ("aggregate", "per_case"):
        raise ChallengeFormatError("verification protocol verdict_scope must be aggregate or per_case")
    if verdict_scope == "aggregate" and "acceptance_profile" not in document:
        raise ChallengeFormatError("aggregate verification protocol requires acceptance_profile")
    if verdict_scope == "per_case" and "acceptance_profile" in document:
        raise ChallengeFormatError("per_case verification protocol must not define a pooled acceptance_profile")
    matrix = document["required_case_targets"]
    if not isinstance(matrix, list) or not matrix:
        raise ChallengeFormatError("verification protocol required_case_targets must be a non-empty array")
    case_ids = set()
    required_targets = set()
    case_targets = {}
    for index, item in enumerate(matrix):
        path = "verification_protocol.required_case_targets[%d]" % index
        _exact_fields(item, set(["case_id", "targets"]), path)
        case_id = _safe_id(item["case_id"], path + ".case_id")
        if case_id in case_ids:
            raise ChallengeFormatError("verification protocol contains duplicate case_id: %s" % case_id)
        case_ids.add(case_id)
        if not isinstance(item["targets"], list) or not item["targets"]:
            raise ChallengeFormatError("%s.targets must be a non-empty array" % path)
        targets = []
        for target_index, target in enumerate(item["targets"]):
            targets.append(_safe_id(target, "%s.targets[%d]" % (path, target_index)))
        if len(set(targets)) != len(targets):
            raise ChallengeFormatError("%s.targets contains duplicates" % path)
        required_targets.update(targets)
        case_targets[case_id] = set(targets)
    if verdict_scope == "aggregate":
        try:
            acceptance = load_threshold_profile(document["acceptance_profile"])
        except ThresholdProfileError as error:
            raise ChallengeFormatError("verification protocol acceptance_profile is invalid: %s" % error)
        missing_acceptance = sorted(required_targets - set(acceptance["targets"]))
        if missing_acceptance:
            raise ChallengeFormatError("verification protocol acceptance_profile has no direct target section for: %s" % ", ".join(missing_acceptance))
    strata = document["stress_strata"]
    if not isinstance(strata, list) or not strata:
        raise ChallengeFormatError("verification protocol stress_strata must be a non-empty array")
    stratum_ids = set()
    covered_cases = set()
    for index, item in enumerate(strata):
        path = "verification_protocol.stress_strata[%d]" % index
        _exact_fields(item, set(["id", "case_ids"]), path)
        stratum_id = _safe_id(item["id"], path + ".id")
        if stratum_id in stratum_ids:
            raise ChallengeFormatError("verification protocol contains duplicate stress stratum: %s" % stratum_id)
        stratum_ids.add(stratum_id)
        if not isinstance(item["case_ids"], list) or not item["case_ids"]:
            raise ChallengeFormatError("%s.case_ids must be a non-empty array" % path)
        normalized = [_safe_id(value, "%s.case_ids[%d]" % (path, case_index)) for case_index, value in enumerate(item["case_ids"])]
        if len(set(normalized)) != len(normalized):
            raise ChallengeFormatError("%s.case_ids contains duplicates" % path)
        unknown = sorted(set(normalized) - case_ids)
        if unknown:
            raise ChallengeFormatError("%s.case_ids contains unknown cases: %s" % (path, ", ".join(unknown)))
        covered_cases.update(normalized)
    missing_cases = sorted(case_ids - covered_cases)
    if missing_cases:
        raise ChallengeFormatError("verification protocol stress_strata do not cover cases: %s" % ", ".join(missing_cases))
    acceptance_strata = document.get("acceptance_strata", [])
    if "acceptance_strata" in document and (not isinstance(acceptance_strata, list) or not acceptance_strata):
        raise ChallengeFormatError("verification protocol acceptance_strata must be a non-empty array")
    if verdict_scope == "per_case" and not acceptance_strata:
        raise ChallengeFormatError("per_case verification protocol requires one acceptance stratum per case")
    acceptance_ids = set()
    covered_case_targets = set()
    for index, item in enumerate(acceptance_strata):
        path = "verification_protocol.acceptance_strata[%d]" % index
        _exact_fields(item, set(["id", "case_ids", "acceptance_profile"]), path)
        acceptance_id = _safe_id(item["id"], path + ".id")
        if acceptance_id in acceptance_ids:
            raise ChallengeFormatError("verification protocol contains duplicate acceptance stratum: %s" % acceptance_id)
        acceptance_ids.add(acceptance_id)
        if not isinstance(item["case_ids"], list) or not item["case_ids"]:
            raise ChallengeFormatError("%s.case_ids must be a non-empty array" % path)
        normalized = [_safe_id(value, "%s.case_ids[%d]" % (path, case_index)) for case_index, value in enumerate(item["case_ids"])]
        if len(set(normalized)) != len(normalized):
            raise ChallengeFormatError("%s.case_ids contains duplicates" % path)
        unknown = sorted(set(normalized) - case_ids)
        if unknown:
            raise ChallengeFormatError("%s.case_ids contains unknown cases: %s" % (path, ", ".join(unknown)))
        try:
            stratum_profile = load_threshold_profile(item["acceptance_profile"])
        except ThresholdProfileError as error:
            raise ChallengeFormatError("%s.acceptance_profile is invalid: %s" % (path, error))
        unknown_targets = sorted(set(stratum_profile["targets"]) - required_targets)
        if unknown_targets:
            raise ChallengeFormatError("%s.acceptance_profile contains targets absent from the protocol matrix: %s" % (path, ", ".join(unknown_targets)))
        if verdict_scope == "per_case":
            if len(normalized) != 1:
                raise ChallengeFormatError("%s must contain exactly one case in per_case mode" % path)
            expected_targets = case_targets[normalized[0]]
            actual_targets = set(stratum_profile["targets"])
            if actual_targets != expected_targets:
                raise ChallengeFormatError("%s acceptance_profile targets must exactly match case %s (expected=%s, actual=%s)" % (
                    path, normalized[0], sorted(expected_targets), sorted(actual_targets),
                ))
        pairs = set((case_id, target) for case_id in normalized for target in stratum_profile["targets"])
        overlap = sorted(pairs & covered_case_targets)
        if overlap:
            raise ChallengeFormatError("%s overlaps an earlier acceptance stratum for: %s" % (path, ", ".join("%s/%s" % pair for pair in overlap)))
        covered_case_targets.update(pairs)
    if verdict_scope == "per_case":
        required_pairs = set(
            (case_id, target)
            for case_id, targets in case_targets.items()
            for target in targets
        )
        missing_pairs = sorted(required_pairs - covered_case_targets)
        if missing_pairs:
            raise ChallengeFormatError("per_case acceptance strata do not cover: %s" % ", ".join(
                "%s/%s" % pair for pair in missing_pairs
            ))
    if not isinstance(document["truth_policy"], dict) or not document["truth_policy"]:
        raise ChallengeFormatError("verification protocol truth_policy must be a non-empty object")
    _string(document["evidence_boundary"], "verification_protocol.evidence_boundary")


def _sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        while True:
            block = handle.read(1024 * 1024)
            if not block:
                break
            digest.update(block)
    return "sha256:" + digest.hexdigest()


def read_waveform_csv(path):
    with open(path, "r") as handle:
        reader = csv.reader(handle)
        columns = next(reader)
        rows = []
        for row in reader:
            parsed = []
            for value in row:
                try:
                    parsed.append(float(value))
                except ValueError:
                    parsed.append(value)
            rows.append(parsed)
    return WaveformTable(columns, rows)
