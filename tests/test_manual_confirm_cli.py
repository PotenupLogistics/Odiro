from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "manual_confirm.py"
SOURCE_RESULTS = ROOT / "data" / "sources" / "review" / "confirmed" / "manual_confirmation_results.json"


def copy_results(tmp_path: Path) -> Path:
    target = tmp_path / "manual_confirmation_results.json"
    shutil.copyfile(SOURCE_RESULTS, target)
    return target


def run_cli(results_path: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--file", str(results_path), *args],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def first_candidate_id(path: Path) -> str:
    payload = json.loads(read_text(path))
    return payload["items"][0]["candidateId"]


def test_list_does_not_modify_file(tmp_path: Path) -> None:
    results = copy_results(tmp_path)
    before = read_text(results)
    completed = run_cli(results, "list")
    assert completed.returncode == 0
    assert read_text(results) == before


def test_summary_does_not_modify_file(tmp_path: Path) -> None:
    results = copy_results(tmp_path)
    before = read_text(results)
    completed = run_cli(results, "summary")
    assert completed.returncode == 0
    assert "pending_manual_confirmation" in completed.stdout
    assert read_text(results) == before


def test_show_outputs_existing_candidate(tmp_path: Path) -> None:
    results = copy_results(tmp_path)
    candidate_id = first_candidate_id(results)
    completed = run_cli(results, "show", candidate_id)
    assert completed.returncode == 0
    assert candidate_id in completed.stdout


def test_confirm_without_yes_does_not_modify_file(tmp_path: Path) -> None:
    results = copy_results(tmp_path)
    candidate_id = first_candidate_id(results)
    before = read_text(results)
    completed = run_cli(
        results,
        "confirm",
        candidate_id,
        "--page",
        "p.1",
        "--text",
        "confirmed text",
        "--reviewer",
        "tester",
        "--reason",
        "manual test",
    )
    assert completed.returncode == 0
    assert "Dry-run" in completed.stdout
    assert read_text(results) == before


def test_confirm_requires_required_fields(tmp_path: Path) -> None:
    results = copy_results(tmp_path)
    candidate_id = first_candidate_id(results)
    completed = run_cli(results, "confirm", candidate_id, "--page", "p.1")
    assert completed.returncode != 0


def test_confirm_with_yes_updates_temp_file(tmp_path: Path) -> None:
    results = copy_results(tmp_path)
    candidate_id = first_candidate_id(results)
    completed = run_cli(
        results,
        "confirm",
        candidate_id,
        "--page",
        "p.1",
        "--text",
        "confirmed text",
        "--reviewer",
        "tester",
        "--reason",
        "manual test",
        "--yes",
    )
    assert completed.returncode == 0
    payload = json.loads(read_text(results))
    item = next(item for item in payload["items"] if item["candidateId"] == candidate_id)
    assert item["manualReviewStatus"] == "confirmed"


def test_reject_without_yes_does_not_modify_file(tmp_path: Path) -> None:
    results = copy_results(tmp_path)
    candidate_id = first_candidate_id(results)
    before = read_text(results)
    completed = run_cli(
        results,
        "reject",
        candidate_id,
        "--reviewer",
        "tester",
        "--reason",
        "manual reject",
    )
    assert completed.returncode == 0
    assert "Dry-run" in completed.stdout
    assert read_text(results) == before


def test_reject_requires_reason(tmp_path: Path) -> None:
    results = copy_results(tmp_path)
    candidate_id = first_candidate_id(results)
    completed = run_cli(results, "reject", candidate_id, "--reviewer", "tester")
    assert completed.returncode != 0


def test_reject_with_yes_updates_temp_file(tmp_path: Path) -> None:
    results = copy_results(tmp_path)
    candidate_id = first_candidate_id(results)
    completed = run_cli(
        results,
        "reject",
        candidate_id,
        "--reviewer",
        "tester",
        "--reason",
        "manual reject",
        "--yes",
    )
    assert completed.returncode == 0
    payload = json.loads(read_text(results))
    item = next(item for item in payload["items"] if item["candidateId"] == candidate_id)
    assert item["manualReviewStatus"] == "rejected"
