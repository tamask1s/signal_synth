import csv
import hashlib
import json
import os
import shutil
import tempfile
import zipfile


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
        with open(self._case_file_path("wearable_timebase_truth.json"), "r") as handle:
            return json.load(handle)

    def wearable_alignment_truth(self):
        with open(self._case_file_path("wearable_alignment_truth.json"), "r") as handle:
            return json.load(handle)

    def realism_metrics(self):
        with open(self.file_path("realism_metrics_json"), "r") as handle:
            return json.load(handle)

    def realism_table(self):
        return read_waveform_csv(self.file_path("realism_metrics_csv"))

    def realism_report_path(self):
        return self.file_path("realism_report_html")

    def annotations(self):
        with open(self.file_path("annotations_json"), "r") as handle:
            return json.load(handle)

    def case_summary(self):
        return self.package.read_json("cases/%s/case_summary.json" % self.id)

    def metadata(self):
        return self.package.read_json("cases/%s/metadata.json" % self.id)

    def ground_truth_metrics(self):
        return self.package.read_json("cases/%s/ground_truth_metrics.json" % self.id)

    def hrv_metrics(self):
        return self.package.read_json("cases/%s/hrv_metrics.json" % self.id)

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
        normalized = relative_path.replace("\\", "/")
        if os.path.isabs(normalized) or ".." in normalized.split("/"):
            raise ValueError("challenge package path is not safe: %s" % relative_path)
        return os.path.join(self.root, normalized)

    def case(self, case_id):
        return self._case_map[case_id]

    def case_ids(self):
        return [item.id for item in self.cases]

    def file_by_role(self, role):
        for item in self.files:
            if item.get("role") == role:
                return self.resolve(item.get("path", ""))
        raise KeyError(role)

    def read_json(self, relative_path):
        with open(self.resolve(relative_path), "r") as handle:
            return json.load(handle)

    def scoring_manifest(self):
        return self.read_json("scoring_manifest.json")

    def realism_population(self):
        return self.read_json("realism_population.json")

    def verify_integrity(self):
        errors = []
        checked_files = 0
        total_bytes = 0
        for item in self.files:
            relative_path = item.get("path", "")
            try:
                path = self.resolve(relative_path)
            except ValueError as error:
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
    root = path
    tempdir = None
    if not os.path.isdir(path):
        tempdir = _TemporaryDirectory()
        with zipfile.ZipFile(path, "r") as archive:
            _extract_archive_safely(archive, tempdir.name)
        root = tempdir.name
    manifest_path = os.path.join(root, "manifest.json")
    with open(manifest_path, "r") as handle:
        manifest = json.load(handle)
    return ChallengePackage(root, manifest, tempdir)


def _extract_archive_safely(archive, destination):
    for member in archive.infolist():
        name = member.filename.replace("\\", "/")
        if os.path.isabs(name) or ".." in name.split("/"):
            raise ValueError("challenge archive path is not safe: %s" % member.filename)
        target = os.path.join(destination, name)
        if member.is_dir():
            if not os.path.exists(target):
                os.makedirs(target)
            continue
        parent = os.path.dirname(target)
        if parent and not os.path.exists(parent):
            os.makedirs(parent)
        with archive.open(member, "r") as source:
            with open(target, "wb") as handle:
                shutil.copyfileobj(source, handle)


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
