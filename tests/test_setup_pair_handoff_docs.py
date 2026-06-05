from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs"


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def test_setup_pair_handoff_docs_exist() -> None:
    assert (DOCS / "handoff" / "HANDOFF_RELEASE_NOTES.md").exists()
    assert (DOCS / "handoff" / "UE_SETUP_PAIR_HANDOFF_PACKAGE.md").exists()


def test_setup_pair_handoff_result_documents_verified_response_format_and_pair() -> None:
    text = _read(DOCS / "handoff" / "HANDOFF_RELEASE_NOTES.md")

    assert "setup pair" in text
    assert "EpisodeSetup + DeliveryBotSetup pair" in text
    assert "episodeSetupValidationPassed" in text
    assert "deliveryBotSetupValidationPassed" in text
    assert "setupPairTraceExists=true" in text


def test_ue_setup_pair_package_documents_candidate_no_commit_policy() -> None:
    text = _read(DOCS / "handoff" / "UE_SETUP_PAIR_HANDOFF_PACKAGE.md")

    assert "data/fine_tuning_candidates/20260604T040540Z_UE-HANDOFF-SETUP-PAIR-001" in text
    assert "git commit 대상이 아님" in text
    assert "EpisodeSetup JSON" in text
    assert "DeliveryBotSetup JSON" in text


def test_ue_setup_pair_package_documents_ue_checklist() -> None:
    text = _read(DOCS / "handoff" / "UE_SETUP_PAIR_HANDOFF_PACKAGE.md")

    assert "EpisodeSetup JSON이 UE compiler" in text
    assert "FDeliveryBotSetupInfo" in text
    assert "robot" in text
    assert "obstacle" in text
    assert "stop_distance_m=1.2" in text
    assert "slow_down_distance_m=3.5" in text


def test_ue_team_message_mentions_setup_pair_delivery_scope() -> None:
    text = _read(DOCS / "handoff" / "UE_TEAM_MESSAGE_DRAFT.md")

    assert "/api/v1/scenarios/generate" in text
    assert "RunQueue JSON" in text
    assert "EpisodeSetup + DeliveryBotSetup pair" in text
    assert "actor spawn" in text
    assert "route injection" in text
    assert "lidar/drive/path_follow" in text


def test_readmes_link_setup_pair_handoff_result() -> None:
    text = _read(ROOT / "README.md") + "\n" + _read(DOCS / "README.md")

    assert "HANDOFF_RELEASE_NOTES.md" in text


def test_fine_tuning_candidate_archive_is_gitignored() -> None:
    text = _read(ROOT / ".gitignore")

    assert "data/fine_tuning_candidates/" in text


def test_setup_pair_handoff_doc_tests_do_not_import_live_provider_sdks() -> None:
    text = Path(__file__).read_text(encoding="utf-8")
    forbidden_openai_import = "import " + "open" + "ai"
    forbidden_ollama_import = "import " + "olla" + "ma"

    assert forbidden_openai_import not in text.lower()
    assert forbidden_ollama_import not in text.lower()
