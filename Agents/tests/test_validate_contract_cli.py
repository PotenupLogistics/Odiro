from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

from tests.test_json_contract_validator import valid_policy_config


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "validate_contract.py"


def run_cli(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), *args],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )


def test_cli_help_outputs_usage() -> None:
    completed = run_cli("--help")
    assert completed.returncode == 0
    assert "--type" in completed.stdout
    assert "--file" in completed.stdout


def test_cli_validates_temp_json_file(tmp_path: Path) -> None:
    payload_path = tmp_path / "policy.json"
    payload_path.write_text(json.dumps(valid_policy_config()), encoding="utf-8")

    completed = run_cli("--type", "policy_config", "--file", str(payload_path))

    assert completed.returncode == 0
    assert "PASS" in completed.stdout


def test_cli_returns_fail_for_invalid_temp_json_file(tmp_path: Path) -> None:
    payload = valid_policy_config()
    payload["availableActions"] = ["FlyAway"]
    payload_path = tmp_path / "policy.json"
    payload_path.write_text(json.dumps(payload), encoding="utf-8")

    completed = run_cli("--type", "policy_config", "--file", str(payload_path))

    assert completed.returncode == 1
    assert "FAIL" in completed.stdout


def test_cli_writes_report_only_when_requested(tmp_path: Path) -> None:
    payload_path = tmp_path / "policy.json"
    report_path = tmp_path / "contract_validation_report.json"
    payload_path.write_text(json.dumps(valid_policy_config()), encoding="utf-8")

    completed = run_cli(
        "--type",
        "policy_config",
        "--file",
        str(payload_path),
        "--report",
        str(report_path),
    )

    assert completed.returncode == 0
    report = json.loads(report_path.read_text(encoding="utf-8"))
    assert report["contractType"] == "policy_config"
    assert report["valid"] is True


def test_no_samples_or_fixtures_are_created() -> None:
    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
