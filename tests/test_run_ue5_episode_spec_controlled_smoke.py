from __future__ import annotations

import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "run_ue5_episode_spec_controlled_smoke.py"


def test_controlled_smoke_script_help_works() -> None:
    completed = subprocess.run(
        [sys.executable, str(SCRIPT), "--help"],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )

    assert completed.returncode == 0
    assert "--response-format" in completed.stdout
    assert "--print-episode-spec" in completed.stdout
    assert "--report" in completed.stdout


def test_controlled_smoke_script_dry_run_does_not_create_files(tmp_path: Path) -> None:
    before = {
        path.relative_to(ROOT).as_posix()
        for path in ROOT.rglob("*")
        if path.is_file() and ".venv" not in path.parts and "__pycache__" not in path.parts
    }

    completed = subprocess.run(
        [sys.executable, str(SCRIPT), "--provider", "disabled", "--dry-run"],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )

    after = {
        path.relative_to(ROOT).as_posix()
        for path in ROOT.rglob("*")
        if path.is_file() and ".venv" not in path.parts and "__pycache__" not in path.parts
    }
    assert completed.returncode == 0
    assert "Dry run" in completed.stdout
    assert before == after
    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()

