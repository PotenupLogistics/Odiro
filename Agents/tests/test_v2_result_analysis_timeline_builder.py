from __future__ import annotations

from app.agents.result_analysis_v2.representative_selector import RepresentativeFailedEpisodeSelectorV2
from app.agents.result_analysis_v2.timeline_builder import EventTimelineBuilderV2


def test_event_type_field_builds_timeline() -> None:
    builder = EventTimelineBuilderV2()

    timeline = builder.build_single_episode_timeline(
        experiment_id="Experiment1",
        run_id="000001",
        episode_id="000003",
        events=[
            {
                "time_s": 3.2,
                "event_type": "obstacle_detected",
                "summary": "전방 장애물 감지",
                "_source_path": "events.jsonl",
            }
        ],
    )

    assert timeline["timeline"][0]["event_type"] == "obstacle_detected"
    assert timeline["timeline"][0]["time_s"] == 3.2
    assert timeline["timeline"][0]["source"] == "events.jsonl"


def test_type_field_is_normalized_to_event_type() -> None:
    builder = EventTimelineBuilderV2()

    event = builder.normalize_event_record({"type": "near_miss", "timestamp_s": 1.5}, "events.jsonl")

    assert event is not None
    assert event["event_type"] == "near_miss"
    assert event["time_s"] == 1.5


def test_missing_time_s_does_not_fail() -> None:
    builder = EventTimelineBuilderV2()

    timeline = builder.build_single_episode_timeline(
        experiment_id="Experiment1",
        run_id="000001",
        episode_id="000004",
        events=[{"event_type": "collision"}],
    )

    assert timeline["timeline"][0]["event_type"] == "collision"
    assert timeline["timeline"][0]["time_s"] is None


def test_select_key_events_filters_non_key_events() -> None:
    builder = EventTimelineBuilderV2()

    selected = builder.select_key_events(
        [
            {"event_type": "debug_tick"},
            {"event_type": "collision"},
            {"event_type": "goal_reached"},
        ]
    )

    assert [event["event_type"] for event in selected] == ["collision", "goal_reached"]


def test_blocked_region_violation_timeline_included() -> None:
    builder = EventTimelineBuilderV2()

    timelines = builder.build_episode_timelines(
        parsed_artifacts={
            "episodes": [
                {
                    "experiment_id": "Experiment1",
                    "run_id": "000001",
                    "episode_id": "000003",
                    "events": [{"type": "blocked_region_violation", "time": 6.4}],
                    "source_path": "events.jsonl",
                }
            ]
        },
        episode_metrics=[
            {
                "experiment_id": "Experiment1",
                "run_id": "000001",
                "episode_id": "000003",
            }
        ],
    )

    assert timelines[0]["timeline"][0]["event_type"] == "blocked_region_violation"
    assert "blocked region violation" in timelines[0]["timeline_summary"]


def test_event_specific_source_path_is_preserved() -> None:
    builder = EventTimelineBuilderV2()

    timelines = builder.build_episode_timelines(
        parsed_artifacts={
            "episodes": [
                {
                    "experiment_id": "Experiment1",
                    "run_id": "000001",
                    "episode_id": "000003",
                    "events": [{"type": "collision", "_source_path": "events.jsonl"}],
                    "source_path": "result.json",
                }
            ]
        },
        episode_metrics=[
            {
                "experiment_id": "Experiment1",
                "run_id": "000001",
                "episode_id": "000003",
            }
        ],
    )

    assert timelines[0]["timeline"][0]["source"] == "events.jsonl"


def test_representative_selector_prioritizes_collision_blocked_near_miss() -> None:
    selector = RepresentativeFailedEpisodeSelectorV2()

    selected = selector.select(
        episode_metrics=[
            {"experiment_id": "E", "run_id": "R", "episode_id": "near", "near_miss_count": 1},
            {
                "experiment_id": "E",
                "run_id": "R",
                "episode_id": "blocked",
                "blocked_region_violation_count": 1,
            },
            {"experiment_id": "E", "run_id": "R", "episode_id": "collision", "collision_count": 1},
        ],
        episode_timelines=[],
        max_episodes=3,
    )

    assert [episode["episode_id"] for episode in selected] == ["collision", "blocked", "near"]
