from __future__ import annotations

from app.services.natural_language_normalizer import (
    extract_scenario_keywords,
    infer_retrieval_queries,
    normalize_prompt,
)


def test_emergency_stop_prompt_infers_emergency_query() -> None:
    prompt = "\ube44\uc0c1\uc815\uc9c0\uac00 \ud544\uc694\ud55c \ucda9\ub3cc \uc704\ud5d8 \uc0c1\ud669"

    assert normalize_prompt(prompt) == prompt
    assert "\ube44\uc0c1\uc815\uc9c0" in infer_retrieval_queries(prompt)


def test_sidewalk_kickboard_crossing_prompt_infers_multiple_queries() -> None:
    prompt = (
        "\uc881\uc740 \ubcf4\ub3c4\uc5d0\uc11c \ud0a5\ubcf4\ub4dc\uac00 \ub85c\ubd07 \uacbd\ub85c\ub97c "
        "\ub9c9\uace0 \ubcf4\ud589\uc790\uac00 \ud6a1\ub2e8\ud55c\ub2e4"
    )

    keywords = extract_scenario_keywords(prompt)
    queries = infer_retrieval_queries(prompt)

    assert "\ubcf4\ub3c4" in keywords
    assert "\uc7a5\uc560\ubb3c \uac10\uc9c0" in queries
    assert "\ud6a1\ub2e8\ubcf4\ub3c4" in queries

