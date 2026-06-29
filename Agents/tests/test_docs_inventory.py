from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REPO_ROOT = ROOT.parent
DOCS = ROOT / "docs"

LEGACY_TERMINOLOGY_NAME = "SCENARIO_" + "EPISODE_TERMINOLOGY.md"
MISSING_EVALUATION_REPORT_NAME = "EpisodeEvaluationReport" + ".json.md"


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def test_temporary_management_docs_are_removed_after_cleanup() -> None:
    assert not (DOCS / "DOCS_INVENTORY.md").exists()
    assert not (DOCS / "KOREAN_DOCS_CONVERSION_PLAN.md").exists()
    assert not (DOCS / "DOCUMENT_CLEANUP_PLAN.md").exists()


def test_current_and_legacy_contract_paths_have_expected_status() -> None:
    contract_specs = REPO_ROOT / "contracts" / "specs"

    assert (contract_specs / "user-project-data.md").exists()
    assert (contract_specs / "EpisodeSetup.json.md").exists()
    assert (contract_specs / "DeliveryBotSetup.json.md").exists()
    assert (contract_specs / "RunQueue.json.md").exists()
    assert not (contract_specs / MISSING_EVALUATION_REPORT_NAME).exists()
    assert (DOCS / "policy_server" / "POLICY_DECISION_JSON_GUIDE.md").exists()


def test_docs_are_split_into_expected_folders() -> None:
    assert (DOCS / "archive" / "previous_episode_spec").exists()
    assert (DOCS / "archive" / "deprecated").exists()
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


def test_legacy_terminology_doc_is_deprecated_archive_only() -> None:
    archived = DOCS / "archive" / "deprecated" / LEGACY_TERMINOLOGY_NAME

    assert archived.exists()
    assert "Deprecated" in _read(archived)
    assert not (DOCS / "architecture" / LEGACY_TERMINOLOGY_NAME).exists()


def test_provider_research_handoff_and_architecture_docs_are_in_expected_locations() -> None:
    assert (DOCS / "providers" / "OPENAI_PROVIDER_GUIDE.md").exists()
    assert (DOCS / "providers" / "OLLAMA_PROVIDER_GUIDE.md").exists()
    assert (DOCS / "research" / "RESEARCH_ALIGNMENT.md").exists()
    assert (DOCS / "handoff" / "UE_SETUP_PAIR_HANDOFF_PACKAGE.md").exists()
    assert (DOCS / "architecture" / "UE_CONTRACT_MIGRATION_PLAN.md").exists()
    assert (DOCS / "environment" / "ENVIRONMENT_PARAMETER_SPEC.md").exists()
    assert not (DOCS / "environment" / "environment-catalog.md").exists()
    assert (DOCS / "policy" / "POLICY_SOURCE_REGISTRY.md").exists()
    assert (DOCS / "rag" / "RAG_RETRIEVAL_STRATEGY.md").exists()
    assert (DOCS / "tooling" / "HARNESS_GUIDE.md").exists()
    assert (DOCS / "json_contracts" / "JSON_CONTRACTS.md").exists()


def test_docs_readme_links_current_v2_agent_documents() -> None:
    text = _read(DOCS / "README.md")

    for term in [
        "../../docs/specs/project-structure.md",
        "../../docs/specs/simulation-interface.md",
        "../../contracts/specs/user-project-data.md",
        "api/V2_AGENT_APIS.md",
        "agents/V2_AGENT_ARCHITECTURE.md",
        "agents/V2_LANGGRAPH_DESIGN.md",
        "../../Client/Json/environment-catalog.md",
    ]:
        assert term in text

    assert MISSING_EVALUATION_REPORT_NAME not in text
    assert LEGACY_TERMINOLOGY_NAME not in text
    assert ("Scenario / " + "Episode Terminology") not in text


def test_environment_catalog_temporary_agent_copy_is_removed() -> None:
    docs_readme = _read(DOCS / "README.md")

    assert not (DOCS / "environment" / "environment-catalog.md").exists()
    assert "Temporary AI-side catalog" not in docs_readme
    assert "Agents/docs/environment/environment-catalog.md" not in docs_readme


def test_docs_inventory_tests_do_not_import_live_provider_sdks() -> None:
    text = Path(__file__).read_text(encoding="utf-8")
    forbidden_openai_import = "import " + "open" + "ai"
    forbidden_ollama_import = "import " + "olla" + "ma"

    assert forbidden_openai_import not in text.lower()
    assert forbidden_ollama_import not in text.lower()
