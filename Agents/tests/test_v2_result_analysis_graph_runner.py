from __future__ import annotations

import json

import pytest
from fastapi.testclient import TestClient

from app.agents.common.llm_json_client import AgentLlmJsonClient
from app.agents.result_analysis_v2 import rag_context_builder as rag_module
from app.agents.result_analysis_v2.agent import ResultAnalysisV2Agent
from app.agents.result_analysis_v2.graph_runner import ResultAnalysisGraphRunnerV2, StateGraph
from app.core.settings import Settings
from app.main import app
from app.models.analysis_v2 import AnalysisRunV2Request


# Internal source markers that unsafe LLM output must not expose publicly.
FORBIDDEN_LLM_PUBLIC_TEXT = (
    "KOR-",
    "policy card",
    "관련 정책 문서",
    "p.33",
    "근거 문서",
    "RAG",
)

FORBIDDEN_PUBLIC_PATH_FIELDS = (
    "review_dir",
    "status_path",
    "request_path",
    "report_path",
    "manifest_path",
    "recommendations_path",
    "rag_evidence_path",
)

FORBIDDEN_RAG_PUBLIC_METADATA = (
    "chunk_id",
    "card_id",
    "matched_fields",
    "retrieval_score",
    "chunk retrieval score",
)


class _FakeJsonClient:
    """Capture result-analysis LLM calls while returning deterministic JSON payloads."""

    def __init__(self, responses: list[dict]) -> None:
        self.responses = list(responses)
        self.calls: list[dict[str, str]] = []

    def generate_json(self, *, system_prompt: str, user_prompt: str, response_name: str) -> dict:
        """Record the prompt boundary and return the next fake JSON response."""
        self.calls.append(
            {
                "system_prompt": system_prompt,
                "user_prompt": user_prompt,
                "response_name": response_name,
            }
        )
        return self.responses.pop(0)


class _FakeSpecContextLoader:
    """Provide stable spec context so LLM prompt tests do not read broad project docs."""

    def build_prompt_block(self) -> str:
        """Return a small prompt block compatible with SpecContextLoader callers."""
        return "<SPEC_CONTEXT>\n테스트용 결과 분석 문맥\n</SPEC_CONTEXT>"


def _write_blocked_episode(experiments, episode_id: str) -> None:
    episode_dir = experiments / "Experiment1" / "runs" / "000001" / "episodes" / episode_id
    episode_dir.mkdir(parents=True)
    (episode_dir / "result.json").write_text(
        json.dumps({"success": False, "failure_type": "blocked_region_violation"}),
        encoding="utf-8",
    )
    (episode_dir / "events.jsonl").write_text(
        '{"event_type": "blocked_region_violation", "time_s": 6.4}\n',
        encoding="utf-8",
    )
    _upsert_summary_row(experiments / "Experiment1" / "runs" / "000001", episode_id, blocked=True)


def _write_project_blocked_episode(project, episode_id: str) -> None:
    episode_dir = project / "runs" / "000001" / "episodes" / episode_id
    episode_dir.mkdir(parents=True)
    (episode_dir / "result.json").write_text(
        json.dumps({"success": False, "failure_type": "blocked_region_violation"}),
        encoding="utf-8",
    )
    (episode_dir / "events.jsonl").write_text(
        '{"event_type": "blocked_region_violation", "time_s": 6.4}\n',
        encoding="utf-8",
    )
    _upsert_summary_row(project / "runs" / "000001", episode_id, blocked=True)


def _write_project_success_episode(project, episode_id: str) -> None:
    episode_dir = project / "runs" / "000001" / "episodes" / episode_id
    episode_dir.mkdir(parents=True)
    (episode_dir / "result.json").write_text(
        json.dumps({"success": True, "goal_reached": True}),
        encoding="utf-8",
    )
    _upsert_summary_row(project / "runs" / "000001", episode_id, blocked=False)


def _write_project_summary(project, summary: dict) -> None:
    run_dir = project / "runs" / "000001"
    run_dir.mkdir(parents=True, exist_ok=True)
    (run_dir / "summary.json").write_text(json.dumps(summary), encoding="utf-8")


