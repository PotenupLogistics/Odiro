from __future__ import annotations

from io import BytesIO
import json
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

from fastapi import APIRouter, HTTPException
from fastapi.responses import StreamingResponse

from app.models.run_queue import EpisodeRunQueue
from app.models.scenario_generation import ScenarioDriveArtifactResponse, ScenarioGenerateRequest
from app.services.google_drive_upload_service import GoogleDriveUploadError, upload_scenario_artifacts_to_drive
from app.services.scenario_generation_service import ScenarioGenerationArtifacts, generate_scenario_artifacts, generate_scenario_run_queue
from app.utils.json_sanitizer import remove_json_nulls


router = APIRouter()


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
    return generate_scenario_run_queue(request)


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
    artifacts = generate_scenario_artifacts(request)
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
        artifacts = generate_scenario_artifacts(request)
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
