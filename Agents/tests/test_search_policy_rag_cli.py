from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "search_policy_rag.py"


def run_cli(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), *args],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )


def test_cli_help_works() -> None:
    completed = run_cli("--help")
    assert completed.returncode == 0
    assert "--query" in completed.stdout


def test_cli_query_returns_results() -> None:
    completed = run_cli("--query", "비상정지")
    assert completed.returncode == 0
    assert "CHUNK-" in completed.stdout


def test_cli_category_search_returns_results() -> None:
    completed = run_cli("--category", "speed_policy")
    assert completed.returncode == 0
    assert "speed_policy" in completed.stdout


def test_cli_action_search_returns_results() -> None:
    completed = run_cli("--action", "EmergencyStop")
    assert completed.returncode == 0
    assert "EmergencyStop" in completed.stdout


def test_cli_report_is_written_only_when_requested(tmp_path: Path) -> None:
    report_path = tmp_path / "rag_retrieval_report.json"
    completed = run_cli("--query", "비상정지", "--report", str(report_path))
    assert completed.returncode == 0
    report = json.loads(report_path.read_text(encoding="utf-8"))
    assert report["resultCount"] >= 1
    assert report["query"] == "비상정지"


def test_cli_without_report_does_not_create_report(tmp_path: Path) -> None:
    report_path = tmp_path / "rag_retrieval_report.json"
    completed = run_cli("--query", "비상정지")
    assert completed.returncode == 0
    assert not report_path.exists()
