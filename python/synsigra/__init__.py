"""Local Synsigra challenge loading and scoring helpers.

The Python package is intentionally a thin convenience layer. Generation and
scoring semantics remain in the C++ command-line tool.
"""

from .challenge import ChallengeCase, ChallengeIntegrityError, ChallengePackage, WaveformTable, load_challenge
from .detections import DetectionDocument, DetectionEvent, load_detections
from .scoring import ScoreReport, compare_beat_classes, compare_ppg_peaks, compare_rpeaks, score_hrv, score_pack

__all__ = [
    "ChallengeCase",
    "ChallengeIntegrityError",
    "ChallengePackage",
    "DetectionDocument",
    "DetectionEvent",
    "ScoreReport",
    "WaveformTable",
    "compare_beat_classes",
    "compare_ppg_peaks",
    "compare_rpeaks",
    "load_challenge",
    "load_detections",
    "score_hrv",
    "score_pack",
]
