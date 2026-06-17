검증 오류를 보고 Project Scenario JSON 객체를 수정한다.
원래 사용자 의도는 유지한다.
파일 저장 경로, sample/run/experiment 필드는 추가하지 않는다.
출력은 수정된 JSON 하나만 반환한다.
Markdown 코드블록을 사용하지 않는다.
schema/version/scenario_id/intent/corridor/obstacles/pedestrians/robot root를 유지한다.
legacy 입력이면 schema: scenario_template은 scenario로 바꾸고, template_id는 scenario_id로 이동한 뒤 제거한다.
corridor.axis.type은 "polyline"이어야 한다.
corridor.axis.points_m은 2개 이상의 [x, y] meter 좌표를 가져야 한다.
corridor.walkway_width_m은 {"min": number, "max": number} 형태여야 한다.
corridor.segments는 id/type/along_range_m을 가진 segment 목록이어야 한다.
obstacles는 object이며 min_clear_width_m과 placements를 포함해야 한다.
pedestrians는 path가 아니라 background와 encounters를 포함해야 한다.
보행자 한 명만 요청된 prompt라면 background.count는 {"min": 0, "max": 0}으로 고친다.
vulnerable persona가 낮은 personal_space_m을 갖고 있고 사용자가 근접 상황을 명시하지 않았다면 {"min": 0.6, "max": 0.9}로 고친다.
allow_blocking true는 통로 차단이나 통행 불가 의도가 명시된 경우에만 유지한다.
장애물 두 개 또는 2개 gate prompt인데 중복 gate pair가 있으면 left/right 한 쌍만 남긴다.
robot start/goal anchor만 요청된 prompt인데 pedestrians.encounters가 있으면 []로 제거하고 background.count는 {"min": 0, "max": 0}으로 둔다.
robot.start와 robot.goal은 필수이며 기본값은 {"type": "entry"}와 {"type": "exit"}다.
ground_model, static_obstacles, pedestrians.path, robot.start_area, robot.goal_area는 금지한다.
episode, source, params, semantic, validation, template_id는 금지한다.
seed, base_seed, episode_count, sample_count, experiment_id, run_id, template_path, scenario_path, sample_id, generated_count, scenario_sample은 금지한다.
