"""Local Synsigra challenge loading and scoring helpers.

Generation remains in the C++ core. The customer-facing local verifier uses
only exported challenge package ground truth and user detection outputs, so a
verification package can be used without shipping generator source code.
"""

from .challenge import ChallengeCase, ChallengeIntegrityError, ChallengePackage, WaveformTable, load_challenge
from .detections import DetectionDocument, DetectionEvent, load_detections
from .local_verify import VerificationError, VerificationReport, verify_package
from .profiles import ThresholdProfileError, load_threshold_profile, threshold_profile_names
from .scoring import ScoreReport, compare_beat_classes, compare_ppg_onsets, compare_ppg_peaks, compare_rpeaks, score_hrv, score_pack

__all__ = [
    "ChallengeCase",
    "ChallengeIntegrityError",
    "ChallengePackage",
    "DetectionDocument",
    "DetectionEvent",
    "ScoreReport",
    "VerificationError",
    "VerificationReport",
    "ThresholdProfileError",
    "WaveformTable",
    "compare_beat_classes",
    "compare_ppg_peaks",
    "compare_ppg_onsets",
    "compare_rpeaks",
    "load_challenge",
    "load_detections",
    "load_threshold_profile",
    "score_hrv",
    "score_pack",
    "threshold_profile_names",
    "verify_package",
]
