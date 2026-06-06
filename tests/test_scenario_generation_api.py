from __future__ import annotations

import json
from types import SimpleNamespace
from zipfile import ZipFile

from fastapi.testclient import TestClient

import app.api.routes as routes
from app.core.settings import Settings
from app.main import app
from app.models.run_queue import EpisodeRunQueue, EpisodeRunQueueItem
from app.models.scenario_generation import ScenarioDriveArtifactBackup, ScenarioDriveArtifactFile, ScenarioDriveArtifactResponse
from app.services.google_drive_upload_service import GoogleDriveUploadError
from app.utils.json_sanitizer import contains_json_null


class _Dumpable:
    def __init__(self, payload: dict) -> None:
        self.payload = payload

    def model_dump(self, **kwargs):
        return self.payload


def _queue(run_count: int = 1) -> EpisodeRunQueue:
    return EpisodeRunQueue(
        runs=[
            EpisodeRunQueueItem(
                pair_id=f"obstacle_ahead_{index:03d}",
                episode_setup=f"Json/Input/EpisodeSetup_obstacle_ahead_{index:03d}.json",
                delivery_bot_setup=f"Json/Input/DeliveryBotSetup_obstacle_ahead_{index:03d}.json",
            )
            for index in range(run_count)
        ]
    )


def test_scenario_generation_route_accepts_prompt_only_and_returns_run_queue(monkeypatch) -> None:
    def stub_generate(request):
        assert request.episode_count is None
        return _queue(run_count=Settings().scenarioEpisodeDefaultCount)

    monkeypatch.setattr(routes, "generate_scenario_run_queue", stub_generate)

    response = TestClient(app).post("/api/v1/scenarios/generate", json={"prompt": "장애물이 경로를 막는 상황"})

    assert response.status_code == 200
    payload = response.json()
    assert set(payload) == {"schema", "version", "runs"}
    assert len(payload["runs"]) == Settings().scenarioEpisodeDefaultCount
    assert set(payload["runs"][0]) == {"pair_id", "episode_setup", "delivery_bot_setup"}


def test_scenario_generation_route_accepts_optional_episode_count(monkeypatch) -> None:
    observed_counts = []

    def stub_generate(request):
        observed_counts.append(request.episode_count)
        return _queue(run_count=request.episode_count)

    monkeypatch.setattr(routes, "generate_scenario_run_queue", stub_generate)
    client = TestClient(app)

    one_response = client.post("/api/v1/scenarios/generate", json={"prompt": "test", "episode_count": 1})
    three_response = client.post("/api/v1/scenarios/generate", json={"prompt": "test", "episode_count": 3})

    assert one_response.status_code == 200
    assert three_response.status_code == 200
    assert len(one_response.json()["runs"]) == 1
    assert len(three_response.json()["runs"]) == 3
    assert observed_counts == [1, 3]


def test_scenario_generation_route_accepts_episode_count_at_max(monkeypatch) -> None:
    max_count = Settings().scenarioEpisodeMaxCount

    def stub_generate(request):
        return _queue(run_count=request.episode_count)

    monkeypatch.setattr(routes, "generate_scenario_run_queue", stub_generate)

    response = TestClient(app).post("/api/v1/scenarios/generate", json={"prompt": "test", "episode_count": max_count})

    assert response.status_code == 200
    assert len(response.json()["runs"]) == max_count


def test_scenario_generation_route_rejects_extra_fields_and_empty_prompt() -> None:
    client = TestClient(app)

    extra_response = client.post("/api/v1/scenarios/generate", json={"prompt": "x", "episodeCount": 3})
    empty_response = client.post("/api/v1/scenarios/generate", json={"prompt": "   "})

    assert extra_response.status_code == 422
    assert empty_response.status_code == 422


def test_scenario_generation_route_rejects_invalid_episode_count_values() -> None:
    client = TestClient(app)

    max_count = Settings().scenarioEpisodeMaxCount
    for value in [0, -1, 1.5, "3", max_count + 1]:
        response = client.post("/api/v1/scenarios/generate", json={"prompt": "test", "episode_count": value})
        assert response.status_code == 422


