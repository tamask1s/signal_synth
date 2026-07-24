import copy
import json
import os

from synsigra.challenge import ChallengeFormatError, _validate_verification_protocol
from synsigra.local_verify import VerificationError, _verification_configuration


def rejected(document, pack_id):
    try:
        _validate_verification_protocol(document, pack_id)
    except ChallengeFormatError:
        return
    raise AssertionError("invalid protocol was accepted")


class Package(object):
    def __init__(self, protocol):
        self.package_id = protocol["pack_id"]
        self._protocol = protocol

    def verification_protocol(self):
        return self._protocol

    def verification_protocol_identity(self):
        return {"protocol_id": self._protocol["protocol_id"], "contract": self._protocol["contract"], "path": "verification_protocol.json", "size_bytes": 1, "sha256": "sha256:" + "0" * 64}

    def case_ids(self):
        return [item["case_id"] for item in self._protocol["required_case_targets"]]


def scoring_manifest(protocol):
    return {
        "schema_version": 3,
        "contract": "synsigra_scoring_manifest_v3",
        "scoring_manifest_contract_version": "synsigra_scoring_manifest_v3",
        "package_id": protocol["pack_id"],
        "targets": [{"target": target} for target in sorted(set(target for item in protocol["required_case_targets"] for target in item["targets"]))],
        "cases": [{"case_id": item["case_id"], "scoring": [{"target": target, "supported": True} for target in item["targets"]]} for item in protocol["required_case_targets"]],
    }


def main():
    source = os.environ["SIGNAL_SYNTH_SOURCE_DIR"]
    path = os.path.join(source, "examples", "packs", "r_peak_rr_noise_v1_expectations.json")
    with open(path, "r") as handle:
        valid = json.load(handle)
    _validate_verification_protocol(valid, "r_peak_rr_noise_v1")

    malformed = copy.deepcopy(valid)
    malformed["schema_version"] = 1
    rejected(malformed, valid["pack_id"])

    malformed = copy.deepcopy(valid)
    malformed["unexpected"] = True
    rejected(malformed, valid["pack_id"])

    malformed = copy.deepcopy(valid)
    malformed["required_case_targets"].append(copy.deepcopy(malformed["required_case_targets"][0]))
    rejected(malformed, valid["pack_id"])

    malformed = copy.deepcopy(valid)
    malformed["required_case_targets"][0]["targets"].append(malformed["required_case_targets"][0]["targets"][0])
    rejected(malformed, valid["pack_id"])

    malformed = copy.deepcopy(valid)
    del malformed["acceptance_profile"]["targets"]["rr_interval"]
    rejected(malformed, valid["pack_id"])

    malformed = copy.deepcopy(valid)
    malformed["acceptance_profile"]["unexpected"] = True
    rejected(malformed, valid["pack_id"])

    malformed = copy.deepcopy(valid)
    malformed["acceptance_profile"]["targets"]["rr_interval"]["overall"] = {}
    rejected(malformed, valid["pack_id"])

    malformed = copy.deepcopy(valid)
    malformed["acceptance_profile"]["targets"]["rr_interval"]["overall"]["truth_match_fraction"] = {"min": 0.9, "max": 0.8}
    rejected(malformed, valid["pack_id"])

    malformed = copy.deepcopy(valid)
    malformed["stress_strata"] = malformed["stress_strata"][:1]
    rejected(malformed, valid["pack_id"])

    malformed = copy.deepcopy(valid)
    malformed["stress_strata"][0]["case_ids"].append("unknown_case")
    rejected(malformed, valid["pack_id"])

    malformed = copy.deepcopy(valid)
    malformed["acceptance_strata"][1]["id"] = malformed["acceptance_strata"][0]["id"]
    rejected(malformed, valid["pack_id"])

    malformed = copy.deepcopy(valid)
    malformed["acceptance_strata"][0]["case_ids"].append("unknown_case")
    rejected(malformed, valid["pack_id"])

    malformed = copy.deepcopy(valid)
    malformed["acceptance_strata"][1]["case_ids"] = ["clean_70"]
    rejected(malformed, valid["pack_id"])

    malformed = copy.deepcopy(valid)
    malformed["acceptance_strata"][0]["acceptance_profile"]["targets"]["unknown_target"] = {"overall": {"truth_match_fraction": {"min": 0.1}}}
    rejected(malformed, valid["pack_id"])

    malformed = copy.deepcopy(valid)
    malformed["scoring_contract"] = "synsigra_local_verification_v1"
    rejected(malformed, valid["pack_id"])

    package = Package(valid)
    manifest = scoring_manifest(valid)
    configuration = _verification_configuration(package, manifest, "evidence", None, None, None)
    assert configuration["required_matrix"] and configuration["threshold_profile"]["profile_id"] == "r_peak_rr_noise_v1_acceptance"
    assert [item["id"] for item in configuration["acceptance_strata"]] == ["rr_standard_cases", "rr_external_extreme"]

    per_case = copy.deepcopy(valid)
    per_case["verdict_scope"] = "per_case"
    del per_case["acceptance_profile"]
    per_case["acceptance_strata"] = []
    for required_case in per_case["required_case_targets"]:
        case_id = required_case["case_id"]
        per_case["acceptance_strata"].append({
            "id": case_id,
            "case_ids": [case_id],
            "acceptance_profile": {
                "schema_version": 1,
                "profile_id": "case_%s" % case_id,
                "description": "Independent case verdict.",
                "targets": {
                    target: copy.deepcopy(valid["acceptance_profile"]["targets"][target])
                    for target in required_case["targets"]
                },
            },
        })
    _validate_verification_protocol(per_case, per_case["pack_id"])
    configuration = _verification_configuration(
        Package(per_case), scoring_manifest(per_case), "evidence", None, None, None
    )
    assert configuration["threshold_profile"]["profile_id"] == "per_case_profiles"
    assert configuration["protocol"]["verdict_scope"] == "per_case"

    malformed = copy.deepcopy(per_case)
    malformed["acceptance_profile"] = copy.deepcopy(valid["acceptance_profile"])
    rejected(malformed, malformed["pack_id"])

    malformed = copy.deepcopy(per_case)
    malformed["acceptance_strata"][0]["case_ids"].append(
        malformed["acceptance_strata"][1]["case_ids"][0]
    )
    rejected(malformed, malformed["pack_id"])

    malformed = copy.deepcopy(per_case)
    del malformed["acceptance_strata"][-1]
    rejected(malformed, malformed["pack_id"])

    malformed = copy.deepcopy(per_case)
    del malformed["acceptance_strata"][0]["acceptance_profile"]["targets"]["rr_interval"]
    rejected(malformed, malformed["pack_id"])
    malformed_manifest = copy.deepcopy(manifest)
    malformed_manifest["cases"][0]["scoring"].pop()
    try:
        _verification_configuration(package, malformed_manifest, "evidence", None, None, None)
        raise AssertionError("protocol/scoring matrix mismatch was accepted")
    except VerificationError:
        pass
    malformed_manifest = copy.deepcopy(manifest)
    malformed_manifest["schema_version"] = 2
    try:
        _verification_configuration(package, malformed_manifest, "evidence", None, None, None)
        raise AssertionError("old scoring manifest contract was accepted")
    except VerificationError:
        pass

    malformed_manifest = copy.deepcopy(manifest)
    malformed_manifest["cases"][0]["scoring"][0]["supported"] = "yes"
    try:
        _verification_configuration(package, malformed_manifest, "evidence", None, None, None)
        raise AssertionError("non-boolean scoring support flag was accepted")
    except VerificationError:
        pass

    print("protocol_v2_test=passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
