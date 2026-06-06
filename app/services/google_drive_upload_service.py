from __future__ import annotations

from dataclasses import dataclass
from io import BytesIO
import json
from pathlib import Path
from typing import Any

from app.core.settings import Settings
from app.models.scenario_generation import (
    ScenarioDriveArtifactBackup,
    ScenarioDriveArtifactBackupFile,
    ScenarioDriveArtifactFile,
    ScenarioDriveArtifactResponse,
)
from app.services.scenario_generation_service import ScenarioGenerationArtifacts
from app.utils.json_sanitizer import remove_json_nulls


DRIVE_JSON_MIMETYPE = "application/json"
DRIVE_SCOPES = ["https://www.googleapis.com/auth/drive"]


@dataclass
class GoogleDriveUploadError(Exception):
    code: str
    message: str
    filename: str | None = None


@dataclass(frozen=True)
class DriveJsonArtifact:
    kind: str
    filename: str
    payload: dict[str, Any]


def _safe_error_message(code: str, filename: str | None = None) -> str:
    if code == "GOOGLE_DRIVE_CREDENTIALS_NOT_FOUND":
        return "Google Drive service account credentials file was not found."
    if code == "GOOGLE_DRIVE_FOLDER_NOT_CONFIGURED":
        return "Google Drive folder id is not configured."
    if code == "GOOGLE_DRIVE_AUTH_FAILED":
        return "Google Drive authentication failed."
    if code == "GOOGLE_DRIVE_OAUTH_CLIENT_NOT_FOUND":
        return "Google Drive OAuth client file was not found."
    if code == "GOOGLE_DRIVE_OAUTH_FAILED":
        return "Google Drive OAuth authentication failed."
    if code == "GOOGLE_DRIVE_BACKUP_FOLDER_NOT_FOUND":
        return "Google Drive backup folder was not found."
    if code == "GOOGLE_DRIVE_BACKUP_FOLDER_AMBIGUOUS":
        return "Multiple Google Drive backup folders matched the configured name."
    if code == "GOOGLE_DRIVE_BACKUP_FOLDER_INVALID":
        return "Google Drive backup folder must differ from the target upload folder."
    if code == "GOOGLE_DRIVE_LIST_FAILED":
        return "Failed to list Google Drive folder items."
    if code == "GOOGLE_DRIVE_BACKUP_MOVE_FAILED":
        return "Failed to move existing Google Drive folder item to backup."
    if filename:
        return f"Failed to upload {filename} to Google Drive."
    return "Failed to upload scenario artifacts to Google Drive."


def _json_bytes(payload: dict[str, Any]) -> bytes:
    return (json.dumps(payload, ensure_ascii=False, indent=2) + "\n").encode("utf-8")


def _client_from_service_account(credentials_path: Path):
    try:
        from google.oauth2 import service_account
        from googleapiclient.discovery import build
    except Exception as exc:  # pragma: no cover - depends on optional import failure shape.
        raise GoogleDriveUploadError("GOOGLE_DRIVE_AUTH_FAILED", _safe_error_message("GOOGLE_DRIVE_AUTH_FAILED")) from exc

    try:
        credentials = service_account.Credentials.from_service_account_file(
            str(credentials_path),
            scopes=DRIVE_SCOPES,
        )
        return build("drive", "v3", credentials=credentials, cache_discovery=False)
    except Exception as exc:
        raise GoogleDriveUploadError("GOOGLE_DRIVE_AUTH_FAILED", _safe_error_message("GOOGLE_DRIVE_AUTH_FAILED")) from exc


def _client_from_oauth(client_file: Path, token_file: Path):
    try:
        from google.auth.transport.requests import Request
        from google.oauth2.credentials import Credentials
        from google_auth_oauthlib.flow import InstalledAppFlow
        from googleapiclient.discovery import build
    except Exception as exc:  # pragma: no cover - depends on optional import failure shape.
        raise GoogleDriveUploadError("GOOGLE_DRIVE_OAUTH_FAILED", _safe_error_message("GOOGLE_DRIVE_OAUTH_FAILED")) from exc

    try:
        credentials = None
        if token_file.exists():
            credentials = Credentials.from_authorized_user_file(str(token_file), DRIVE_SCOPES)
        if credentials is not None and credentials.expired and credentials.refresh_token:
            credentials.refresh(Request())
        if credentials is None or not credentials.valid:
            flow = InstalledAppFlow.from_client_secrets_file(str(client_file), DRIVE_SCOPES)
            credentials = flow.run_local_server(port=0)
        token_file.parent.mkdir(parents=True, exist_ok=True)
        token_file.write_text(credentials.to_json(), encoding="utf-8")
        return build("drive", "v3", credentials=credentials, cache_discovery=False)
    except GoogleDriveUploadError:
        raise
    except Exception as exc:
        raise GoogleDriveUploadError("GOOGLE_DRIVE_OAUTH_FAILED", _safe_error_message("GOOGLE_DRIVE_OAUTH_FAILED")) from exc


