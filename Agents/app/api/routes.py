from __future__ import annotations

from pathlib import Path

from fastapi import APIRouter, HTTPException

from app.agents.result_analysis_v2.graph_runner import ResultAnalysisGraphRunnerV2
from app.agents.scenario_generation_v2.graph_runner import ScenarioGenerationGraphRunnerV2
from app.core.settings import Settings
from app.models.analysis_v2 import AnalysisRunV2Request, AnalysisRunV2Response
from app.models.recommendation import EpisodeAnalysisRequest, IntegratedRecommendationResult
from app.models.scenario_generation_v2 import ProjectScenarioV1Response, ScenarioGenerateV2Request
from app.services.policy_recommendation_orchestrator import analyze_full_setup_and_recommend


router = APIRouter()

# 프로젝트 루트 (app/api/routes.py → 2단계 위)
_PROJECT_ROOT = Path(__file__).resolve().parents[2]
# 정책 서버 소스는 서버 내부 코드 — 고정 기본경로 사용
_POLICY_SERVER_SOURCE_PATH = _PROJECT_ROOT / "test_sample" / "policy_server.py"
_SENSITIVE_ERROR_TOKENS = (
    "api_key",
    "apikey",
    "authorization",
    "bearer",
    "credential",
    "credentials",
    "private_key",
    "secret",
    "token",
)


def _redact_error_message(message: str) -> str:
    safe_message = message or "Scenario generation failed."
    for token in _SENSITIVE_ERROR_TOKENS:
        safe_message = safe_message.replace(token, "[redacted]")
    return safe_message


def _scenario_generation_error_detail(exc: Exception) -> dict[str, str]:
    return {
        "code": "SCENARIO_GENERATION_FAILED",
        "message": _redact_error_message(str(exc)),
        "stage": "scenario_generation",
    }


@router.get("/health")
def health() -> dict[str, str]:
    return {"status": "ok", "service": "proto-ai", "version": "0.1.0"}


@router.post(
    "/api/v1/scenarios/generate",
)
def scenario_generate_endpoint(
) -> None:
    raise HTTPException(
        status_code=410,
        detail={
            "code": "RUN_QUEUE_REMOVED",
            "message": "RunQueue scenario generation was removed. Use /api/v2/scenarios/generate and user project runs.",
        },
    )


@router.post(
    "/api/v2/scenarios/generate",
    response_model=ProjectScenarioV1Response,
)
def scenario_generate_v2_endpoint(
    request: ScenarioGenerateV2Request,
) -> ProjectScenarioV1Response:
    settings = Settings()
    response = ScenarioGenerationGraphRunnerV2(settings=settings).run(request)
    if response.scenario is None or not response.validation.valid:
        raise HTTPException(status_code=500, detail=_scenario_generation_error_detail(Exception(response.summary)))
    return ProjectScenarioV1Response.model_validate(response.scenario)


@router.post(
    "/api/v1/analysis/run",
    response_model=IntegratedRecommendationResult,
)
def analysis_run_endpoint(
    request: EpisodeAnalysisRequest,
) -> IntegratedRecommendationResult:
    """언리얼 에피소드 결과 분석·추천 실행.

    서버 로컬 파일 경로 4개를 받아 5-입력 통합 파이프라인을 돌리고,
    추천 + nextBotSetup/nextEpisodeSetup 전체를 JSON으로 반환한다.
    policy_server.py는 서버 내부 고정경로를 사용한다.
    """
    # 1. 입력 파일 존재 확인
    required_paths = {
        "evaluation_report_path": request.evaluation_report_path,
        "measurement_log_path": request.measurement_log_path,
        "episode_setup_path": request.episode_setup_path,
        "bot_setup_path": request.bot_setup_path,
    }
    for field, raw_path in required_paths.items():
        if not Path(raw_path).is_file():
            raise HTTPException(
                status_code=400,
                detail={
                    "code": "INPUT_FILE_NOT_FOUND",
                    "message": f"Input file not found for {field}.",
                    "missing_path": raw_path,
                },
            )

    # 2. 분석 실행 (policy_server는 서버 내부 고정경로, 결과는 파일 저장 안 함)
    try:
        return analyze_full_setup_and_recommend(
            evaluation_report_path=request.evaluation_report_path,
            measurement_log_path=request.measurement_log_path,
            episode_setup_path=request.episode_setup_path,
            bot_setup_path=request.bot_setup_path,
            policy_server_path=_POLICY_SERVER_SOURCE_PATH,
            fallback_only=request.fallback_only,
            output_path=None,
        )
    except Exception as exc:
        raise HTTPException(
            status_code=500,
            detail={
                "code": "ANALYSIS_FAILED",
                "message": "Episode analysis failed.",
            },
        ) from exc


@router.post(
    "/api/v2/analysis/run",
    response_model=AnalysisRunV2Response,
)
def analysis_run_v2_endpoint(
    request: AnalysisRunV2Request,
) -> AnalysisRunV2Response:
    settings = Settings()
    return ResultAnalysisGraphRunnerV2(settings=settings).run(request)
