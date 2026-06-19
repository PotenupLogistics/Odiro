# DeliveryBot LiDAR Point Cloud capture folder validation CLI.
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any


# Metadata file name written beside the accumulated point cloud files.
SUMMARY_FILE_NAME = "capture_summary.json"

# Point Cloud manifest file name written for import/review guidance.
MANIFEST_FILE_NAME = "manifest.json"

# Main Unreal LiDAR Point Cloud plugin review file.
MAP_ACCUMULATED_FILE_NAME = "map_accumulated.xyz"

# Raw Unreal world-coordinate validation file.
WORLD_ACCUMULATED_FILE_NAME = "world_accumulated.xyz"

# Per-sensor-frame debug point cloud directory name.
FRAME_DIRECTORY_NAME = "frames"

# Per-sensor-frame index file name.
FRAME_INDEX_FILE_NAME = "frames.jsonl"


# CLI input path to the actual captures/lidar_point_cloud directory.
def _resolve_lidar_capture_directory(input_path: Path) -> Path:
    nested_capture_path = input_path / "captures" / "lidar_point_cloud"
    if nested_capture_path.exists():
        return nested_capture_path

    return input_path


# Read a JSON file and return None when it cannot be parsed.
def _load_json_file(file_path: Path, errors: list[str]) -> dict[str, Any] | None:
    try:
        data = json.loads(file_path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        errors.append(f"missing file: {file_path.name}")
        return None
    except json.JSONDecodeError as error:
        errors.append(f"invalid json: {file_path.name}: {error}")
        return None
    except OSError as error:
        errors.append(f"cannot read file: {file_path.name}: {error}")
        return None

    if not isinstance(data, dict):
        errors.append(f"json root is not object: {file_path.name}")
        return None

    return data


# Count non-empty point rows in an xyz/xyzrgb ASCII file.
def _count_xyz_points(file_path: Path, errors: list[str]) -> int:
    try:
        with file_path.open("r", encoding="utf-8") as xyz_file:
            return sum(1 for line in xyz_file if line.strip())
    except FileNotFoundError:
        errors.append(f"missing file: {file_path.name}")
    except OSError as error:
        errors.append(f"cannot read file: {file_path.name}: {error}")

    return 0


# Return sorted per-frame xyz files from the debug frame directory.
def _list_frame_files(frame_directory: Path, errors: list[str]) -> list[Path]:
    if not frame_directory.exists():
        errors.append(f"missing directory: {FRAME_DIRECTORY_NAME}")
        return []

    if not frame_directory.is_dir():
        errors.append(f"not a directory: {FRAME_DIRECTORY_NAME}")
        return []

    return sorted(frame_directory.glob("frame_*.xyz"))


# Read frames.jsonl records and skip invalid lines with warnings.
def _load_frame_index_records(index_path: Path, warnings: list[str]) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []

    if not index_path.exists():
        warnings.append(f"missing optional index: {FRAME_INDEX_FILE_NAME}")
        return records

    try:
        with index_path.open("r", encoding="utf-8") as index_file:
            for line_number, line in enumerate(index_file, start=1):
                stripped_line = line.strip()
                if not stripped_line:
                    continue

                try:
                    record = json.loads(stripped_line)
                except json.JSONDecodeError:
                    warnings.append(f"invalid jsonl line: {FRAME_INDEX_FILE_NAME}:{line_number}")
                    continue

                if isinstance(record, dict):
                    records.append(record)
                else:
                    warnings.append(f"jsonl line is not object: {FRAME_INDEX_FILE_NAME}:{line_number}")
    except OSError as error:
        warnings.append(f"cannot read optional index: {FRAME_INDEX_FILE_NAME}: {error}")

    return records


# Convert a metadata value to int while preserving validation flow.
def _read_int_value(data: dict[str, Any], key: str, default_value: int, warnings: list[str]) -> int:
    try:
        return int(data.get(key, default_value))
    except (TypeError, ValueError):
        warnings.append(f"invalid integer field: {key}")
        return default_value


# Validate manifest values that define the official review files.
def _validate_manifest(
    manifest: dict[str, Any] | None,
    expected_profile: str | None,
    errors: list[str],
    warnings: list[str],
) -> str:
    if manifest is None:
        return ""

    profile = str(manifest.get("profile", ""))
    if expected_profile and profile != expected_profile:
        errors.append(f"profile mismatch: expected={expected_profile}, actual={profile}")

    if manifest.get("summaryFile") != SUMMARY_FILE_NAME:
        warnings.append(f"manifest summaryFile is not {SUMMARY_FILE_NAME}")

    unreal_import = manifest.get("unrealImport", {})
    if isinstance(unreal_import, dict):
        if unreal_import.get("mainReviewFile") != MAP_ACCUMULATED_FILE_NAME:
            errors.append(f"manifest unrealImport.mainReviewFile is not {MAP_ACCUMULATED_FILE_NAME}")
        if unreal_import.get("debugWorldFile") != WORLD_ACCUMULATED_FILE_NAME:
            warnings.append(f"manifest unrealImport.debugWorldFile is not {WORLD_ACCUMULATED_FILE_NAME}")
    else:
        warnings.append("manifest unrealImport is not object")

    return profile


# Validate summary metadata against generated point files.
def _validate_summary_counts(
    summary: dict[str, Any] | None,
    frame_file_count: int,
    map_point_count: int,
    world_point_count: int,
    frame_index_records: list[dict[str, Any]],
    errors: list[str],
    warnings: list[str],
) -> dict[str, int]:
    if summary is None:
        return {
            "summaryFrameCount": 0,
            "summaryTotalPointCount": 0,
            "summaryGroundPointCount": 0,
            "summaryObstaclePointCount": 0,
        }

    summary_frame_count = _read_int_value(summary, "frameCount", 0, warnings)
    summary_total_point_count = _read_int_value(summary, "totalPointCount", 0, warnings)
    summary_ground_point_count = _read_int_value(summary, "groundPointCount", 0, warnings)
    summary_obstacle_point_count = _read_int_value(summary, "obstaclePointCount", 0, warnings)

    if summary.get("mainReviewFile") != MAP_ACCUMULATED_FILE_NAME:
        errors.append(f"summary mainReviewFile is not {MAP_ACCUMULATED_FILE_NAME}")

    if summary.get("debugWorldFile") != WORLD_ACCUMULATED_FILE_NAME:
        warnings.append(f"summary debugWorldFile is not {WORLD_ACCUMULATED_FILE_NAME}")

    if summary.get("frameDirectory") != FRAME_DIRECTORY_NAME:
        warnings.append(f"summary frameDirectory is not {FRAME_DIRECTORY_NAME}")

    if summary_frame_count != frame_file_count:
        errors.append(f"frame count mismatch: summary={summary_frame_count}, files={frame_file_count}")

    if summary_total_point_count != map_point_count:
        errors.append(f"map point count mismatch: summary={summary_total_point_count}, map={map_point_count}")

    if summary_total_point_count != world_point_count:
        errors.append(f"world point count mismatch: summary={summary_total_point_count}, world={world_point_count}")

    if frame_index_records and len(frame_index_records) != summary_frame_count:
        errors.append(f"frame index count mismatch: summary={summary_frame_count}, index={len(frame_index_records)}")

    indexed_point_count = sum(_read_int_value(record, "pointCount", 0, warnings) for record in frame_index_records)
    if frame_index_records and indexed_point_count != summary_total_point_count:
        errors.append(f"frame index point count mismatch: summary={summary_total_point_count}, index={indexed_point_count}")

    if summary_total_point_count != summary_ground_point_count + summary_obstacle_point_count + _read_int_value(summary, "unknownPointCount", 0, warnings):
        warnings.append("classification point counts do not add up to totalPointCount")

    return {
        "summaryFrameCount": summary_frame_count,
        "summaryTotalPointCount": summary_total_point_count,
        "summaryGroundPointCount": summary_ground_point_count,
        "summaryObstaclePointCount": summary_obstacle_point_count,
    }


# Build a validation report for one LiDAR Point Cloud capture folder.
def build_capture_report(capture_input_path: Path, expected_profile: str | None = None) -> dict[str, Any]:
    errors: list[str] = []
    warnings: list[str] = []
    capture_directory = _resolve_lidar_capture_directory(capture_input_path)

    if not capture_directory.exists():
        errors.append(f"capture directory does not exist: {capture_directory}")
        return {
            "ok": False,
            "captureDirectory": str(capture_directory),
            "profile": "",
            "errors": errors,
            "warnings": warnings,
        }

    if not capture_directory.is_dir():
        errors.append(f"capture path is not directory: {capture_directory}")
        return {
            "ok": False,
            "captureDirectory": str(capture_directory),
            "profile": "",
            "errors": errors,
            "warnings": warnings,
        }

    summary = _load_json_file(capture_directory / SUMMARY_FILE_NAME, errors)
    manifest = _load_json_file(capture_directory / MANIFEST_FILE_NAME, errors)
    frame_files = _list_frame_files(capture_directory / FRAME_DIRECTORY_NAME, errors)
    frame_index_records = _load_frame_index_records(capture_directory / FRAME_INDEX_FILE_NAME, warnings)
    map_point_count = _count_xyz_points(capture_directory / MAP_ACCUMULATED_FILE_NAME, errors)
    world_point_count = _count_xyz_points(capture_directory / WORLD_ACCUMULATED_FILE_NAME, errors)
    profile = _validate_manifest(manifest, expected_profile, errors, warnings)
    summary_counts = _validate_summary_counts(
        summary,
        len(frame_files),
        map_point_count,
        world_point_count,
        frame_index_records,
        errors,
        warnings,
    )

    return {
        "ok": len(errors) == 0,
        "captureDirectory": str(capture_directory),
        "profile": profile,
        "expectedProfile": expected_profile or "",
        "frameFileCount": len(frame_files),
        "frameIndexCount": len(frame_index_records),
        "mapPointCount": map_point_count,
        "worldPointCount": world_point_count,
        **summary_counts,
        "errors": errors,
        "warnings": warnings,
    }


# Convert a validation report to a compact human-readable text summary.
def _format_report(report: dict[str, Any]) -> str:
    status = "OK" if report.get("ok") else "FAIL"
    lines = [
        f"[{status}] LiDAR Point Cloud capture validation",
        f"- captureDirectory: {report.get('captureDirectory', '')}",
        f"- profile: {report.get('profile', '')}",
        f"- frameCount: summary={report.get('summaryFrameCount', 0)}, files={report.get('frameFileCount', 0)}, index={report.get('frameIndexCount', 0)}",
        f"- pointCount: summary={report.get('summaryTotalPointCount', 0)}, map={report.get('mapPointCount', 0)}, world={report.get('worldPointCount', 0)}",
        f"- classification: ground={report.get('summaryGroundPointCount', 0)}, obstacle={report.get('summaryObstaclePointCount', 0)}",
    ]

    errors = report.get("errors", [])
    if errors:
        lines.append("- errors:")
        lines.extend(f"  - {error}" for error in errors)

    warnings = report.get("warnings", [])
    if warnings:
        lines.append("- warnings:")
        lines.extend(f"  - {warning}" for warning in warnings)

    return "\n".join(lines)


# Parse command-line arguments for capture validation.
def _parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Validate a DeliveryBot LiDAR Point Cloud capture folder.")
    parser.add_argument(
        "capture_path",
        help="Path to a seq_* run folder or its captures/lidar_point_cloud folder.",
    )
    parser.add_argument(
        "--expected-profile",
        default=None,
        help="Optional expected manifest profile, such as realtime_point_cloud or quality_point_cloud.",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Print the full validation report as JSON.",
    )
    return parser.parse_args()


# Run the validation CLI and return a process exit code.
def main() -> int:
    arguments = _parse_arguments()
    report = build_capture_report(
        Path(arguments.capture_path),
        expected_profile=arguments.expected_profile,
    )

    if arguments.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
    else:
        print(_format_report(report))

    return 0 if report["ok"] else 1


# Execute the CLI when this file is run directly.
if __name__ == "__main__":
    sys.exit(main())