def build_google_drive_client(settings: Settings):
    auth_mode = settings.googleDriveAuthMode.strip().lower()
    if auth_mode == "oauth":
        client_file = Path(settings.googleDriveOauthClientFile)
        if not client_file.exists():
            raise GoogleDriveUploadError(
                "GOOGLE_DRIVE_OAUTH_CLIENT_NOT_FOUND",
                _safe_error_message("GOOGLE_DRIVE_OAUTH_CLIENT_NOT_FOUND"),
            )
        try:
            return _client_from_oauth(client_file, Path(settings.googleDriveOauthTokenFile))
        except GoogleDriveUploadError:
            raise
        except Exception as exc:
            raise GoogleDriveUploadError("GOOGLE_DRIVE_OAUTH_FAILED", _safe_error_message("GOOGLE_DRIVE_OAUTH_FAILED")) from exc

    credentials_path = Path(settings.googleDriveServiceAccountFile)
    if not credentials_path.exists():
        raise GoogleDriveUploadError(
            "GOOGLE_DRIVE_CREDENTIALS_NOT_FOUND",
            _safe_error_message("GOOGLE_DRIVE_CREDENTIALS_NOT_FOUND"),
        )
    return _client_from_service_account(credentials_path)


def _media_upload(payload: dict[str, Any]):
    from googleapiclient.http import MediaIoBaseUpload

    return MediaIoBaseUpload(BytesIO(_json_bytes(payload)), mimetype=DRIVE_JSON_MIMETYPE, resumable=False)


def _drive_url(metadata: dict[str, Any]) -> str:
    web_view_link = metadata.get("webViewLink")
    if isinstance(web_view_link, str) and web_view_link:
        return web_view_link
    file_id = str(metadata.get("id", ""))
    return f"https://drive.google.com/file/d/{file_id}/view"


def _drive_query_string(value: str) -> str:
    return value.replace("\\", "\\\\").replace("'", "\\'")


def _execute_files_list(client: Any, query: str) -> list[dict[str, Any]]:
    files: list[dict[str, Any]] = []
    page_token: str | None = None
    while True:
        try:
            response = (
                client.files()
                .list(
                    q=query,
                    fields="nextPageToken,files(id,name,mimeType,parents)",
                    supportsAllDrives=True,
                    includeItemsFromAllDrives=True,
                    pageSize=1000,
                    pageToken=page_token,
                )
                .execute()
            )
        except Exception as exc:
            raise GoogleDriveUploadError("GOOGLE_DRIVE_LIST_FAILED", _safe_error_message("GOOGLE_DRIVE_LIST_FAILED")) from exc
        if not isinstance(response, dict):
            return files
        files.extend(file for file in response.get("files", []) if isinstance(file, dict))
        next_page_token = response.get("nextPageToken")
        if not isinstance(next_page_token, str) or not next_page_token:
            return files
        page_token = next_page_token


def _find_backup_folder_id(client: Any, target_folder_id: str, backup_folder_name: str) -> str:
    query = (
        f"name = '{_drive_query_string(backup_folder_name)}' "
        "and mimeType = 'application/vnd.google-apps.folder' "
        "and trashed = false "
        f"and '{_drive_query_string(target_folder_id)}' in parents"
    )
    matches = _execute_files_list(client, query)
    if not matches:
        raise GoogleDriveUploadError(
            "GOOGLE_DRIVE_BACKUP_FOLDER_NOT_FOUND",
            _safe_error_message("GOOGLE_DRIVE_BACKUP_FOLDER_NOT_FOUND"),
        )
    if len(matches) > 1:
        raise GoogleDriveUploadError(
            "GOOGLE_DRIVE_BACKUP_FOLDER_AMBIGUOUS",
            _safe_error_message("GOOGLE_DRIVE_BACKUP_FOLDER_AMBIGUOUS"),
        )
    return str(matches[0].get("id", ""))


def _move_existing_children_to_backup(
    client: Any,
    *,
    target_folder_id: str,
    backup_folder_id: str,
    backup_folder_name: str,
) -> ScenarioDriveArtifactBackup:
    if backup_folder_id == target_folder_id:
        raise GoogleDriveUploadError(
            "GOOGLE_DRIVE_BACKUP_FOLDER_INVALID",
            _safe_error_message("GOOGLE_DRIVE_BACKUP_FOLDER_INVALID"),
        )
    query = f"'{_drive_query_string(target_folder_id)}' in parents and trashed = false"
    children = _execute_files_list(client, query)
    moved_files: list[ScenarioDriveArtifactBackupFile] = []
    for child in children:
        child_id = str(child.get("id", ""))
        if not child_id or child_id == backup_folder_id:
            continue
        filename = str(child.get("name", ""))
        try:
            (
                client.files()
                .update(
                    fileId=child_id,
                    addParents=backup_folder_id,
                    removeParents=target_folder_id,
                    fields="id,name",
                    supportsAllDrives=True,
                )
                .execute()
            )
        except Exception as exc:
            raise GoogleDriveUploadError(
                "GOOGLE_DRIVE_BACKUP_MOVE_FAILED",
                _safe_error_message("GOOGLE_DRIVE_BACKUP_MOVE_FAILED"),
                filename=filename,
            ) from exc
        moved_files.append(ScenarioDriveArtifactBackupFile(filename=filename, drive_file_id=child_id))
    return ScenarioDriveArtifactBackup(
        enabled=True,
        backup_folder_id=backup_folder_id,
        backup_folder_name=backup_folder_name,
        moved_count=len(moved_files),
        moved_files=moved_files,
    )


