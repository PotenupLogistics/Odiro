from __future__ import annotations

import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "run_ue5_handoff_smoke.py"


def test_ue5_handoff_smoke_script_help_works() -> None:
    completed = subprocess.run(
        [sys.executable, str(SCRIPT), "--help"],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )

    assert completed.returncode == 0
    assert "--dry-run" in completed.stdout
    assert "--print-world-config" in completed.stdout


def test_ue5_handoff_smoke_dry_run_does_not_call_provider_or_write_files(tmp_path: Path) -> None:
    report_path = tmp_path / "should_not_exist.json"
    completed = subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--provider",
            "ollama",
            "--prompt",
            "좁은 보도에서 킥보드가 경로를 막고 보행자가 횡단",
            "--dry-run",
            "--report",
            str(report_path),
        ],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )

    assert completed.returncode == 0
    assert "Dry run" in completed.stdout
    assert not report_path.exists()


def test_no_sample_fixture_vector_or_embedding_artifacts_exist() -> None:
    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "embeddings").exists()
