# Scenario Intent Extraction

## 1. Purpose

Scenario intent extraction converts a Korean natural-language World Config prompt into explicit scenario hints before RAG retrieval and prompt generation.

## 2. Extracted Intent

The extractor identifies:

- narrow sidewalk map hints
- Kickboard or obstacle hints
- generic/static obstacle hints
- explicit obstacle position hints
- explicit obstacle `blockingRatio` hints
- explicit sidewalk width hints
- explicit no-pedestrian hints
- path blocking hints
- pedestrian hints
- pedestrian crossing hints
- terrain risk hints
- operator/control hints

## 3. Keyword Mapping

- `좁은 보도`, `보도 폭`, `좁은 길` -> `narrow_sidewalk`, `sidewalk_operation`, `speed_policy`
- `킥보드`, `공유 킥보드`, `전동킥보드` -> `Kickboard`, `perception_requirement`
- `장애물`, `정적 장애물`, `로봇 경로 중앙`, `경로 중앙`, `막고`, `막힘`, `경로를 막`, `차단`, `blockingRatio` -> `Obstacle`, path blocking, `LocalAvoidance`, `ReplanPath`, `Stop`
- `x=400, y=0, z=0` near obstacle wording -> `obstaclePositionHint`
- `blockingRatio 0.6` -> `obstacleBlockingRatio=0.6`
- `보도 폭은 120cm` -> `sidewalkWidthCm=120`
- `보행자는 없는`, `보행자 없음` -> `explicitNoPedestrian=true`
- `보행자`, `사람` -> `Pedestrian`, `sidewalk_operation` unless explicit no-pedestrian intent is present
- `횡단`, `건너`, `가로질러` -> pedestrian crossing, `YieldWait`, `Stop`
- `경사`, `턱`, `넘어짐`, `기울어짐` -> `terrain_or_dynamic_safety`
- `관제`, `원격`, `수동` -> `operator_control`

## 4. RAG Connection

Intent categories, actions, and policy parameters are used as retrieval queries and fallback filters. This prevents Korean prompts from returning zero policy contexts when plain keyword search is insufficient.

Generic obstacle prompts also add fallback retrieval queries such as `장애물 감지`, `장애물 회피`, `경로 차단`, and `perception_requirement`. Retrieval may still return zero chunks, but scenario requirements are generated from intent regardless of RAG results.

## 5. Scope

This is a rule-based extraction layer. It does not call an external LLM and does not modify JSON Schema.

## Route-relative placement

* "경로 중앙", "경로 중간", "로봇 경로 중앙", "중앙 근처", `path center`, `middle of the path` 표현은 `obstaclePlacementHint=route_midpoint`로 추출한다.
* 명시 좌표가 있으면 `obstaclePositionHint`가 우선하며 route midpoint requirement는 만들지 않는다.
* 명시 좌표가 없으면 `obstacle_route_midpoint` requirement를 생성하고 `obstacles[].position`은 robot.spawn/robot.goal midpoint를 기대한다.
