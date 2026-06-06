"""policy_server.py 소스 정적 분석.

서버 내부에 고정된 정책 서버 코드(policy_server.py)를 AST/정규식으로 분석해서
FORCED_ACTION 활성화 여부와 기본 거리 임계값을 추출하고, BotSetup의 거리값과
정책 서버 기본값이 어긋나는지(정합성)를 검사한다.

원래 scripts/analyze_test_sample.py 안에 있던 로직을 API에서도 재사용할 수 있도록
서비스 모듈로 분리했다.
"""

from __future__ import annotations

import ast
import re
from pathlib import Path
from typing import Any


def analyze_policy_server_source(path: str | Path) -> dict[str, Any]:
    """policy_server.py 소스를 분석해 FORCED_ACTION / 기본 거리값 / 경고를 추출."""
    src = Path(path).read_text(encoding="utf-8")
    forced_action = None
    try:
        tree = ast.parse(src)
        for node in ast.walk(tree):
            if isinstance(node, ast.Assign):
                for t in node.targets:
                    if isinstance(t, ast.Name) and t.id == "FORCED_ACTION":
                        v = node.value
                        forced_action = v.value if isinstance(v, ast.Constant) else None
    except SyntaxError:
        pass

    stop_m = re.search(r'stop_distance_m.*?get\([^,]+,\s*([\d.]+)\)', src)
    slow_m = re.search(r'slow_down_distance_m.*?get\([^,]+,\s*([\d.]+)\)', src)
    default_stop = float(stop_m.group(1)) if stop_m else 1.2
    default_slow = float(slow_m.group(1)) if slow_m else 3.5

    warning = None
    if forced_action is not None:
        warning = (
            f"FORCED_ACTION='{forced_action}' 활성화 — 실제 거리 기반 로직이 무시됩니다. "
            "운영 환경에서는 None으로 설정하세요."
        )

    return {
        "forced_action": forced_action,
        "default_stop_distance_m": default_stop,
        "default_slow_down_distance_m": default_slow,
        "logic_summary": [
            "hasFrontObject=False → None",
            "inRepathMoveGraceTime=True → SlowDown",
            f"dist ≤ {default_stop}m + canRepath=True → Repath",
            f"dist ≤ {default_stop}m + canRepath=False → Stop",
            f"dist ≤ {default_slow}m → SlowDown",
            "그 외 → None",
        ],
        "warning": warning,
    }


def check_param_consistency(
    bot_setup_raw: dict[str, Any],
    episode_setup: dict[str, Any],
    policy_source: dict[str, Any],
) -> dict[str, Any]:
    """BotSetup 거리값과 PolicyServer 기본값이 어긋나는지 검사."""
    lidar = (bot_setup_raw.get("robot", {}) or {}).get("lidar", {}) or {}
    issues = []

    for param, bot_key, ps_key in [
        ("stop_distance_m", "stop_distance_m", "default_stop_distance_m"),
        ("slow_down_distance_m", "slow_down_distance_m", "default_slow_down_distance_m"),
    ]:
        bot_val = lidar.get(bot_key)
        ps_val = policy_source.get(ps_key)
        if bot_val is not None and ps_val is not None and abs(bot_val - ps_val) > 0.01:
            issues.append({
                "param": param,
                "bot_value": bot_val,
                "policy_server_value": ps_val,
                "gap": round(bot_val - ps_val, 3),
                "description": f"BotSetup({bot_val}m) ≠ PolicyServer 기본값({ps_val}m)",
            })

    near_miss_thresh = (
        (episode_setup.get("evaluation", {}) or {})
        .get("near_miss", {})
        .get("distance_m")
    )
    return {
        "ok": len(issues) == 0,
        "near_miss_threshold_m": near_miss_thresh,
        "issues": issues,
    }
