from __future__ import annotations

from io import BytesIO
import json
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

from fastapi import APIRouter, HTTPException
from fastapi.responses import StreamingResponse

from app.models.recommendation import EpisodeAnalysisRequest, IntegratedRecommendationResult
from app.models.run_queue import EpisodeRunQueue
from app.models.scenario_generation import ScenarioDriveArtifactResponse, ScenarioGenerateRequest
from app.services.google_drive_upload_service import GoogleDriveUploadError, upload_scenario_artifacts_to_drive
from app.services.policy_recommendation_orchestrator import analyze_full_setup_and_recommend
from app.services.scenario_generation_service import (
    ScenarioArtifactStorageError,
    ScenarioGenerationArtifacts,
    generate_scenario_artifacts,
    write_scenario_artifacts_to_local_dir,
)
from app.utils.json_sanitizer import remove_json_nulls


router = APIRouter()

# 프로젝트 루트 (app/api/routes.py → 2단계 위)
_PROJECT_ROOT = Path(__file__).resolve().parents[2]
# 정책 서버 소스는 서버 내부 코드 — 고정 기본경로 사용
_POLICY_SERVER_SOURCE_PATH = _PROJECT_ROOT / "test_sample" / "policy_server.py"


@router.get("/health")
def health() -> dict[str, str]:
    return {"status": "ok", "service": "proto-ai", "version": "0.1.0"}


@router.post(
    "/api/v1/scenarios/generate",
    response_model=EpisodeRunQueue,
)
def scenario_generate_endpoint(
    request: ScenarioGenerateRequest,
) -> EpisodeRunQueue:
    artifacts = generate_scenario_artifacts(request, write_export=False)
    try:
        write_scenario_artifacts_to_local_dir(artifacts)
    except ScenarioArtifactStorageError as exc:
        detail = {"code": exc.code, "message": exc.message}
        if exc.filename:
            detail["filename"] = exc.filename
        raise HTTPException(status_code=500, detail=detail) from exc
    return artifacts.queue.run_queue


def _json_bytes(payload: object) -> bytes:
    return (json.dumps(payload, ensure_ascii=False, indent=2) + "\n").encode("utf-8")


def _artifact_zip_bytes(artifacts: ScenarioGenerationArtifacts) -> bytes:
    buffer = BytesIO()
    used_names: set[str] = set()

    def unique_name(name: str) -> str:
        if name not in used_names:
            used_names.add(name)
            return name
        path = Path(name)
        stem = path.stem
        suffix = path.suffix
        index = 1
        while f"{stem}_{index}{suffix}" in used_names:
            index += 1
        unique_name = f"{stem}_{index}{suffix}"
        used_names.add(unique_name)
        return unique_name

    with ZipFile(buffer, "w", ZIP_DEFLATED) as archive:
        archive.writestr(
            "response.json",
            _json_bytes(artifacts.queue.run_queue.model_dump(mode="json", by_alias=True)),
        )
        archive.writestr(
            unique_name(Path(artifacts.queue.run_queue_path).name),
            _json_bytes(artifacts.queue.run_queue.model_dump(mode="json", by_alias=True)),
        )
        for item in artifacts.queue.items:
            archive.writestr(
                unique_name(Path(item.episode_setup_path).name),
                _json_bytes(
                    remove_json_nulls(
                        item.episode_setup.model_dump(mode="json", by_alias=True),
                        drop_empty_object_keys={"properties"},
                    )
                ),
            )
            archive.writestr(
                unique_name(Path(item.delivery_bot_setup_path).name),
                _json_bytes(remove_json_nulls(item.delivery_bot_setup.model_dump(mode="json", by_alias=True))),
            )

    payload = buffer.getvalue()
    with ZipFile(BytesIO(payload)) as archive:
        if not archive.namelist():
            raise RuntimeError("Scenario artifact zip is empty.")
    return payload


@router.post(
    "/api/v1/scenarios/generate-artifacts",
    response_class=StreamingResponse,
    responses={
        200: {
            "description": "Scenario generation response and generated JSON artifacts as a zip file.",
            "content": {
                "application/zip": {
                    "schema": {"type": "string", "format": "binary"},
                },
            },
        },
    },
)
def scenario_generate_artifacts_endpoint(
    request: ScenarioGenerateRequest,
) -> StreamingResponse:
    artifacts = generate_scenario_artifacts(request, write_export=False)
    zip_payload = _artifact_zip_bytes(artifacts)
    return StreamingResponse(
        BytesIO(zip_payload),
        media_type="application/zip",
        headers={
            "Content-Disposition": 'attachment; filename="scenario_artifacts.zip"',
            "Content-Length": str(len(zip_payload)),
        },
    )


def _drive_error_detail(error: GoogleDriveUploadError) -> dict[str, str]:
    detail = {"code": error.code, "message": error.message}
    if error.filename:
        detail["filename"] = error.filename
    return detail


@router.post(
    "/api/v1/scenarios/generate-drive",
    response_model=ScenarioDriveArtifactResponse,
)
def scenario_generate_drive_endpoint(
    request: ScenarioGenerateRequest,
) -> ScenarioDriveArtifactResponse:
    try:
        artifacts = generate_scenario_artifacts(request, write_export=False)
    except Exception as exc:
        raise HTTPException(
            status_code=500,
            detail={
                "code": "SCENARIO_GENERATION_FAILED",
                "message": "Scenario generation failed.",
            },
        ) from exc
    try:
        return upload_scenario_artifacts_to_drive(artifacts)
    except GoogleDriveUploadError as exc:
        raise HTTPException(status_code=500, detail=_drive_error_detail(exc)) from exc


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
