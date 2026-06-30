from __future__ import annotations

import pytest

from app.agents.result_analysis_v2.recommendation_validator import RecommendationValidator


KNOWN_EPISODE_REFS = {("Experiment1", "000001", "000001")}


def _policy_parameter_recommendation(content: dict) -> dict:
    """Create a policy parameter recommendation for validator boundary tests."""
    return {
        "id": "REC-LLM-001",
        "target": "policy",
        "priority": "high",
        "title": "정책 파라미터 검토",
        "reason": "반복 실패 근거가 확인되었습니다.",
        "recommendation": "주행 정책 파라미터를 보수적으로 검토하세요.",
        "evidence": [
            {
                "experiment_id": "Experiment1",
                "run_id": "000001",
                "episode_id": "000001",
            }
        ],
        "proposed_change": {
            "type": "policy_parameter_adjustment",
            "content": content,
        },
    }


def test_recommendation_validator_accepts_supported_policy_parameter_adjustment() -> None:
    """Supported policy parameter caps remain valid recommendations."""
    recommendation = _policy_parameter_recommendation(
        {
            "followSpeedKmh_max": 3.5,
            "maxPathErrorM_max": 0.8,
            "lookAheadDistanceM_max": 1.0,
            "pathSmoothingDistanceM_max": 0.25,
            "maxSteeringDelta_max": 0.06,
        }
    )

    validated = RecommendationValidator().validate([recommendation], KNOWN_EPISODE_REFS)

    assert validated == [recommendation]


@pytest.mark.parametrize(
    "content",
    [
        {"unsupportedPolicyParameter_max": 1.0},
        {"followSpeedKmh_max": 3.5001},
        {"followSpeedKmh_max": "3.5"},
    ],
)
def test_recommendation_validator_rejects_invalid_policy_parameter_adjustment(content: dict) -> None:
    """Unknown, out-of-range, and non-numeric parameter changes are invalid."""
    recommendation = _policy_parameter_recommendation(content)

    validated = RecommendationValidator().validate([recommendation], KNOWN_EPISODE_REFS)

    assert validated == []