def _write_project_summary_rows(project, rows: list[dict]) -> None:
    """Create a run summary with dashboard rows."""
    _write_project_summary(
        project,
        {
            "schema": "run_summary",
            "version": 1,
            "run": {"run_id": "000001", "project_id": project.name},
            "rows": rows,
        },
    )


def _upsert_summary_row(run_dir, episode_id: str, *, blocked: bool) -> None:
    """Keep graph runner fixtures paired with public summary rows."""
    summary_path = run_dir / "summary.json"
    if summary_path.is_file():
        summary = json.loads(summary_path.read_text(encoding="utf-8"))
    else:
        summary = {
            "schema": "run_summary",
            "version": 1,
            "run": {"run_id": "000001", "project_id": run_dir.parent.parent.name},
            "rows": [],
        }
    row = {
        "episode_id": episode_id,
        "outcome": "Failure" if blocked else "Success",
        "terminal_reason": "BlockedRegionViolation" if blocked else "GoalReached",
        "duration_s": 6.4,
        "metrics": {
            "goal_reached": 0 if blocked else 1,
            "blocked_region_violation_count": 1 if blocked else 0,
            "blocked_region_collision_count": 1 if blocked else 0,
        },
    }
    summary["rows"] = [item for item in summary.get("rows", []) if item.get("episode_id") != episode_id]
    summary["rows"].append(row)
    summary["rows"].sort(key=lambda item: str(item.get("episode_id", "")))
    run_dir.mkdir(parents=True, exist_ok=True)
    summary_path.write_text(json.dumps(summary), encoding="utf-8")


def _llm_policy_recommendation(*, reason: str, recommendation: str) -> dict:
    """Create a complete LLM recommendation payload with real episode evidence."""
    return {
        "id": "REC-LLM-001",
        "target": "policy",
        "priority": "high",
        "title": "보행 경계 이탈 가능성 검토",
        "reason": reason,
        "recommendation": recommendation,
        "evidence": [
            {
                "experiment_id": "Experiment1",
                "run_id": "000001",
                "episode_id": "000001",
            }
        ],
        "proposed_change": {
            "type": "policy_parameter_adjustment",
            "content": {
                "maxPathErrorM_max": 0.8,
            },
        },
    }


def _runner_with_fake_llm(experiments, fake: _FakeJsonClient) -> ResultAnalysisGraphRunnerV2:
    """Build a graph runner that exercises the real analysis nodes with a fake LLM boundary."""
    settings = Settings(_env_file=None, v2AgentLlmEnabled=True)
    agent = ResultAnalysisV2Agent(
        experiments_root=experiments,
        settings=settings,
        llm_client=fake,
        spec_context_loader=_FakeSpecContextLoader(),
    )
    return ResultAnalysisGraphRunnerV2(
        experiments_root=experiments,
        settings=settings,
        fallback_agent=agent,
    )


@pytest.fixture(autouse=True)
def _disable_endpoint_llm(monkeypatch) -> None:
    """Keep endpoint tests on the deterministic recommendation path."""
    monkeypatch.setenv("V2_AGENT_LLM_ENABLED", "false")


def test_result_analysis_graph_runner_imports_without_langgraph_dependency() -> None:
    assert ResultAnalysisGraphRunnerV2


def test_result_analysis_graph_runner_uses_compiled_langgraph_when_available(tmp_path) -> None:
    runner = ResultAnalysisGraphRunnerV2(experiments_root=tmp_path / "missing", settings=Settings(_env_file=None))

    if StateGraph is None:
        assert runner.compiled_graph is None
    else:
        assert runner.compiled_graph is not None


def test_graph_runner_builds_insufficient_data_response_without_langgraph(tmp_path) -> None:
    runner = ResultAnalysisGraphRunnerV2(experiments_root=tmp_path / "missing", settings=Settings(_env_file=None))

    response = runner.run()

    assert response.schema_ == "analysis_run_response_v2"
    assert response.summary.overall_judgement == "insufficient_data"
    assert runner.last_state["analysis_route"] == "insufficient_data"
    assert runner.last_state["rag_route"] == "skipped"
    assert runner.last_state["recommendation_route"] == "none"


