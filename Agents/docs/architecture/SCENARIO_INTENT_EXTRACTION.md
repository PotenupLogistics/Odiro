# Scenario Intent Extraction

## 1. 목적 (Purpose)

Scenario intent extraction은 한국어 자연어 World Config prompt를 RAG retrieval과 prompt generation 전에 명시적인 scenario hint로 변환한다.

## 2. Extracted Intent

extractor는 다음 intent를 식별한다.

- narrow sidewalk map hint
- Kickboard 또는 obstacle hint
- generic/static obstacle hint
- 명시적 obstacle position hint
- 명시적 obstacle `blockingRatio` hint
- 명시적 sidewalk width hint
- 명시적 no-pedestrian hint
- path blocking hint
- pedestrian hint
- pedestrian crossing hint
- terrain risk hint
- operator/control hint

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

intent category, action, policy parameter는 retrieval query와 fallback filter로 사용한다. 단순 keyword search만으로는 한국어 prompt가 policy context를 찾지 못할 수 있으므로, intent 기반 query expansion으로 검색 누락을 줄인다.

generic obstacle prompt는 `장애물 감지`, `장애물 회피`, `경로 차단`, `perception_requirement` 같은 fallback retrieval query도 추가한다. retrieval 결과가 0개일 수는 있지만, scenario requirement는 RAG 결과와 별개로 intent에서 생성한다.

## 5. Scope

이 계층은 rule-based extraction layer다. 외부 LLM을 호출하지 않고 JSON Schema를 수정하지 않는다.

## Route-relative placement

* "경로 중앙", "경로 중간", "로봇 경로 중앙", "중앙 근처", `path center`, `middle of the path` 표현은 `obstaclePlacementHint=route_midpoint`로 추출한다.
* 명시 좌표가 있으면 `obstaclePositionHint`가 우선하며 route midpoint requirement는 만들지 않는다.
* 명시 좌표가 없으면 `obstacle_route_midpoint` requirement를 생성하고 `obstacles[].position`은 robot.spawn/robot.goal midpoint를 기대한다.
