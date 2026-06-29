from __future__ import annotations

import re
from pathlib import Path

from app.agents.result_analysis_v2 import ResultAnalysisV2Agent
from app.agents.scenario_generation_v2 import ScenarioGenerationV2Agent
from app.core.settings import Settings
from app.models.scenario_generation_v2 import ScenarioGenerateV2Request


ROOT = Path(__file__).resolve().parents[1]
ENVIRONMENT_CATALOG = ROOT.parent / "Client" / "Json" / "environment-catalog.md"


class _FakeSpecContextLoader:
    def __init__(self) -> None:
        self.calls = 0

    def build_prompt_block(self) -> str:
        self.calls += 1
        return "\n\n".join(
            [
                "공식 문서 우선 적용 안내",
                "<SPEC_CONTEXT>",
                "# SPEC FILE: docs/specs/simulation-interface.md",
                "scenario context",
                "# SPEC FILE: Client/Json/environment-catalog.md",
                "Prop Bounding Boxes",
                "</SPEC_CONTEXT>",
            ]
        )


class _ExplodingSpecContextLoader:
    def build_prompt_block(self) -> str:
        raise AssertionError("deterministic path must not load spec context")


def _catalog_prop_ids() -> set[str]:
    text = ENVIRONMENT_CATALOG.read_text(encoding="utf-8-sig")
    return set(re.findall(r"`(obstacle\.[a-z0-9_]+)`", text))


def test_scenario_template_prompt_includes_spec_context() -> None:
    loader = _FakeSpecContextLoader()
    agent = ScenarioGenerationV2Agent(
        settings=Settings(_env_file=None, v2AgentLlmEnabled=True),
        spec_context_loader=loader,
    )

    prompt = agent._template_user_prompt("좁은 보도에서 대향 보행자")

    assert "공식 문서 우선 적용 안내" in prompt
    assert "<SPEC_CONTEXT>" in prompt
    assert "# SPEC FILE: docs/specs/simulation-interface.md" in prompt
    assert "# SPEC FILE: Client/Json/environment-catalog.md" in prompt
    assert "Prop Bounding Boxes" in prompt
    assert "</SPEC_CONTEXT>" in prompt
    assert "사용자 prompt:\n좁은 보도에서 대향 보행자" in prompt
    assert loader.calls == 1


def test_scenario_template_prompt_uses_catalog_prop_examples() -> None:
    agent = ScenarioGenerationV2Agent(
        settings=Settings(_env_file=None, v2AgentLlmEnabled=True),
        spec_context_loader=_FakeSpecContextLoader(),
    )

    prompt = agent._template_user_prompt("좁은 보도에서 정적 장애물")
    prompt_prop_ids = set(re.findall(r"obstacle\.[a-z0-9_]+", prompt))

    assert prompt_prop_ids
    assert prompt_prop_ids <= _catalog_prop_ids()


def test_scenario_repair_prompt_includes_spec_context() -> None:
    loader = _FakeSpecContextLoader()
    agent = ScenarioGenerationV2Agent(
        settings=Settings(_env_file=None, v2AgentLlmEnabled=True),
        spec_context_loader=loader,
    )

    prompt = agent._repair_user_prompt(
        "좁은 보도에서 대향 보행자",
        {"schema": "scenario"},
        {"errors": ["missing robot"]},
    )

    assert "<SPEC_CONTEXT>" in prompt
    assert "# SPEC FILE: docs/specs/simulation-interface.md" in prompt
    assert "# SPEC FILE: Client/Json/environment-catalog.md" in prompt
    assert "Prop Bounding Boxes" in prompt
    assert "검증 결과:" in prompt
    assert "수정 대상 JSON:" in prompt
    assert loader.calls == 1


def test_scenario_deterministic_path_does_not_load_spec_context() -> None:
    agent = ScenarioGenerationV2Agent(
        settings=Settings(_env_file=None, v2AgentLlmEnabled=False),
        spec_context_loader=_ExplodingSpecContextLoader(),
    )

    response = agent.generate(ScenarioGenerateV2Request(prompt="좁은 보도 장애물"))

    assert response.status == "success"
    assert response.generation_mode == "deterministic"


def test_result_analysis_prompt_includes_spec_context() -> None:
    loader = _FakeSpecContextLoader()
    agent = ResultAnalysisV2Agent(
        settings=Settings(_env_file=None, v2AgentLlmEnabled=True),
        spec_context_loader=loader,
    )

    prompt = agent._analysis_user_prompt({"summary": {"overall_judgement": "change_recommended"}})

    assert "공식 문서 우선 적용 안내" in prompt
    assert "<SPEC_CONTEXT>" in prompt
    assert "# SPEC FILE: docs/specs/simulation-interface.md" in prompt
    assert "# SPEC FILE: Client/Json/environment-catalog.md" in prompt
    assert "Prop Bounding Boxes" in prompt
    assert "</SPEC_CONTEXT>" in prompt
    assert '"overall_judgement": "change_recommended"' in prompt
    assert loader.calls == 1


def test_result_analysis_prompt_does_not_request_full_raw_jsonl() -> None:
    agent = ResultAnalysisV2Agent(
        settings=Settings(_env_file=None, v2AgentLlmEnabled=True),
        spec_context_loader=_FakeSpecContextLoader(),
    )

    prompt = agent._analysis_user_prompt({"patterns": [{"type": "blocked_region_violation_repeated"}]})

    forbidden_raw_log_requests = (
        "raw events.jsonl",
        "full events.jsonl",
        "full actions.jsonl",
        "full trace.jsonl",
        "events.jsonl 전체",
        "actions.jsonl 전체",
        "trace.jsonl 전체",
    )
    assert not any(fragment in prompt for fragment in forbidden_raw_log_requests)
    assert "evidence" in prompt or "근거" in prompt
