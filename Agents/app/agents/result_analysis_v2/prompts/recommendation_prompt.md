제공된 analysis context를 바탕으로 결과 분석 recommendation JSON을 생성하십시오.

- system prompt의 근거 제한, 금지사항, public 응답 안전 규칙을 우선합니다.
- recommendations 배열만 포함한 JSON object를 출력합니다.
- 입력에 포함된 patterns, metrics, warnings, refs로 뒷받침되는 항목만 추천합니다.
- evidence는 실제 제공된 episode 참조만 사용합니다.
- target은 "policy" 또는 "environment"만 사용합니다.
- 근거가 부족하거나 원인 분류가 none이면 recommendations는 빈 배열로 둡니다.
- 각 recommendation에는 id를 `REC-LLM-001`, `REC-LLM-002`처럼 안정적으로 부여합니다.
- proposed_change는 문자열이 아니라 object여야 합니다.
- policy 추천의 proposed_change는 `{ "type": "policy_parameter_adjustment", "content": { ... } }` 형식이며 content에는 `followSpeedKmh_max`, `maxPathErrorM_max`, `lookAheadDistanceM_max`, `pathSmoothingDistanceM_max`, `maxSteeringDelta_max` 숫자 값을 모두 포함합니다.
- policy 숫자 값은 각각 `followSpeedKmh_max <= 3.5`, `maxPathErrorM_max <= 0.8`, `lookAheadDistanceM_max <= 1.0`, `pathSmoothingDistanceM_max <= 0.25`, `maxSteeringDelta_max <= 0.06` 범위를 넘지 않습니다.
- environment 추천의 proposed_change는 `{ "type": "environment_scenario_adjustment", "content": { ... } }` 형식이며 content에는 `increase_min_clear_width_m`, `disable_allow_blocking`, `increase_walkway_width_m`, `reason`을 모두 포함합니다.
