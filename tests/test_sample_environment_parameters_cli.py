from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "sample_environment_parameters.py"


def _project_files() -> set[str]:
    ignored_parts = {".venv", ".pytest_cache", "__pycache__"}
    return {
        path.relative_to(ROOT).as_posix()
        for path in ROOT.rglob("*")
        if path.is_file() and not any(part in ignored_parts for part in path.parts)
    }


def test_cli_help_works() -> None:
    completed = subprocess.run(
        [sys.executable, str(SCRIPT), "--help"],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )

    assert completed.returncode == 0
    assert "--seed" in completed.stdout
    assert "--scenario-type" in completed.stdout
    assert "--fixed" in completed.stdout


def test_cli_default_run_does_not_create_files() -> None:
    before = _project_files()

    completed = subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--seed",
            "1001",
            "--scenario-type",
            "narrow_sidewalk_kickboard_crossing",
        ],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )

    after = _project_files()
    assert completed.returncode == 0
    assert "parameters:" in completed.stdout
    assert before == after
    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "embeddings").exists()


def test_cli_json_and_fixed_parameters() -> None:
    completed = subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--seed",
            "1001",
            "--scenario-type",
            "narrow_sidewalk_kickboard_crossing",
            "--fixed",
            "sidewalkWidthCm=120",
            "--fixed",
            "pedestrianCount=3",
            "--json",
        ],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )

    assert completed.returncode == 0
    payload = json.loads(completed.stdout)
    assert payload["parameters"]["sidewalkWidthCm"] == 120
    assert payload["parameters"]["pedestrianCount"] == 3


def test_cli_rejects_low_middle_high_values() -> None:
    completed = subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--seed",
            "1001",
            "--scenario-type",
            "generic_sidewalk",
            "--fixed",
            "robotSpeedKmh=high",
        ],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )

    assert completed.returncode != 0
    assert "low/middle/high" in completed.stderr
