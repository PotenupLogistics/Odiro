from __future__ import annotations

import json
from types import SimpleNamespace

import pytest

from app.core.settings import Settings
from app.models.run_queue import EpisodeRunQueue, EpisodeRunQueueItem
from app.services.google_drive_upload_service import (
    GoogleDriveUploadError,
    build_google_drive_client,
    upload_scenario_artifacts_to_drive,
)
from app.services.setup_pair_queue_generator import generate_setup_pair_queue


class _Dumpable:
    def __init__(self, payload: dict) -> None:
        self.payload = payload

    def model_dump(self, **kwargs):
        return self.payload


class _FakeDriveFiles:
    def __init__(self) -> None:
        self.uploads: list[dict] = []
        self.children: list[dict] = []
        self.lists: list[dict] = []
        self.updates: list[dict] = []
        self.fail_list = False
        self.fail_update_ids: set[str] = set()

    def list(self, *, q, fields, supportsAllDrives=False, includeItemsFromAllDrives=False, pageSize=None, pageToken=None):
        self.lists.append(
            {
                "q": q,
                "fields": fields,
                "supportsAllDrives": supportsAllDrives,
                "includeItemsFromAllDrives": includeItemsFromAllDrives,
                "pageSize": pageSize,
                "pageToken": pageToken,
            }
        )

        class _Request:
            def execute(request_self):
                if self.fail_list:
                    raise RuntimeError("list failed private_key should not leak")
                if "mimeType = 'application/vnd.google-apps.folder'" in q:
                    name = q.split("name = '", 1)[1].split("'", 1)[0]
                    files = [
                        item
                        for item in self.children
                        if item.get("name") == name
                        and item.get("mimeType") == "application/vnd.google-apps.folder"
                        and "folder-123" in item.get("parents", [])
                        and not item.get("trashed", False)
                    ]
                    return {"files": files}
                return {
                    "files": [
                        item
                        for item in self.children
                        if "folder-123" in item.get("parents", []) and not item.get("trashed", False)
                    ]
                }

        return _Request()

    def update(self, *, fileId, addParents, removeParents, fields, supportsAllDrives=False):
        self.updates.append(
            {
                "fileId": fileId,
                "addParents": addParents,
                "removeParents": removeParents,
                "fields": fields,
                "supportsAllDrives": supportsAllDrives,
            }
        )

        class _Request:
            def execute(request_self):
                if fileId in self.fail_update_ids:
                    raise RuntimeError("move failed access token should not leak")
                for item in self.children:
                    if item.get("id") == fileId:
                        parents = [parent for parent in item.get("parents", []) if parent != removeParents]
                        if addParents not in parents:
                            parents.append(addParents)
                        item["parents"] = parents
                        return {"id": fileId, "name": item.get("name", "")}
                return {"id": fileId}

        return _Request()

    def create(self, *, body, media_body, fields, supportsAllDrives=False):
        media_body._fd.seek(0)
        payload = media_body._fd.read().decode("utf-8")
        filename = body["name"]
        self.uploads.append(
            {
                "body": body,
                "payload": payload,
                "mimetype": media_body.mimetype(),
                "fields": fields,
                "supportsAllDrives": supportsAllDrives,
            }
        )

        class _Request:
            def execute(self_inner):
                return {
                    "id": f"id-{filename}",
                    "name": filename,
                    "webViewLink": f"https://drive.google.com/file/d/id-{filename}/view",
                }

        return _Request()


class _FakeDriveClient:
    def __init__(self) -> None:
        self.files_resource = _FakeDriveFiles()

    def files(self):
        return self.files_resource


