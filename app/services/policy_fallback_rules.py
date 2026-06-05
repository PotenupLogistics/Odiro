from __future__ import annotations

from dataclasses import dataclass

from app.models.recommendation import (
    AnalysisStatistics,
    BotSetupRecommendation,
    EpisodeSetupRecommendation,
    EvaluationReportStatistics,
    ParamRecommendation,
    PolicyServerRecommendation,
)


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


# ── EvaluationReport 기반 fallback 규칙 ──────────────────────────────────

def apply_episode_fallback_rules(
    statistics: EvaluationReportStatistics,
    current_stop_distance_m: float = 1.2,
    current_slow_down_distance_m: float = 3.5,
    current_max_speed_kmh: float = 10.0,
    current_target_speed_kmh: float = 10.0,
    current_obstacle_slow_speed_kmh: float = 1.5,
    current_look_ahead_distance_m: float = 1.0,
) -> list[BotSetupRecommendation]:
    """EvaluationReport 통계 기반 결정론적 추천. LLM 실패 시 호출."""
    recs: list[BotSetupRecommendation] = []

    # 규칙 1: 보행자 근접 — stop 거리 상향
    if statistics.near_miss_min_distance_m is not None and statistics.near_miss_min_distance_m < 0.5:
        recs.append(BotSetupRecommendation(
            param="stop_distance_m",
            current=current_stop_distance_m,
            suggested=_round(current_stop_distance_m * 1.25),
            reason=(
                f"보행자 근접 최소거리 {statistics.near_miss_min_distance_m:.2f}m로 임계(0.5m) 미만. "
                f"정지 거리를 25% 상향해 안전여유 확보."
            ),
        ))

    # 규칙 2: 보행자 충돌 — stop 거리 대폭 상향
    if statistics.pedestrian_collision_count > 0:
        recs.append(BotSetupRecommendation(
            param="stop_distance_m",
            current=current_stop_distance_m,
            suggested=_round(current_stop_distance_m * 1.5),
            reason=(
                f"보행자 충돌 {statistics.pedestrian_collision_count}회 발생. "
                f"정지 거리를 50% 상향."
            ),
        ))

    # 규칙 3: 정적 장애물 충돌 — 감속 시작 거리 상향
    if statistics.static_obstacle_collision_count > 0:
        recs.append(BotSetupRecommendation(
            param="slow_down_distance_m",
            current=current_slow_down_distance_m,
            suggested=_round(current_slow_down_distance_m * 1.2),
            reason=(
                f"정적 장애물 충돌 {statistics.static_obstacle_collision_count}회. "
                f"감속 시작 거리를 20% 상향해 충돌 여유 확보."
            ),
        ))

    # 규칙 4: Stuck — 장애물 감속 속도 낮추기 (더 느리게 통과)
    if statistics.terminal_reason == "Stuck" or statistics.failure_type == "Stuck":
        recs.append(BotSetupRecommendation(
            param="obstacle_slow_speed_kmh",
            current=current_obstacle_slow_speed_kmh,
            suggested=_round(current_obstacle_slow_speed_kmh * 0.8),
            reason=(
                "로봇이 Stuck(이동 불가) 상태로 종료. 장애물 구간 목표 속도를 20% 낮춰 "
                "저속 통과를 유도."
            ),
        ))

    # 규칙 5: Timeout — 목표 속도 상향
    if statistics.terminal_reason == "Timeout":
        recs.append(BotSetupRecommendation(
            param="target_speed_kmh",
            current=current_target_speed_kmh,
            suggested=_round(current_target_speed_kmh * 1.1),
            reason=(
                f"제한시간({statistics.duration_s:.1f}s) 초과로 종료. "
                f"경로 추종 목표 속도를 10% 상향해 배송 시간 단축."
            ),
        ))

    # 규칙 6: 전복 — 최대 속도 하향
    if statistics.robot_tip_over_count > 0:
        recs.append(BotSetupRecommendation(
            param="max_speed_kmh",
            current=current_max_speed_kmh,
            suggested=_round(current_max_speed_kmh * 0.8),
            reason=(
                f"로봇 전복 {statistics.robot_tip_over_count}회. "
                f"최대 속도를 20% 낮춰 안정성 확보."
            ),
        ))

    # 규칙 7: 경로 탐색 실패 — look ahead 거리 상향
    if statistics.terminal_reason == "PathFindingFailed" or statistics.failure_type == "PathFindingFailed":
        recs.append(BotSetupRecommendation(
            param="look_ahead_distance_m",
            current=current_look_ahead_distance_m,
            suggested=_round(current_look_ahead_distance_m * 1.2),
            reason=(
                "경로 탐색 실패로 종료. look ahead 거리를 20% 늘려 "
                "경로 추종 시야를 넓힘."
            ),
        ))

    return recs