def _backup_existing_items_if_enabled(client: Any, settings: Settings, target_folder_id: str) -> ScenarioDriveArtifactBackup:
    backup_folder_name = settings.googleDriveBackupFolderName.strip() or "백업"
    if not settings.googleDriveBackupBeforeUpload:
        return ScenarioDriveArtifactBackup(
            enabled=False,
            backup_folder_id=None,
            backup_folder_name=backup_folder_name,
            moved_count=0,
            moved_files=[],
        )
    configured_backup_folder_id = settings.googleDriveBackupFolderId.strip()
    backup_folder_id = configured_backup_folder_id or _find_backup_folder_id(client, target_folder_id, backup_folder_name)
    return _move_existing_children_to_backup(
        client,
        target_folder_id=target_folder_id,
        backup_folder_id=backup_folder_id,
        backup_folder_name=backup_folder_name,
    )


def _upload_error_message(exc: Exception, filename: str) -> str:
    text = str(exc)
    if "Service Accounts do not have storage quota" in text:
        return f"Failed to upload {filename} to Google Drive. My Drive folders require OAuth mode (GOOGLE_DRIVE_AUTH_MODE=oauth)."
    return _safe_error_message("GOOGLE_DRIVE_UPLOAD_FAILED", filename)


def _artifact_payloads(artifacts: ScenarioGenerationArtifacts) -> list[DriveJsonArtifact]:
    queue = artifacts.queue
    payloads = [
        DriveJsonArtifact(
            kind="episode_run_queue",
            filename=Path(queue.run_queue_path).name,
            payload=queue.run_queue.model_dump(mode="json", by_alias=True),
        )
    ]
    for item in queue.items:
        payloads.append(
            DriveJsonArtifact(
                kind="episode_setup",
                filename=Path(item.episode_setup_path).name,
                payload=remove_json_nulls(
                    item.episode_setup.model_dump(mode="json", by_alias=True),
                    drop_empty_object_keys={"properties"},
                ),
            )
        )
        payloads.append(
            DriveJsonArtifact(
                kind="delivery_bot_setup",
                filename=Path(item.delivery_bot_setup_path).name,
                payload=remove_json_nulls(item.delivery_bot_setup.model_dump(mode="json", by_alias=True)),
            )
        )
    return payloads


def upload_scenario_artifacts_to_drive(
    artifacts: ScenarioGenerationArtifacts,
    *,
    settings: Settings | None = None,
    drive_client: Any | None = None,
) -> ScenarioDriveArtifactResponse:
    resolved_settings = settings or Settings()
    folder_id = resolved_settings.googleDriveFolderId.strip()
    if not folder_id:
        raise GoogleDriveUploadError(
            "GOOGLE_DRIVE_FOLDER_NOT_CONFIGURED",
            _safe_error_message("GOOGLE_DRIVE_FOLDER_NOT_CONFIGURED"),
        )

    client = drive_client or build_google_drive_client(resolved_settings)
    backup = _backup_existing_items_if_enabled(client, resolved_settings, folder_id)
    uploaded_files: list[ScenarioDriveArtifactFile] = []
    for artifact in _artifact_payloads(artifacts):
        try:
            metadata = (
                client.files()
                .create(
                    body={"name": artifact.filename, "parents": [folder_id], "mimeType": DRIVE_JSON_MIMETYPE},
                    media_body=_media_upload(artifact.payload),
                    fields="id,name,webViewLink",
                    supportsAllDrives=True,
                )
                .execute()
            )
        except GoogleDriveUploadError:
            raise
        except Exception as exc:
            raise GoogleDriveUploadError(
                "GOOGLE_DRIVE_UPLOAD_FAILED",
                _upload_error_message(exc, artifact.filename),
                filename=artifact.filename,
            ) from exc
        uploaded_files.append(
            ScenarioDriveArtifactFile(
                kind=artifact.kind,
                filename=artifact.filename,
                drive_file_id=str(metadata.get("id", "")),
                drive_url=_drive_url(metadata),
            )
        )

    run_queue_file = Path(artifacts.queue.run_queue_path).name
    if run_queue_file not in {file.filename for file in uploaded_files}:
        raise GoogleDriveUploadError(
            "GOOGLE_DRIVE_UPLOAD_FAILED",
            _safe_error_message("GOOGLE_DRIVE_UPLOAD_FAILED", run_queue_file),
            filename=run_queue_file,
        )
    return ScenarioDriveArtifactResponse(
        drive_folder_id=folder_id,
        run_queue_file=run_queue_file,
        backup=backup,
        files=uploaded_files,
    )
