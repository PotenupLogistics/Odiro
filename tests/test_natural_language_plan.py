from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DOCS = [
    ROOT / "docs" / "NATURAL_LANGUAGE_INPUT_PLAN.md",
    ROOT / "docs" / "LLM_WORLD_CONFIG_GENERATION_FLOW.md",
    ROOT / "docs" / "WORLD_CONFIG_PROMPT_SPEC.md",
    ROOT / "docs" / "NATURAL_LANGUAGE_GENERATION_CONTRACT.md",
]


def test_natural_language_design_docs_exist() -> None:
    for path in DOCS:
        assert path.exists()


def test_input_plan_says_cli_is_for_json_validation_not_natural_language() -> None:
    text = (ROOT / "docs" / "NATURAL_LANGUAGE_INPUT_PLAN.md").read_text(encoding="utf-8-sig")
    assert "CLI는 자연어 입력용이 아니라 JSON 검증용" in text


def test_generation_flow_mentions_validation_layer_and_repair_loop() -> None:
    text = (ROOT / "docs" / "LLM_WORLD_CONFIG_GENERATION_FLOW.md").read_text(encoding="utf-8-sig")
    assert "validation layer" in text
    assert "Repair Loop" in text


def test_prompt_spec_requires_json_only_output() -> None:
    text = (ROOT / "docs" / "WORLD_CONFIG_PROMPT_SPEC.md").read_text(encoding="utf-8-sig")
    assert "JSON object 하나만" in text


def test_generation_contract_limits_target_to_world_config() -> None:
    text = (ROOT / "docs" / "NATURAL_LANGUAGE_GENERATION_CONTRACT.md").read_text(encoding="utf-8-sig")
    assert "targetContractType은 world_config만 허용" in text


def test_no_sample_or_fixture_artifacts_are_created() -> None:
    forbidden_names = {
        "world_config.json",
        "policy_config.json",
        "decision_request.json",
        "decision_response.json",
    }
    for folder in ["data", "docs", "harness", "scripts", "tests", "app"]:
        root = ROOT / folder
        for path in root.rglob("*"):
            assert path.name not in forbidden_names
    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
