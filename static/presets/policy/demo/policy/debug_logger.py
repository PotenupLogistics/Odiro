from __future__ import annotations

import json
from pathlib import Path
from typing import Any


WATCH_ENABLED = True

# Watch Fields 값이 바뀔 때마다
WATCH_FIELDS = {
    "reason",
    "bColliding",
    "collisionActorName",
    "steering",
    "targetSpeedKmh",
    "direction",
    "frontMinM",
    "pathIndex",
    "targetPathIndex",
    "closestPathDistanceCm",
    "lookAheadDistanceM",
    "recoveryUntilSeconds",
    "obstacleWarning",
    "sensorSequence",
    "sensorSequenceChangeCount",
    "lidarRayCount",
    "selectedLidarPolicyMode",
    "selectedLidarRaySource",
    "selectedLidarRayCount",
    "selectedLidarHorizontalPitchDegree",
    "repathDecision",
    "bRepathRequested",
    "bRepathForced",
    "bRepathFrontObstacle",
    "bRepathNeeded",
    "bRepathCanRunNow",
    "bRepathFrontRayExists",
    "bRepathFrontRayInNearObject",
    "bRepathDebounced",
    "repathFrontRayDistanceM",
    "repathDistanceGapM",
    "dynamicBlockedCellCount",
    "pathfindTotalMs",
    "pathfindSoftCostMs",
    "pathfindSearchMs",
    "pathfindGridCacheHit",
    "pathfindSearchCellCount",
    "pathfindGridCellCount",
    "pathfindBlockedCellCount",
    "pathfindVisitedNodeCount",
}

# 숫자 필드는 이 차이 이상 변했을 때만 출력한다.
WATCH_THRESHOLDS = {
    "targetSpeedKmh": 0.5,
    "steering": 0.1,
    "brake": 0.1,
    "closestPathDistanceCm": 15.0,
    "lookAheadDistanceM": 0.1,
    "recoveryUntilSeconds": 0.1,
    "repathFrontRayDistanceM": 0.2,
    "repathDistanceGapM": 0.2,
    "pathfindTotalMs": 1.0,
    "pathfindSoftCostMs": 1.0,
    "pathfindSearchMs": 1.0,
    "pathfindSearchCellCount": 50.0,
}

# 첫 snapshot에서 이미 이 값보다 커져 있으면 변화로 출력한다.
WATCH_INITIAL_VALUES = {
    "obstacleWarning": 0,
    "blockedCorridor": 0,
    "sensorSequence": -1,
    "sensorSequenceChangeCount": -1,
    "lidarRayCount": -1,
    "selectedLidarPolicyMode": "",
    "selectedLidarRaySource": "",
    "selectedLidarRayCount": -1,
    "selectedLidarHorizontalPitchDegree": None,
    "repathDecision": "",
    "bRepathRequested": False,
    "bRepathForced": False,
    "bRepathFrontObstacle": False,
    "bRepathNeeded": False,
    "bRepathCanRunNow": False,
    "bRepathFrontRayExists": False,
    "bRepathFrontRayInNearObject": False,
    "bRepathDebounced": False,
    "repathFrontRayDistanceM": None,
    "repathDistanceGapM": None,
    "dynamicBlockedCellCount": 0,
    "pathfindTotalMs": 0.0,
    "pathfindSoftCostMs": 0.0,
    "pathfindSearchMs": 0.0,
    "pathfindGridCacheHit": False,
    "pathfindSearchCellCount": 0,
    "pathfindGridCellCount": 0,
    "pathfindBlockedCellCount": 0,
    "pathfindVisitedNodeCount": 0,
}

# 로그 한 줄에 같이 붙여 볼 주변 정보다. 이 필드는 변화 감지 대상이 아니다.
CONTEXT_FIELDS = (
    "reason",
    "policy",
    "bColliding",
    "collisionActorName",
    "sensorSequence",
    "sensorTimeSeconds",
    "sensorSequenceRepeatCount",
    "sensorSequenceChangeCount",
    "lastSensorDeltaSeconds",
    "lidarRayCount",
    "lidarMode",
    "lidarRayPayloadMode",
    "bSendFullLidarRays",
    "lidarRays1dCount",
    "lidarRays2dCount",
    "lidarRays3dCount",
    "legacyLidarRayCount",
    "selectedLidarPolicyMode",
    "selectedLidarRaySource",
    "selectedLidarRayCount",
    "selectedLidarHorizontalPitchDegree",
    "repathDecision",
    "bRepathRequested",
    "bRepathForced",
    "bRepathFrontObstacle",
    "bRepathNeeded",
    "bRepathCanRunNow",
    "bRepathFrontRayExists",
    "bRepathFrontRayInNearObject",
    "bRepathDebounced",
    "repathFrontRayDistanceM",
    "repathFrontRayYawDegree",
    "repathFrontRayActorName",
    "repathNearObjectDistanceM",
    "repathDistanceGapM",
    "repathFrontAngleDegree",
    "repathBlockRadiusCells",
    "dynamicBlockedCellCount",
    "pathfindTotalMs",
    "pathfindCellLookupMs",
    "pathfindSoftCostMs",
    "pathfindSearchMs",
    "pathfindSmoothMs",
    "pathfindGridCacheHit",
    "pathfindSearchCellCount",
    "pathfindGridCellCount",
    "pathfindBlockedCellCount",
    "pathfindSoftCostCellCount",
    "pathfindVisitedNodeCount",
    "pathfindNeighborCheckCount",
    "pathfindOpenPushCount",
    "pathfindPathCellCount",
    "pathfindReason",
    "repathDebounceKey",
    "repathDebounceCount",
    "frontMinM",
    "direction",
    "targetSpeedKmh",
    "steering",
    "brake",
    "pathIndex",
    "pathLength",
    "targetPathIndex",
    "targetWorldPoint",
    "closestPathDistanceCm",
    "maxPathErrorCm",
    "lookAheadDistanceM",
    "recoveryUntilSeconds",
    "lastObstacleWarningSource",
)


