from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REPORT_JSON = ROOT / "harness" / "reports" / "ue5_episode_spec_handoff_smoke.json"
SUMMARY_DOC = ROOT / "docs" / "UE5_EPISODE_SPEC_HANDOFF_SUMMARY.md"


def test_ue5_episode_spec_handoff_summary_doc_exists() -> None:
    assert SUMMARY_DOC.exists()
    text = SUMMARY_DOC.read_text(encoding="utf-8-sig")
    assert "EpisodeSpec" in text
    assert "responseFormat=episode_spec" in text


def test_smoke_report_structure_when_present() -> None:
    assert REPORT_JSON.exists()
    payload = json.loads(REPORT_JSON.read_text(encoding="utf-8-sig"))
    episode = payload["responseFormatEpisodeSpec"]
    assert "episodeSpecExists" in episode
    assert "conversionWarnings" in episode
    assert "kickboardMapping" in episode
    assert "ueCompilerReadiness" in episode
    assert "actorSummary" in episode


def test_no_forbidden_artifacts_created_by_smoke() -> None:
    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "embeddings").exists()

