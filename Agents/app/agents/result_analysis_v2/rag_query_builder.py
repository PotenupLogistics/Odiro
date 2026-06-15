from __future__ import annotations

PATTERN_QUERY_TYPE_MAP = {
    "blocked_region_violation_repeated": "policy_safety",
    "near_miss_repeated": "pedestrian_safety",
    "collision_repeated": "collision_prevention",
    "timeout_repeated": "navigation_efficiency",
    "goal_not_reached_repeated": "navigation_efficiency",
}

QUERY_DETAILS = {
    "policy_safety": {
        "keywords": ["blocked region violation", "narrow sidewalk", "obstacle avoidance", "stop first policy"],
        "natural_language_query": "좁은 보도에서 장애물 회피 중 주행 가능 영역을 벗어나는 경우 적용할 수 있는 정지 우선 정책과 안전 기준을 검색한다.",
        "expected_context": ["보도 주행 안전 기준", "장애물 회피 정책", "정지 우선 조건", "주행 가능 영역 이탈 페널티"],
    },
    "pedestrian_safety": {
        "keywords": ["near miss", "pedestrian safety", "sidewalk robot", "minimum distance"],
        "natural_language_query": "보행자 근접 위험이 반복될 때 적용할 수 있는 감속, 정지, 최소 이격거리 기준을 검색한다.",
        "expected_context": ["보행자 안전 기준", "최소 이격거리", "감속 조건", "정지 조건"],
    },
    "collision_prevention": {
        "keywords": ["collision prevention", "static obstacle", "pedestrian collision", "avoidance policy"],
        "natural_language_query": "충돌이 반복되는 에피소드에서 회피 정책과 충돌 예방 안전 기준을 검색한다.",
        "expected_context": ["충돌 예방 정책", "장애물 회피 기준", "보행자 충돌 방지", "안전 정지"],
    },
    "navigation_efficiency": {
        "keywords": ["timeout", "goal not reached", "navigation efficiency", "route planning"],
        "natural_language_query": "목표 미도달 또는 timeout이 반복될 때 경로 계획과 주행 효율 개선 기준을 검색한다.",
        "expected_context": ["경로 계획 개선", "timeout 완화", "목표 도달률", "정체 감지"],
    },
    "general_safety": {
        "keywords": ["sidewalk robot safety", "policy recommendation", "risk mitigation"],
        "natural_language_query": "실외이동로봇 주행 실패 패턴에 적용 가능한 일반 안전 정책과 개선 기준을 검색한다.",
        "expected_context": ["일반 안전 정책", "위험 완화", "운행 안전 기준"],
    },
}


class RagQueryBuilderV2:
    def build_queries(
        self,
        *,
        failure_patterns: list[dict],
        experiment_aggregates: list[dict],
        representative_failed_episodes: list[dict],
    ) -> list[dict]:
        queries = [self.build_query_for_pattern(pattern) for pattern in failure_patterns]
        for index, query in enumerate(queries, start=1):
            query["query_id"] = f"RAGQ-{index:03d}"
        return queries

    def build_query_for_pattern(
        self,
        pattern: dict,
    ) -> dict:
        pattern_type = str(pattern.get("type") or "unknown")
        query_type = PATTERN_QUERY_TYPE_MAP.get(pattern_type, "general_safety")
        details = QUERY_DETAILS[query_type]
        return {
            "query_id": "RAGQ-001",
            "pattern_id": str(pattern.get("pattern_id") or "PATTERN-UNKNOWN"),
            "query_type": query_type,
            "keywords": list(details["keywords"]),
            "natural_language_query": str(details["natural_language_query"]),
            "expected_context": list(details["expected_context"]),
            "source_pattern_type": pattern_type,
        }
