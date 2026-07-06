# Golden Challenge Compatibility

**Document ID:** SYN-INC-042

**Version:** 1.0

**Status:** Implemented

**Owner role:** Engineering

**Date:** 2026-07-06

**Implementation issue:** [signal_synth#66](https://github.com/tamask1s/signal_synth/issues/66)

## 1. Decision

Add a compact golden compatibility fixture for the released beta
`r_peak_stress_v1` challenge pack. The fixture protects the core evidence
chain between:

- C++ challenge package generation;
- SaaS pack contract and package fingerprints;
- local Python package loading and integrity checks;
- `synsigra-verify` pass/fail exit semantics;
- machine-readable verification summary structure.

The golden fixture is intentionally compact. It keeps only the files required
by the local verifier: package manifest, pack/scoring metadata, case summaries,
scenarios, annotations, pass detections, fail detections, and normalized
expected summaries. Waveform CSV, WFDB, EDF, BDF, HTML reports, and other
bulky render payloads remain covered by generator and native-format tests.

## 2. Public Contract

The fixture contract is stored in:

```text
python/tests/fixtures/golden/r_peak_stress_v1/golden_contract.json
```

It records:

- issue URL for traceability;
- released pack ID and version;
- full generated package fingerprint from the authoritative C++ CLI;
- pack fingerprint;
- expected full package waveform formats;
- compact fixture manifest/scoring/package hashes;
- deterministic compact archive hash;
- expected pass and fail summary contracts;
- extension points for HRV, PPG, and beat-classification golden fixtures.

## 3. Verification

`TEST-GOLDEN-CHALLENGE-001` builds a deterministic `.synsigra` archive from
the compact fixture, verifies challenge integrity, checks package/scoring
contract fields, and runs:

```text
python -m synsigra.cli verify <archive> detections/pass <out> --target r_peak --profile regression --force
python -m synsigra.cli verify <archive> detections/fail <out> --target r_peak --profile regression --force
```

The passing detector must exit `0` and match
`expected/pass_summary_contract.json`. The failing detector must exit `1`,
remain scoreable, and match `expected/fail_summary_contract.json`.

This deliberately distinguishes scoring failures from policy failures. A bad
user algorithm should usually produce a scored report with failed thresholds,
not a verifier crash or `scoring_error`.

## 4. Update Process

Use this only for intentional released-contract changes:

1. Build the current CLI.
2. Render the full pack:

```text
signal-synth pack challenge examples/packs/r_peak_stress_v1.json --out /tmp/r_peak_stress_v1
```

3. Confirm whether the full package fingerprint change is intentional.
4. Refresh the compact fixture from the rendered output, preserving only the
   verifier-required files listed above.
5. Run:

```text
SYNSIGRA_UPDATE_GOLDEN=1 PYTHONPATH=python SIGNAL_SYNTH_SOURCE_DIR=$PWD python3 teszt/golden_challenge_fixture_test.py
```

6. Run the same test again without `SYNSIGRA_UPDATE_GOLDEN`.
7. Review the changed contract, expected summary, and hashes before commit.

Do not update golden fixtures merely to make CI pass. A changed golden hash is
a release-contract decision.

## 5. SaaS Compatibility Boundary

The SaaS worker uses:

```text
signal-synth pack challenge <pack.json> --out <job-work-dir>
```

and parses the CLI stdout fields `status`, `output_directory`, `package_id`,
`scenario_count`, `pack_fingerprint`, and `package_fingerprint`.

This increment does not require the SaaS repository in the core CI job.
Instead it pins the core-side package contract and local verifier behavior
that the SaaS depends on.

When a sibling SaaS checkout is available, the same test can compare the live
SaaS curated pack contract:

```text
SIGNAL_SYNTH_SAAS_DIR=../signal_synth_saas PYTHONPATH=python SIGNAL_SYNTH_SOURCE_DIR=$PWD python3 teszt/golden_challenge_fixture_test.py
```

Cross-repo CI can later set `SIGNAL_SYNTH_SAAS_DIR` or consume the same golden
fixture from the SaaS repository.

## 6. DataBrowser And SVN Impact

None. No DataBrowser API or script is added.

## 7. Verification Evidence

Locally verified on 2026-07-06:

- `TEST-GOLDEN-CHALLENGE-001` passes in direct Python execution;
- the test found and fixed an empty-detector regression in the local verifier;
- the compact fixture integrity checks cover 15 manifest-listed files;
- the pass fixture scores 4/4 R-peak case-targets successfully;
- the fail fixture scores 4/4 case-targets successfully but fails the
  regression threshold policy.
