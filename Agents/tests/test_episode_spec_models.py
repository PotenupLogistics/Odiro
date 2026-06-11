from __future__ import annotations

from app.models.episode_spec import EpisodeSpec


def test_episode_spec_defaults_match_ue_contract() -> None:
    episode = EpisodeSpec(
        scenario_id="scenario-1",
        run={"base_seed": 1001, "iteration_index": 0, "time_limit_s": 120},
        ground_model={
            "default_region_type": "walkable",
            "regions": [
                {
                    "region_id": "sidewalk_main",
                    "region_type": "walkable",
                    "shape": {
                        "type": "rectangle",
                        "center_m": [0.0, 0.0, 0.0],
                        "size_m": [10.0, 1.2],
                        "yaw_deg": 0.0,
                    },
                    "traversability_score": 1.0,
                }
            ],
        },
        paths=[],
        actors={
            "robot": {
                "instance_id": "bot-1",
                "asset_id": "delivery_bot",
                "spawn_only": False,
                "transform": {
                    "location_m": [0.0, 0.0, 0.0],
                    "rotation_deg": {"yaw": 0.0, "pitch": 0.0, "roll": 0.0},
                    "scale": [1.0, 1.0, 1.0],
                },
                "route": {"goal_m": [1.0, 0.0, 0.0], "auto_start": True},
            },
            "pedestrians": [],
            "static_obstacles": [],
        },
    )

    assert episode.schema == "episode_actor_spawn_mvp"
    assert episode.version == 1
    assert episode.map_id == "EpisodeSandbox"
    assert episode.units.distance == "m"
    assert episode.units.angle == "deg"

