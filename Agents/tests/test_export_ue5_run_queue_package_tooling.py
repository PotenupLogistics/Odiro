from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "export_ue5_run_queue_package.py"


def _write_request_json(path: Path) -> None:
    path.write_text(
        json.dumps(
            {
                "worldConfig": {
                    "schemaVersion": "1.0",
                    "worldId": "world-1",
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
                }
            }
        ),
        encoding="utf-8",
    )


def test_run_queue_export_cli_help_works() -> None:
    completed = subprocess.run(
        [sys.executable, str(SCRIPT), "--help"],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )

    assert completed.returncode == 0
    assert "--prompt" in completed.stdout
    assert "--episode-count" in completed.stdout
    assert "--base-seed" in completed.stdout
    assert "--output-dir" in completed.stdout
    assert "--dry-run" in completed.stdout
    assert "--request-json" in completed.stdout
    assert "--provider" in completed.stdout
    assert "--fixed" in completed.stdout


def test_run_queue_export_cli_request_json_exports_without_live_provider(tmp_path: Path) -> None:
    request_json = tmp_path / "request.json"
    output_dir = tmp_path / "export"
    _write_request_json(request_json)

    completed = subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--request-json",
            str(request_json),
            "--prompt",
            "정적 장애물이 경로를 막는 상황",
            "--episode-count",
            "2",
            "--base-seed",
            "3000",
            "--output-dir",
            str(output_dir),
        ],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )

    assert completed.returncode == 0
    stdout = json.loads(completed.stdout)
    assert stdout["runQueueExists"] is True
    assert stdout["runCount"] == 2
    run_queue_path = output_dir / "Json" / "Input" / "EpisodeRunQueue_obstacle_ahead.json"
    assert run_queue_path.exists()
    assert set(json.loads(run_queue_path.read_text(encoding="utf-8"))) == {"schema", "version", "runs"}


def test_run_queue_export_cli_dry_run_does_not_create_files(tmp_path: Path) -> None:
    request_json = tmp_path / "request.json"
    output_dir = tmp_path / "dry_run_export"
    _write_request_json(request_json)

    completed = subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--request-json",
            str(request_json),
            "--prompt",
            "정적 장애물이 경로를 막는 상황",
            "--dry-run",
            "--output-dir",
            str(output_dir),
        ],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )

    assert completed.returncode == 0
    stdout = json.loads(completed.stdout)
    assert stdout["runQueueExists"] is True
    assert stdout["exportPath"] is None
    assert not output_dir.exists()


def test_run_queue_export_cli_rejects_missing_input_without_live_provider_call() -> None:
    completed = subprocess.run(
        [sys.executable, str(SCRIPT), "--episode-count", "5"],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )

    assert completed.returncode == 2
    assert "--request-json or --prompt is required" in completed.stderr