def build_episode_fallback_summary(recommendations: list[BotSetupRecommendation]) -> str:
    if not recommendations:
        return "fallback 규칙 기준으로 조정이 필요한 파라미터가 없음."
    params = ", ".join(rec.param for rec in recommendations)
    return f"fallback 규칙 기반으로 {len(recommendations)}개 파라미터 조정 권장: {params}"


# ── EpisodeSetup 환경/평가 파라미터 fallback 규칙 ─────────────────────────

def apply_episode_setup_fallback_rules(
    episode_stats: EvaluationReportStatistics,
    measurement_stats: AnalysisStatistics | None,
    current_time_limit_s: float = 60.0,
    current_goal_acceptance_radius_m: float = 1.0,
    current_near_miss_distance_m: float = 0.5,
    current_tip_over_angle_deg: float = 60.0,
    current_pedestrian_speed_mps: float | None = None,
) -> list[EpisodeSetupRecommendation]:
    """EpisodeSetup 측 환경/평가 파라미터 결정론적 추천."""
    recs: list[EpisodeSetupRecommendation] = []

    # 규칙 A1: 보행자 충돌이 있었음 → 보행자 속도 낮춤 + near_miss 거리 키움
    if episode_stats.pedestrian_collision_count > 0:
        if current_pedestrian_speed_mps is not None:
            recs.append(EpisodeSetupRecommendation(
                param="actors.pedestrians[*].movement.speed_mps",
                current=current_pedestrian_speed_mps,
                suggested=_round(max(0.8, current_pedestrian_speed_mps * 0.85)),
                reason=(
                    f"보행자 충돌 {episode_stats.pedestrian_collision_count}회. "
                    f"다음 회차 검증에서는 보행자 속도를 15% 낮춰 회피 여유를 확보."
                ),
                citations=["PRJ-DOE", "KOR-003"],
            ))
        recs.append(EpisodeSetupRecommendation(
            param="evaluation.near_miss.distance_m",
            current=current_near_miss_distance_m,
            suggested=_round(min(0.8, current_near_miss_distance_m + 0.2)),
            reason=(
                "보행자 충돌 발생. 평가 기준의 near_miss 거리를 0.2m 키워 "
                "다음 회차 평가가 더 엄격해지도록 조정."
            ),
            citations=["PRJ-EVAL"],
        ))

    # 규칙 A2: Timeout 또는 goal 미달성 → time_limit 상향
    if (
        episode_stats.terminal_reason == "Timeout"
        or (not episode_stats.goal_reached and episode_stats.score < 0)
    ):
        recs.append(EpisodeSetupRecommendation(
            param="run.time_limit_s",
            current=current_time_limit_s,
            suggested=_round(current_time_limit_s * 1.5, 1),
            reason=(
                f"종료사유 {episode_stats.terminal_reason}, goal_reached={episode_stats.goal_reached}. "
                f"시간 한도를 50% 상향해 경로 완주 가능성을 확보."
            ),
            citations=["PRJ-DOE"],
        ))

    # 규칙 A3: 전복 발생 → tip_over_angle 검토 (안전 한계는 더 낮추지 않음 — 위로만 60 상한)
    if episode_stats.robot_tip_over_count > 0:
        recs.append(EpisodeSetupRecommendation(
            param="evaluation.tip_over_angle_deg",
            current=current_tip_over_angle_deg,
            suggested=_round(max(45.0, current_tip_over_angle_deg - 5.0), 1),
            reason=(
                f"로봇 전복 {episode_stats.robot_tip_over_count}회. "
                f"평가 임계 각도를 5deg 낮춰 더 보수적인 안정성 평가를 유도."
            ),
            citations=["PRJ-EVAL"],
        ))

    # 규칙 A4: near_miss 최소거리가 매우 가까웠음 → near_miss 거리 키움 (충돌 없을 때)
    if (
        episode_stats.pedestrian_collision_count == 0
        and episode_stats.near_miss_min_distance_m is not None
        and episode_stats.near_miss_min_distance_m < 0.4
    ):
        recs.append(EpisodeSetupRecommendation(
            param="evaluation.near_miss.distance_m",
            current=current_near_miss_distance_m,
            suggested=_round(min(0.8, current_near_miss_distance_m + 0.1)),
            reason=(
                f"보행자 최소 근접거리 {episode_stats.near_miss_min_distance_m:.2f}m로 "
                f"임계(0.4m) 미만. near_miss 거리를 0.1m 키워 더 엄격하게 평가."
            ),
            citations=["PRJ-EVAL"],
        ))

    return recs


