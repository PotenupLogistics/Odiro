from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "run_ollama_world_config_smoke.py"


def run_cli(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), *args],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )


def test_live_smoke_help_works() -> None:
    completed = run_cli("--help")

    assert completed.returncode == 0
    assert "--prompt" in completed.stdout
    assert "--dry-run" in completed.stdout
    assert "--include-raw-attempts" in completed.stdout
    assert "--include-extracted-json" in completed.stdout
    assert "--raw-preview-chars" in completed.stdout


def test_dry_run_prints_prompt_package_without_ollama_call() -> None:
    completed = run_cli(
        "--prompt",
        "좁은 보도에서 킥보드가 경로를 막고 보행자가 횡단",
        "--dry-run",
    )

    assert completed.returncode == 0
    assert "DRY RUN" in completed.stdout
    assert "retrievedContexts" in completed.stdout
    assert "ollama_connection_failed" not in completed.stdout


def test_missing_prompt_fails() -> None:
    completed = run_cli("--dry-run")

    assert completed.returncode != 0
    assert "usage:" in completed.stderr.lower()


def test_report_option_writes_report_to_temp_path(tmp_path: Path) -> None:
    report_path = tmp_path / "manual_ollama_world_config_smoke.json"

    completed = run_cli(
        "--prompt",
        "경사로와 턱이 있는 보도 주행 상황",
        "--dry-run",
        "--report",
        str(report_path),
    )

    assert completed.returncode == 0
    report = json.loads(report_path.read_text(encoding="utf-8"))
    assert report["provider"] == "ollama"
    assert report["success"] is False
    assert report["validationPassed"] is False
    assert report["retrievedContextCount"] >= 0
    assert "attemptsDetail" in report
    assert "validationErrorSummary" in report
    assert "extractionSummary" in report
    assert "outputContractIncluded" in report
    assert "scenarioRequirementCount" in report
    assert "scenarioRequirementPaths" in report
    assert "generatedPayload" not in report


def test_no_sample_fixture_vector_or_embedding_artifacts_exist() -> None:
    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "embeddings").exists()