class DecisionLogWatcher:
    def __init__(
        self,
        watch_fields: set[str] | None = None,
        thresholds: dict[str, float] | None = None,
        context_fields: tuple[str, ...] | None = None,
        initial_values: dict[str, Any] | None = None,
        jsonl_path: str | None = None,
        enabled: bool = WATCH_ENABLED,
    ):
        self.watchFields = set(WATCH_FIELDS if watch_fields is None else watch_fields)
        self.thresholds = dict(WATCH_THRESHOLDS if thresholds is None else thresholds)
        self.contextFields = CONTEXT_FIELDS if context_fields is None else context_fields
        self.initialValues = dict(WATCH_INITIAL_VALUES if initial_values is None else initial_values)
        self.jsonlPath = Path(jsonl_path) if jsonl_path else None
        self.enabled = enabled
        self.lastSnapshot: dict[str, Any] | None = None

    def reset(self) -> None:
        self.lastSnapshot = None

    def emit_if_changed(self, snapshot: dict[str, Any]) -> None:
        if not self.enabled:
            self.lastSnapshot = dict(snapshot)
            return

        if self.lastSnapshot is None:
            self.lastSnapshot = self.build_initial_snapshot(snapshot)

        changes = self.get_changes(snapshot)
        self.lastSnapshot = dict(snapshot)

        if not changes:
            return

        print(self.format_change_line(snapshot, changes), flush=True)
        self.append_jsonl(snapshot, changes)

    def get_changes(self, snapshot: dict[str, Any]) -> list[dict[str, Any]]:
        changes: list[dict[str, Any]] = []

        for field_name in sorted(self.watchFields):
            if field_name not in snapshot:
                continue

            before = self.lastSnapshot.get(field_name) if self.lastSnapshot else None
            after = snapshot.get(field_name)

            if not self.has_changed(field_name, before, after):
                continue

            changes.append({
                "field": field_name,
                "before": before,
                "after": after,
            })

        return changes

    def build_initial_snapshot(self, snapshot: dict[str, Any]) -> dict[str, Any]:
        initial_snapshot = dict(snapshot)

        for field_name in self.watchFields:
            if field_name in self.initialValues:
                initial_snapshot[field_name] = self.initialValues[field_name]

        return initial_snapshot

    def has_changed(self, field_name: str, before: Any, after: Any) -> bool:
        threshold = self.thresholds.get(field_name)

        if threshold is not None and self.is_number(before) and self.is_number(after):
            return abs(float(after) - float(before)) >= threshold

        return before != after

    def format_change_line(self, snapshot: dict[str, Any], changes: list[dict[str, Any]]) -> str:
        seq = snapshot.get("seq", "?")
        change_text = " ".join(
            f"{change['field']}={self.format_value(change['before'])}->{self.format_value(change['after'])}"
            for change in changes
        )
        context_text = " ".join(
            f"{field_name}={self.format_value(snapshot.get(field_name))}"
            for field_name in self.contextFields
            if snapshot.get(field_name) not in (None, "")
        )

        if context_text:
            return f"[watch] seq={seq} {change_text} {context_text}"

        return f"[watch] seq={seq} {change_text}"

    def append_jsonl(self, snapshot: dict[str, Any], changes: list[dict[str, Any]]) -> None:
        if self.jsonlPath is None:
            return

        self.jsonlPath.parent.mkdir(parents=True, exist_ok=True)
        event = {
            "type": "decision_watch",
            "seq": snapshot.get("seq"),
            "changes": changes,
            "snapshot": snapshot,
        }

        with self.jsonlPath.open("a", encoding="utf-8") as log_file:
            log_file.write(json.dumps(event, ensure_ascii=False) + "\n")

    @staticmethod
    def is_number(value: Any) -> bool:
        return isinstance(value, (int, float)) and not isinstance(value, bool)

    @staticmethod
    def format_value(value: Any) -> str:
        if isinstance(value, float):
            return f"{value:.3f}"

        return str(value)


# 사용 방법:
# 1. 현재 기본 설정은 경로 추종 문제 추적용이다. reason, steering, speed, pathIndex,
#    targetPathIndex, closestPathDistanceCm, recoveryUntilSeconds, obstacleWarning 변화를 출력한다.
# 2. Obstacle warning만 보고 싶으면 WATCH_FIELDS = {"obstacleWarning"} 로 줄인다.
# 3. targetSpeedKmh, steering, brake 같은 숫자 필드는 WATCH_THRESHOLDS 값 이상 변할 때만 출력된다.
# 4. 로그 한 줄에 같이 붙는 참고 정보는 CONTEXT_FIELDS에서 추가하거나 제거한다.
# 5. 파일 저장이 필요하면 user_agent.py의 DecisionLogWatcher(...) 생성자에
#    jsonl_path="Logs/PythonAgent/decision_watch.jsonl" 을 넣는다.
# 6. 설정을 바꾼 뒤에는 실행 중인 PythonAgent 서버를 재시작해야 반영된다.
