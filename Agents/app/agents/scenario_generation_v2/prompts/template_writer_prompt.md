출력은 JSON만 허용한다.
Markdown 코드블록을 사용하지 않는다.
scenario_template은 schema/version/template_id/intent/corridor/obstacles/pedestrians/robot root를 포함한다.
scenario_template은 범위값을 포함할 수 있다.
범위값은 {"min": ..., "max": ...} 형태를 사용한다.
좌표 단위는 meter, 각도 단위는 degree를 사용한다.
아래 최소 구조를 반드시 채운다.

{
  "schema": "scenario_template",
  "version": 1,
  "template_id": "snake_case_id",
  "intent": "검증하려는 상황 또는 안전 가설",
  "corridor": {
    "axis": {
      "type": "polyline",
      "points_m": [[0.0, 0.0], [18.0, 0.0]]
    },
    "walkway_width_m": {"min": 1.4, "max": 1.8},
    "building_side": [{"surface": "wall", "width_m": 0.3}],
    "curb_side": [{"surface": "road", "width_m": 4.0}],
    "segments": [
      {"id": "approach", "type": "straight", "along_range_m": [0.0, 5.0]},
      {"id": "conflict", "type": "narrowing", "along_range_m": [5.0, 11.0]},
      {"id": "exit", "type": "straight", "along_range_m": [11.0, 18.0]}
    ]
  },
  "obstacles": {
    "min_clear_width_m": 0.9,
    "placements": []
  },
  "pedestrians": {
    "background": {
      "count": {"min": 0, "max": 1},
      "speed_mps": {"min": 0.8, "max": 1.2}
    },
    "encounters": [
      {
        "id": "main_conflict",
        "type": "oncoming_pass",
        "at": "conflict",
        "meet_offset_m": 0.0,
        "persona": "assertive",
        "overrides": {
          "cooperation": {"min": 0.15, "max": 0.4}
        }
      }
    ]
  },
  "robot": {
    "start": {"type": "entry"},
    "goal": {"type": "exit"}
  }
}

정적 장애물이 필요한 prompt이면 obstacles.placements에 다음 catalog 형태를 사용한다.

{
  "kind": "fixed",
  "id": "center_obstacle",
  "prop": "traffic_cone_01",
  "at": {
    "segment": "conflict",
    "along_m": {"min": 6.5, "max": 8.5},
    "offset_m": {"min": -0.2, "max": 0.2},
    "lane": "center"
  },
  "yaw_deg": 0
}

보행자 이동 경로를 직접 좌표로 만들지 말고 pedestrians.encounters만 사용한다.
대향 보행자는 type="oncoming_pass", 잘 비켜주지 않는 성향은 persona="assertive"를 우선 사용한다.
옆에서 가로지르는 보행자는 type="cross_path"를 사용한다.
robot은 기본적으로 "start": {"type": "entry"}, "goal": {"type": "exit"}만 사용한다.
ground_model, static_obstacles, pedestrians.path, scenario_id, template_path는 만들지 않는다.
robot.start_area, robot.goal_area, seed, base_seed, sample_count, experiment_id, run_id, scenario_path, sample_id, generated_count, scenario_sample은 만들지 않는다.
지원하지 않는 catalog surface, prop, encounter type, persona를 임의로 만들지 않는다.
