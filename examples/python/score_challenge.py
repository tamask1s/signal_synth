import sys

import synsigra as ss


def main(argv):
    if len(argv) == 4:
        report = ss.verify_package(argv[1], argv[2], argv[3])
        print("status=%s" % report.evidence["status"])
        print("case_target_count=%s" % report.evidence["case_target_count"])
        return 0 if report.evidence["success"] else 1
    if len(argv) != 5:
        print("usage: score_challenge.py <challenge-dir-or.synsigra> <detections-dir> <out-dir>", file=sys.stderr)
        print("   or: score_challenge.py <challenge-dir> <case-id> <detections.json|csv> <out-dir>", file=sys.stderr)
        return 2
    challenge = ss.load_challenge(argv[1])
    detections = ss.load_detections(argv[3], target="r_peak")
    report = ss.compare_rpeaks(challenge.case(argv[2]), detections)
    report.write(argv[4])
    print("f1_score=%s" % report.json["comparison"]["metrics"]["total"]["f1_score"])
    report.close()
    challenge.close()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
