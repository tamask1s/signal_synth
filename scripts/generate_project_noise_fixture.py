#!/usr/bin/env python3
"""Regenerate the small project-owned external-noise test fixture."""

import argparse
import math
import pathlib
import random


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=pathlib.Path, default=pathlib.Path("examples/assets/noise/synsigra_project_noise_v1.csv"))
    args = parser.parse_args()
    args.out.parent.mkdir(parents=True, exist_ok=True)
    randomizer = random.Random(80001)
    sample_rate = 50.0
    previous = 0.0
    rows = ["baseline_wander,muscle,electrode_motion"]
    for sample in range(201):
        time = sample / sample_rate
        baseline = 0.75 * math.sin(2.0 * math.pi * 0.28 * time) + 0.22 * math.sin(2.0 * math.pi * 0.11 * time + 0.4)
        raw = randomizer.uniform(-1.0, 1.0)
        muscle = raw - 0.62 * previous
        previous = raw
        burst = math.exp(-0.5 * ((time - 1.8) / 0.22) ** 2) - 0.65 * math.exp(-0.5 * ((time - 2.25) / 0.35) ** 2)
        rows.append("{:.12f},{:.12f},{:.12f}".format(baseline, muscle, burst))
    args.out.write_text("\n".join(rows) + "\n", encoding="ascii")


if __name__ == "__main__":
    main()
