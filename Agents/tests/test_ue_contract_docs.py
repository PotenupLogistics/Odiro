from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REPO_ROOT = ROOT.parent
DOCS = ROOT / "docs"
CONTRACT_SPECS = REPO_ROOT / "contracts" / "specs"
ROOT_SPECS = REPO_ROOT / "docs" / "specs"

LEGACY_TERMINOLOGY_NAME = "SCENARIO_" + "EPISODE_TERMINOLOGY.md"
MISSING_EVALUATION_REPORT_NAME = "EpisodeEvaluationReport" + ".json.md"


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def test_legacy_ue_contract_docs_are_retained_but_not_current_contract_set() -> None:
    for name in [
        "EpisodeSetup.json.md",
        "DeliveryBotSetup.json.md",
        "RunQueue.json.md",
    ]:
        assert (CONTRACT_SPECS / name).exists()

    assert not (CONTRACT_SPECS / MISSING_EVALUATION_REPORT_NAME).exists()
    assert (ROOT_SPECS / "simulation-interface.md").exists()
    assert (DOCS / "archive" / "deprecated" / LEGACY_TERMINOLOGY_NAME).exists()
    assert (DOCS / "architecture" / "UE_CONTRACT_MIGRATION_PLAN.md").exists()
    assert (DOCS / "archive" / "previous_episode_spec" / "UE5_EPISODE_SPEC_ADAPTER.md").exists()


def test_simulation_interface_distinguishes_current_execution_terms() -> None:
    text = _read(ROOT_SPECS / "simulation-interface.md")

    for term in [
        "Scenario",
        "EpisodeScenario",
        "Run",
        "Episode",
        "RunId",
        "EpisodeId",
        "Project",
    ]:
        assert term in text


def test_episode_setup_contract_uses_new_coordinate_fields_and_forbids_legacy_transform_fields() -> None:
    text = _read(CONTRACT_SPECS / "EpisodeSetup.json.md")

    for term in ["xy_m", "yaw_deg", "goal_xy_m", "center_xy_m", "points_xy_m"]:
        assert term in text
    for term in ["transform", "location_m", "rotation_deg", "scale", "units"]:
        assert term in text
    assert "출력하지 않는다" in text
    assert "로봇 배치" in text
    assert "로봇 목적지" in text


def test_episode_setup_contract_documents_robot_profile() -> None:
    text = _read(CONTRACT_SPECS / "EpisodeSetup.json.md")

    for term in [
        "robot_profile",
        "delivery_bot_alpha",
        "0.44",
        "1.00",
        "0.64",
        "min_passable_width_m",
        "0.84",
        "서버 기본",
        "API request",
        "collision box",
    ]:
        assert term in text


def test_delivery_bot_setup_contract_keeps_tuning_separate_from_episode_placement() -> None:
    text = _read(CONTRACT_SPECS / "DeliveryBotSetup.json.md")

    for term in ["drive", "path_follow", "lidar"]:
        assert term in text
    for term in ["run", "actors", "instance_id", "asset_id", "route", "location"]:
        assert term in text
    assert "넣지 않는다" in text
    assert "주행 속도 튜닝" in text
    assert "경로 추종 튜닝" in text
    assert "라이다 반응 튜닝" in text


def test_run_queue_contract_is_legacy_and_evaluation_report_link_is_absent() -> None:
    run_queue_text = _read(CONTRACT_SPECS / "RunQueue.json.md")
    docs_readme = _read(DOCS / "README.md")

    for term in ["pair_id", "episode_setup", "delivery_bot_setup"]:
        assert term in run_queue_text
    assert "순서대로 실행" in run_queue_text
    assert MISSING_EVALUATION_REPORT_NAME not in docs_readme


def test_readmes_link_current_simulation_and_user_project_contracts() -> None:
    text = _read(ROOT / "README.md") + "\n" + _read(DOCS / "README.md")

    assert "simulation-interface.md" in text
    assert "user-project-data.md" in text
    assert "Client/Json/environment-catalog.md" in text
    assert LEGACY_TERMINOLOGY_NAME not in text
    assert MISSING_EVALUATION_REPORT_NAME not in text


def test_ue_contract_doc_tests_do_not_import_live_provider_sdks() -> None:
    text = Path(__file__).read_text(encoding="utf-8")
    forbidden_openai_import = "import " + "open" + "ai"
    forbidden_ollama_import = "import " + "olla" + "ma"

    assert forbidden_openai_import not in text.lower()
    assert forbidden_ollama_import not in text.lower()
