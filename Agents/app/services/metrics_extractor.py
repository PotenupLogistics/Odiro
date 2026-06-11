from __future__ import annotations

import math
from collections import Counter
from typing import Any

from app.models.recommendation import AnalysisStatistics
from app.models.run_result import RunMetrics
from app.services.measurement_log_parser import MeasurementLogData


CM_PER_S_TO_KMH = 0.036


def _lidar(tick: dict[str, Any]) -> dict[str, Any]:
    return tick.get("robot", {}).get("perception", {}).get("lidar", {}) or {}


def _action(tick: dict[str, Any]) -> dict[str, Any]:
    return tick.get("robot", {}).get("action", {}) or {}


def _truth(tick: dict[str, Any]) -> dict[str, Any]:
    return tick.get("robot", {}).get("truth", {}) or {}


def _velocity_kmh(tick: dict[str, Any]) -> float:
    v = _truth(tick).get("v") or [0.0, 0.0, 0.0]
    if len(v) < 3:
        return 0.0
    speed_cm_s = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
    return speed_cm_s * CM_PER_S_TO_KMH


def _median(values: list[float]) -> float:
    if not values:
        return 0.0
    sorted_values = sorted(values)
    middle = len(sorted_values) // 2
    if len(sorted_values) % 2 == 0:
        return (sorted_values[middle - 1] + sorted_values[middle]) / 2.0
    return sorted_values[middle]


def _variance(values: list[float]) -> float:
    if len(values) < 2:
        return 0.0
    mean = sum(values) / len(values)
    return sum((value - mean) ** 2 for value in values) / len(values)


def extract_statistics(data: MeasurementLogData) -> AnalysisStatistics:
    ticks = data.ticks
    total_ticks = len(ticks)

    delta_sum = sum(float(tick.get("deltaSeconds") or 0.0) for tick in ticks)
    delivery_time = delta_sum
    if delivery_time <= 0:
        last_world_time = float(ticks[-1].get("worldTimeSeconds") or 0.0)
        delivery_time = last_world_time

    front_distances: list[float] = []
    detected_ticks = 0
    for tick in ticks:
        lidar = _lidar(tick)
        if lidar.get("hasFrontObject"):
            detected_ticks += 1
            front_distance = float(lidar.get("frontDistanceM") or 0.0)
            front_distances.append(front_distance)

    if front_distances:
        avg_front = sum(front_distances) / len(front_distances)
        min_front = min(front_distances)
        median_front = _median(front_distances)
    else:
        avg_front = 0.0
        min_front = 0.0
        median_front = 0.0

    detection_rate = detected_ticks / total_ticks if total_ticks else 0.0

    reason_counter: Counter[str] = Counter()
    brake_applied_count = 0
    target_speeds: list[float] = []
    for tick in ticks:
        action = _action(tick)
        reason = str(action.get("reason") or "unknown")
        reason_counter[reason] += 1
        if action.get("brakeApplied"):
            brake_applied_count += 1
        target_speeds.append(float(action.get("targetSpeedKmh") or 0.0))

    actual_speeds = [_velocity_kmh(tick) for tick in ticks]

    avg_target_speed = sum(target_speeds) / len(target_speeds) if target_speeds else 0.0
    target_speed_variance = _variance(target_speeds)
    avg_actual_speed = sum(actual_speeds) / len(actual_speeds) if actual_speeds else 0.0

    brake_ratio = brake_applied_count / total_ticks if total_ticks else 0.0

    def _ratio(reason_name: str) -> float:
        return reason_counter.get(reason_name, 0) / total_ticks if total_ticks else 0.0

    diagnostics = data.footer.get("diagnostics") or []
    warning_count = sum(
        1 for diagnostic in diagnostics if str(diagnostic.get("severity")) == "warning"
    )

    return AnalysisStatistics(
        totalTicks=total_ticks,
        deliveryTimeSec=delivery_time,
        closeReason=str(data.footer.get("closeReason") or "unknown"),
        diagnosticsWarningCount=warning_count,
        avgFrontDistanceM=avg_front,
        minFrontDistanceM=min_front,
        medianFrontDistanceM=median_front,
        frontObjectDetectionRate=detection_rate,
        actionReasonFreq=dict(reason_counter),
        brakeAppliedCount=brake_applied_count,
        brakeAppliedRatio=brake_ratio,
        avgTargetSpeedKmh=avg_target_speed,
        targetSpeedVariance=target_speed_variance,
        avgActualSpeedKmh=avg_actual_speed,
        stopActionRatio=_ratio("stop"),
        slowdownActionRatio=_ratio("slowdown"),
        repathActionRatio=_ratio("repath"),
    )


def extract_run_metrics(
    data: MeasurementLogData,
    statistics: AnalysisStatistics,
) -> RunMetrics:
    """기존 RunMetrics 스키마로 매핑. 일부 필드는 로그에 직접 없어서 보수적으로 0/False."""
    reason_freq = statistics.actionReasonFreq

    min_pedestrian_distance_cm = statistics.minFrontDistanceM * 100.0

    return RunMetrics(
        collisionCount=0,
        nearMissCount=0,
        minPedestrianDistanceCm=min_pedestrian_distance_cm,
        rerouteCount=reason_freq.get("repath", 0),
        stopCount=reason_freq.get("stop", 0),
        deliveryTimeSec=statistics.deliveryTimeSec,
        manualOverrideRequired=False,
        fallDetected=False,
        sidewalkDepartureCount=0,
    )
