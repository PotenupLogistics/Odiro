"""5-입력 통합 분석·추천 (analyze_full_setup_and_recommend) 테스트."""
from __future__ import annotations

import json
from pathlib import Path
from types import SimpleNamespace

from app.models.llm import (
    LlmGenerationRequest,
    LlmGenerationResponse,
    LlmProvider,
)
from app.models.recommendation import (
    EpisodeSetupRecommendation,
    IntegratedRecommendationResult,
)
from app.services.episode_setup_patcher import apply_episode_setup_recommendations
from app.services.policy_recommendation_orchestrator import analyze_full_setup_and_recommend
from app.services.policy_server_inspector import extract_policy_defaults_from_source


ROOT = Path(__file__).resolve().parents[1]
FIXTURES = Path(__file__).parent / "fixtures"
SAMPLE_REPORT = FIXTURES / "sample_evaluation_report.json"


def _write_policy_server(tmp_path: Path) -> Path:
    path = tmp_path / "policy_server.py"
    path.write_text(
        "\n".join(
            [
                'FORCED_ACTION = "Repath"',
                'policy.add_threshold("stopDistanceM", 1.2)',
                'policy.add_threshold("slowDownDistanceM", 3.5)',
            ]
        )
        + "\n",
        encoding="utf-8",
    )
    return path


def _write_measurement_log(tmp_path: Path) -> Path:
    path = tmp_path / "measurement_log_minimal.jsonl"
    records = [
        {"type": "header", "runId": "integrated-test-run"},
        {
            "type": "tick",
            "deltaSeconds": 1.0,
            "worldTimeSeconds": 1.0,
            "robot": {
                "perception": {"lidar": {"hasFrontObject": True, "frontDistanceM": 0.5}},
                "action": {"reason": "slowdown", "brakeApplied": True, "targetSpeedKmh": 2.0},
                "truth": {"v": [100.0, 0.0, 0.0]},
            },
        },
        {
            "type": "tick",
            "deltaSeconds": 1.0,
            "worldTimeSeconds": 2.0,
            "robot": {
                "perception": {"lidar": {"hasFrontObject": True, "frontDistanceM": 1.0}},
                "action": {"reason": "repath", "brakeApplied": False, "targetSpeedKmh": 4.0},
                "truth": {"v": [120.0, 0.0, 0.0]},
            },
        },
        {
            "type": "footer",
            "closeReason": "world_end_play",
            "diagnostics": [{"severity": "warning", "message": "unit test diagnostic"}],
        },
    ]
    path.write_text(
        "\n".join(json.dumps(record, ensure_ascii=False) for record in records) + "\n",
        encoding="utf-8",
    )
    return path


def _write_episode_setup(tmp_path: Path) -> Path:
    path = tmp_path / "episode_setup.json"
    path.write_text(
        json.dumps(
            {
                "scenario_id": "integrated_test",
                "run": {"time_limit_s": 60.0},
                "evaluation": {
                    "goal_acceptance_radius_m": 1.0,
                    "near_miss": {"distance_m": 0.5},
                    "tip_over_angle_deg": 60.0,
                },
                "actors": {
                    "pedestrians": [{"movement": {"speed_mps": 1.2}, "spawn_time_s": 2.0}],
                    "static_obstacles": [{"xy_m": [3.0, 1.0]}],
                },
            },
            ensure_ascii=False,
        ),
        encoding="utf-8",
    )
    return path


def _write_bot_setup(tmp_path: Path) -> Path:
    path = tmp_path / "delivery_bot_setup.json"
    path.write_text(
        json.dumps(
            {
                "schema": "delivery_bot_setup",
                "version": 1,
                "robot": {
                    "drive": {"max_speed_kmh": 10.0},
                    "path_follow": {
                        "target_speed_kmh": 10.0,
                        "look_ahead_distance_m": 1.0,
                        "obstacle_slow_speed_kmh": 1.5,
                    },
                    "lidar": {
                        "stop_distance_m": 1.2,
                        "slow_down_distance_m": 3.5,
                        "scan_range_m": 5.0,
                        "angle_step_degree": 2.0,
                    },
                },
            },
            ensure_ascii=False,
        ),
        encoding="utf-8",
    )
    return path


def _write_integrated_inputs(tmp_path: Path) -> SimpleNamespace:
    return SimpleNamespace(
        measurement_log=_write_measurement_log(tmp_path),
        episode_setup=_write_episode_setup(tmp_path),
        bot_setup=_write_bot_setup(tmp_path),
        policy_server=_write_policy_server(tmp_path),
    )


# ── policy_server_inspector ────────────────────────────────────────────────


