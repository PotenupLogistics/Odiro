from __future__ import annotations

from typing import Any


def select_policy_candidate(candidates: list[dict[str, Any]]) -> dict[str, Any] | None:
    valid_candidates = [
        candidate
        for candidate in candidates
        if isinstance(candidate, dict) and isinstance(candidate.get("action"), dict)
    ]

    if not valid_candidates:
        return None

    return min(valid_candidates, key=lambda candidate: int(candidate.get("priority", 100) or 100))
