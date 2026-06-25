from __future__ import annotations

from dataclasses import dataclass
from enum import StrEnum
from typing import Literal


class RepairDiagnosticCode(StrEnum):
    """Stable repair event codes used by tests and internal diagnostics."""

    LEGACY_POSE_MIGRATED = "legacy_pose_migrated"
    LEGACY_POSE_REMOVED_DUE_TO_EXISTING_AT = "legacy_pose_removed_due_to_existing_at"
    PROP_NORMALIZED = "prop_normalized"
    ROBOT_ANCHOR_NORMALIZED = "robot_anchor_normalized"
    LANE_HINT_NORMALIZED = "lane_hint_normalized"
    RANGE_SWAPPED = "range_swapped"
    OBSTACLE_RELOCATED_ANCHOR_CLEARANCE = "obstacle_relocated_anchor_clearance"
    OBSTACLE_REMOVED_ANCHOR_CLEARANCE = "obstacle_removed_anchor_clearance"
    OBSTACLE_OVERLAP_REDISTRIBUTED = "obstacle_overlap_redistributed"
    MIN_CLEAR_WIDTH_OFFSET_ADJUSTED = "min_clear_width_offset_adjusted"
    OBSTACLE_REMOVED_MIN_CLEAR_WIDTH = "obstacle_removed_min_clear_width"
    OBSTACLE_REMOVED_INVALID_ANCHOR = "obstacle_removed_invalid_anchor"
    OBSTACLE_REMOVED_NO_VALID_INTERVAL = "obstacle_removed_no_valid_interval"


RepairDiagnosticSeverity = Literal["info", "warning"]
"""Allowed severity labels for internal repair diagnostics."""


@dataclass(frozen=True)
class RepairDiagnostic:
    """One compact internal event describing a deterministic repair decision."""

    code: str
    stage: str
    path: str
    target_id: str | None
    reason: str
    before: str | None
    after: str | None
    severity: RepairDiagnosticSeverity = "info"

    def as_dict(self) -> dict[str, str | None]:
        """Return a JSON-friendly representation for graph state and tests."""
        return {
            "code": self.code,
            "stage": self.stage,
            "path": self.path,
            "target_id": self.target_id,
            "reason": self.reason,
            "before": self.before,
            "after": self.after,
            "severity": self.severity,
        }


class RepairDiagnosticCollector:
    """Append-only collector for repair events created during one generation run."""

    def __init__(self) -> None:
        """Create an empty repair event collector."""
        self._events: list[RepairDiagnostic] = []

    @property
    def events(self) -> tuple[RepairDiagnostic, ...]:
        """Return collected events without allowing callers to mutate storage."""
        return tuple(self._events)

    def add(
        self,
        code: RepairDiagnosticCode,
        *,
        stage: str,
        path: str,
        target_id: str | None,
        reason: str,
        before: str | None,
        after: str | None,
        severity: RepairDiagnosticSeverity = "info",
    ) -> None:
        """Append one event when a repair changed a compact scalar/range value."""
        self._events.append(
            RepairDiagnostic(
                code=code.value,
                stage=stage,
                path=path,
                target_id=target_id,
                reason=reason,
                before=before,
                after=after,
                severity=severity,
            )
        )

    def as_dicts(self) -> list[dict[str, str | None]]:
        """Return all collected events as JSON-friendly dictionaries."""
        return [event.as_dict() for event in self._events]


def summarize_repair_events(events: list[dict[str, object]]) -> dict[str, int]:
    """Count repair events by code for coarse diagnostics."""
    counts: dict[str, int] = {}
    for event in events:
        code = event.get("code")
        if isinstance(code, str):
            counts[code] = counts.get(code, 0) + 1
    return counts


def format_repair_event_summary(events: list[dict[str, object]]) -> str:
    """Format repair event counts as a stable single-line summary."""
    counts = summarize_repair_events(events)
    return ", ".join(f"{code}={count}" for code, count in sorted(counts.items()))
