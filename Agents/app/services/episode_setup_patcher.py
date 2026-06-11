"""EpisodeSetup dict에 EpisodeSetupRecommendation 결과를 dotted path로 적용.

`actors.pedestrians[*].movement.speed_mps` 처럼 `[*]`가 들어간 경로는
"해당 리스트 모든 원소"에 적용. 단일 인덱스 패턴은 지원하지 않는다 (의미상
불필요 — 변형은 보통 일괄 적용).

`actors.static_obstacles[*].xy_m` 처럼 위치 좌표 자체를 자유 텍스트로
설명하는 경우(`suggested`가 dict/list가 아닌 문자열)는 적용을 건너뛰고
warning을 호출자에게 알린다.
"""
from __future__ import annotations

import copy
from typing import Any

from app.models.recommendation import EpisodeSetupRecommendation


class EpisodeSetupPatchWarning(Exception):
    """경로를 적용할 수 없는 추천을 만났을 때."""


def _set_dotted(target: dict[str, Any], path: str, value: Any) -> None:
    parts = path.split(".")
    cursor: Any = target
    for i, part in enumerate(parts):
        is_last = i == len(parts) - 1
        if part.endswith("[*]"):
            key = part[:-3]
            if not isinstance(cursor, dict) or key not in cursor:
                raise EpisodeSetupPatchWarning(f"key 없음: {key} in {path}")
            seq = cursor[key]
            if not isinstance(seq, list):
                raise EpisodeSetupPatchWarning(f"list 아님: {key} in {path}")
            tail = parts[i + 1 :]
            if not tail:
                raise EpisodeSetupPatchWarning(f"[*] 다음 path 필요: {path}")
            sub_path = ".".join(tail)
            for elem in seq:
                if not isinstance(elem, dict):
                    raise EpisodeSetupPatchWarning(f"list 원소가 dict 아님: {path}")
                _set_dotted(elem, sub_path, value)
            return
        if not isinstance(cursor, dict):
            raise EpisodeSetupPatchWarning(f"중간 노드가 dict 아님: {path}")
        if is_last:
            cursor[part] = value
            return
        if part not in cursor or not isinstance(cursor[part], (dict, list)):
            raise EpisodeSetupPatchWarning(f"중간 노드 없음/형식 불일치: {part} in {path}")
        cursor = cursor[part]


def apply_episode_setup_recommendations(
    current: dict[str, Any],
    recommendations: list[EpisodeSetupRecommendation],
) -> tuple[dict[str, Any], list[str]]:
    """recommendations를 deep-copy한 EpisodeSetup dict에 적용.

    적용 실패한 항목은 warnings로 반환하되 전체 흐름은 멈추지 않는다.
    """
    patched = copy.deepcopy(current)
    warnings: list[str] = []
    for rec in recommendations:
        if rec.suggested is None:
            warnings.append(f"suggested 누락: {rec.param}")
            continue
        # xy_m처럼 자유 텍스트 추천은 건너뜀
        if rec.param == "actors.static_obstacles[*].xy_m" and not isinstance(
            rec.suggested, (list, tuple)
        ):
            warnings.append(
                f"{rec.param}는 list[float] suggested만 적용 가능 — 자유 텍스트 추천 무시"
            )
            continue
        try:
            _set_dotted(patched, rec.param, rec.suggested)
        except EpisodeSetupPatchWarning as exc:
            warnings.append(f"{rec.param} 적용 실패: {exc}")
    return patched, warnings