def test_scenario_generation_openapi_marks_episode_count_optional() -> None:
    schema = TestClient(app).get("/openapi.json").json()
    request_ref = schema["paths"]["/api/v1/scenarios/generate"]["post"]["requestBody"]["content"]["application/json"]["schema"]["$ref"]
    component_name = request_ref.rsplit("/", 1)[-1]
    request_schema = schema["components"]["schemas"][component_name]

    assert request_schema["required"] == ["prompt"]
    assert "episode_count" in request_schema["properties"]
    assert "minimum" in request_schema["properties"]["episode_count"]["anyOf"][0]
    assert request_schema["properties"]["episode_count"]["anyOf"][0]["maximum"] == Settings().scenarioEpisodeMaxCount


def test_openapi_exposes_no_other_api_v1_routes() -> None:
    schema = TestClient(app).get("/openapi.json").json()

    api_v1_paths = sorted(path for path in schema["paths"] if path.startswith("/api/v1/"))

    assert api_v1_paths == [
        "/api/v1/scenarios/generate",
        "/api/v1/scenarios/generate-artifacts",
        "/api/v1/scenarios/generate-drive",
    ]


def test_openapi_documents_artifact_response_as_zip() -> None:
    schema = TestClient(app).get("/openapi.json").json()
    response_schema = schema["paths"]["/api/v1/scenarios/generate-artifacts"]["post"]["responses"]["200"]

    assert set(response_schema["content"]) == {"application/zip"}
    assert response_schema["content"]["application/zip"]["schema"] == {"type": "string", "format": "binary"}


def _artifact_result(tmp_path, run_count: int):
    items = []
    runs = []
    for index in range(run_count):
        episode_relative_path = f"Json/Input/EpisodeSetup_test_{index:03d}.json"
        bot_relative_path = f"Json/Input/DeliveryBotSetup_test_{index:03d}.json"
        items.append(
            SimpleNamespace(
                episode_setup=_Dumpable({"episode": index}),
                delivery_bot_setup=_Dumpable({"bot": index}),
                episode_setup_path=episode_relative_path,
                delivery_bot_setup_path=bot_relative_path,
            )
        )
        runs.append(
            EpisodeRunQueueItem(
                pair_id=f"artifact_pair_{index:03d}",
                episode_setup=episode_relative_path,
                delivery_bot_setup=bot_relative_path,
            )
        )
    run_queue = EpisodeRunQueue(runs=runs)
    return SimpleNamespace(
        queue=SimpleNamespace(
            run_queue=run_queue,
            run_queue_path="Json/Input/EpisodeRunQueue_test.json",
            items=items,
        ),
        export=SimpleNamespace(export_root=tmp_path, run_queue_path=None),
    )


def _artifact_result_with_null_artifacts(tmp_path):
    episode_relative_path = "Json/Input/EpisodeSetup_test_000.json"
    bot_relative_path = "Json/Input/DeliveryBotSetup_test_000.json"
    run_queue = EpisodeRunQueue(
        runs=[
            EpisodeRunQueueItem(
                pair_id="artifact_pair_000",
                episode_setup=episode_relative_path,
                delivery_bot_setup=bot_relative_path,
            )
        ]
    )
    return SimpleNamespace(
        queue=SimpleNamespace(
            run_queue=run_queue,
            run_queue_path="Json/Input/EpisodeRunQueue_test.json",
            items=[
                SimpleNamespace(
                    episode_setup=_Dumpable({"episode": 0, "properties": {"empty": None}}),
                    delivery_bot_setup=_Dumpable(
                        {
                            "schema": "delivery_bot_setup",
                            "version": 1,
                            "robot": {
                                "drive": {"max_speed_kmh": 10.0, "speed_limit_brake": None},
                                "path_follow": {"target_speed_kmh": 10.0, "draw_debug": None},
                                "lidar": {"scan_range_m": 5.0, "ignore_tags": None},
                            },
                        }
                    ),
                    episode_setup_path=episode_relative_path,
                    delivery_bot_setup_path=bot_relative_path,
                )
            ],
        ),
        export=SimpleNamespace(export_root=tmp_path, run_queue_path=None),
    )