def test_graph_runner_records_no_change_route_defaults(tmp_path) -> None:
    project = tmp_path / "Project1"
    _write_project_success_episode(project, "000001")
    runner = ResultAnalysisGraphRunnerV2(settings=Settings(_env_file=None))

    response = runner.run(AnalysisRunV2Request(project_path=str(project), run_id="000001"))

    assert response.summary.overall_judgement == "no_change_needed"
    assert runner.last_state["analysis_route"] == "no_change_needed"
    assert runner.last_state["rag_route"] == "skipped"
    assert runner.last_state["recommendation_route"] == "none"


def test_graph_runner_generates_recommendations_for_repeated_blocked_region(tmp_path) -> None:
    experiments = tmp_path / "experiments"
    _write_blocked_episode(experiments, "000001")
    _write_blocked_episode(experiments, "000002")
    runner = ResultAnalysisGraphRunnerV2(experiments_root=experiments, settings=Settings(_env_file=None))

    response = runner.run()

    assert response.summary.overall_judgement == "change_recommended"
    assert response.patterns[0]["type"] == "blocked_region_violation_repeated"
    assert response.recommendations[0]["target"] == "policy"
    assert "analysis_mode" not in response.model_dump(by_alias=True)
    assert runner.last_state["analysis_route"] == "patterns_found"
    assert runner.last_state["recommendation_route"] == "rule_based_fallback"


def test_graph_runner_retrieves_file_based_rag_context_for_patterns(tmp_path) -> None:
    experiments = tmp_path / "experiments"
    _write_blocked_episode(experiments, "000001")
    _write_blocked_episode(experiments, "000002")
    runner = ResultAnalysisGraphRunnerV2(experiments_root=experiments, settings=Settings(_env_file=None))

    response = runner.run()
    state = runner.last_state

    assert response.summary.overall_judgement == "change_recommended"
    assert state["rag_route"] == "retrieved"
    assert state["rag_diagnostic"]["backend"] == "file_based_jsonl"
    assert state["rag_diagnostic"]["used"] is True
    assert state["rag_diagnostic"]["retrieved_chunk_count"] > 0
    assert state["rag_context"]["retrieval_mode"] == "file_based_jsonl"
    assert state["retrieved_context"]
    assert all("chunk_id" not in item for item in state["retrieved_context"])
    assert all("card_id" not in item for item in state["retrieved_context"])
    assert all("matched_fields" not in item for item in state["retrieved_context"])
    assert all("retrieval_score" not in item for item in state["retrieved_context"])


def test_graph_runner_rag_store_missing_falls_back_without_public_leak(monkeypatch, tmp_path) -> None:
    def missing_store(_query):
        raise FileNotFoundError("C:/internal/path/policy_rag_chunks.jsonl")

    monkeypatch.setattr(rag_module, "search_policy_chunks", missing_store, raising=False)
    experiments = tmp_path / "experiments"
    _write_blocked_episode(experiments, "000001")
    _write_blocked_episode(experiments, "000002")
    runner = ResultAnalysisGraphRunnerV2(experiments_root=experiments, settings=Settings(_env_file=None))

    response = runner.run()
    state = runner.last_state

    assert response.summary.overall_judgement == "change_recommended"
    assert state["rag_route"] == "store_missing"
    assert state["rag_diagnostic"]["fallback_reason"] == "store_missing"
    assert "C:/internal/path" not in json.dumps(state["rag_diagnostic"])
    assert "policy_rag_chunks.jsonl" not in json.dumps(state["rag_diagnostic"])


