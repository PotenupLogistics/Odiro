"""Shared text and artifact shape helpers for analysis review outputs."""

from __future__ import annotations

from typing import Any


# Stable marker used to avoid appending duplicate policy candidate metadata.
POLICY_CANDIDATE_MARKER = "ANALYSIS_REVIEW_POLICY_CANDIDATE"

# Review-only metadata block appended to copied policy candidates.
POLICY_CANDIDATE_BLOCK = f'''

# Analysis review candidate metadata for copied policy artifacts only.
{POLICY_CANDIDATE_MARKER} = {{
    "intent": "Prefer slow down or stop before unsafe boundary or navigation decisions.",
    "recommended_action": "slow_down_or_stop",
    "source": "analysis_v2_review",
}}
'''

# User-facing summary for cases where result/events logs are not sufficient for analysis.
INSUFFICIENT_DATA_SUMMARY_MESSAGE = (
    "분석 가능한 로그가 부족해 실패 원인을 판단하기 어렵습니다. "
    "result/events 로그를 확인한 뒤 다시 분석해 주세요."
)

# User-facing top-level recommendation reason for successful runs with policy review evidence.
SUCCESS_POLICY_REVIEW_REASON = (
    "주행은 성공했지만, 패널티 구역 침범과 경로 재탐색 반복 등 정책 검토가 필요한 근거가 확인되었습니다."
)


def default_artifacts() -> dict[str, dict[str, Any]]:
    """Return the stable artifact status object used by recommendations and manifest."""
    return {
        "policy": {"generated": False, "path": None},
        "environment": {"generated": False, "path": None},
    }


# User-facing reasons for recommendations.json while internal recommendation_type codes stay stable.
RECOMMENDATION_TYPE_REASONS = {
    "environment_review": "환경 또는 장애물 배치와 관련된 실패 근거가 확인되었습니다.",
    "policy_review": "주행 정책 검토가 필요한 실패 근거가 확인되었습니다.",
    "none": "정책 또는 환경 수정이 필요하다고 판단할 만한 반복 근거가 확인되지 않았습니다.",
    "insufficient_data": "분석에 필요한 실행 로그가 부족하여 정책 또는 환경 수정 여부를 판단하기 어렵습니다.",
}


def recommendation_type_reason(recommendation_type: str) -> str:
    """Return the Korean display reason for a stable recommendation type code."""
    return RECOMMENDATION_TYPE_REASONS.get(recommendation_type, recommendation_type)


# User-facing Korean labels for report findings while preserving internal English codes.
FINDING_DISPLAY_LABELS = {
    "penalty_region_violation": ("패널티 구역 침범", "이"),
    "static_obstacle_collision": ("정적 장애물 충돌", "이"),
    "blocked_region_collision": ("차단 구역 충돌 또는 침범", "이"),
    "pedestrian_collision": ("보행자 충돌", "이"),
    "near_miss": ("근접 위험", "이"),
    "repath": ("경로 재탐색 반복", "이"),
    "goal_not_reached": ("목표 미도달", "이"),
    "timeout": ("제한 시간 초과", "가"),
    "policy_decision_error": ("정책 판단 오류", "가"),
    "stuck": ("로봇 정체 상태", "가"),
    "robot_tip_over": ("로봇 전도", "가"),
}


def finding_title(finding_type: str) -> str:
    """Return a user-facing report title for an internal finding code."""
    label, _ = FINDING_DISPLAY_LABELS.get(finding_type, (finding_type, "이"))
    return f"{label} 근거가 확인되었습니다."


def finding_summary(finding_type: str, evidence_count: int) -> str:
    """Return a user-facing report summary for an internal finding code."""
    label, particle = FINDING_DISPLAY_LABELS.get(finding_type, (finding_type, "이"))
    return f"{label}{particle} {evidence_count}개의 근거로 확인되었습니다."


def evidence_message(finding_type: str, value: int) -> str:
    """Return a user-facing evidence message while metrics keep internal names."""
    if finding_type == "penalty_region_violation":
        return f"패널티 구역 침범이 {value}회 발생했습니다."
    if finding_type == "static_obstacle_collision":
        return f"정적 장애물 충돌이 {value}회 발생했습니다."
    if finding_type == "blocked_region_collision":
        return f"차단 구역 충돌 또는 침범이 {value}회 발생했습니다."
    if finding_type == "pedestrian_collision":
        return f"보행자 충돌이 {value}회 발생했습니다."
    if finding_type == "near_miss":
        return f"근접 위험이 {value}회 발생했습니다."
    if finding_type == "repath":
        return f"경로 재탐색이 {value}회 발생했습니다."
    if finding_type == "timeout":
        return "에피소드가 제한 시간을 초과했습니다."
    if finding_type == "policy_decision_error":
        return f"정책 판단 오류가 {value}회 발생했습니다."
    if finding_type == "stuck":
        return "로봇 정체 상태가 보고되었습니다."
    if finding_type == "robot_tip_over":
        return f"로봇 전도가 {value}회 발생했습니다."
    if finding_type == "goal_not_reached":
        return "에피소드가 목표에 도달하지 못했습니다."
    return f"{finding_type}이 {value}개의 근거로 확인되었습니다."