def test_extract_policy_defaults_from_source(tmp_path) -> None:
    defaults = extract_policy_defaults_from_source(_write_policy_server(tmp_path))
    assert defaults["force_action_override"] == "Repath"
    assert defaults["stop_distance_m_threshold"] == 1.2
    assert defaults["slow_down_distance_m_threshold"] == 3.5


def test_extract_policy_defaults_missing_file() -> None:
    defaults = extract_policy_defaults_from_source("__no_such_file__.py")
    assert defaults["force_action_override"] is None
    assert defaults["stop_distance_m_threshold"] == 1.2


# ── episode_setup_patcher ──────────────────────────────────────────────────


def test_episode_setup_patcher_dotted_path() -> None:
    current = {
        "run": {"time_limit_s": 60.0},
        "evaluation": {"near_miss": {"distance_m": 0.5}},
        "actors": {"pedestrians": [{"movement": {"speed_mps": 1.2}}]},
    }
    recs = [
        EpisodeSetupRecommendation(
            param="run.time_limit_s", current=60.0, suggested=120.0, reason="x", citations=[]
        ),
        EpisodeSetupRecommendation(
            param="evaluation.near_miss.distance_m", current=0.5, suggested=0.7,
            reason="x", citations=[]
        ),
        EpisodeSetupRecommendation(
            param="actors.pedestrians[*].movement.speed_mps",
            current=1.2, suggested=0.9, reason="x", citations=[]
        ),
    ]
    patched, warnings = apply_episode_setup_recommendations(current, recs)
    assert warnings == []
    assert patched["run"]["time_limit_s"] == 120.0
    assert patched["evaluation"]["near_miss"]["distance_m"] == 0.7
    for p in patched["actors"]["pedestrians"]:
        assert p["movement"]["speed_mps"] == 0.9


def test_episode_setup_patcher_unknown_path_warns() -> None:
    current = {"foo": {"bar": 1}}
    rec = EpisodeSetupRecommendation(
        param="run.time_limit_s", current=60.0, suggested=120.0, reason="x", citations=[]
    )
    patched, warnings = apply_episode_setup_recommendations(current, [rec])
    assert any("run.time_limit_s" in w for w in warnings)
    assert patched == {"foo": {"bar": 1}}


# ── 통합 분석 fallback 경로 ─────────────────────────────────────────────────


def test_analyze_full_setup_fallback_only(tmp_path) -> None:
    inputs = _write_integrated_inputs(tmp_path)
    output = tmp_path / "integrated.json"
    result = analyze_full_setup_and_recommend(
        evaluation_report_path=SAMPLE_REPORT,
        measurement_log_path=inputs.measurement_log,
        episode_setup_path=inputs.episode_setup,
        bot_setup_path=inputs.bot_setup,
        policy_server_path=inputs.policy_server,
        provider=LlmProvider.ollama,
        fallback_only=True,
        output_path=output,
    )

    assert isinstance(result, IntegratedRecommendationResult)
    assert result.generationMethod == "fallback_rules"
    assert result.episodeStatistics.outcome == "Failure"
    assert result.measurementStatistics.totalTicks > 0
    assert result.setupSnapshot["policy_server"]["force_action_override"] == "Repath"
    assert result.nextBotSetup is not None
    assert result.nextEpisodeSetup is not None

    # 출력 파일이 작성되었고 다시 로드 가능해야 한다.
    assert output.exists()
    payload = json.loads(output.read_text(encoding="utf-8"))
    assert payload["analysisId"] == result.analysisId
    assert "botSetupRecommendations" in payload
    assert "episodeSetupRecommendations" in payload
    assert "policyServerRecommendations" in payload


def test_analyze_full_setup_unusable_report(tmp_path) -> None:
    inputs = _write_integrated_inputs(tmp_path)
    data = json.loads(SAMPLE_REPORT.read_text(encoding="utf-8"))
    data["summary"]["usable_for_llm_tuning"] = False
    p = tmp_path / "unusable.json"
    p.write_text(json.dumps(data), encoding="utf-8")

    result = analyze_full_setup_and_recommend(
        evaluation_report_path=p,
        measurement_log_path=inputs.measurement_log,
        episode_setup_path=inputs.episode_setup,
        bot_setup_path=inputs.bot_setup,
        policy_server_path=inputs.policy_server,
        provider=LlmProvider.ollama,
        fallback_only=True,
    )
    assert result.botSetupRecommendations == []
    assert result.episodeSetupRecommendations == []
    assert result.policyServerRecommendations == []
    assert result.nextBotSetup is None
    assert result.nextEpisodeSetup is None
    assert any("usable_for_llm_tuning" in w for w in result.llmWarnings)