def test_graph_runner_rag_query_empty_falls_back(tmp_path) -> None:
    experiments = tmp_path / "experiments"
    _write_blocked_episode(experiments, "000001")
    _write_blocked_episode(experiments, "000002")
    runner = ResultAnalysisGraphRunnerV2(experiments_root=experiments, settings=Settings(_env_file=None))
    runner.agent.rag_query_builder.build_queries = lambda **_kwargs: []

    response = runner.run()
    state = runner.last_state

    assert response.summary.overall_judgement == "change_recommended"
    assert state["rag_route"] == "no_query"
    assert state["rag_diagnostic"]["fallback_reason"] == "query_empty"
    assert state["recommendation_route"] == "rule_based_fallback"


def test_graph_runner_sequential_fallback_shares_route_decisions(tmp_path) -> None:
    experiments = tmp_path / "experiments"
    _write_blocked_episode(experiments, "000001")
    _write_blocked_episode(experiments, "000002")
    graph_runner = ResultAnalysisGraphRunnerV2(experiments_root=experiments, settings=Settings(_env_file=None))
    fallback_runner = ResultAnalysisGraphRunnerV2(experiments_root=experiments, settings=Settings(_env_file=None))
    fallback_runner.compiled_graph = None

    graph_runner.run()
    fallback_runner.run()

    assert graph_runner.last_state["analysis_route"] == fallback_runner.last_state["analysis_route"]
    assert graph_runner.last_state["rag_route"] == fallback_runner.last_state["rag_route"]
    assert graph_runner.last_state["recommendation_route"] == fallback_runner.last_state["recommendation_route"]


def test_graph_runner_exposes_node_state_without_response_schema_changes(tmp_path) -> None:
    experiments = tmp_path / "experiments"
    _write_blocked_episode(experiments, "000001")
    _write_blocked_episode(experiments, "000002")
    runner = ResultAnalysisGraphRunnerV2(experiments_root=experiments, settings=Settings(_env_file=None))

    response = runner.run()
    state = runner.last_state

    assert "episode_timelines" in state
    assert "representative_failed_episodes" in state
    assert "rag_queries" in state
    assert "retrieved_context" in state
    assert "analysis_context" in state
    assert "episode_timelines" not in response.model_dump(by_alias=True)
    assert "rag_queries" not in response.model_dump(by_alias=True)


def test_graph_runner_passes_result_analysis_system_prompt_to_llm(tmp_path) -> None:
    experiments = tmp_path / "experiments"
    _write_blocked_episode(experiments, "000001")
    _write_blocked_episode(experiments, "000002")
    fake = _FakeJsonClient(
        [
            {
                "recommendations": [
                    _llm_policy_recommendation(
                        reason="차단 구역 침범이 반복되어 주행 정책 조건 검토가 필요합니다.",
                        recommendation="보행 경계 이탈 가능성을 줄이도록 감속 조건을 검토하는 것이 좋습니다.",
                    )
                ]
            }
        ]
    )
    runner = _runner_with_fake_llm(experiments, fake)

    response = runner.run()

    assert fake.calls
    assert fake.calls[0]["response_name"] == "analysis_recommendations_v2"
    assert "결과 분석 에이전트" in fake.calls[0]["system_prompt"]
    assert "episode evidence" in fake.calls[0]["system_prompt"]
    assert response.recommendations[0]["recommendation"] == (
        "보행 경계 이탈 가능성을 줄이도록 감속 조건을 검토하는 것이 좋습니다."
    )


def test_graph_runner_falls_back_when_llm_public_text_contains_internal_source_terms(tmp_path) -> None:
    experiments = tmp_path / "experiments"
    _write_blocked_episode(experiments, "000001")
    _write_blocked_episode(experiments, "000002")
    fake = _FakeJsonClient(
        [
            {
                "recommendations": [
                    _llm_policy_recommendation(
                        reason="KOR-003 p.33 관련 정책 문서 RAG 근거 문서가 있습니다.",
                        recommendation="policy card 기준을 반영했습니다.",
                    )
                ]
            }
        ]
    )
    runner = _runner_with_fake_llm(experiments, fake)

    response = runner.run()

    public_text = "\n".join(
        [
            response.summary.message,
            *(
                f"{item.get('reason', '')}\n{item.get('recommendation', '')}"
                for item in response.recommendations
            ),
            *(insight.description for insight in response.insights),
        ]
    )
    assert all(forbidden not in public_text for forbidden in FORBIDDEN_LLM_PUBLIC_TEXT)
    assert any("rule-based recommendation fallback" in warning for warning in response.warnings)
    assert response.recommendations[0]["recommendation"] != "policy card 기준을 반영했습니다."
    assert runner.last_state["recommendation_route"] == "rule_based_fallback"


