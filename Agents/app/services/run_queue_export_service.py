from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
import shutil

from app.services.setup_pair_queue_generator import SetupPairQueueResult
from app.utils.report_serialization import write_json_report
from app.utils.run_queue_summary import summarize_run_queue_result
from app.utils.json_sanitizer import contains_json_null, remove_json_nulls


@dataclass(frozen=True)
class RunQueueExportResult:
    exported: bool
    export_root: Path
    run_queue_path: Path | None
    summary_path: Path | None
    validation_summary_path: Path | None
    trace_summary_path: Path | None
    backup_path: Path | None = None


def _default_export_root(queue: SetupPairQueueResult) -> Path:
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    safe_request_id = "".join(ch if ch.isalnum() or ch in {"-", "_"} else "_" for ch in queue.request_id)
    return Path(queue.export_base_dir) / f"{stamp}_{safe_request_id}"


def _backup_existing_export(export_root: Path) -> Path | None:
    input_dir = export_root / "Json" / "Input"
    if not input_dir.exists() or not list(input_dir.glob("*.json")):
        return None
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    backup_root = export_root.parent / "_backup"
    backup_root.mkdir(parents=True, exist_ok=True)
    backup_path = backup_root / f"{export_root.name}_{stamp}"
    moved_file_count = sum(1 for path in export_root.rglob("*") if path.is_file())
    shutil.move(str(export_root), str(backup_path))
    write_json_report(
        backup_path / "backup_summary.json",
        {
            "originalPath": str(export_root),
            "backupPath": str(backup_path),
            "movedFileCount": moved_file_count,
            "createdAt": datetime.now(timezone.utc).isoformat(),
        },
    )
    return backup_path


def export_run_queue_package(
    queue: SetupPairQueueResult,
    output_dir: str | Path | None = None,
) -> RunQueueExportResult:
    export_root = Path(output_dir) if output_dir is not None else _default_export_root(queue)
    backup_path = _backup_existing_export(export_root)
    if not queue.run_queue_validation.valid or any(not item.episode_setup_validation.valid or not item.delivery_bot_setup_validation.valid for item in queue.items):
        export_root.mkdir(parents=True, exist_ok=True)
        failed_path = export_root / "failed_summary.json"
        write_json_report(failed_path, summarize_run_queue_result(queue, export_path=None))
        return RunQueueExportResult(False, export_root, None, failed_path, None, None, backup_path)

    input_dir = export_root / "Json" / "Input"
    input_dir.mkdir(parents=True, exist_ok=True)
    for item in queue.items:
        episode_payload = remove_json_nulls(
            item.episode_setup.model_dump(mode="json", by_alias=True),
            drop_empty_object_keys={"properties"},
        )
        bot_payload = remove_json_nulls(item.delivery_bot_setup.model_dump(mode="json", by_alias=True))
        if contains_json_null(episode_payload) or contains_json_null(bot_payload):
            failed_path = export_root / "failed_summary.json"
            write_json_report(failed_path, {"error": "null_value_detected_before_export"})
            return RunQueueExportResult(False, export_root, None, failed_path, None, None, backup_path)
        write_json_report(input_dir / Path(item.episode_setup_path).name, episode_payload)
        write_json_report(input_dir / Path(item.delivery_bot_setup_path).name, bot_payload)
    run_queue_path = input_dir / Path(queue.run_queue_path).name
    write_json_report(run_queue_path, queue.run_queue.model_dump(mode="json", by_alias=True))

    summary_path = export_root / "export_summary.json"
    validation_path = export_root / "validation_summary.json"
    trace_path = export_root / "trace_summary.json"
    write_json_report(summary_path, summarize_run_queue_result(queue, export_path=str(export_root)))
    write_json_report(
        validation_path,
        {
            "runQueueValidationPassed": queue.run_queue_validation.valid,
            "episodeSetupValidation": [item.episode_setup_validation.model_dump(mode="json") for item in queue.items],
            "deliveryBotSetupValidation": [item.delivery_bot_setup_validation.model_dump(mode="json") for item in queue.items],
        },
    )
    write_json_report(
        trace_path,
        {
            "scenarioId": queue.scenario_id,
            "changedParameters": [
                {"pairId": item.pair_id, "changedParameters": item.variant.changed_parameters}
                for item in queue.items
            ],
        },
    )
    return RunQueueExportResult(True, export_root, run_queue_path, summary_path, validation_path, trace_path, backup_path)