def _zip_payload(response):
    from io import BytesIO

    with ZipFile(BytesIO(response.content)) as archive:
        return {name: json.loads(archive.read(name).decode("utf-8")) for name in archive.namelist()}


def test_scenario_generation_artifacts_returns_zip_for_one_episode(monkeypatch, tmp_path) -> None:
    def stub_generate_artifacts(request):
        assert request.episode_count == 1
        return _artifact_result(tmp_path, run_count=1)

    monkeypatch.setattr(routes, "generate_scenario_artifacts", stub_generate_artifacts)

    response = TestClient(app).post("/api/v1/scenarios/generate-artifacts", json={"prompt": "test", "episode_count": 1})

    assert response.status_code == 200
    assert response.headers["content-type"] == "application/zip"
    assert response.headers["content-disposition"] == 'attachment; filename="scenario_artifacts.zip"'
    assert int(response.headers["content-length"]) == len(response.content)
    payloads = _zip_payload(response)
    assert sorted(payloads) == [
        "DeliveryBotSetup_test_000.json",
        "EpisodeRunQueue_test.json",
        "EpisodeSetup_test_000.json",
        "response.json",
    ]
    assert set(payloads["response.json"]) == {"schema", "version", "runs"}
    assert len(payloads["response.json"]["runs"]) == 1


def test_scenario_generation_artifacts_returns_zip_for_three_episodes(monkeypatch, tmp_path) -> None:
    def stub_generate_artifacts(request):
        assert request.episode_count == 3
        return _artifact_result(tmp_path, run_count=3)

    monkeypatch.setattr(routes, "generate_scenario_artifacts", stub_generate_artifacts)

    response = TestClient(app).post("/api/v1/scenarios/generate-artifacts", json={"prompt": "test", "episode_count": 3})

    assert response.status_code == 200
    assert int(response.headers["content-length"]) == len(response.content)
    payloads = _zip_payload(response)
    assert len(payloads["response.json"]["runs"]) == 3
    assert sum(1 for name in payloads if name.startswith("EpisodeSetup")) == 3
    assert sum(1 for name in payloads if name.startswith("DeliveryBotSetup")) == 3
    assert sum(1 for name in payloads if name.startswith("EpisodeRunQueue")) == 1


def test_scenario_generation_artifacts_zip_payloads_are_null_free(monkeypatch, tmp_path) -> None:
    def stub_generate_artifacts(request):
        return _artifact_result_with_null_artifacts(tmp_path)

    monkeypatch.setattr(routes, "generate_scenario_artifacts", stub_generate_artifacts)

    response = TestClient(app).post("/api/v1/scenarios/generate-artifacts", json={"prompt": "test", "episode_count": 1})

    assert response.status_code == 200
    payloads = _zip_payload(response)
    assert contains_json_null(payloads["EpisodeSetup_test_000.json"]) is False
    assert contains_json_null(payloads["DeliveryBotSetup_test_000.json"]) is False
    assert "speed_limit_brake" not in payloads["DeliveryBotSetup_test_000.json"]["robot"]["drive"]
    assert "draw_debug" not in payloads["DeliveryBotSetup_test_000.json"]["robot"]["path_follow"]
    assert "ignore_tags" not in payloads["DeliveryBotSetup_test_000.json"]["robot"]["lidar"]


def _drive_response(run_queue_file: str, file_count: int) -> ScenarioDriveArtifactResponse:
    files = [
        ScenarioDriveArtifactFile(
            kind="episode_run_queue" if index == 0 else "episode_setup",
            filename=run_queue_file if index == 0 else f"EpisodeSetup_test_{index - 1:03d}.json",
            drive_file_id=f"drive-{index}",
            drive_url=f"https://drive.google.com/file/d/drive-{index}/view",
        )
        for index in range(file_count)
    ]
    return ScenarioDriveArtifactResponse(
        drive_folder_id="folder-123",
        run_queue_file=run_queue_file,
        backup=ScenarioDriveArtifactBackup(
            enabled=True,
            backup_folder_id="backup-123",
            backup_folder_name="백업",
            moved_count=1,
            moved_files=[{"filename": "old_file.json", "drive_file_id": "old-1"}],
        ),
        files=files,
    )


