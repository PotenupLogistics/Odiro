from __future__ import annotations

from pathlib import Path


# Repository root for locating prompt files under test.
ROOT = Path(__file__).resolve().parents[1]

# Result analysis system prompt path used by the LLM-enabled graph path.
PROMPT_PATH = ROOT / "app" / "agents" / "result_analysis_v2" / "prompts" / "system_prompt.md"


def test_result_analysis_system_prompt_defines_operational_guardrails() -> None:
    text = PROMPT_PATH.read_text(encoding="utf-8")

    assert text.strip() != "Analyze experiment summaries only. Do not invent episode evidence."
    assert len(text.splitlines()) >= 20
    for fragment in (
        "결과 분석 에이전트",
        "run summary",
        "metrics",
        "patterns",
        "warnings",
        "refs",
        "episode evidence",
        "시간",
        "위치",
        "속도",
        "센서값",
        "environment",
        "policy",
        "none",
        "setup failed",
        "incomplete data",
        "JSON",
        "markdown",
        "한국어",
    ):
        assert fragment in text


def test_result_analysis_system_prompt_blocks_internal_source_terms() -> None:
    text = PROMPT_PATH.read_text(encoding="utf-8")

    for forbidden in (
        "KOR-",
        "policy card",
        "관련 정책 문서",
        "p.33",
        "근거 문서",
        "RAG",
    ):
        assert forbidden in text
    assert "public API 응답" in text
    assert "넣지" in text or "노출" in text
