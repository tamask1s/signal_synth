import argparse
import sys

from .challenge import ChallengeIntegrityError
from .local_verify import VerificationError, verify_package
from .profiles import ThresholdProfileError, threshold_profile_names


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    if argv and argv[0] == "verify":
        argv = argv[1:]
    parser = argparse.ArgumentParser(prog="synsigra-verify", description="Verify local algorithm outputs against a Synsigra challenge package.")
    parser.add_argument("challenge", help="Challenge package directory, .zip archive, or .synsigra archive.")
    parser.add_argument("submission_dir", help="Submission directory containing submission.json and declared user output files.")
    parser.add_argument("output_dir", help="New output directory for verification reports.")
    parser.add_argument("--case", dest="cases", action="append", help="Restrict verification to a case id. Can be repeated.")
    parser.add_argument("--target", dest="targets", action="append", help="Restrict verification to a target. Can be repeated.")
    parser.add_argument("--mode", choices=("evidence", "diagnostic"), default="evidence", help="Evidence uses the complete packaged protocol; diagnostic permits filters and custom profiles.")
    parser.add_argument("--profile", help="Diagnostic threshold profile name (%s) or JSON file." % ", ".join(threshold_profile_names()))
    parser.add_argument("--force", action="store_true", help="Replace output_dir if it already exists.")
    args = parser.parse_args(argv)
    try:
        report = verify_package(args.challenge, args.submission_dir, args.output_dir, cases=args.cases, targets=args.targets, mode=args.mode, profile=args.profile, force=args.force)
    except (ChallengeIntegrityError, ThresholdProfileError, VerificationError, OSError, ValueError) as error:
        print("status=failed")
        print("error=%s" % str(error))
        return 1
    evidence = report.evidence
    print("status=%s" % evidence["status"])
    print("mode=%s" % evidence["verification"]["mode"])
    print("evidence_eligible=%s" % str(evidence["verification"]["evidence_eligible"]).lower())
    print("package_id=%s" % evidence["package"].get("package_id", ""))
    print("case_target_count=%s" % evidence["case_target_count"])
    print("completed_case_target_count=%s" % evidence["completed_case_target_count"])
    print("incomplete_case_target_count=%s" % evidence["incomplete_case_target_count"])
    print("threshold_profile=%s" % evidence["policy"]["profile_id"])
    print("failed_policy_check_count=%s" % evidence["policy"]["failed_check_count"])
    print("output_directory=%s" % report.output_dir)
    return 0 if evidence.get("success", False) else 1


if __name__ == "__main__":
    sys.exit(main())
