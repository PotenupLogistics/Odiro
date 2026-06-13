from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REPO_ROOT = ROOT.parent
DOCS = ROOT / "docs"


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def test_temporary_management_docs_are_removed_after_cleanup() -> None:
    assert not (DOCS / "DOCS_INVENTORY.md").exists()
    assert not (DOCS / "KOREAN_DOCS_CONVERSION_PLAN.md").exists()
    assert not (DOCS / "DOCUMENT_CLEANUP_PLAN.md").exists()


def test_canonical_contract_paths_are_not_moved_or_renamed() -> None:
    contract_specs = REPO_ROOT / "contracts" / "specs"
    assert (contract_specs / "EpisodeSetup.json.md").exists()
    assert (contract_specs / "DeliveryBotSetup.json.md").exists()
    assert (contract_specs / "RunQueue.json.md").exists()
    assert (contract_specs / "EpisodeEvaluationReport.json.md").exists()
    assert (DOCS / "policy_server" / "POLICY_DECISION_JSON_GUIDE.md").exists()


def test_docs_are_split_into_expected_folders() -> None:
    assert (DOCS / "archive" / "previous_episode_spec").exists()
    assert (DOCS / "providers").exists()
    assert (DOCS / "research").exists()
    assert (DOCS / "references").exists()
    assert (DOCS / "handoff").exists()
    assert (DOCS / "architecture").exists()
    assert (DOCS / "environment").exists()
    assert (DOCS / "experiment").exists()
    assert (DOCS / "policy").exists()
    assert (DOCS / "rag").exists()
    assert (DOCS / "manual_review").exists()
    assert (DOCS / "tooling").exists()
    assert (DOCS / "json_contracts").exists()


def test_legacy_episode_spec_docs_are_archived() -> None:
    archive = DOCS / "archive" / "previous_episode_spec"

    assert (archive / "UE5_EPISODE_SPEC_ADAPTER.md").exists()
    assert (archive / "UE5_EPISODE_SPEC_HANDOFF_SUMMARY.md").exists()
    assert (archive / "UE5_EPISODE_SPEC_CONTROLLED_SMOKE_RESULT.md").exists()
    assert (archive / "UE5_EPISODE_SPEC_SCENARIO_REFLECTION.md").exists()
    assert (archive / "UE_EPISODE_SPEC_JSON_GUIDE.md").exists()
    assert (archive / "UE5_WORLD_CONFIG_FIELD_MAPPING.md").exists()


def test_provider_research_handoff_and_architecture_docs_are_in_new_locations() -> None:
    assert (DOCS / "providers" / "OPENAI_PROVIDER_GUIDE.md").exists()
    assert (DOCS / "providers" / "OLLAMA_PROVIDER_GUIDE.md").exists()
    assert (DOCS / "research" / "RESEARCH_ALIGNMENT.md").exists()
    assert (DOCS / "handoff" / "UE_SETUP_PAIR_HANDOFF_PACKAGE.md").exists()
    assert (DOCS / "architecture" / "SCENARIO_EPISODE_TERMINOLOGY.md").exists()
    assert (DOCS / "architecture" / "UE_CONTRACT_MIGRATION_PLAN.md").exists()
    assert (DOCS / "environment" / "ENVIRONMENT_PARAMETER_SPEC.md").exists()
    assert (DOCS / "policy" / "POLICY_SOURCE_REGISTRY.md").exists()
    assert (DOCS / "rag" / "RAG_RETRIEVAL_STRATEGY.md").exists()
    assert (DOCS / "tooling" / "HARNESS_GUIDE.md").exists()
    assert (DOCS / "json_contracts" / "JSON_CONTRACTS.md").exists()


def test_readmes_link_new_structure_and_canonical_contracts() -> None:
    text = _read(ROOT / "README.md") + "\n" + _read(DOCS / "README.md")

    assert "docs/architecture/UE_CONTRACT_MIGRATION_PLAN.md" in text
    assert "docs/providers/OPENAI_PROVIDER_GUIDE.md" in text
    assert "archive/previous_episode_spec" in text
    assert "contracts/specs/EpisodeSetup.json.md" in text
    assert "docs/policy_server/POLICY_DECISION_JSON_GUIDE.md" in text
    assert "experiment/EXPERIMENT_WORKSPACE_LAYOUT.md" in text


def test_docs_inventory_tests_do_not_import_live_provider_sdks() -> None:
    text = Path(__file__).read_text(encoding="utf-8")
    forbidden_openai_import = "import " + "open" + "ai"
    forbidden_ollama_import = "import " + "olla" + "ma"

    assert forbidden_openai_import not in text.lower()
    assert forbidden_ollama_import not in text.lower()