def build_episode_setup_fallback_summary(
    recommendations: list[EpisodeSetupRecommendation],
) -> str:
    if not recommendations:
        return "EpisodeSetup fallback: 조정 필요 없음."
    params = ", ".join(rec.param for rec in recommendations)
    return f"EpisodeSetup fallback 규칙으로 {len(recommendations)}개 권장: {params}"


# ── PolicyServer 로직/임계값 fallback 규칙 ────────────────────────────────

def apply_policy_server_fallback_rules(
    episode_stats: EvaluationReportStatistics,
    measurement_stats: AnalysisStatistics | None,
    current_stop_distance_m_threshold: float = 1.2,
    current_slow_down_distance_m_threshold: float = 3.5,
    current_repath_grace_time_s: float = 1.0,
    current_force_action_override: str | None = None,
) -> list[PolicyServerRecommendation]:
    """policy_server.py 임계값/로직 결정론적 추천."""
    recs: list[PolicyServerRecommendation] = []

    # 규칙 B1: FORCED_ACTION이 설정되어 있으면 운영 회차에서는 해제 권장
    if current_force_action_override is not None:
        recs.append(PolicyServerRecommendation(
            param="force_action_override",
            current=current_force_action_override,
            suggested="None",
            reason=(
                f"FORCED_ACTION='{current_force_action_override}'가 설정되어 있어 정책서버가 "
                "정상 판단 로직을 우회 중. 운영/평가 회차에서는 해제해 fallback 규칙으로 평가."
            ),
            citations=["PRJ-AGENT"],
        ))

    # 규칙 B2: 정적 장애물 충돌 또는 brake 비율 과다 → 정지 임계값 상향
    if episode_stats.static_obstacle_collision_count > 0 or (
        measurement_stats is not None and measurement_stats.brakeAppliedRatio > 0.3
    ):
        recs.append(PolicyServerRecommendation(
            param="stop_distance_m_threshold",
            current=current_stop_distance_m_threshold,
            suggested=_round(current_stop_distance_m_threshold * 1.2),
            reason=(
                f"정적 장애물 충돌 {episode_stats.static_obstacle_collision_count}회. "
                "정책서버 stop 임계값을 20% 상향, LiDAR stop_distance_m과 동기화 권고."
            ),
            citations=["PRJ-AGENT", "KOR-003"],
        ))

    # 규칙 B3: 감속 비율 과다 → 감속 임계값 축소 (필요 이상 일찍 감속)
    if measurement_stats is not None and measurement_stats.slowdownActionRatio > 0.5:
        recs.append(PolicyServerRecommendation(
            param="slow_down_distance_m_threshold",
            current=current_slow_down_distance_m_threshold,
            suggested=_round(current_slow_down_distance_m_threshold * 0.9),
            reason=(
                f"감속 비율 {measurement_stats.slowdownActionRatio:.1%}로 임계(50%) 초과. "
                "감속 시작 임계값을 10% 축소해 불필요한 감속 빈도 감소."
            ),
            citations=["PRJ-AGENT"],
        ))

    # 규칙 B4: 재경로 비율 과다 → repath grace 시간 상향
    if measurement_stats is not None and measurement_stats.repathActionRatio > 0.1:
        recs.append(PolicyServerRecommendation(
            param="repath_grace_time_s",
            current=current_repath_grace_time_s,
            suggested=_round(current_repath_grace_time_s + 0.5, 1),
            reason=(
                f"재경로 비율 {measurement_stats.repathActionRatio:.1%}로 임계(10%) 초과. "
                "repath grace 시간을 0.5s 늘려 재경로 진동(oscillation) 억제."
            ),
            citations=["PRJ-AGENT", "PRJ-DOE"],
        ))

    return recs


def build_policy_server_fallback_summary(
    recommendations: list[PolicyServerRecommendation],
) -> str:
    if not recommendations:
        return "PolicyServer fallback: 조정 필요 없음."
    params = ", ".join(rec.param for rec in recommendations)
    return f"PolicyServer fallback 규칙으로 {len(recommendations)}개 권장: {params}"