def test_graph_runner_falls_back_when_llm_policy_parameter_change_is_invalid(tmp_path) -> None:
    experiments = tmp_path / "experiments"
    _write_blocked_episode(experiments, "000001")
    _write_blocked_episode(experiments, "000002")
    recommendation = _llm_policy_recommendation(
        reason="차단 구역 침범이 반복되어 주행 정책 조건 검토가 필요합니다.",
        recommendation="LLM invalid change should not be public.",
    )
    recommendation["proposed_change"]["content"] = {"unsupportedPolicyParameter_max": 1.0}
    fake = _FakeJsonClient([{"recommendations": [recommendation]}])
    runner = _runner_with_fake_llm(experiments, fake)

    response = runner.run()

    assert any("rule-based recommendation fallback" in warning for warning in response.warnings)
    assert response.recommendations[0]["recommendation"] != "LLM invalid change should not be public."
    assert runner.last_state["recommendation_route"] == "rule_based_fallback"


def test_analysis_api_preserves_response_schema(tmp_path) -> None:
    project = tmp_path / "Project1"
    _write_project_blocked_episode(project, "000001")
    _write_project_blocked_episode(project, "000002")

    response = TestClient(app).post("/api/v2/analysis/run", json={"project_path": str(project), "run_id": "000001"})

    assert response.status_code == 200
    payload = response.json()
    assert payload["schema"] == "analysis_run_response_v2"
    assert payload["summary"]["overall_judgement"] == "change_recommended"
    assert "episode_timelines" not in payload
    assert all(field not in payload for field in FORBIDDEN_PUBLIC_PATH_FIELDS)


def test_analysis_api_uses_graph_runner_path(tmp_path) -> None:
    project = tmp_path / "Project1"
    _write_project_blocked_episode(project, "000001")
    _write_project_blocked_episode(project, "000002")

    response = TestClient(app).post("/api/v2/analysis/run", json={"project_path": str(project), "run_id": "000001"})

    assert response.status_code == 200
    payload = response.json()
    assert payload["schema"] == "analysis_run_response_v2"
    assert payload["summary"]["overall_judgement"] == "change_recommended"
    assert "analysis_mode" not in payload

    review_dir = project / "runs" / "000001" / "review" / "0001"
    report = json.loads((review_dir / "report.json").read_text(encoding="utf-8"))
    recommendations = json.loads((review_dir / "recommendations.json").read_text(encoding="utf-8"))
    manifest = json.loads((review_dir / "manifest.json").read_text(encoding="utf-8"))
    public_surfaces = "\n".join(
        [
            json.dumps(payload, ensure_ascii=False),
            json.dumps(report, ensure_ascii=False),
            json.dumps(recommendations, ensure_ascii=False),
        ]
    )
    assert not (review_dir / "rag_evidence.json").exists()
    assert not (review_dir / "internal").exists()
    assert "rag_diagnostic" not in manifest
    assert all("rag_evidence" not in item for item in manifest.get("generated_files", []))
    assert all(term not in public_surfaces for term in FORBIDDEN_LLM_PUBLIC_TEXT)
    assert all(term not in public_surfaces for term in FORBIDDEN_RAG_PUBLIC_METADATA)


