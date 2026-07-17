import os
import shutil
import subprocess

import synsigra


def run(command):
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()
    if process.returncode != 0 or stderr:
        raise RuntimeError("command failed: %s\n%s\n%s" % (" ".join(command), stdout.decode("utf-8"), stderr.decode("utf-8")))
    return stdout.decode("utf-8")


def main():
    source = os.environ["SIGNAL_SYNTH_SOURCE_DIR"]
    cli = os.environ["SIGNAL_SYNTH_CLI"]
    work = os.environ["SIGNAL_SYNTH_PPG_OPTICAL_WORK_DIR"]
    if os.path.exists(work):
        shutil.rmtree(work)
    os.makedirs(work)
    challenge_dir = os.path.join(work, "challenge")
    run([cli, "pack", "challenge", os.path.join(source, "examples", "packs", "ppg_optical_v2.json"), "--out", challenge_dir])
    challenge = synsigra.load_challenge(challenge_dir)
    assert challenge.verify_integrity()["ok"]
    case = challenge.case("desaturation")
    latent = case.ppg_optical_latent()
    truth = case.ppg_optical_truth()
    measurements = case.measurement_truth("ppg_optical")
    assert latent.columns == ["sample_index", "time_seconds", "red_latent_au", "infrared_latent_au", "red_sensor_au", "infrared_sensor_au"]
    assert len(latent) > 0 and truth["contract"] == "synsigra_ppg_optical_truth_v2"
    assert truth["profile_id"] == "finger_transmissive_v1" and len(truth["channels"]) == 2
    assert min(pulse["spo2_percent"] for pulse in truth["pulses"]) <= 88.0
    assert measurements and set(item["measurement"]["name"] for item in measurements) == {
        "spo2_target", "ratio_of_ratios", "red_perfusion_index",
        "infrared_perfusion_index", "red_ac_dc_ratio", "infrared_ac_dc_ratio",
    }
    challenge.close()
    print("ppg_optical_python_test=passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
