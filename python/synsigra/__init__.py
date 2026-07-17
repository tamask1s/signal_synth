"""Local Synsigra challenge loading and scoring helpers.

Generation remains in the C++ core. The customer-facing local verifier uses
only exported challenge package ground truth and user detection outputs, so a
verification package can be used without shipping generator source code.
"""

from .challenge import ChallengeCase, ChallengeIntegrityError, ChallengePackage, WaveformTable, load_challenge
from .detections import DetectionDocument, DetectionEvent, load_detections
from .delineation import DelineationDocument, DelineationEvent, load_delineations
from .intervals import IntervalDocument, IntervalEvent, load_intervals
from .local_verify import VerificationError, VerificationReport, verify_package
from .profiles import ThresholdProfileError, load_threshold_profile, threshold_profile_names
from .scoring import ScoreReport, compare_beat_classes, compare_ppg_onsets, compare_ppg_peaks, compare_rpeaks, score_delineation, score_hrv, score_intervals, score_pack, score_rhythm_episodes, score_signal_quality

__all__ = [
    "ChallengeCase",
    "ChallengeIntegrityError",
    "ChallengePackage",
    "DetectionDocument",
    "DetectionEvent",
    "DelineationDocument",
    "DelineationEvent",
    "IntervalDocument",
    "IntervalEvent",
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
    "load_delineations",
    "load_intervals",
    "load_threshold_profile",
    "score_hrv",
    "score_delineation",
    "score_intervals",
    "score_pack",
    "score_rhythm_episodes",
    "score_signal_quality",
    "threshold_profile_names",
    "verify_package",
]