def test_analysis_api_falls_back_when_openai_fails_without_ollama_attempt(monkeypatch, tmp_path) -> None:
    """Keep result analysis stable when the first configured LLM provider fails."""
    project = tmp_path / "Project1"
    _write_project_blocked_episode(project, "000001")
    _write_project_blocked_episode(project, "000002")
    providers: list[str] = []

    def fail_generate_json(self, **_kwargs) -> dict:
        providers.append(self.provider.value)
        raise ValueError("forced OpenAI failure")

    monkeypatch.setenv("V2_AGENT_LLM_ENABLED", "true")
    monkeypatch.setenv("LLM_PROVIDER_CHAIN", "openai,ollama")
    monkeypatch.setattr(AgentLlmJsonClient, "generate_json", fail_generate_json)

    response = TestClient(app).post("/api/v2/analysis/run", json={"project_path": str(project), "run_id": "000001"})

    assert response.status_code == 200
    payload = response.json()
    assert providers == ["openai"]
    assert payload["schema"] == "analysis_run_response_v2"
    assert "analysis_mode" not in payload
    assert payload["summary"]["overall_judgement"] == "change_recommended"
    assert payload["recommendations"]
    assert any("rule-based recommendation fallback" in warning for warning in payload["warnings"])


def test_analysis_api_falls_back_when_ollama_provider_fails(monkeypatch, tmp_path) -> None:
    """Treat a selected local Ollama provider failure as a non-fatal LLM failure."""
    project = tmp_path / "Project1"
    _write_project_blocked_episode(project, "000001")
    _write_project_blocked_episode(project, "000002")
    providers: list[str] = []

    def fail_generate_json(self, **_kwargs) -> dict:
        providers.append(self.provider.value)
        raise ValueError("forced Ollama failure")

    monkeypatch.setenv("V2_AGENT_LLM_ENABLED", "true")
    monkeypatch.setenv("LLM_PROVIDER_CHAIN", "ollama")
    monkeypatch.setattr(AgentLlmJsonClient, "generate_json", fail_generate_json)

    response = TestClient(app).post("/api/v2/analysis/run", json={"project_path": str(project), "run_id": "000001"})

    assert response.status_code == 200
    payload = response.json()
    assert providers == ["ollama"]
    assert payload["schema"] == "analysis_run_response_v2"
    assert "analysis_mode" not in payload
    assert payload["summary"]["overall_judgement"] == "change_recommended"
    assert payload["recommendations"]
    assert any("rule-based recommendation fallback" in warning for warning in payload["warnings"])


def test_analysis_api_uses_summary_without_episode_results(tmp_path) -> None:
    project = tmp_path / "Project1"
    _write_project_summary_rows(
        project,
        [
            {
                "episode_id": "000001",
                "outcome": "Success",
                "terminal_reason": "GoalReached",
                "duration_s": 10.0,
                "metrics": {"goal_reached": 1},
            },
            {
                "episode_id": "000002",
                "outcome": "Failure",
                "terminal_reason": "Timeout",
                "duration_s": 10.0,
                "metrics": {"goal_reached": 0},
            },
        ],
    )

    response = TestClient(app).post("/api/v2/analysis/run", json={"project_path": str(project), "run_id": "000001"})

    assert response.status_code == 200
    payload = response.json()
    assert payload["schema"] == "analysis_run_response_v2"
    assert payload["analysis_scope"] == {"experiments_count": 1, "runs_count": 1, "episodes_count": 2}
    assert payload["metrics"]["success_count"] == 1
    assert payload["metrics"]["failure_count"] == 1


def test_analysis_api_accepts_prompt_without_schema_changes(tmp_path) -> None:
    project = tmp_path / "Project1"
    _write_project_blocked_episode(project, "000001")
    _write_project_blocked_episode(project, "000002")

    response = TestClient(app).post(
        "/api/v2/analysis/run",
        json={
            "project_path": str(project),
            "run_id": "000001",
            "prompt": "보도이탈과 페널티 중심으로 다시 분석해줘",
        },
    )

    assert response.status_code == 200
    payload = response.json()
    assert payload["schema"] == "analysis_run_response_v2"
    assert payload["summary"]["overall_judgement"] == "change_recommended"
    assert "사용자 요청 관점" in payload["summary"]["message"]
    assert "route_deviation" in payload["summary"]["message"]
    assert "penalty" in payload["summary"]["message"]
    assert "prompt_focus" not in payload