# ── 통합 분석 LLM 경로 (stub client) ────────────────────────────────────────


class _StubLlmClient:
    """미리 정해둔 JSON을 항상 반환하는 stub LLM client."""

    def __init__(self, content: str, success: bool = True) -> None:
        self._content = content
        self._success = success

    def generate(self, request: LlmGenerationRequest) -> LlmGenerationResponse:
        return LlmGenerationResponse(
            requestId=request.requestId,
            provider=request.provider,
            model=request.model,
            success=self._success,
            content=self._content if self._success else None,
            rawContent=self._content,
            error=None,
        )


def test_analyze_full_setup_llm_path_with_stub_client(tmp_path) -> None:
    inputs = _write_integrated_inputs(tmp_path)
    stub_payload = {
        "botSetupRecommendations": [
            {
                "param": "stop_distance_m",
                "current": 1.2,
                "suggested": 1.5,
                "reason": "정적 장애물 충돌 1회 — stop 거리 상향",
                "citations": ["KOR-003"],
            }
        ],
        "episodeSetupRecommendations": [
            {
                "param": "run.time_limit_s",
                "current": 60.0,
                "suggested": 90.0,
                "reason": "Timeout 발생 — 시간 한도 50% 상향",
                "citations": ["PRJ-DOE"],
            }
        ],
        "policyServerRecommendations": [
            {
                "param": "force_action_override",
                "current": "Repath",
                "suggested": "None",
                "reason": "운영 회차에서는 강제 액션 해제",
                "citations": ["PRJ-AGENT"],
            }
        ],
        "summary": "통합 추천 stub 테스트",
    }
    client = _StubLlmClient(json.dumps(stub_payload, ensure_ascii=False))

    result = analyze_full_setup_and_recommend(
        evaluation_report_path=SAMPLE_REPORT,
        measurement_log_path=inputs.measurement_log,
        episode_setup_path=inputs.episode_setup,
        bot_setup_path=inputs.bot_setup,
        policy_server_path=inputs.policy_server,
        provider=LlmProvider.openai,
        fallback_only=False,
        llm_client=client,
    )

    assert result.generationMethod == "llm_rag"
    assert len(result.botSetupRecommendations) == 1
    assert result.botSetupRecommendations[0].param == "stop_distance_m"
    assert len(result.episodeSetupRecommendations) == 1
    assert result.episodeSetupRecommendations[0].param == "run.time_limit_s"
    assert len(result.policyServerRecommendations) == 1
    assert result.policyServerRecommendations[0].suggested == "None"
    assert result.nextEpisodeSetup is not None
    assert result.nextEpisodeSetup["run"]["time_limit_s"] == 90.0


def test_analyze_full_setup_llm_failure_falls_back(tmp_path) -> None:
    inputs = _write_integrated_inputs(tmp_path)
    client = _StubLlmClient("garbage", success=False)
    result = analyze_full_setup_and_recommend(
        evaluation_report_path=SAMPLE_REPORT,
        measurement_log_path=inputs.measurement_log,
        episode_setup_path=inputs.episode_setup,
        bot_setup_path=inputs.bot_setup,
        policy_server_path=inputs.policy_server,
        provider=LlmProvider.openai,
        fallback_only=False,
        llm_client=client,
    )
    assert result.generationMethod == "fallback_after_llm_failure"
    assert result.nextBotSetup is not None
    assert any("LLM 호출 실패" in w for w in result.llmWarnings)


# ── CLI smoke ──────────────────────────────────────────────────────────────


def _load_cli_module():
    import importlib.util
    script_path = ROOT / "scripts" / "analyze_measurement_log.py"
    spec = importlib.util.spec_from_file_location("analyze_measurement_log", script_path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_full_mode_cli_smoke(tmp_path) -> None:
    """argparse + 오케스트레이터 fallback 경로 전체 흐름."""
    cli_mod = _load_cli_module()
    inputs = _write_integrated_inputs(tmp_path)

    output = tmp_path / "cli_out.json"
    rc = cli_mod.main([
        "full",
        "--evaluation-report", str(SAMPLE_REPORT),
        "--measurement-log", str(inputs.measurement_log),
        "--episode-setup", str(inputs.episode_setup),
        "--bot-setup", str(inputs.bot_setup),
        "--policy-server", str(inputs.policy_server),
        "--provider", "ollama",
        "--fallback-only",
        "--output", str(output),
    ])
    assert rc == 0
    assert output.exists()
    payload = json.loads(output.read_text(encoding="utf-8"))
    assert payload["generationMethod"] == "fallback_rules"
    assert "botSetupRecommendations" in payload
