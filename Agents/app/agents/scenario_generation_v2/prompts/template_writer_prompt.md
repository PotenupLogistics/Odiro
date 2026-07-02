출력은 JSON만 허용한다.
Markdown 코드블록을 사용하지 않는다.
Project Scenario는 <UserProject>/scenario.json에 저장될 수 있는 원본 scenario다.
Project Scenario는 범위값을 포함할 수 있다.
범위값은 {"min": ..., "max": ...} 형태를 사용한다.
수치 필드는 고정 숫자 또는 {"min": ..., "max": ...} 범위값을 사용할 수 있다.
좌표 단위는 meter, 각도 단위는 degree를 사용한다.

필수 root는 schema, version, scenario_id, intent, corridor, robot이다.
obstacles는 정적 장애물이 필요한 prompt에서만 포함한다.
정적 장애물이 없거나 사용자가 장애물이 없다고 요청하면 obstacles root를 생략한다.
pedestrians root는 만들지 않는다.

아래 최소 구조를 기준으로 채운다.

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
    "building_side": [{"surface": "building", "width_m": 0.3}],
    "curb_side": [{"surface": "road", "width_m": 4.0}],
    "segments": [
      {"id": "approach", "type": "straight", "along_range_m": [0.0, 5.0]},
      {"id": "conflict", "type": "straight", "along_range_m": [5.0, 11.0]},
      {"id": "exit", "type": "straight", "along_range_m": [11.0, 18.0]}
    ]
  },
  "robot": {
    "start": {"type": "corridor_pose", "segment": "approach", "along_m": 1.0, "offset_m": 0.0, "lane": "walkway", "heading": "forward"},
    "goal": {"type": "corridor_pose", "segment": "exit", "along_m": 16.0, "offset_m": 0.0, "lane": "walkway", "heading": "forward"}
  }
}

정적 장애물이 필요한 prompt이면 obstacles.placements에 다음 catalog 형태를 사용한다.
SPEC_CONTEXT의 Environment Catalog에 있는 Prop Bounding Boxes 표를 참고해 prop별 크기에 맞게 at.offset_m을 산정한다.
bbox_m, footprint_m, catalog 정의, bounding box 표 자체는 scenario.json에 복사하지 않는다.
큰 obstacle은 좁은 보도 중앙에 무심코 배치하지 않는다.
사용자가 path blocking, gate, 통행 불가, blocked path처럼 경로 차단 의도를 명시한 경우에만 road_barrier 계열을 barrier/gate 배치로 사용한다.

{
  "obstacles": {
    "min_clear_width_m": 0.9,
    "placements": [
      {
        "kind": "fixed",
        "id": "center_obstacle",
        "prop": "obstacle.road_cone_01",
        "at": {
          "segment": "conflict",
          "along_m": {"min": 6.5, "max": 8.5},
          "offset_m": {"min": -0.2, "max": 0.2},
          "lane": "center"
        },
        "yaw_deg": 0
      }
    ]
  }
}

placement kind는 fixed만 사용한다.
pattern placement와 scatter placement는 만들지 않는다.
사용자가 장애물 개수를 명시하면 그 개수를 초과하지 않는다.
"두 개", "2개", "two objects", "two panels", "two cones"라고 하면 fixed placement 2개로 표현한다.
두 쌍, 네 개, 여러 쌍이라고 말하지 않은 이상 gate pair를 중복 생성하지 않는다.
의도적으로 최소 통로 폭을 깨야 하는 placement에는 optional boolean allow_blocking을 사용할 수 있다.
allow_blocking은 사용자가 통로가 막힌 상황, 지나갈 수 없을 정도로 좁은 상황, 일부러 길을 막는 장애물, 통행 불가, blocked path, intentionally blocking을 명시한 경우에만 true로 둔다.
일반적인 게이트형 장애물이나 좁지만 통과 가능한 안내판 배치는 allow_blocking을 생략하거나 false로 둔다.

corridor.building_side[].surface는 "building"을 사용한다.
corridor.curb_side[].surface는 "road"를 사용한다.
corridor.segments[].type은 "straight"만 사용한다.
좁아지는 구간, 공사 구간, conflict 구간 같은 의미는 segment id와 along_range_m으로 표현한다.
문자열 choices는 corridor.segments[].replaced_by에서만 사용한다.

robot은 기본적으로 corridor_pose start/goal을 우선 사용한다.
구체 anchor가 필요할 때만 type="corridor_pose"와 segment, along_m, offset_m, lane, heading을 사용한다.
사용자가 robot start/goal anchor만 요청하고 장애물/위험요소를 요청하지 않으면 obstacles를 생략한다.
abstract robot anchor와 concrete corridor pose field를 섞지 않는다.
robot.start.type 또는 robot.goal.type이 "entry" 또는 "exit"이면 {"type": "entry"} 또는 {"type": "exit"}만 출력한다.
segment, along_m, offset_m, lane, heading이 필요하면 type을 반드시 "corridor_pose"로 설정한다.

center_xy_m, center, radius_m, r_m, world_xy, actors.robot.xy_m, route.goal_xy_m은 만들지 않는다.
ground_model, static_obstacles, pedestrians, template_id, template_path는 만들지 않는다.
안내판 계열 prop이 catalog에 없으면 temporary_sign_01, construction_sign_01, guide_board_01 같은 새 prop id를 만들지 말고 obstacle.road_cone_01처럼 허용된 prop을 사용한다.
episode_scenario, episode, source, params, semantic, validation은 만들지 않는다.
robot.start_area, robot.goal_area, seed, base_seed, episode_count, sample_count, experiment_id, run_id, scenario_path, sample_id, generated_count, scenario_sample은 만들지 않는다.
지원하지 않는 catalog surface와 prop을 임의로 만들지 않는다.
