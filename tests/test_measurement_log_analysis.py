from __future__ import annotations

import json
from pathlib import Path

import pytest

from app.models.recommendation import (
    AnalysisStatistics,
    ParamRecommendation,
    PolicyRecommendationResult,
)
from app.services.measurement_log_parser import (
    MeasurementLogParseError,
    parse_measurement_log,
)
from app.services.metrics_extractor import extract_run_metrics, extract_statistics
from app.services.policy_fallback_rules import (
    DEFAULT_MAX_SPEED_KMH,
    DEFAULT_SLOW_DOWN_DISTANCE_M,
    DEFAULT_STOP_DISTANCE_M,
    PolicyParamDefaults,
    apply_fallback_rules,
    build_fallback_summary,
)
from app.services.policy_recommendation_orchestrator import analyze_and_recommend
from app.services.policy_recommendation_rag_retriever import retrieve_policy_context


ROOT = Path(__file__).resolve().parents[1]


def _write_measurement_log(tmp_path: Path) -> Path:
    """자동 pytest는 대용량/수동 MeasurementLog를 repo 필수 입력으로 요구하지 않는다."""
    path = tmp_path / "measurement_log_minimal.jsonl"
    records = [
        {"type": "header", "runId": "unit-test-run"},
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
                "action": {"reason": "path_follow", "brakeApplied": False, "targetSpeedKmh": 5.0},
                "truth": {"v": [120.0, 0.0, 0.0]},
            },
        },
        {
            "type": "tick",
            "deltaSeconds": 1.0,
            "worldTimeSeconds": 3.0,
            "robot": {
                "perception": {"lidar": {"hasFrontObject": False, "frontDistanceM": 0.0}},
                "action": {"reason": "stop", "brakeApplied": True, "targetSpeedKmh": 0.0},
                "truth": {"v": [0.0, 0.0, 0.0]},
            },
        },
        {
            "type": "tick",
            "deltaSeconds": 1.0,
            "worldTimeSeconds": 4.0,
            "robot": {
                "perception": {"lidar": {"hasFrontObject": True, "frontDistanceM": 2.0}},
                "action": {"reason": "repath", "brakeApplied": False, "targetSpeedKmh": 3.0},
                "truth": {"v": [90.0, 0.0, 0.0]},
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


def _make_statistics(**overrides) -> AnalysisStatistics:
    defaults = dict(
        totalTicks=602,
        deliveryTimeSec=20.0,
        closeReason="world_end_play",
        diagnosticsWarningCount=5,
        avgFrontDistanceM=2.0,
        minFrontDistanceM=1.5,
        medianFrontDistanceM=2.0,
        frontObjectDetectionRate=0.5,
        actionReasonFreq={"slowdown": 300, "path_follow": 200, "stop": 50, "repath": 10, "unknown": 42},
        brakeAppliedCount=60,
        brakeAppliedRatio=0.1,
        avgTargetSpeedKmh=5.0,
        targetSpeedVariance=1.0,
        avgActualSpeedKmh=4.5,
        stopActionRatio=50 / 602,
        slowdownActionRatio=300 / 602,
        repathActionRatio=10 / 602,
    )
    defaults.update(overrides)
    return AnalysisStatistics(**defaults)


# -------- parser tests --------


def test_parse_measurement_log_returns_header_ticks_footer(tmp_path: Path) -> None:
    data = parse_measurement_log(_write_measurement_log(tmp_path))
    assert data.header["type"] == "header"
    assert data.footer["type"] == "footer"
    assert len(data.ticks) > 0
    assert all(tick.get("type") == "tick" for tick in data.ticks)


def test_parse_measurement_log_missing_file() -> None:
    with pytest.raises(MeasurementLogParseError):
        parse_measurement_log("nonexistent_path.jsonl")


def test_parse_measurement_log_empty_file(tmp_path) -> None:
    empty = tmp_path / "empty.jsonl"
    empty.write_text("", encoding="utf-8")
    with pytest.raises(MeasurementLogParseError):
        parse_measurement_log(empty)


def test_parse_measurement_log_invalid_json(tmp_path) -> None:
    bad = tmp_path / "bad.jsonl"
    bad.write_text('{"type":"header"}\nnot json\n{"type":"footer"}\n', encoding="utf-8")
    with pytest.raises(MeasurementLogParseError):
        parse_measurement_log(bad)


# -------- metrics extractor tests --------


def test_extract_statistics_from_self_contained_log(tmp_path: Path) -> None:
    data = parse_measurement_log(_write_measurement_log(tmp_path))
    statistics = extract_statistics(data)

    assert statistics.totalTicks == 4
    assert statistics.deliveryTimeSec > 0
    assert statistics.closeReason == "world_end_play"
    assert 0 <= statistics.frontObjectDetectionRate <= 1
    # 로그에서 slowdown reason이 있어야 함 (라이다 감지)
    assert "slowdown" in statistics.actionReasonFreq or "path_follow" in statistics.actionReasonFreq
    assert statistics.brakeAppliedRatio >= 0
    assert statistics.avgFrontDistanceM > 0


def test_extract_run_metrics_maps_basic_fields(tmp_path: Path) -> None:
    data = parse_measurement_log(_write_measurement_log(tmp_path))
    statistics = extract_statistics(data)
    metrics = extract_run_metrics(data, statistics)

    assert metrics.deliveryTimeSec == statistics.deliveryTimeSec
    assert metrics.stopCount == statistics.actionReasonFreq.get("stop", 0)
    assert metrics.rerouteCount == statistics.actionReasonFreq.get("repath", 0)
    assert metrics.collisionCount == 0
    assert metrics.minPedestrianDistanceCm == statistics.minFrontDistanceM * 100.0


# -------- fallback rules tests --------


def test_fallback_rules_trigger_stop_distance_when_min_close() -> None:
    statistics = _make_statistics(minFrontDistanceM=0.5)
    recommendations = apply_fallback_rules(statistics)
    params = {rec.param for rec in recommendations}
    assert "stopDistanceM" in params


def test_fallback_rules_trigger_slow_down_when_brake_ratio_high() -> None:
    statistics = _make_statistics(brakeAppliedRatio=0.4, brakeAppliedCount=240)
    recommendations = apply_fallback_rules(statistics)
    params = {rec.param for rec in recommendations}
    assert "slowDownDistanceM" in params


def test_fallback_rules_trigger_can_repath_off_when_repath_heavy() -> None:
    statistics = _make_statistics(
        repathActionRatio=0.1, deliveryTimeSec=30.0,
    )
    recommendations = apply_fallback_rules(statistics)
    canrepath_recs = [rec for rec in recommendations if rec.param == "canRepath"]
    assert canrepath_recs
    assert canrepath_recs[0].suggested is False


def test_fallback_rules_empty_when_all_nominal() -> None:
    statistics = _make_statistics()
    recommendations = apply_fallback_rules(statistics)
    assert recommendations == []
    summary = build_fallback_summary(recommendations)
    assert "조정이 필요한 파라미터가 없음" in summary


def test_fallback_defaults_match_claude_md_constants() -> None:
    defaults = PolicyParamDefaults()
    assert defaults.stopDistanceM == DEFAULT_STOP_DISTANCE_M == 1.2
    assert defaults.slowDownDistanceM == DEFAULT_SLOW_DOWN_DISTANCE_M == 3.5
    assert defaults.maxSpeedKmh == DEFAULT_MAX_SPEED_KMH == 10.0


# -------- RAG retriever tests --------


def test_retrieve_policy_context_returns_contexts_for_nominal_input() -> None:
    statistics = _make_statistics()
    contexts = retrieve_policy_context(statistics)
    assert contexts  # 트리거 없어도 기본 컨텍스트는 반환
    for context in contexts:
        assert context.param in {"stopDistanceM", "slowDownDistanceM", "maxSpeedKmh", "canRepath"}


def test_retrieve_policy_context_triggers_more_queries_on_high_brake() -> None:
    statistics = _make_statistics(brakeAppliedRatio=0.4, minFrontDistanceM=0.5)
    contexts = retrieve_policy_context(statistics)
    params = {context.param for context in contexts}
    assert "stopDistanceM" in params
    assert "slowDownDistanceM" in params


# -------- orchestrator E2E tests (fallback path만) --------


def test_orchestrator_fallback_only_produces_result(tmp_path: Path) -> None:
    sample_log = _write_measurement_log(tmp_path)
    result = analyze_and_recommend(
        log_path=sample_log,
        fallback_only=True,
    )
    assert isinstance(result, PolicyRecommendationResult)
    assert result.generationMethod == "fallback_rules"
    assert result.statistics.totalTicks > 0
    assert isinstance(result.recommendations, list)
    assert result.logPath.endswith(".jsonl")


def test_orchestrator_result_is_json_serializable(tmp_path: Path) -> None:
    sample_log = _write_measurement_log(tmp_path)
    result = analyze_and_recommend(
        log_path=sample_log,
        fallback_only=True,
    )
    payload = result.model_dump(mode="json")
    text = json.dumps(payload, ensure_ascii=False)
    reloaded = json.loads(text)
    assert reloaded["analysisId"] == result.analysisId
    assert reloaded["generationMethod"] == "fallback_rules"


def test_recommendation_validation_rejects_invalid_param() -> None:
    with pytest.raises(Exception):
        ParamRecommendation(
            param="invalidParam",  # type: ignore[arg-type]
            current=1.0,
            suggested=1.5,
            reason="test",
        )


# -------- LLM 래퍼 단위 테스트 (mock 클라이언트) --------


def test_llm_recommendation_parses_valid_openai_response() -> None:
    from app.models.llm import (
        LlmGenerationRequest,
        LlmGenerationResponse,
        LlmProvider,
    )
    from app.services.policy_recommendation_llm_client import (
        generate_recommendations,
    )

    class _StubClient:
        def __init__(self) -> None:
            self.last_request: LlmGenerationRequest | None = None

        def generate(self, request: LlmGenerationRequest) -> LlmGenerationResponse:
            self.last_request = request
            return LlmGenerationResponse(
                requestId=request.requestId,
                provider=LlmProvider.openai,
                model="gpt-4o-mini",
                success=True,
                content=(
                    '{"recommendations":['
                    '{"param":"stopDistanceM","current":1.2,"suggested":1.5,'
                    '"reason":"통계 근거","citations":["KOR-003"]}'
                    '],"summary":"한 개 추천"}'
                ),
                rawContent=None,
                usage=None,
                error=None,
                warnings=[],
            )

    stub = _StubClient()
    statistics = _make_statistics(minFrontDistanceM=0.5, brakeAppliedRatio=0.3)
    outcome = generate_recommendations(
        statistics=statistics,
        contexts=[],
        provider=LlmProvider.openai,
        client=stub,
    )

    assert outcome.success is True
    assert len(outcome.recommendations) == 1
    assert outcome.recommendations[0].param == "stopDistanceM"
    assert outcome.summary == "한 개 추천"
    # request body가 policy-recommendation 모델로 전달됐는지 확인
    assert stub.last_request is not None
    assert stub.last_request.model == "policy-recommendation"


def test_llm_recommendation_falls_through_on_invalid_param() -> None:
    from app.models.llm import (
        LlmGenerationRequest,
        LlmGenerationResponse,
        LlmProvider,
    )
    from app.services.policy_recommendation_llm_client import (
        generate_recommendations,
    )

    class _StubClient:
        def generate(self, request: LlmGenerationRequest) -> LlmGenerationResponse:
            return LlmGenerationResponse(
                requestId=request.requestId,
                provider=LlmProvider.openai,
                model="gpt-4o-mini",
                success=True,
                content=(
                    '{"recommendations":['
                    '{"param":"unsupported","current":1,"suggested":2,"reason":"x"}'
                    '],"summary":"ok"}'
                ),
                rawContent=None,
                usage=None,
                error=None,
                warnings=[],
            )

    outcome = generate_recommendations(
        statistics=_make_statistics(),
        contexts=[],
        provider=__import__(
            "app.models.llm", fromlist=["LlmProvider"]
        ).LlmProvider.openai,
        client=_StubClient(),
    )

    # 허용되지 않은 param은 무시되고 success=True 유지 (빈 리스트)
    assert outcome.success is True
    assert outcome.recommendations == []
    assert any("허용되지 않은 param" in w for w in outcome.warnings)
