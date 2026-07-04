import csv
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

    def annotations(self):
        with open(self.file_path("annotations_json"), "r") as handle:
            return json.load(handle)


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
