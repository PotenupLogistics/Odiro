"""5-입력 통합 분석·추천 (analyze_full_setup_and_recommend) 테스트."""
from __future__ import annotations

import json
from pathlib import Path

import pytest

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
SAMPLE_DIR = ROOT / "SetupSample_0"
# SetupSample_0 의 MeasurementLog는 footer가 없는 실측 로그라 parser가 거부한다.
# OpenAI 실행 검증 단계에서 만든 footer-추가 사본을 통합 테스트도 함께 쓴다.
SAMPLE_LOG = SAMPLE_DIR / "test" / "MeasurementLog_with_footer.jsonl"
SAMPLE_EPISODE_SETUP = SAMPLE_DIR / "EpisodeSetupSample_0.json"
SAMPLE_BOT_SETUP = SAMPLE_DIR / "DeliveryBotSetupSample_0.json"
SAMPLE_POLICY_SERVER = SAMPLE_DIR / "policy_server.py"


# ── policy_server_inspector ────────────────────────────────────────────────


def test_extract_policy_defaults_from_source() -> None:
    defaults = extract_policy_defaults_from_source(SAMPLE_POLICY_SERVER)
    assert defaults["force_action_override"] == "Repath"
    assert defaults["stop_distance_m_threshold"] == 1.2
    assert defaults["slow_down_distance_m_threshold"] == 3.5


def test_extract_policy_defaults_missing_file() -> None:
    defaults = extract_policy_defaults_from_source("__no_such_file__.py")
    assert defaults["force_action_override"] is None
    assert defaults["stop_distance_m_threshold"] == 1.2


# ── episode_setup_patcher ──────────────────────────────────────────────────


def test_episode_setup_patcher_dotted_path() -> None:
    current = json.loads(SAMPLE_EPISODE_SETUP.read_text(encoding="utf-8-sig"))
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
    output = tmp_path / "integrated.json"
    result = analyze_full_setup_and_recommend(
        evaluation_report_path=SAMPLE_REPORT,
        measurement_log_path=SAMPLE_LOG,
        episode_setup_path=SAMPLE_EPISODE_SETUP,
        bot_setup_path=SAMPLE_BOT_SETUP,
        policy_server_path=SAMPLE_POLICY_SERVER,
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
    data = json.loads(SAMPLE_REPORT.read_text(encoding="utf-8"))
    data["summary"]["usable_for_llm_tuning"] = False
    p = tmp_path / "unusable.json"
    p.write_text(json.dumps(data), encoding="utf-8")

    result = analyze_full_setup_and_recommend(
        evaluation_report_path=p,
        measurement_log_path=SAMPLE_LOG,
        episode_setup_path=SAMPLE_EPISODE_SETUP,
        bot_setup_path=SAMPLE_BOT_SETUP,
        policy_server_path=SAMPLE_POLICY_SERVER,
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
        measurement_log_path=SAMPLE_LOG,
        episode_setup_path=SAMPLE_EPISODE_SETUP,
        bot_setup_path=SAMPLE_BOT_SETUP,
        policy_server_path=SAMPLE_POLICY_SERVER,
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
    client = _StubLlmClient("garbage", success=False)
    result = analyze_full_setup_and_recommend(
        evaluation_report_path=SAMPLE_REPORT,
        measurement_log_path=SAMPLE_LOG,
        episode_setup_path=SAMPLE_EPISODE_SETUP,
        bot_setup_path=SAMPLE_BOT_SETUP,
        policy_server_path=SAMPLE_POLICY_SERVER,
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

    output = tmp_path / "cli_out.json"
    rc = cli_mod.main([
        "full",
        "--evaluation-report", str(SAMPLE_REPORT),
        "--measurement-log", str(SAMPLE_LOG),
        "--episode-setup", str(SAMPLE_EPISODE_SETUP),
        "--bot-setup", str(SAMPLE_BOT_SETUP),
        "--policy-server", str(SAMPLE_POLICY_SERVER),
        "--provider", "ollama",
        "--fallback-only",
        "--output", str(output),
    ])
    assert rc == 0
    assert output.exists()
    payload = json.loads(output.read_text(encoding="utf-8"))
    assert payload["generationMethod"] == "fallback_rules"
    assert "botSetupRecommendations" in payload
