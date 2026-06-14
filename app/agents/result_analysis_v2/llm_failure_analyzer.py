from __future__ import annotations

from typing import Any


class LlmFailureAnalyzer:
    def analyze(self, context: dict[str, Any]) -> dict[str, Any]:
        patterns = context.get("failure_patterns", [])
        if not patterns:
            return {"recommendations": [], "explanation": "반복 실패 패턴이 확인되지 않았습니다."}
        return {
            "recommendations": [],
            "explanation": "반복 패턴은 확인되었지만 MVP 경로에서는 근거 기반 자동 수정안을 생성하지 않습니다.",
        }

