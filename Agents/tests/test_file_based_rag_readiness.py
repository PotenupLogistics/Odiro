from __future__ import annotations

import subprocess
import sys
from pathlib import Path
from types import SimpleNamespace

import scripts.check_file_based_rag_readiness as readiness
from harness.checks.check_file_based_rag_readiness import run_check


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "check_file_based_rag_readiness.py"


def _runtime_result(
    passed: bool = True,
    errors: list[str] | None = None,
    runtime_source_status_guard_passed: bool = True,
    vector_db_directories_absent: bool = True,
) -> SimpleNamespace:
    return SimpleNamespace(
        passed=passed,
        errors=errors or [],
        source_inventory_exists=True,
        runtime_source_status_guard_passed=runtime_source_status_guard_passed,
        vector_db_directories_absent=vector_db_directories_absent,
    )


def _candidate_result(passed: bool = True, errors: list[str] | None = None) -> SimpleNamespace:
    return SimpleNamespace(passed=passed, errors=errors or [], warnings=[], candidate_count=0)


def test_readiness_cli_passes_current_repo_state() -> None:
    completed = subprocess.run(
        [sys.executable, str(SCRIPT)],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )

    assert completed.returncode == 0
    assert "file-based RAG readiness check" in completed.stdout
    assert "runtime store validation: PASS" in completed.stdout
    assert "source inventory validation: PASS" in completed.stdout
    assert "runtime source status guard: PASS" in completed.stdout
    assert "candidate chunk validation: PASS" in completed.stdout
    assert "vector DB directories: absent" in completed.stdout
    assert "result: PASS" in completed.stdout


def test_readiness_fails_when_runtime_validator_fails(monkeypatch) -> None:
    monkeypatch.setattr(
        readiness,
        "validate_file_based_rag_store",
        lambda root: _runtime_result(False, ["runtime chunk file missing"]),
    )
    monkeypatch.setattr(
        readiness,
        "validate_policy_chunk_candidates",
        lambda root: _candidate_result(True),
    )

    result = readiness.check_file_based_rag_readiness(ROOT)

    assert result.passed is False
    assert result.runtime_validation_passed is False
    assert "runtime chunk file missing" in result.errors


def test_readiness_fails_when_candidate_validator_fails(monkeypatch) -> None:
    monkeypatch.setattr(
        readiness,
        "validate_file_based_rag_store",
        lambda root: _runtime_result(True),
    )
    monkeypatch.setattr(
        readiness,
        "validate_policy_chunk_candidates",
        lambda root: _candidate_result(False, ["candidate source_id KOR-999 is not registered"]),
    )

    result = readiness.check_file_based_rag_readiness(ROOT)

    assert result.passed is False
    assert result.candidate_validation_passed is False
    assert "candidate source_id KOR-999 is not registered" in result.errors


def test_skip_candidates_allows_readiness_to_pass_when_candidate_validator_would_fail(monkeypatch) -> None:
    monkeypatch.setattr(
        readiness,
        "validate_file_based_rag_store",
        lambda root: _runtime_result(True),
    )
    monkeypatch.setattr(
        readiness,
        "validate_policy_chunk_candidates",
        lambda root: _candidate_result(False, ["candidate failure"]),
    )

    result = readiness.check_file_based_rag_readiness(ROOT, skip_candidates=True)

    assert result.passed is True
    assert result.candidate_validation_passed is None
    assert not result.errors


def test_main_returns_nonzero_when_readiness_fails(monkeypatch) -> None:
    monkeypatch.setattr(
        readiness,
        "validate_file_based_rag_store",
        lambda root: _runtime_result(False, ["runtime failure"]),
    )
    monkeypatch.setattr(
        readiness,
        "validate_policy_chunk_candidates",
        lambda root: _candidate_result(True),
    )

    assert readiness.main(["--root", str(ROOT)]) == 1


def test_main_returns_zero_when_readiness_passes(monkeypatch) -> None:
    monkeypatch.setattr(
        readiness,
        "validate_file_based_rag_store",
        lambda root: _runtime_result(True),
    )
    monkeypatch.setattr(
        readiness,
        "validate_policy_chunk_candidates",
        lambda root: _candidate_result(True),
    )

    assert readiness.main(["--root", str(ROOT)]) == 0


def test_harness_readiness_check_returns_pass_dict() -> None:
    result = run_check()

    assert result["passed"] is True
    assert result["runtimeStoreValidationPassed"] is True
    assert result["sourceInventoryValidationPassed"] is True
    assert result["runtimeSourceStatusGuardPassed"] is True
    assert result["candidateChunkValidationPassed"] is True
    assert result["vectorDbDirectoriesAbsent"] is True
