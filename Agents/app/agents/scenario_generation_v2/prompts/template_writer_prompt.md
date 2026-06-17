출력은 JSON만 허용한다.
Markdown 코드블록을 사용하지 않는다.
Project Scenario는 schema/version/scenario_id/intent/corridor/obstacles/pedestrians/robot root를 포함한다.
Project Scenario는 <UserProject>/scenario.json에 저장될 수 있는 원본 scenario다.
Project Scenario는 범위값을 포함할 수 있다.
범위값은 {"min": ..., "max": ...} 형태를 사용한다.
수치 필드는 고정 숫자 또는 {"min": ..., "max": ...} 범위값을 사용할 수 있다.
좌표 단위는 meter, 각도 단위는 degree를 사용한다.
아래 최소 구조를 반드시 채운다.

{
  "schema": "scenario",
  "version": 1,
  "scenario_id": "snake_case_id",
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

placement kind는 fixed, pattern, scatter를 사용할 수 있다. 기본 생성은 fixed를 우선 사용한다.
pattern placement는 pattern, prop, at, count, spacing_m 또는 gap_width_m을 사용한다.
scatter placement는 zone.segments, zone.lanes, density_per_10m, palette.categories, palette.classes를 사용한다.
사용자가 장애물 개수를 명시하면 그 개수를 초과하지 않는다.
"두 개", "2개", "two objects", "two panels", "two cones"라고 하면 fixed placement 2개 또는 pattern count 2로 표현한다.
두 쌍, 네 개, 여러 쌍이라고 말하지 않은 이상 gate pair를 중복 생성하지 않는다.
의도적으로 최소 통로 폭을 깨야 하는 placement에는 optional boolean allow_blocking을 사용할 수 있다.
allow_blocking은 사용자가 통로가 막힌 상황, 지나갈 수 없을 정도로 좁은 상황, 일부러 길을 막는 장애물, 통행 불가, blocked path, intentionally blocking을 명시한 경우에만 true로 둔다.
일반적인 게이트형 장애물이나 좁지만 통과 가능한 안내판 배치는 allow_blocking을 생략하거나 false로 둔다.
배경 보행자 등장 구간을 제한해야 하면 pedestrians.background.spawn_zone.segments에 corridor segment id 목록을 사용한다.
보행자 한 명, 보행자 1명, 한 명이, single pedestrian, one pedestrian처럼 핵심 보행자가 1명으로 제한된 prompt에서는 encounter는 유지하고 background.count를 {"min": 0, "max": 0}으로 둔다.
보행자 이동 경로를 직접 좌표로 만들지 말고 pedestrians.encounters만 사용한다.
대향 보행자는 type="oncoming_pass", 잘 비켜주지 않는 성향은 persona="assertive"를 우선 사용한다.
옆에서 가로지르는 보행자는 type="cross_path"를 사용한다.
persona가 "vulnerable"이고 사용자가 좁은 personal space나 근접 상황을 명시하지 않았다면 personal_space_m 기본값은 {"min": 0.6, "max": 0.9}를 사용한다.
overrides는 cooperation, evasiveness, personal_space_m, awareness_horizon_s, max_yield_wait_s, sidestep_distance_m만 사용하고 각 값은 숫자 또는 min/max 범위값을 사용한다.
문자열 choices는 corridor.segments[].replaced_by에서만 사용한다.
robot은 기본적으로 "start": {"type": "entry"}, "goal": {"type": "exit"}만 사용한다.
구체 anchor가 필요할 때만 type="corridor_pose"와 segment, along_m, offset_m, lane, heading을 사용한다.
사용자가 robot start/goal anchor만 요청하고 장애물/보행자/위험요소를 요청하지 않으면 obstacles.placements와 pedestrians.encounters는 빈 배열로 최소화한다.
이 경우 기본 위험 encounter를 임의로 추가하지 않는다.
abstract robot anchor와 concrete corridor pose field를 섞지 않는다.
robot.start.type 또는 robot.goal.type이 "entry" 또는 "exit"이면 {"type": "entry"} 또는 {"type": "exit"}만 출력한다.
segment, along_m, offset_m, lane, heading이 필요하면 type을 반드시 "corridor_pose"로 설정한다.
center_xy_m, center, radius_m, r_m, world_xy, actors.robot.xy_m, route.goal_xy_m은 만들지 않는다.
ground_model, static_obstacles, pedestrians.path, template_id, template_path는 만들지 않는다.
안내판 계열 prop이 catalog에 없으면 temporary_sign_01, construction_sign_01, guide_board_01 같은 새 prop id를 만들지 말고 traffic_cone_01처럼 허용된 prop을 사용한다.
episode_scenario, episode, source, params, semantic, validation은 만들지 않는다.
robot.start_area, robot.goal_area, seed, base_seed, episode_count, sample_count, experiment_id, run_id, scenario_path, sample_id, generated_count, scenario_sample은 만들지 않는다.
지원하지 않는 catalog surface, prop, encounter type, persona를 임의로 만들지 않는다.
