#!/usr/bin/env python3
"""Acquire or verify explicitly licensed external-noise assets for a scenario."""

import argparse
import hashlib
import json
import os
import pathlib
import sys
import urllib.parse
import urllib.request


def checksum(content):
    return "sha256:" + hashlib.sha256(content).hexdigest()


def safe_id(value):
    return value and len(value) <= 128 and all(c.isalnum() or c in "._-" for c in value)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("scenario", type=pathlib.Path)
    parser.add_argument("--asset-dir", required=True, type=pathlib.Path)
    parser.add_argument("--download", action="store_true", help="download missing HTTPS assets")
    parser.add_argument("--accept-license", action="append", default=[], metavar="ASSET_ID", help="explicitly accept the declared license for one downloaded asset")
    args = parser.parse_args()

    document = json.loads(args.scenario.read_text(encoding="utf-8"))
    manifests = document.get("external_noise", {}).get("assets", [])
    accepted = set(args.accept_license)
    args.asset_dir.mkdir(parents=True, exist_ok=True)
    failures = []
    for manifest in manifests:
        asset_id = manifest.get("id", "")
        expected = manifest.get("content_sha256", "")
        if not safe_id(asset_id):
            failures.append("invalid asset id: {!r}".format(asset_id))
            continue
        path = args.asset_dir / (asset_id + ".csv")
        if not path.exists() and args.download:
            uri = manifest.get("source_uri", "")
            if urllib.parse.urlparse(uri).scheme != "https":
                failures.append("{}: automatic acquisition requires HTTPS".format(asset_id))
                continue
            if asset_id not in accepted:
                failures.append("{}: add --accept-license {} after reviewing license {!r}".format(asset_id, asset_id, manifest.get("license", "")))
                continue
            temporary = path.with_suffix(".csv.tmp")
            try:
                with urllib.request.urlopen(uri) as response, temporary.open("wb") as output:
                    while True:
                        block = response.read(1024 * 1024)
                        if not block:
                            break
                        output.write(block)
                if checksum(temporary.read_bytes()) != expected:
                    temporary.unlink()
                    failures.append("{}: downloaded checksum mismatch".format(asset_id))
                    continue
                os.replace(str(temporary), str(path))
            except Exception as error:
                if temporary.exists():
                    temporary.unlink()
                failures.append("{}: download failed: {}".format(asset_id, error))
                continue
        if not path.exists():
            failures.append("{}: missing {}".format(asset_id, path))
            continue
        actual = checksum(path.read_bytes())
        if actual != expected:
            failures.append("{}: checksum mismatch, expected {}, got {}".format(asset_id, expected, actual))
            continue
        print("verified={} redistribution={} license={}".format(asset_id, manifest.get("redistribution", ""), manifest.get("license", "")))

    if failures:
        for failure in failures:
            print("error={}".format(failure), file=sys.stderr)
        return 1
    print("status=ready asset_count={}".format(len(manifests)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
