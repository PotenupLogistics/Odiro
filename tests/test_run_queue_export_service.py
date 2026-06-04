from __future__ import annotations

import json
from pathlib import Path

from app.services.run_queue_export_service import export_run_queue_package
from app.services.setup_pair_queue_generator import generate_setup_pair_queue


def _world_config() -> dict:
    return {
        "scenarioId": "obstacle_ahead",
        "seed": 1001,
        "map": {"type": "Sidewalk", "lengthCm": 800, "sidewalkWidthCm": 120},
        "robot": {
            "botId": "robot_01",
            "spawn": {"x": 0, "y": 0, "z": 0},
            "goal": {"x": 800, "y": 0, "z": 0},
        },
        "obstacles": [
            {"objectId": "obstacle_01", "type": "Obstacle", "position": {"x": 400, "y": 0, "z": 0}, "blockingRatio": 0.6}
        ],
        "pedestrians": [],
        "runtime": {"maxDurationSec": 60},
    }


def test_export_run_queue_package_writes_ue_contract_files_under_tmp_path(tmp_path: Path) -> None:
    queue = generate_setup_pair_queue(_world_config(), episode_count=2, request_id="REQ-001")

    result = export_run_queue_package(queue, output_dir=tmp_path)

    input_dir = result.export_root / "Json" / "Input"
    assert result.exported is True
    assert (input_dir / "EpisodeSetup_narrow_sidewalk_fixed_center_block.json").exists()
    assert (input_dir / "DeliveryBotSetup_policy_000_baseline.json").exists()
    assert (input_dir / "DeliveryBotSetup_policy_001_short_stop.json").exists()
    assert (input_dir / "EpisodeRunQueue_narrow_sidewalk_policy_comparison.json").exists()
    assert (result.export_root / "export_summary.json").exists()
    assert (result.export_root / "validation_summary.json").exists()
    assert (result.export_root / "trace_summary.json").exists()


def test_exported_run_queue_contains_only_ue_contract_fields(tmp_path: Path) -> None:
    queue = generate_setup_pair_queue(_world_config(), episode_count=1, request_id="REQ-001")

    result = export_run_queue_package(queue, output_dir=tmp_path)

    payload = json.loads((result.export_root / "Json" / "Input" / "EpisodeRunQueue_narrow_sidewalk_policy_comparison.json").read_text(encoding="utf-8"))
    assert set(payload) == {"schema", "version", "runs"}
    assert set(payload["runs"][0]) == {"pair_id", "episode_setup", "delivery_bot_setup"}
    assert "success" not in payload
    assert "diagnostics" not in payload
    assert "setupPairs" not in payload


def test_exported_run_queue_uses_single_episode_setup_for_all_policy_runs(tmp_path: Path) -> None:
    queue = generate_setup_pair_queue(_world_config(), episode_count=5, request_id="REQ-001")

    result = export_run_queue_package(queue, output_dir=tmp_path)

    input_dir = result.export_root / "Json" / "Input"
    payload = json.loads((input_dir / "EpisodeRunQueue_narrow_sidewalk_policy_comparison.json").read_text(encoding="utf-8"))
    episode_paths = {run["episode_setup"] for run in payload["runs"]}
    assert episode_paths == {"Json/Input/EpisodeSetup_narrow_sidewalk_fixed_center_block.json"}
    assert [run["delivery_bot_setup"] for run in payload["runs"]] == [
        "Json/Input/DeliveryBotSetup_policy_000_baseline.json",
        "Json/Input/DeliveryBotSetup_policy_001_short_stop.json",
        "Json/Input/DeliveryBotSetup_policy_002_long_stop.json",
        "Json/Input/DeliveryBotSetup_policy_003_early_slowdown.json",
        "Json/Input/DeliveryBotSetup_policy_004_low_speed.json",
    ]
    assert len(list(input_dir.glob("EpisodeSetup_*.json"))) == 1
    assert len(list(input_dir.glob("DeliveryBotSetup_*.json"))) == 5


def test_exported_episode_and_delivery_bot_payloads_are_null_free(tmp_path: Path) -> None:
    queue = generate_setup_pair_queue(_world_config(), episode_count=1, request_id="REQ-001")

    result = export_run_queue_package(queue, output_dir=tmp_path)

    exported_text = "\n".join(path.read_text(encoding="utf-8") for path in (result.export_root / "Json" / "Input").glob("*.json"))
    assert "null" not in exported_text


def test_export_service_moves_existing_target_to_backup_before_writing(tmp_path: Path) -> None:
    output_dir = tmp_path / "export"
    existing_input = output_dir / "Json" / "Input"
    existing_input.mkdir(parents=True)
    (existing_input / "old.json").write_text("{}", encoding="utf-8")
    queue = generate_setup_pair_queue(_world_config(), episode_count=1, request_id="REQ-001")

    result = export_run_queue_package(queue, output_dir=output_dir)

    backup_root = tmp_path / "_backup"
    backups = list(backup_root.glob("export_*"))
    assert result.exported is True
    assert backups
    assert (backups[0] / "Json" / "Input" / "old.json").exists()
    assert (backups[0] / "backup_summary.json").exists()


def test_export_summary_does_not_store_raw_content_or_secret_like_values(tmp_path: Path) -> None:
    queue = generate_setup_pair_queue(_world_config(), episode_count=1, request_id="REQ-001")

    result = export_run_queue_package(queue, output_dir=tmp_path)

    summary_text = (result.export_root / "export_summary.json").read_text(encoding="utf-8")
    assert "rawContent" not in summary_text
    assert "OPENAI_API_KEY" not in summary_text
    assert "api_key" not in summary_text.lower()
