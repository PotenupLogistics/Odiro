"""policy_server.py 소스에서 디폴트/임계값 추출.

LLM·fallback 추천에서 "현재 정책서버 설정"을 보여주기 위한 가벼운 정적 분석.
AST가 아닌 regex 기반 — 파일이 바뀌어도 안전하게 best-effort로 동작한다.
실패해도 빈 dict 또는 CLAUDE.md 고정값을 반환한다.
"""
from __future__ import annotations

import re
from pathlib import Path
from typing import Any


CLAUDE_MD_DEFAULTS: dict[str, Any] = {
    "stop_distance_m_threshold": 1.2,
    "slow_down_distance_m_threshold": 3.5,
    "max_speed_kmh": 10.0,
    "force_action_override": None,
}


_FORCED_ACTION_RE = re.compile(
    r"""FORCED_ACTION\s*=\s*(None|"[^"]*"|'[^']*')""",
    re.MULTILINE,
)
_STOP_DISTANCE_RE = re.compile(
    r"""stopDistanceM["']?\s*,\s*([0-9]+(?:\.[0-9]+)?)""",
)
_SLOW_DOWN_DISTANCE_RE = re.compile(
    r"""slowDownDistanceM["']?\s*,\s*([0-9]+(?:\.[0-9]+)?)""",
)


def _strip_quotes(value: str) -> str | None:
    value = value.strip()
    if value == "None":
        return None
    if (value.startswith('"') and value.endswith('"')) or (
        value.startswith("'") and value.endswith("'")
    ):
        return value[1:-1]
    return value


def extract_policy_defaults_from_source(path: Path | str | None) -> dict[str, Any]:
    """policy_server.py에서 force_action_override·임계값을 best-effort로 추출.

    파일이 없거나 읽기 실패 시 CLAUDE_MD_DEFAULTS 사본을 반환.
    """
    defaults = dict(CLAUDE_MD_DEFAULTS)
    if path is None:
        return defaults
    source_path = Path(path)
    if not source_path.is_file():
        return defaults

    try:
        source = source_path.read_text(encoding="utf-8")
    except OSError:
        return defaults

    match = _FORCED_ACTION_RE.search(source)
    if match is not None:
        defaults["force_action_override"] = _strip_quotes(match.group(1))

    stop_match = _STOP_DISTANCE_RE.search(source)
    if stop_match is not None:
        try:
            defaults["stop_distance_m_threshold"] = float(stop_match.group(1))
        except ValueError:
            pass

    slow_match = _SLOW_DOWN_DISTANCE_RE.search(source)
    if slow_match is not None:
        try:
            defaults["slow_down_distance_m_threshold"] = float(slow_match.group(1))
        except ValueError:
            pass

    return defaults
