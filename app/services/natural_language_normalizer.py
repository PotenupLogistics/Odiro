from __future__ import annotations


KEYWORD_RULES: list[tuple[tuple[str, ...], str]] = [
    (("비상", "정지", "충돌"), "비상정지"),
    (("속도", "천천히", "보호구역"), "속도"),
    (("장애물", "킥보드", "공유 킥보드", "막힘", "막고", "경로를 막"), "장애물 감지"),
    (("장애물", "킥보드", "막힘", "막고", "경로를 막"), "장애물 회피"),
    (("횡단보도", "신호등", "횡단", "건너"), "횡단보도"),
    (("보도", "좁은 보도", "좁은 길"), "보도"),
    (("관제", "원격", "수동"), "관제장치"),
    (("턱", "경사", "넘어짐", "기울어짐"), "동적 안정성"),
]


def normalize_prompt(prompt: str) -> str:
    return " ".join(prompt.strip().split())


def extract_scenario_keywords(prompt: str) -> list[str]:
    normalized = normalize_prompt(prompt)
    found: list[str] = []
    for keywords, _query in KEYWORD_RULES:
        for keyword in keywords:
            if keyword in normalized and keyword not in found:
                found.append(keyword)
    return found


def infer_retrieval_queries(prompt: str) -> list[str]:
    normalized = normalize_prompt(prompt)
    queries: list[str] = []
    for keywords, query in KEYWORD_RULES:
        if any(keyword in normalized for keyword in keywords) and query not in queries:
            queries.append(query)
    if not queries and normalized:
        queries.append(normalized)
    return queries
