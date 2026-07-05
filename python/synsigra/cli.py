import argparse
import sys

from .challenge import ChallengeIntegrityError
from .local_verify import VerificationError, verify_package


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    if argv and argv[0] == "verify":
        argv = argv[1:]
    parser = argparse.ArgumentParser(prog="synsigra-verify", description="Verify local algorithm outputs against a Synsigra challenge package.")
    parser.add_argument("challenge", help="Challenge package directory or .synsigra archive.")
    parser.add_argument("detections_dir", help="Directory containing user detection JSON/CSV files.")
    parser.add_argument("output_dir", help="New output directory for verification reports.")
    parser.add_argument("--case", dest="cases", action="append", help="Restrict verification to a case id. Can be repeated.")
    parser.add_argument("--target", dest="targets", action="append", help="Restrict verification to a target. Can be repeated.")
    parser.add_argument("--force", action="store_true", help="Replace output_dir if it already exists.")
    args = parser.parse_args(argv)
    try:
        report = verify_package(args.challenge, args.detections_dir, args.output_dir, cases=args.cases, targets=args.targets, force=args.force)
    except (ChallengeIntegrityError, VerificationError, OSError, ValueError) as error:
        print("status=failed")
        print("error=%s" % str(error))
        return 1
    summary = report.summary
    print("status=%s" % summary["status"])
    print("package_id=%s" % summary["package"].get("package_id", ""))
    print("case_target_count=%s" % summary["case_target_count"])
    print("passed_case_target_count=%s" % summary["passed_case_target_count"])
    print("failed_case_target_count=%s" % summary["failed_case_target_count"])
    print("output_directory=%s" % report.output_dir)
    return 0 if summary.get("success", False) else 1


if __name__ == "__main__":
    sys.exit(main())
