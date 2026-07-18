"""Local Synsigra challenge loading and scoring helpers.

Generation remains in the C++ core. The customer-facing local verifier uses
only exported challenge package ground truth and user detection outputs, so a
verification package can be used without shipping generator source code.
"""

from .challenge import ChallengeCase, ChallengeFormatError, ChallengeIntegrityError, ChallengePackage, WaveformTable, load_challenge
from .detections import DetectionDocument, DetectionEvent, load_detections
from .delineation import DelineationDocument, DelineationEvent, load_delineations
from .intervals import IntervalDocument, IntervalEvent, load_intervals
from .measurements import MeasurementError, load_measurement_truth, load_measurements, score_measurements
from .local_verify import VerificationError, VerificationReport, verify_package
from .profiles import ThresholdProfileError, load_threshold_profile, threshold_profile_names
from .submission import Submission, SubmissionError, SubmissionOutput, load_submission
from .scoring import ScoreReport, compare_beat_classes, compare_ppg_onsets, compare_ppg_peaks, compare_rpeaks, score_delineation, score_intervals, score_pack, score_rhythm_episodes, score_signal_quality

__all__ = [
    "ChallengeCase",
    "ChallengeFormatError",
    "ChallengeIntegrityError",
    "ChallengePackage",
    "DetectionDocument",
    "DetectionEvent",
    "DelineationDocument",
    "DelineationEvent",
    "IntervalDocument",
    "IntervalEvent",
    "MeasurementError",
    "ScoreReport",
    "Submission",
    "SubmissionError",
    "SubmissionOutput",
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
    "load_measurement_truth",
    "load_measurements",
    "load_submission",
    "load_threshold_profile",
    "score_delineation",
    "score_intervals",
    "score_measurements",
    "score_pack",
    "score_rhythm_episodes",
    "score_signal_quality",
    "threshold_profile_names",
    "verify_package",
]
