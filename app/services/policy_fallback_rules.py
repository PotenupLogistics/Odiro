from __future__ import annotations

from dataclasses import dataclass

from app.models.recommendation import AnalysisStatistics, ParamRecommendation


# CLAUDE.md 명시 고정값. 로그에 실제 적용값이 없으므로 1차 구현은 이 기본값을 current로 가정.
DEFAULT_STOP_DISTANCE_M = 1.2
DEFAULT_SLOW_DOWN_DISTANCE_M = 3.5
DEFAULT_MAX_SPEED_KMH = 10.0
DEFAULT_CAN_REPATH = True


@dataclass(frozen=True)
class PolicyParamDefaults:
    stopDistanceM: float = DEFAULT_STOP_DISTANCE_M
    slowDownDistanceM: float = DEFAULT_SLOW_DOWN_DISTANCE_M
    maxSpeedKmh: float = DEFAULT_MAX_SPEED_KMH
    canRepath: bool = DEFAULT_CAN_REPATH


def _round(value: float, ndigits: int = 2) -> float:
    return round(value, ndigits)


def apply_fallback_rules(
    statistics: AnalysisStatistics,
    defaults: PolicyParamDefaults | None = None,
) -> list[ParamRecommendation]:
    """규칙 기반 결정론적 추천. RAG/LLM 실패 시 호출."""
    params = defaults or PolicyParamDefaults()
    recommendations: list[ParamRecommendation] = []

    # 규칙 1: 최소 전방거리가 위험 수준
    if 0 < statistics.minFrontDistanceM < 0.8:
        recommendations.append(
            ParamRecommendation(
                param="stopDistanceM",
                current=params.stopDistanceM,
                suggested=_round(params.stopDistanceM * 1.25),
                reason=(
                    f"주행 중 최소 전방거리가 {statistics.minFrontDistanceM:.2f}m로 "
                    f"안전여유 임계(0.8m) 미만. stopDistance를 25% 상향."
                ),
            )
        )

    # 규칙 2: 브레이크 발동 비율 과다
    if statistics.brakeAppliedRatio > 0.25:
        recommendations.append(
            ParamRecommendation(
                param="slowDownDistanceM",
                current=params.slowDownDistanceM,
                suggested=_round(params.slowDownDistanceM * 1.25),
                reason=(
                    f"브레이크 발동 비율 {statistics.brakeAppliedRatio:.1%}로 임계(25%) 초과. "
                    f"감속 시작 거리를 25% 상향해 응급 정지 빈도를 낮춤."
                ),
            )
        )

    # 규칙 3: 재탐색이 잦고 배송시간 지연
    if statistics.repathActionRatio > 0.05 and statistics.deliveryTimeSec > 25.0:
        recommendations.append(
            ParamRecommendation(
                param="canRepath",
                current=params.canRepath,
                suggested=False,
                reason=(
                    f"repath reason 비율 {statistics.repathActionRatio:.1%}, "
                    f"배송시간 {statistics.deliveryTimeSec:.1f}s. 재탐색 비용이 효익 초과."
                ),
            )
        )

    # 규칙 4: 충돌/근접 없이 과도하게 느림
    if statistics.avgTargetSpeedKmh < 3.0 and statistics.minFrontDistanceM >= 1.0:
        recommendations.append(
            ParamRecommendation(
                param="maxSpeedKmh",
                current=params.maxSpeedKmh,
                suggested=_round(params.maxSpeedKmh * 1.2),
                reason=(
                    f"평균 목표속도 {statistics.avgTargetSpeedKmh:.2f}km/h, "
                    f"최소 전방거리 {statistics.minFrontDistanceM:.2f}m. "
                    f"안전 여유가 있고 효율이 낮음 — 최대속도 20% 상향."
                ),
            )
        )

    # 규칙 5: 속도 분산이 큼 (불안정한 가감속)
    if statistics.targetSpeedVariance > 4.0:
        recommendations.append(
            ParamRecommendation(
                param="maxSpeedKmh",
                current=params.maxSpeedKmh,
                suggested=_round(params.maxSpeedKmh * 0.9),
                reason=(
                    f"targetSpeed 분산 {statistics.targetSpeedVariance:.2f}로 임계(4.0) 초과. "
                    f"최고속도를 10% 낮춰 가감속 안정화."
                ),
            )
        )

    # 규칙 6: 평균 전방거리가 매우 길고 정지/감속 적음 (과도한 보수성)
    if (
        statistics.avgFrontDistanceM > 5.0
        and statistics.stopActionRatio < 0.05
        and statistics.slowdownActionRatio < 0.2
    ):
        recommendations.append(
            ParamRecommendation(
                param="slowDownDistanceM",
                current=params.slowDownDistanceM,
                suggested=_round(params.slowDownDistanceM * 0.8),
                reason=(
                    f"평균 전방거리 {statistics.avgFrontDistanceM:.2f}m, "
                    f"정지/감속 비율 낮음. 감속 시작 거리를 20% 축소해 효율 개선."
                ),
            )
        )

    # 규칙 7: 감속 의존도 과다
    if statistics.slowdownActionRatio > 0.6:
        recommendations.append(
            ParamRecommendation(
                param="slowDownDistanceM",
                current=params.slowDownDistanceM,
                suggested=_round(params.slowDownDistanceM * 0.9),
                reason=(
                    f"slowdown reason 비율 {statistics.slowdownActionRatio:.1%}로 임계(60%) 초과. "
                    f"불필요한 감속을 줄이기 위해 감속 시작 거리 10% 축소."
                ),
            )
        )

    # 규칙 8: 인지 감지율 매우 낮음 → 보수적 정책
    if statistics.frontObjectDetectionRate < 0.15:
        recommendations.append(
            ParamRecommendation(
                param="slowDownDistanceM",
                current=params.slowDownDistanceM,
                suggested=_round(params.slowDownDistanceM * 1.5),
                reason=(
                    f"전방 객체 감지율 {statistics.frontObjectDetectionRate:.1%}로 매우 낮음. "
                    f"인지 신뢰성을 고려해 감속 시작 거리 50% 상향."
                ),
            )
        )

    return recommendations


def build_fallback_summary(recommendations: list[ParamRecommendation]) -> str:
    if not recommendations:
        return "fallback 규칙 기준으로 조정이 필요한 파라미터가 없음."
    params = ", ".join(rec.param for rec in recommendations)
    return f"fallback 규칙 기반으로 {len(recommendations)}개 파라미터 조정 권장: {params}"
