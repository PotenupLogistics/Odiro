from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "export_ue5_handoff_payload.py"


def _project_files() -> set[str]:
    ignored_parts = {".venv", ".pytest_cache", "__pycache__"}
    return {
        path.relative_to(ROOT).as_posix()
        for path in ROOT.rglob("*")
        if path.is_file() and not any(part in ignored_parts for part in path.parts)
    }


def test_export_cli_help_works() -> None:
    completed = subprocess.run(
        [sys.executable, str(SCRIPT), "--help"],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )

    assert completed.returncode == 0
    assert "--prompt" in completed.stdout
    assert "--world-config-only" in completed.stdout
    assert "--format" in completed.stdout
    assert "--out" in completed.stdout


def test_export_cli_without_out_does_not_create_file(tmp_path: Path) -> None:
    before = _project_files()

    completed = subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--provider",
            "disabled",
            "--prompt",
            "좁은 보도에서 킥보드가 경로를 막고 보행자가 횡단",
            "--world-config-only",
        ],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )

    after = _project_files()
    assert completed.returncode == 0
    assert "worldConfig" in completed.stdout or "null" in completed.stdout
    assert before == after
    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()


def test_export_cli_writes_only_explicit_out_path(tmp_path: Path) -> None:
    out_path = tmp_path / "world_config_for_ue5.json"

    completed = subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--provider",
            "disabled",
            "--prompt",
            "좁은 보도에서 킥보드가 경로를 막고 보행자가 횡단",
            "--out",
            str(out_path),
        ],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )

    assert completed.returncode == 0
    assert out_path.exists()
    payload = json.loads(out_path.read_text(encoding="utf-8"))
    assert payload is None


def test_export_cli_supports_episode_spec_format_with_disabled_provider(tmp_path: Path) -> None:
    out_path = tmp_path / "episode_spec.json"

    completed = subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--provider",
            "disabled",
            "--prompt",
            "좁은 보도에서 킥보드가 경로를 막고 보행자가 횡단",
            "--format",
            "episode_spec",
            "--out",
            str(out_path),
        ],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )

    assert completed.returncode == 0
    payload = json.loads(out_path.read_text(encoding="utf-8"))
    assert payload is None


def test_no_vector_embedding_sample_fixture_artifacts_exist() -> None:
    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "embeddings").exists()
