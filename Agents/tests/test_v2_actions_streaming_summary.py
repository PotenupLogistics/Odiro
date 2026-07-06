from __future__ import annotations

import json
from pathlib import Path

from app.agents.result_analysis_v2.artifact_classifier import ArtifactClassifier
from app.agents.result_analysis_v2.artifact_parser import ArtifactParser, ParsedArtifact
from app.agents.result_analysis_v2.data_coverage import DataCoverageBuilder
from app.agents.result_analysis_v2.workspace_scanner import WorkspaceScanner


def _write_actions_jsonl(path: Path, rows: list[dict], *, broken_lines: list[str] | None = None) -> None:
    """Write action rows and optional malformed JSONL lines to a fixture file."""
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [json.dumps(row, ensure_ascii=False) for row in rows]
    lines.extend(broken_lines or [])
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def _actions_artifact(project: Path, actions_path: Path) -> ParsedArtifact:
    """Classify and parse one actions.jsonl artifact through production code."""
    classifier = ArtifactClassifier()
    parser = ArtifactParser()
    return parser.parse(classifier.classify(project, actions_path))


def test_artifact_parser_streams_actions_jsonl_into_summary(tmp_path) -> None:
    project = tmp_path / "Project1"
    actions_path = project / "runs" / "000001" / "episodes" / "000001" / "actions.jsonl"
    _write_actions_jsonl(
        actions_path,
        [
            {
                "timestamp": 1.0,
                "action": "repath",
                "reason": "blocked",
                "speed_mps": 0.5,
                "path_error": 0.7,
                "goal_distance": 10.0,
            },
            {
                "time_s": 2.0,
                "action_type": "slowdown",
                "decision_reason": "pedestrian",
                "velocity": 0.2,
                "path_error": 1.0,
                "goal_distance": 9.0,
            },
            {
                "t": 3.0,
                "command": "stop",
                "cause": "near_miss",
                "speed": 0.0,
                "path_error": 1.5,
                "goal_distance": 9.0,
            },
            {
                "timestamp": 4.0,
                "type": "follow_path",
                "reason": "clear",
                "speed": 1.0,
                "path_error": 0.3,
                "goal_distance": 8.0,
            },
        ],
        broken_lines=["{broken json}"],
    )

    parsed = _actions_artifact(project, actions_path)

    assert isinstance(parsed.data, dict)
    assert parsed.data["summary_type"] == "actions_streaming_summary"
    assert parsed.data["actions_line_count"] == 5
    assert parsed.data["parsed_actions_line_count"] == 4
    assert parsed.data["broken_actions_line_count"] == 1
    assert parsed.data["action_type_counts"] == {
        "follow_path": 1,
        "repath": 1,
        "slowdown": 1,
        "stop": 1,
    }
    assert parsed.data["decision_reason_counts"] == {
        "blocked": 1,
        "clear": 1,
        "near_miss": 1,
        "pedestrian": 1,
    }
    assert parsed.data["repath_action_count"] == 1
    assert parsed.data["slowdown_action_count"] == 1
    assert parsed.data["stop_action_count"] == 1
    assert parsed.data["follow_path_action_count"] == 1
    assert parsed.data["speed_stats"] == {"count": 4, "min": 0.0, "max": 1.0, "avg": 0.425}
    assert parsed.data["path_error_stats"] == {"count": 4, "min": 0.3, "max": 1.5, "avg": 0.875}
    assert parsed.data["goal_distance_change"] == {"first": 10.0, "last": 8.0, "delta": -2.0}
    assert parsed.data["first_timestamp"] == 1.0
    assert parsed.data["last_timestamp"] == 4.0
    assert "raw_actions" not in parsed.data
    assert parsed.warnings == ["runs/000001/episodes/000001/actions.jsonl: action summary skipped 1 broken JSONL line(s)."]


def test_workspace_scanner_keeps_large_actions_for_streaming_but_skips_large_binary(tmp_path) -> None:
    project = tmp_path / "Project1"
    actions_path = project / "runs" / "000001" / "episodes" / "000001" / "actions.jsonl"
    _write_actions_jsonl(
        actions_path,
        [
            {
                "action": "repath",
                "timestamp": 1.0,
                "speed_mps": 0.2,
                "padding": "x" * 5_000_001,
            }
        ],
    )
    binary_path = project / "runs" / "000001" / "episodes" / "000001" / "rays.frames.bin"
    binary_path.write_bytes(b"0" * 5_000_001)

    scan = WorkspaceScanner().scan(project)
    scanned_files = {path.relative_to(project).as_posix() for path in scan.files}

    assert "runs/000001/episodes/000001/actions.jsonl" in scanned_files
    assert "runs/000001/episodes/000001/rays.frames.bin" not in scanned_files
    assert not any("actions.jsonl" in warning and warning.startswith("skipped large file:") for warning in scan.warnings)
    assert any("rays.frames.bin" in warning and warning.startswith("skipped large file:") for warning in scan.warnings)


def test_data_coverage_counts_streamed_actions_summary(tmp_path) -> None:
    project = tmp_path / "Project1"
    run_path = project / "runs" / "000001"
    actions_path = run_path / "episodes" / "000001" / "actions.jsonl"
    _write_actions_jsonl(
        actions_path,
        [
            {"action": "repath", "timestamp": 1.0, "speed": 0.4},
            {"action": "follow_path", "timestamp": 2.0, "speed": 0.8},
        ],
        broken_lines=["{broken json}"],
    )
    parsed = _actions_artifact(project, actions_path)

    coverage = DataCoverageBuilder().build(run_path=run_path, parsed_artifacts=[parsed], warnings=parsed.warnings)

    assert coverage["actions_file_count"] == 1
    assert coverage["parsed_actions_file_count"] == 1
    assert coverage["summarized_actions_file_count"] == 1
    assert coverage["skipped_large_actions_file_count"] == 0
    assert coverage["actions_summary_line_count"] == 3
    assert coverage["actions_summary_parsed_line_count"] == 2
    assert coverage["actions_summary_broken_line_count"] == 1
    assert coverage["actions_summary_truncated_count"] == 0
    assert coverage["actions_summary_warnings"] == [
        "runs/000001/episodes/000001/actions.jsonl: action summary skipped 1 broken JSONL line(s)."
    ]


def test_actions_summary_limits_counter_fields_to_top_ten(tmp_path) -> None:
    project = tmp_path / "Project1"
    actions_path = project / "runs" / "000001" / "episodes" / "000001" / "actions.jsonl"
    rows: list[dict] = []
    for index in range(12):
        for _ in range(12 - index):
            rows.append(
                {
                    "action": f"action_{index:02d}",
                    "reason": f"reason_{index:02d}",
                    "timestamp": float(index),
                }
            )
    _write_actions_jsonl(actions_path, rows)

    parsed = _actions_artifact(project, actions_path)

    assert list(parsed.data["action_type_counts"]) == [f"action_{index:02d}" for index in range(10)]
    assert list(parsed.data["decision_reason_counts"]) == [f"reason_{index:02d}" for index in range(10)]
    assert len(parsed.data["action_type_counts"]) == 10
    assert len(parsed.data["decision_reason_counts"]) == 10
