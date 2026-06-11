"""POST /api/v1/analysis/run 엔드포인트 테스트.

언리얼이 호출하는 분석·추천 API. fallback_only=True로 LLM 없이 결정적으로 검증한다.
"""

from __future__ import annotations

from pathlib import Path

from fastapi.testclient import TestClient

from app.main import app

ROOT = Path(__file__).resolve().parent.parent
SAMPLE_DIR = ROOT / "test_sample"

_EVALUATION_REPORT = (
    SAMPLE_DIR
    / "episode_run_0000_EpisodeSetupSample_1_legacy_route_fallback_001_evaluation_report.json"
)
_MEASUREMENT_LOG = SAMPLE_DIR / "MeasurementLog_20260605_101451_EpisodeSimulationMap.jsonl"
_EPISODE_SETUP = SAMPLE_DIR / "EpisodeSetupSample_1.json"
_BOT_SETUP = SAMPLE_DIR / "DeliveryBotSetupSample_1.json"


def _valid_payload() -> dict:
    return {
        "evaluation_report_path": str(_EVALUATION_REPORT),
        "measurement_log_path": str(_MEASUREMENT_LOG),
        "episode_setup_path": str(_EPISODE_SETUP),
        "bot_setup_path": str(_BOT_SETUP),
        "fallback_only": True,
    }


def test_analysis_run_returns_recommendations_with_fallback() -> None:
    response = TestClient(app).post("/api/v1/analysis/run", json=_valid_payload())

    assert response.status_code == 200, response.text
    body = response.json()

    # fallback_only=True → LLM 미사용 경로
    assert body["generationMethod"] == "fallback_rules"

    # 세 종류 추천 키 + 다음 회차 설정 존재
    assert "botSetupRecommendations" in body
    assert "episodeSetupRecommendations" in body
    assert "policyServerRecommendations" in body
    assert body["nextBotSetup"] is not None
    assert body["nextEpisodeSetup"] is not None

    # 스크립트에서 이식한 추가 필드 포함 확인
    assert body["policySourceAnalysis"] is not None
    assert "forced_action" in body["policySourceAnalysis"]
    assert body["paramConsistencyCheck"] is not None
    assert "ok" in body["paramConsistencyCheck"]


def test_analysis_run_missing_input_returns_400() -> None:
    payload = _valid_payload()
    payload["bot_setup_path"] = str(SAMPLE_DIR / "does_not_exist.json")

    response = TestClient(app).post("/api/v1/analysis/run", json=payload)

    assert response.status_code == 400
    detail = response.json()["detail"]
    assert detail["code"] == "INPUT_FILE_NOT_FOUND"
    assert detail["missing_path"].endswith("does_not_exist.json")


def test_analysis_run_endpoint_in_openapi() -> None:
    schema = TestClient(app).get("/openapi.json").json()
    assert "/api/v1/analysis/run" in schema["paths"]