def test_scenario_generate_drive_reuses_request_and_returns_metadata(monkeypatch, tmp_path) -> None:
    observed_counts = []

    def stub_generate_artifacts(request):
        observed_counts.append(request.episode_count)
        return _artifact_result(tmp_path, run_count=1)

    def stub_upload(artifacts):
        assert artifacts.queue.run_queue_path == "Json/Input/EpisodeRunQueue_test.json"
        return _drive_response("EpisodeRunQueue_test.json", file_count=3)

    monkeypatch.setattr(routes, "generate_scenario_artifacts", stub_generate_artifacts)
    monkeypatch.setattr(routes, "upload_scenario_artifacts_to_drive", stub_upload)

    response = TestClient(app).post("/api/v1/scenarios/generate-drive", json={"prompt": "test", "episode_count": 1})

    assert response.status_code == 200
    payload = response.json()
    assert observed_counts == [1]
    assert payload["status"] == "success"
    assert payload["schema"] == "scenario_drive_artifact_response"
    assert payload["version"] == 1
    assert payload["drive_folder_id"] == "folder-123"
    assert payload["run_queue_file"] == "EpisodeRunQueue_test.json"
    assert payload["backup"] == {
        "enabled": True,
        "backup_folder_id": "backup-123",
        "backup_folder_name": "백업",
        "moved_count": 1,
        "moved_files": [{"filename": "old_file.json", "drive_file_id": "old-1"}],
    }
    assert payload["run_queue_file"] in [item["filename"] for item in payload["files"]]
    assert set(payload["files"][0]) == {"kind", "filename", "drive_file_id", "drive_url"}


def test_scenario_generate_drive_reports_upload_failure(monkeypatch, tmp_path) -> None:
    monkeypatch.setattr(routes, "generate_scenario_artifacts", lambda request: _artifact_result(tmp_path, run_count=1))

    def fail_upload(artifacts):
        raise GoogleDriveUploadError(
            code="GOOGLE_DRIVE_UPLOAD_FAILED",
            message="Failed to upload EpisodeSetup_test_000.json.",
            filename="EpisodeSetup_test_000.json",
        )

    monkeypatch.setattr(routes, "upload_scenario_artifacts_to_drive", fail_upload)

    response = TestClient(app).post("/api/v1/scenarios/generate-drive", json={"prompt": "test", "episode_count": 1})

    assert response.status_code == 500
    assert response.json()["detail"] == {
        "code": "GOOGLE_DRIVE_UPLOAD_FAILED",
        "message": "Failed to upload EpisodeSetup_test_000.json.",
        "filename": "EpisodeSetup_test_000.json",
    }


def test_scenario_generate_drive_reports_credentials_missing(monkeypatch, tmp_path) -> None:
    monkeypatch.setattr(routes, "generate_scenario_artifacts", lambda request: _artifact_result(tmp_path, run_count=1))

    def fail_upload(artifacts):
        raise GoogleDriveUploadError(
            code="GOOGLE_DRIVE_CREDENTIALS_NOT_FOUND",
            message="Google Drive service account credentials file was not found.",
        )

    monkeypatch.setattr(routes, "upload_scenario_artifacts_to_drive", fail_upload)

    response = TestClient(app).post("/api/v1/scenarios/generate-drive", json={"prompt": "test", "episode_count": 1})

    assert response.status_code == 500
    assert response.json()["detail"]["code"] == "GOOGLE_DRIVE_CREDENTIALS_NOT_FOUND"


def test_scenario_generate_drive_reports_generation_failure(monkeypatch) -> None:
    def fail_generation(request):
        raise RuntimeError("generation failed")

    monkeypatch.setattr(routes, "generate_scenario_artifacts", fail_generation)

    response = TestClient(app).post("/api/v1/scenarios/generate-drive", json={"prompt": "test", "episode_count": 1})

    assert response.status_code == 500
    assert response.json()["detail"] == {
        "code": "SCENARIO_GENERATION_FAILED",
        "message": "Scenario generation failed.",
    }
