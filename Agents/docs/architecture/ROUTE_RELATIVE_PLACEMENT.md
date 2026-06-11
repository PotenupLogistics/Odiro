# Route-relative Placement

## 1. 목적

"경로 중앙", "경로 중간" 같은 상대 표현을 LLM 추론이 아니라 deterministic geometry rule로 처리한다.

## 2. 규칙

* robot.spawn과 robot.goal이 있으면 midpoint를 계산한다.
* 명시 좌표가 있으면 명시 좌표가 우선한다.
* 명시 좌표가 없고 route_midpoint intent가 있으면 midpoint를 사용한다.
* LLM이 obstacle을 goal 근처에 배치해도 post-processing에서 midpoint로 보정한다.
* WorldConfig는 cm 기준, EpisodeSpec은 m 기준이다.

## 3. 예시

* spawn=(0,0,0), goal=(800,0,0)
* route midpoint=(400,0,0)
* EpisodeSpec location_m=(4.0,0,0)

## 4. 검증

WorldConfig scenario reflection은 `obstacles[].position`이 midpoint 50cm 이내인지 확인한다.
EpisodeSpec scenario reflection은 `actors.static_obstacles[].transform.location_m`이 midpoint 0.5m 이내인지 확인한다.
요구사항이 있는데 midpoint에서 벗어나면 reflection은 실패하고 UE compiler readiness도 false로 처리한다.
Swagger live 응답이 계속 goal 위치를 반환하면 endpoint 로직보다 stale FastAPI 서버 가능성을 먼저 점검하고 최신 코드로 재시작한다.
