from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "run_openai_world_config_smoke.py"


def _project_files() -> set[str]:
    ignored_parts = {".venv", ".pytest_cache", "__pycache__"}
    return {
        path.relative_to(ROOT).as_posix()
        for path in ROOT.rglob("*")
        if path.is_file() and not any(part in ignored_parts for part in path.parts)
    }


def test_openai_smoke_help_works() -> None:
    completed = subprocess.run(
        [sys.executable, str(SCRIPT), "--help"],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )

    assert completed.returncode == 0
    assert "--prompt" in completed.stdout
    assert "--dry-run" in completed.stdout
    assert "--report" in completed.stdout
    assert "--allow-" + "fallback" not in completed.stdout


def test_openai_smoke_dry_run_does_not_create_files_or_call_openai() -> None:
    before = _project_files()

    completed = subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--prompt",
            "좁은 보도에서 공유 킥보드가 로봇 경로를 막고, 오른쪽에서 보행자가 횡단하는 상황을 만들어줘.",
            "--dry-run",
        ],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )

    after = _project_files()
    assert completed.returncode == 0
    payload = json.loads(completed.stdout)
    assert payload["dryRun"] is True
    assert payload["openaiCalled"] is False
    assert payload["providerChain"] == ["openai"]
    assert before == after
    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "embeddings").exists()
