from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs"
UE_CONTRACTS = DOCS / "ue_contracts"


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def test_ue_contract_docs_and_plans_exist() -> None:
    for name in [
        "EPISODE_SETUP_JSON.md",
        "DELIVERY_BOT_SETUP_JSON.md",
        "RUN_QUEUE_JSON.md",
        "EVALUATION_REPORT_JSON.md",
    ]:
        assert (UE_CONTRACTS / name).exists()

    assert (DOCS / "architecture" / "SCENARIO_EPISODE_TERMINOLOGY.md").exists()
    assert (DOCS / "architecture" / "UE_CONTRACT_MIGRATION_PLAN.md").exists()
    assert (DOCS / "archive" / "previous_episode_spec" / "UE5_EPISODE_SPEC_ADAPTER.md").exists()


def test_scenario_episode_terminology_distinguishes_core_terms() -> None:
    text = _read(DOCS / "architecture" / "SCENARIO_EPISODE_TERMINOLOGY.md")

    assert "Scenario" in text
    assert "추상적인 상황 유형" in text
    assert "Episode" in text
    assert "구체 시뮬레이션 인스턴스" in text
    assert "EpisodeSetup + DeliveryBotSetup" in text
    assert "scenario_id" in text
    assert "pair_id" in text


def test_episode_setup_contract_uses_new_coordinate_fields_and_forbids_legacy_transform_fields() -> None:
    text = _read(UE_CONTRACTS / "EPISODE_SETUP_JSON.md")

    for term in ["xy_m", "yaw_deg", "goal_xy_m", "center_xy_m", "points_xy_m"]:
        assert term in text
    for term in ["transform", "location_m", "rotation_deg", "scale", "units"]:
        assert term in text
    assert "출력하지 않는다" in text
    assert "로봇 배치" in text
    assert "로봇 목적지" in text


def test_episode_setup_contract_documents_robot_profile() -> None:
    text = _read(UE_CONTRACTS / "EPISODE_SETUP_JSON.md")

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
    text = _read(UE_CONTRACTS / "DELIVERY_BOT_SETUP_JSON.md")

    for term in ["drive", "path_follow", "lidar"]:
        assert term in text
    for term in ["run", "actors", "instance_id", "asset_id", "route", "location"]:
        assert term in text
    assert "넣지 않는다" in text
    assert "주행 속도 튜닝" in text
    assert "경로 추종 튜닝" in text
    assert "라이다 반응 튜닝" in text


def test_run_queue_and_evaluation_report_contracts_are_indexed_without_implementation_scope() -> None:
    run_queue_text = _read(UE_CONTRACTS / "RUN_QUEUE_JSON.md")
    evaluation_text = _read(UE_CONTRACTS / "EVALUATION_REPORT_JSON.md")
    migration_text = _read(DOCS / "architecture" / "UE_CONTRACT_MIGRATION_PLAN.md")

    for term in ["pair_id", "episode_setup", "delivery_bot_setup"]:
        assert term in run_queue_text
    assert "순서대로 실행" in run_queue_text

    for term in ["episode_evaluation_report", "summary", "metrics", "event_summary", "events", "usable_for_llm_tuning"]:
        assert term in evaluation_text
    assert "결과 분석" in migration_text
    assert "다른 담당자 범위" in migration_text


def test_readmes_link_ue_contract_migration_and_cleanup_docs() -> None:
    text = _read(ROOT / "README.md") + "\n" + _read(DOCS / "README.md")

    assert "UE_CONTRACT_MIGRATION_PLAN.md" in text
    assert "SCENARIO_EPISODE_TERMINOLOGY.md" in text
    assert "archive/previous_episode_spec" in text
    assert "docs/ue_contracts" in text or "ue_contracts" in text


def test_ue_contract_doc_tests_do_not_import_live_provider_sdks() -> None:
    text = Path(__file__).read_text(encoding="utf-8")
    forbidden_openai_import = "import " + "open" + "ai"
    forbidden_ollama_import = "import " + "olla" + "ma"

    assert forbidden_openai_import not in text.lower()
    assert forbidden_ollama_import not in text.lower()
