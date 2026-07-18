import json
import os
import shutil
import tempfile
import warnings
import zipfile

import synsigra as ss


def read_json(path):
    with open(path, "r") as handle:
        return json.load(handle)


def write_json(path, document):
    with open(path, "w") as handle:
        json.dump(document, handle, sort_keys=True, separators=(",", ":"))


def rejected(path, exception_types=(ss.ChallengeFormatError, ss.ChallengeIntegrityError)):
    try:
        package = ss.load_challenge(path)
        package.close()
        raise AssertionError("invalid challenge was accepted: %s" % path)
    except exception_types:
        pass


def copy_fixture(source, work, name):
    destination = os.path.join(work, name)
    shutil.copytree(source, destination)
    return destination


def zip_fixture(source, destination):
    with zipfile.ZipFile(destination, "w", zipfile.ZIP_DEFLATED) as archive:
        for root, directories, files in os.walk(source):
            directories.sort()
            for name in sorted(files):
                path = os.path.join(root, name)
                archive.write(path, os.path.relpath(path, source).replace(os.sep, "/"))


def main():
    source_dir = os.environ["SIGNAL_SYNTH_SOURCE_DIR"]
    fixture = os.path.join(source_dir, "python", "tests", "fixtures", "distribution_smoke", "challenge")
    work = tempfile.mkdtemp(prefix="synsigra_challenge_security_")
    try:
        package = ss.load_challenge(fixture)
        assert package.scoring_manifest()["package_id"] == "python_distribution_smoke"
        assert package.submission_manifest()["challenge"]["package_id"] == "python_distribution_smoke"
        assert package.submission_formats()["contract"] == "synsigra_submission_formats_v2"
        try:
            package.verification_protocol()
            raise AssertionError("fixture unexpectedly contains a verification protocol")
        except KeyError:
            pass
        package.close()

        duplicate_key = copy_fixture(fixture, work, "duplicate_key")
        manifest_path = os.path.join(duplicate_key, "manifest.json")
        with open(manifest_path, "r") as handle:
            text = handle.read()
        with open(manifest_path, "w") as handle:
            handle.write(text.replace('"schema_version": 1,', '"schema_version": 1,\n  "schema_version": 1,', 1))
        rejected(duplicate_key)

        unknown_field = copy_fixture(fixture, work, "unknown_field")
        document = read_json(os.path.join(unknown_field, "manifest.json"))
        document["unexpected"] = True
        write_json(os.path.join(unknown_field, "manifest.json"), document)
        rejected(unknown_field)

        unsupported_contract = copy_fixture(fixture, work, "unsupported_contract")
        document = read_json(os.path.join(unsupported_contract, "manifest.json"))
        document["contract"] = "synsigra_challenge_package_v99"
        write_json(os.path.join(unsupported_contract, "manifest.json"), document)
        rejected(unsupported_contract)

        unknown_role = copy_fixture(fixture, work, "unknown_role")
        document = read_json(os.path.join(unknown_role, "manifest.json"))
        document["files"][0]["role"] = "guessed_from_filename"
        write_json(os.path.join(unknown_role, "manifest.json"), document)
        rejected(unknown_role)

        noncanonical = copy_fixture(fixture, work, "noncanonical")
        document = read_json(os.path.join(noncanonical, "manifest.json"))
        document["files"][0]["path"] = "./scoring_manifest.json"
        write_json(os.path.join(noncanonical, "manifest.json"), document)
        rejected(noncanonical)

        missing = copy_fixture(fixture, work, "missing")
        os.remove(os.path.join(missing, "cases", "clean", "annotations.json"))
        rejected(missing)

        unlisted = copy_fixture(fixture, work, "unlisted")
        with open(os.path.join(unlisted, "hidden_payload.txt"), "w") as handle:
            handle.write("not declared by the package manifest")
        rejected(unlisted)

        if hasattr(os, "symlink"):
            symlinked = copy_fixture(fixture, work, "symlinked")
            scenario = os.path.join(symlinked, "cases", "clean", "scenario.json")
            os.remove(scenario)
            os.symlink(os.path.join(fixture, "cases", "clean", "scenario.json"), scenario)
            rejected(symlinked)

            symlinked_manifest = copy_fixture(fixture, work, "symlinked_manifest")
            manifest_path = os.path.join(symlinked_manifest, "manifest.json")
            os.remove(manifest_path)
            os.symlink(os.path.join(fixture, "manifest.json"), manifest_path)
            rejected(symlinked_manifest, exception_types=(ss.ChallengeIntegrityError,))

        valid_archive = os.path.join(work, "valid.synsigra")
        zip_fixture(fixture, valid_archive)
        archived = ss.load_challenge(valid_archive)
        assert archived.verify_integrity()["ok"]
        archived.close()

        duplicate_archive = os.path.join(work, "duplicate.synsigra")
        shutil.copyfile(valid_archive, duplicate_archive)
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            with zipfile.ZipFile(duplicate_archive, "a") as archive:
                archive.writestr("manifest.json", "{}")
        rejected(duplicate_archive)

        unsafe_archive = os.path.join(work, "unsafe.synsigra")
        with zipfile.ZipFile(unsafe_archive, "w") as archive:
            archive.writestr("../escape", "payload")
        rejected(unsafe_archive)

        conflict_archive = os.path.join(work, "prefix_conflict.synsigra")
        with zipfile.ZipFile(conflict_archive, "w") as archive:
            archive.writestr("collision", "file")
            archive.writestr("collision/payload", "nested")
        rejected(conflict_archive)
    finally:
        shutil.rmtree(work)
    print("challenge_python_security_test=passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