def _artifact_result(run_count: int):
    items = []
    runs = []
    for index in range(run_count):
        episode_relative_path = f"Json/Input/EpisodeSetup_test_{index:03d}.json"
        bot_relative_path = f"Json/Input/DeliveryBotSetup_test_{index:03d}_baseline.json"
        items.append(
            SimpleNamespace(
                episode_setup=_Dumpable({"episode": index, "label": "한글"}),
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
    )


def _real_artifact_result(run_count: int):
    queue = generate_setup_pair_queue(
        {
            "scenarioId": "obstacle_ahead",
            "seed": 1001,
            "map": {"type": "Sidewalk", "lengthCm": 800, "sidewalkWidthCm": 120},
            "robot": {
                "botId": "robot_01",
                "spawn": {"x": 0, "y": 0, "z": 0},
                "goal": {"x": 800, "y": 0, "z": 0},
            },
            "obstacles": [
                {
                    "objectId": "obstacle_01",
                    "type": "Obstacle",
                    "position": {"x": 400, "y": 0, "z": 0},
                    "blockingRatio": 0.6,
                }
            ],
            "pedestrians": [],
            "runtime": {"maxDurationSec": 60},
        },
        episode_count=run_count,
        request_id="REQ-ROBOT-PROFILE",
    )
    return SimpleNamespace(queue=queue)


def _settings(tmp_path, folder_id: str = "folder-123") -> Settings:
    credentials = tmp_path / "credentials.json"
    credentials.write_text("{}", encoding="utf-8")
    return Settings(
        googleDriveAuthMode="service_account",
        googleDriveFolderId=folder_id,
        googleDriveServiceAccountFile=str(credentials),
        googleDriveBackupBeforeUpload=False,
    )


def _backup_settings(
    tmp_path,
    *,
    folder_id: str = "folder-123",
    backup_folder_id: str = "",
    backup_folder_name: str = "백업",
) -> Settings:
    credentials = tmp_path / "credentials.json"
    credentials.write_text("{}", encoding="utf-8")
    return Settings(
        googleDriveAuthMode="service_account",
        googleDriveFolderId=folder_id,
        googleDriveServiceAccountFile=str(credentials),
        googleDriveBackupBeforeUpload=True,
        googleDriveBackupFolderId=backup_folder_id,
        googleDriveBackupFolderName=backup_folder_name,
    )


def _oauth_settings(tmp_path, folder_id: str = "folder-123") -> Settings:
    client_file = tmp_path / "oauth_client.json"
    token_file = tmp_path / "google_drive_token.json"
    client_file.write_text("{}", encoding="utf-8")
    return Settings(
        googleDriveAuthMode="oauth",
        googleDriveFolderId=folder_id,
        googleDriveOauthClientFile=str(client_file),
        googleDriveOauthTokenFile=str(token_file),
        googleDriveBackupBeforeUpload=False,
    )


def test_upload_scenario_artifacts_uploads_one_run_queue_and_one_pair(tmp_path) -> None:
    client = _FakeDriveClient()

    response = upload_scenario_artifacts_to_drive(
        _artifact_result(run_count=1),
        settings=_settings(tmp_path),
        drive_client=client,
    )

    assert response.drive_folder_id == "folder-123"
    assert response.run_queue_file == "EpisodeRunQueue_test.json"
    assert len(response.files) == 3
    assert [file.kind for file in response.files] == [
        "episode_run_queue",
        "episode_setup",
        "delivery_bot_setup",
    ]
    assert response.run_queue_file in [file.filename for file in response.files]
    assert len(client.files_resource.uploads) == 3
    assert all(upload["body"]["parents"] == ["folder-123"] for upload in client.files_resource.uploads)
    assert all(upload["mimetype"] == "application/json" for upload in client.files_resource.uploads)
    assert all(upload["supportsAllDrives"] is True for upload in client.files_resource.uploads)
    assert '"label": "한글"' in client.files_resource.uploads[1]["payload"]


def test_upload_scenario_artifacts_uploads_episode_setup_robot_profile(tmp_path) -> None:
    client = _FakeDriveClient()

    response = upload_scenario_artifacts_to_drive(
        _real_artifact_result(run_count=1),
        settings=_settings(tmp_path),
        drive_client=client,
    )

    episode_upload = next(upload for upload in client.files_resource.uploads if upload["body"]["name"].startswith("EpisodeSetup_"))
    episode_payload = json.loads(episode_upload["payload"])
    assert response.run_queue_file == "EpisodeRunQueue_obstacle_ahead.json"
    assert episode_payload["robot_profile"]["width_m"] == 0.44
    assert episode_payload["robot_profile"]["depth_m"] == 1.0
    assert episode_payload["robot_profile"]["height_m"] == 0.64
    assert episode_payload["robot_profile"]["min_passable_width_m"] == 0.84
    assert "null" not in episode_upload["payload"]


def test_upload_scenario_artifacts_uploads_three_pairs(tmp_path) -> None:
    client = _FakeDriveClient()

    response = upload_scenario_artifacts_to_drive(
        _artifact_result(run_count=3),
        settings=_settings(tmp_path),
        drive_client=client,
    )

    assert len(response.files) == 7
    assert sum(1 for file in response.files if file.kind == "episode_run_queue") == 1
    assert sum(1 for file in response.files if file.kind == "episode_setup") == 3
    assert sum(1 for file in response.files if file.kind == "delivery_bot_setup") == 3


def test_upload_scenario_artifacts_moves_existing_children_to_backup_before_upload(tmp_path) -> None:
    client = _FakeDriveClient()
    client.files_resource.children = [
        {"id": "backup-id", "name": "백업", "mimeType": "application/vnd.google-apps.folder", "parents": ["folder-123"]},
        {"id": "old-file", "name": "old_file.json", "mimeType": "application/json", "parents": ["folder-123"]},
        {"id": "old-folder", "name": "old_folder", "mimeType": "application/vnd.google-apps.folder", "parents": ["folder-123"]},
        {"id": "nested-old", "name": "nested.json", "mimeType": "application/json", "parents": ["backup-id"]},
    ]

    response = upload_scenario_artifacts_to_drive(
        _artifact_result(run_count=1),
        settings=_backup_settings(tmp_path),
        drive_client=client,
    )

    assert response.backup is not None
    assert response.backup.enabled is True
    assert response.backup.backup_folder_id == "backup-id"
    assert response.backup.backup_folder_name == "백업"
    assert response.backup.moved_count == 2
    assert [(file.filename, file.drive_file_id) for file in response.backup.moved_files] == [
        ("old_file.json", "old-file"),
        ("old_folder", "old-folder"),
    ]
    assert [update["fileId"] for update in client.files_resource.updates] == ["old-file", "old-folder"]
    assert all(update["addParents"] == "backup-id" for update in client.files_resource.updates)
    assert all(update["removeParents"] == "folder-123" for update in client.files_resource.updates)
    assert len(client.files_resource.uploads) == 3
    assert client.files_resource.children[0]["parents"] == ["folder-123"]
    assert client.files_resource.children[3]["parents"] == ["backup-id"]


def test_upload_scenario_artifacts_prefers_backup_folder_id_without_name_lookup(tmp_path) -> None:
    client = _FakeDriveClient()
    client.files_resource.children = [
        {"id": "old-file", "name": "old_file.json", "mimeType": "application/json", "parents": ["folder-123"]},
    ]

    response = upload_scenario_artifacts_to_drive(
        _artifact_result(run_count=1),
        settings=_backup_settings(tmp_path, backup_folder_id="configured-backup"),
        drive_client=client,
    )

    assert response.backup is not None
    assert response.backup.backup_folder_id == "configured-backup"
    assert [entry["q"] for entry in client.files_resource.lists] == ["'folder-123' in parents and trashed = false"]
    assert client.files_resource.updates[0]["addParents"] == "configured-backup"


def test_upload_scenario_artifacts_reports_missing_backup_folder(tmp_path) -> None:
    client = _FakeDriveClient()

    with pytest.raises(GoogleDriveUploadError) as exc_info:
        upload_scenario_artifacts_to_drive(
            _artifact_result(run_count=1),
            settings=_backup_settings(tmp_path),
            drive_client=client,
        )

    assert exc_info.value.code == "GOOGLE_DRIVE_BACKUP_FOLDER_NOT_FOUND"
    assert client.files_resource.uploads == []


def test_upload_scenario_artifacts_reports_ambiguous_backup_folder(tmp_path) -> None:
    client = _FakeDriveClient()
    client.files_resource.children = [
        {"id": "backup-1", "name": "백업", "mimeType": "application/vnd.google-apps.folder", "parents": ["folder-123"]},
        {"id": "backup-2", "name": "백업", "mimeType": "application/vnd.google-apps.folder", "parents": ["folder-123"]},
    ]

    with pytest.raises(GoogleDriveUploadError) as exc_info:
        upload_scenario_artifacts_to_drive(
            _artifact_result(run_count=1),
            settings=_backup_settings(tmp_path),
            drive_client=client,
        )

    assert exc_info.value.code == "GOOGLE_DRIVE_BACKUP_FOLDER_AMBIGUOUS"
    assert client.files_resource.uploads == []


def test_upload_scenario_artifacts_rejects_target_folder_as_backup_folder(tmp_path) -> None:
    client = _FakeDriveClient()

    with pytest.raises(GoogleDriveUploadError) as exc_info:
        upload_scenario_artifacts_to_drive(
            _artifact_result(run_count=1),
            settings=_backup_settings(tmp_path, backup_folder_id="folder-123"),
            drive_client=client,
        )

    assert exc_info.value.code == "GOOGLE_DRIVE_BACKUP_FOLDER_INVALID"
    assert client.files_resource.uploads == []


def test_upload_scenario_artifacts_stops_upload_when_backup_move_fails(tmp_path) -> None:
    client = _FakeDriveClient()
    client.files_resource.children = [
        {"id": "backup-id", "name": "백업", "mimeType": "application/vnd.google-apps.folder", "parents": ["folder-123"]},
        {"id": "old-file", "name": "old_file.json", "mimeType": "application/json", "parents": ["folder-123"]},
    ]
    client.files_resource.fail_update_ids = {"old-file"}

    with pytest.raises(GoogleDriveUploadError) as exc_info:
        upload_scenario_artifacts_to_drive(
            _artifact_result(run_count=1),
            settings=_backup_settings(tmp_path),
            drive_client=client,
        )

    assert exc_info.value.code == "GOOGLE_DRIVE_BACKUP_MOVE_FAILED"
    assert client.files_resource.uploads == []


def test_upload_scenario_artifacts_reports_list_failure_before_upload(tmp_path) -> None:
    client = _FakeDriveClient()
    client.files_resource.fail_list = True

    with pytest.raises(GoogleDriveUploadError) as exc_info:
        upload_scenario_artifacts_to_drive(
            _artifact_result(run_count=1),
            settings=_backup_settings(tmp_path),
            drive_client=client,
        )

    assert exc_info.value.code == "GOOGLE_DRIVE_LIST_FAILED"
    assert client.files_resource.uploads == []


def test_upload_scenario_artifacts_does_not_move_existing_children_when_backup_disabled(tmp_path) -> None:
    client = _FakeDriveClient()
    client.files_resource.children = [
        {"id": "old-file", "name": "old_file.json", "mimeType": "application/json", "parents": ["folder-123"]},
    ]

    response = upload_scenario_artifacts_to_drive(
        _artifact_result(run_count=1),
        settings=_settings(tmp_path),
        drive_client=client,
    )

    assert response.backup is not None
    assert response.backup.enabled is False
    assert client.files_resource.lists == []
    assert client.files_resource.updates == []
    assert len(client.files_resource.uploads) == 3


def test_upload_scenario_artifacts_supports_oauth_mode_with_fake_client(tmp_path) -> None:
    client = _FakeDriveClient()

    response = upload_scenario_artifacts_to_drive(
        _artifact_result(run_count=1),
        settings=_oauth_settings(tmp_path),
        drive_client=client,
    )

    assert response.drive_folder_id == "folder-123"
    assert len(response.files) == 3


def test_build_google_drive_client_uses_service_account_mode(monkeypatch, tmp_path) -> None:
    observed = {}

    def fake_service_account(path):
        observed["path"] = path
        return "service-account-client"

    monkeypatch.setattr("app.services.google_drive_upload_service._client_from_service_account", fake_service_account)

    client = build_google_drive_client(_settings(tmp_path))

    assert client == "service-account-client"
    assert observed["path"].name == "credentials.json"


def test_build_google_drive_client_uses_oauth_mode(monkeypatch, tmp_path) -> None:
    observed = {}

    def fake_oauth(client_file, token_file):
        observed["client_file"] = client_file
        observed["token_file"] = token_file
        return "oauth-client"

    monkeypatch.setattr("app.services.google_drive_upload_service._client_from_oauth", fake_oauth)

    client = build_google_drive_client(_oauth_settings(tmp_path))

    assert client == "oauth-client"
    assert observed["client_file"].name == "oauth_client.json"
    assert observed["token_file"].name == "google_drive_token.json"


def test_upload_scenario_artifacts_requires_folder_id(tmp_path) -> None:
    with pytest.raises(GoogleDriveUploadError) as exc_info:
        upload_scenario_artifacts_to_drive(
            _artifact_result(run_count=1),
            settings=_settings(tmp_path, folder_id=""),
            drive_client=_FakeDriveClient(),
        )

    assert exc_info.value.code == "GOOGLE_DRIVE_FOLDER_NOT_CONFIGURED"


def test_build_google_drive_client_reports_missing_service_account_credentials(tmp_path) -> None:
    with pytest.raises(GoogleDriveUploadError) as exc_info:
        build_google_drive_client(
            Settings(
                googleDriveAuthMode="service_account",
                googleDriveFolderId="folder-123",
                googleDriveServiceAccountFile=str(tmp_path / "missing.json"),
            )
        )

    assert exc_info.value.code == "GOOGLE_DRIVE_CREDENTIALS_NOT_FOUND"


def test_build_google_drive_client_reports_missing_oauth_client_file(tmp_path) -> None:
    with pytest.raises(GoogleDriveUploadError) as exc_info:
        build_google_drive_client(
            Settings(
                googleDriveAuthMode="oauth",
                googleDriveFolderId="folder-123",
                googleDriveOauthClientFile=str(tmp_path / "missing_oauth_client.json"),
                googleDriveOauthTokenFile=str(tmp_path / "token.json"),
            )
        )

    assert exc_info.value.code == "GOOGLE_DRIVE_OAUTH_CLIENT_NOT_FOUND"


def test_build_google_drive_client_wraps_oauth_failure(monkeypatch, tmp_path) -> None:
    def fail_oauth(client_file, token_file):
        raise RuntimeError("client_secret should not leak")

    monkeypatch.setattr("app.services.google_drive_upload_service._client_from_oauth", fail_oauth)

    with pytest.raises(GoogleDriveUploadError) as exc_info:
        build_google_drive_client(_oauth_settings(tmp_path))

    assert exc_info.value.code == "GOOGLE_DRIVE_OAUTH_FAILED"
    assert "client_secret" not in exc_info.value.message


def test_build_google_drive_client_reports_service_account_auth_failure(monkeypatch, tmp_path) -> None:
    def fail_service_account(path):
        raise GoogleDriveUploadError("GOOGLE_DRIVE_AUTH_FAILED", "Google Drive authentication failed.")

    monkeypatch.setattr("app.services.google_drive_upload_service._client_from_service_account", fail_service_account)

    with pytest.raises(GoogleDriveUploadError) as exc_info:
        build_google_drive_client(_settings(tmp_path))

    assert exc_info.value.code == "GOOGLE_DRIVE_AUTH_FAILED"


def test_upload_scenario_artifacts_wraps_upload_failure(tmp_path) -> None:
    class _FailingFiles(_FakeDriveFiles):
        def create(self, *, body, media_body, fields):
            raise RuntimeError("boom private_key should not leak")

    class _FailingClient(_FakeDriveClient):
        def __init__(self) -> None:
            self.files_resource = _FailingFiles()

    with pytest.raises(GoogleDriveUploadError) as exc_info:
        upload_scenario_artifacts_to_drive(
            _artifact_result(run_count=1),
            settings=_settings(tmp_path),
            drive_client=_FailingClient(),
        )

    assert exc_info.value.code == "GOOGLE_DRIVE_UPLOAD_FAILED"
    assert exc_info.value.filename == "EpisodeRunQueue_test.json"
    assert "private_key" not in exc_info.value.message


def test_upload_failure_mentions_oauth_for_service_account_quota_error(tmp_path) -> None:
    class _QuotaFiles(_FakeDriveFiles):
        def create(self, *, body, media_body, fields, supportsAllDrives=False):
            raise RuntimeError("Service Accounts do not have storage quota")

    class _QuotaClient(_FakeDriveClient):
        def __init__(self) -> None:
            self.files_resource = _QuotaFiles()

    with pytest.raises(GoogleDriveUploadError) as exc_info:
        upload_scenario_artifacts_to_drive(
            _artifact_result(run_count=1),
            settings=_settings(tmp_path),
            drive_client=_QuotaClient(),
        )

    assert exc_info.value.code == "GOOGLE_DRIVE_UPLOAD_FAILED"
    assert "OAuth mode" in exc_info.value.message
