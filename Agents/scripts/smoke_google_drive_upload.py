from __future__ import annotations

from datetime import datetime, timezone
from io import BytesIO
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from app.core.settings import Settings  # noqa: E402
from app.services.google_drive_upload_service import DRIVE_JSON_MIMETYPE, GoogleDriveUploadError, build_google_drive_client  # noqa: E402


def _json_bytes(payload: dict) -> bytes:
    return (json.dumps(payload, ensure_ascii=False, indent=2) + "\n").encode("utf-8")


def main() -> int:
    settings = Settings()
    folder_id = settings.googleDriveFolderId.strip()
    if not folder_id:
        print("GOOGLE_DRIVE_FOLDER_NOT_CONFIGURED")
        return 1

    try:
        from googleapiclient.http import MediaIoBaseUpload

        client = build_google_drive_client(settings)
        timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        filename = f"drive_upload_smoke_test_{timestamp}.json"
        payload = {
            "schema": "proto_ai_google_drive_smoke",
            "version": 1,
            "message": "manual smoke upload",
        }
        metadata = (
            client.files()
            .create(
                body={"name": filename, "parents": [folder_id], "mimeType": DRIVE_JSON_MIMETYPE},
                media_body=MediaIoBaseUpload(
                    BytesIO(_json_bytes(payload)),
                    mimetype=DRIVE_JSON_MIMETYPE,
                    resumable=False,
                ),
                fields="id,name,webViewLink",
                supportsAllDrives=True,
            )
            .execute()
        )
    except GoogleDriveUploadError as exc:
        print(json.dumps({"code": exc.code, "message": exc.message}, ensure_ascii=False))
        return 1
    except Exception as exc:
        status = getattr(exc, "status_code", None)
        reason = getattr(exc, "reason", None)
        if status is not None or reason is not None:
            print(json.dumps({"code": "GOOGLE_DRIVE_UPLOAD_FAILED", "status": status, "reason": reason}, ensure_ascii=False))
            return 1
        print(json.dumps({"code": "GOOGLE_DRIVE_UPLOAD_FAILED", "message": "Google Drive upload failed."}, ensure_ascii=False))
        return 1

    print(json.dumps({"filename": metadata.get("name"), "drive_file_id": metadata.get("id"), "drive_url": metadata.get("webViewLink")}, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
