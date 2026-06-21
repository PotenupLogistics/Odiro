"""Builds natural-language API-only analysis summaries."""

from __future__ import annotations

from typing import Any


class AnalysisTextBuilder:
    """Creates user-facing Korean report text for analysis responses."""

    def build(
        self,
        *,
        recommendation_type: str,
        artifacts: dict,
        artifact_warnings: list[str] | None = None,
        metrics: Any | None = None,
        episodes_count: int | None = None,
        patterns: list[dict[str, Any]] | None = None,
        findings: list[dict[str, Any]] | None = None,
        evidence: list[dict[str, Any]] | None = None,
    ) -> str:
        """Return deterministic UI report text without exposing JSON structure."""
        artifact_warnings = artifact_warnings or []
        patterns = patterns or []
        findings = findings or []
        evidence = evidence or []
        metric_values = self._metrics(metrics)
        finding_types = {str(finding.get("type")) for finding in findings}

        if recommendation_type == "insufficient_data":
            return self._sections(
                summary="이번 Run은 분석 가능한 로그가 부족해 실패 원인을 판단하기 어렵습니다.",
                evidence_text="result.json 또는 events.jsonl 같은 result/events 실행 로그가 누락되었거나 손상되었습니다.",
                judgement="현재 데이터만으로는 실패 원인이나 추천 유형을 확정하기 어렵습니다.",
                recommendation="result/events 로그와 실행 로그가 정상적으로 저장되었는지 확인한 뒤 다시 분석하는 것을 권장합니다.",
            )
        if recommendation_type == "policy_review":
            recommendation = "review 폴더에 정책 수정 후보가 생성되었습니다."
            success_safety_case = self._success_safety_case(metric_values)
            if artifacts.get("policy", {}).get("generated"):
                if success_safety_case:
                    recommendation = (
                        f"{recommendation} 성공한 주행에서도 패널티 구역 침범 또는 경로 재탐색이 반복되는지 확인하고, "
                        "경로 추종 조건과 위험 영역 회피 조건을 보수적으로 조정해 재검증하는 것을 권장합니다."
                    )
                else:
                    recommendation = f"{recommendation} 생성된 policy 후보로 동일 조건에서 다시 실행하여 실패 지표가 줄어드는지 비교하는 것을 권장합니다."
            else:
                recommendation = "주행 정책 판단과 관련된 근거가 확인되었지만 정책 원본을 찾지 못해 수정 후보는 생성되지 않았습니다."
            summary = "이번 Run에서는 주행 정책 검토가 필요한 실패 근거가 확인되었습니다."
            if success_safety_case:
                summary = "이번 Run은 주행에 성공했지만 안전/정책 검토가 필요한 근거가 확인되었습니다."
            return self._sections(
                summary=summary,
                evidence_text=self._policy_evidence_text(metric_values=metric_values, finding_types=finding_types),
                judgement="현재 결과는 주행 경로 추종, 감속/정지 조건, 재경로 탐색 조건을 보수적으로 조정해볼 필요가 있는 케이스로 판단됩니다.",
                recommendation=recommendation,
            )
        if recommendation_type == "environment_review":
            recommendation = "review 폴더에 환경 수정 후보가 생성되었습니다."
            if artifacts.get("environment", {}).get("generated"):
                recommendation = f"{recommendation} 생성된 scenario.json 후보로 다시 실행하여 충돌과 제한 시간 초과가 줄어드는지 비교하는 것을 권장합니다."
            elif self._has_warning(artifact_warnings, "scenario source file could not be parsed"):
                recommendation = "환경 또는 장애물 배치와 관련된 근거가 확인되었지만 scenario.json을 읽는 중 오류가 발생해 수정 후보는 생성되지 않았습니다."
            else:
                recommendation = "환경 또는 장애물 배치와 관련된 근거가 확인되었지만 scenario.json을 찾지 못해 수정 후보는 생성되지 않았습니다."
            return self._sections(
                summary="이번 Run에서는 환경 또는 장애물 배치 검토가 필요한 실패 근거가 확인되었습니다.",
                evidence_text=self._environment_evidence_text(
                    metric_values=metric_values,
                    episodes_count=episodes_count,
                    patterns=patterns,
                    evidence=evidence,
                ),
                judgement="이번 결과는 단순한 정책 미세 조정보다 시나리오의 장애물 배치, 통로 폭, 유효 주행 공간 조건을 먼저 검토해야 하는 케이스로 판단됩니다.",
                recommendation=recommendation,
            )
        return self._sections(
            summary="이번 Run에서는 정책이나 환경을 수정해야 한다고 판단할 만한 반복 근거가 확인되지 않았습니다.",
            evidence_text="분석 가능한 로그에서 충돌, near miss, penalty region 침범, 제한 시간 초과와 같은 주요 문제가 반복적으로 나타나지 않았습니다.",
            judgement="현재 로그 기준으로는 별도 수정 후보를 생성하지 않는 것이 적절합니다.",
            recommendation="동일 조건에서 추가 실행을 통해 결과가 안정적으로 유지되는지 확인하는 것을 권장합니다.",
        )

    def _has_warning(self, warnings: list[str], needle: str) -> bool:
        """Detect a non-fatal artifact condition without exposing raw warning details."""
        return any(needle in warning for warning in warnings)

    def _sections(self, *, summary: str, evidence_text: str, judgement: str, recommendation: str) -> str:
        """Format the stable UI report sections."""
        return (
            f"[결과 요약]\n{summary}\n\n"
            f"[주요 근거]\n{evidence_text}\n\n"
            f"[판단]\n{judgement}\n\n"
            f"[추천]\n{recommendation}"
        )

    def _metrics(self, metrics: Any | None) -> dict[str, int]:
        """Read public response metrics from either a pydantic model or dict."""
        if metrics is None:
            return {}
        if hasattr(metrics, "model_dump"):
            values = metrics.model_dump()
        elif isinstance(metrics, dict):
            values = metrics
        else:
            values = {}
        return {key: int(value) for key, value in values.items() if isinstance(value, int | float)}

    def _success_safety_case(self, metric_values: dict[str, int]) -> bool:
        """Detect successful runs that still have policy or safety review evidence."""
        return metric_values.get("success_count", 0) > 0 and metric_values.get("failure_count", 0) == 0

    def _policy_evidence_text(self, *, metric_values: dict[str, int], finding_types: set[str]) -> str:
        """Summarize policy-related evidence without listing raw ids."""
        signals: list[str] = []
        if metric_values.get("penalty_region_violation_count", 0):
            signals.append(f"패널티 구역 침범 {metric_values['penalty_region_violation_count']}회")
        if metric_values.get("pedestrian_collision_count", 0):
            signals.append(f"보행자 충돌 {metric_values['pedestrian_collision_count']}회")
        if "timeout" in finding_types:
            signals.append("제한 시간 초과")
        if "stuck" in finding_types:
            signals.append("정체")
        if "goal_not_reached" in finding_types:
            signals.append("목표 미도달")
        if metric_values.get("near_miss_count", 0):
            signals.append(f"근접 위험 {metric_values['near_miss_count']}회")
        if metric_values.get("repath_count", 0):
            signals.append(f"경로 재탐색 {metric_values['repath_count']}회")
        if metric_values.get("robot_tip_over_count", 0):
            signals.append(f"로봇 전도 {metric_values['robot_tip_over_count']}회")
        if not signals:
            signals.append("정책 관련 검토 신호")
        return f"분석 로그에서 {', '.join(signals)}가 확인되었습니다."

    def _environment_evidence_text(
        self,
        *,
        metric_values: dict[str, int],
        episodes_count: int | None,
        patterns: list[dict[str, Any]],
        evidence: list[dict[str, Any]],
    ) -> str:
        """Summarize environment-related evidence using available counts."""
        parts: list[str] = []
        static_collision_count = self._evidence_metric_total(
            evidence,
            "static_obstacle_collision_count",
        ) or metric_values.get("static_obstacle_collision_count", 0)
        pedestrian_collision_count = self._evidence_metric_total(
            evidence,
            "pedestrian_collision_count",
        ) or metric_values.get("pedestrian_collision_count", 0)
        collision_signals: list[str] = []
        if static_collision_count:
            collision_signals.append(f"정적 장애물 충돌 {static_collision_count}회")
        if pedestrian_collision_count:
            collision_signals.append(f"보행자 충돌 {pedestrian_collision_count}회")
        if len(collision_signals) > 1:
            parts.append(f"로그에서 {self._join_signals(collision_signals)}가 확인되었습니다.")
        elif static_collision_count:
            parts.append(f"로그에서 정적 장애물 충돌이 총 {static_collision_count}회 확인되었습니다.")
        elif pedestrian_collision_count:
            parts.append(f"로그에서 보행자 충돌이 총 {pedestrian_collision_count}회 확인되었습니다.")
        elif collision_count := metric_values.get("collision_count", 0):
            parts.append(f"로그에서 충돌이 총 {collision_count}회 확인되었습니다.")
        blocked_count = metric_values.get("blocked_region_violation_count", 0)
        if blocked_count:
            parts.append(f"차단 구역 충돌 또는 침범은 {blocked_count}회 확인되었습니다.")
        timeout_count = self._pattern_count(patterns, "timeout_repeated")
        if timeout_count:
            parts.append(f"제한 시간 초과로 종료된 에피소드가 {timeout_count}개 확인되었습니다.")
        stuck_count = self._pattern_count(patterns, "stuck_repeated")
        if stuck_count:
            parts.append(f"정체 이벤트도 {stuck_count}개 에피소드에서 반복적으로 발생했습니다.")
        repath_count = metric_values.get("repath_count", 0)
        if repath_count:
            parts.append(f"경로 재탐색 이벤트도 {repath_count}회 확인되었습니다.")
        quiet_signals = []
        if metric_values.get("penalty_region_violation_count", 0) == 0:
            quiet_signals.append("패널티 구역 침범")
        if blocked_count == 0:
            quiet_signals.append("차단 구역 충돌")
        if metric_values.get("near_miss_count", 0) == 0:
            quiet_signals.append("근접 위험")
        if quiet_signals:
            parts.append(f"반면 {', '.join(quiet_signals)} 항목은 확인되지 않았습니다.")
        return " ".join(parts) if parts else "환경 또는 장애물 배치와 관련된 실패 근거가 확인되었습니다."

    def _join_signals(self, signals: list[str]) -> str:
        """Join Korean evidence phrases without exposing raw metric keys."""
        if len(signals) <= 1:
            return "".join(signals)
        if len(signals) == 2:
            return f"{signals[0]}와 {signals[1]}"
        return f"{', '.join(signals[:-1])}와 {signals[-1]}"

    def _evidence_metric_total(self, evidence: list[dict[str, Any]], metric: str) -> int:
        """Sum a metric from report evidence values."""
        total = 0
        for item in evidence:
            if item.get("metric") == metric and isinstance(item.get("value"), int | float):
                total += int(item["value"])
        return total

    def _evidence_episode_count(self, evidence: list[dict[str, Any]], metric: str) -> int:
        """Count unique episodes that contributed a metric evidence item."""
        episode_keys = {
            (str(item.get("run_id")), str(item.get("episode_id")))
            for item in evidence
            if item.get("metric") == metric and item.get("run_id") and item.get("episode_id")
        }
        return len(episode_keys)

    def _pattern_count(self, patterns: list[dict[str, Any]], pattern_type: str) -> int:
        """Return a repeated pattern count by type."""
        for pattern in patterns:
            if pattern.get("type") == pattern_type and isinstance(pattern.get("count"), int | float):
                return int(pattern["count"])
        return 0
