# Environment Parameter Spec

상태: legacy WorldConfig/EpisodeSpec parameter note.

- 최종 user project scenario 계약 아님
- 현재 scenario 기준: `contracts/specs/user-project-data.md`
- 이 문서는 기존 numeric parameter 출처 확인용

## 1. 목적

환경 시나리오는 low / middle / high 같은 모호한 label이 아니라 구체적인 수치 값으로 정의합니다.

## 2. 원칙

* Do not use low / middle / high as JSON values.
* 자연어 설명에는 narrow, crowded, sparse, steep 같은 label을 사용할 수 있지만, 생성된 contract 값은 반드시 numeric value여야 합니다.
* 실험 결과 비교를 위해 같은 label은 같은 scenario set 안에서 반드시 same label must map to the same numeric value 원칙을 따릅니다.
* seed 기반 deterministic sampling은 explicit numeric candidate list에서만 값을 선택합니다.
* sampler는 WorldConfig 또는 EpisodeSpec JSON을 만들지 않고 numeric parameter set만 생성합니다.
* AI 내부 계약은 schema가 cm를 쓰는 필드에서는 cm를 유지합니다.
* UE `EpisodeSpec`은 geometry에 meter를 사용하므로 adapter boundary에서 cm 값을 m로 변환합니다.
* 이 문서는 후보 값을 정의할 뿐이며 sample JSON 또는 fixture 파일을 생성하지 않습니다.

## 3. 정량 파라미터 목록

| parameterName | unit | allowedValues | 의미 | usedIn | notes |
| --- | --- | --- | --- | --- | --- |
| `sidewalkWidthCm` | cm | `100`, `120`, `150`, `200` | 보도 폭 | `WorldConfig.map.sidewalkWidthCm`, `EpisodeSpec.ground_model.regions[].shape.size_m` | narrow sidewalk는 설명상 `100` 또는 `120`으로 표현할 수 있습니다. |
| `pedestrianCount` | count | `1`, `3`, `5` | 보행자 수 | `WorldConfig.pedestrians[]`, `EpisodeSpec.actors.pedestrians[]` | crowded path는 설명상 `5`로 표현할 수 있습니다. |
| `pedestrianSpeedMps` | m/s | `0.8`, `1.2`, `1.6` | 보행자 이동 속도 | `EpisodeSpec.actors.pedestrians[].movement.speed_mps` | 현재 `WorldConfig`는 `speedKmh`를 사용하므로 필요한 경우 adapter 또는 sampler 단계에서만 변환합니다. |
| `obstacleBlockingRatio` | ratio | `0.3`, `0.6`, `0.9` | 장애물이 경로를 막는 정도 | `WorldConfig.obstacles[].blockingRatio`, `EpisodeSpec.actors.static_obstacles[].properties.blocking_ratio` | `0.9`는 강한 경로 차단을 의미합니다. |
| `obstacleLateralOffsetM` | m | `-0.4`, `0.0`, `0.4` | 경로 중심선 기준 좌우 배치 offset | `EpisodeSpec.actors.static_obstacles[].transform.location_m` | 부호 방향은 route-local 좌표 기준으로 정의합니다. |
| `crossingAngleDeg` | degree | `45`, `90` | 보행자 횡단 각도 | `EpisodeSpec.paths[].points_m` | `90`은 직각 횡단입니다. |
| `robotSpeedKmh` | km/h | `3`, `5`, `8` | 로봇 목표 주행 속도 | `WorldConfig.robot`, future policy/run config | UE component가 m/s를 요구할 때만 변환합니다. |
| `slopeDegree` | degree | `0`, `3`, `5` | 보도 경사 | `WorldConfig.map.slopeDegree`, `EpisodeSpec.ground_model.regions[].slope_deg` | `5`는 현재 terrain risk policy evidence와 연결됩니다. |
| `curbHeightCm` | cm | `0`, `3`, `5` | 턱 높이 | `WorldConfig.terrain`, future `EpisodeSpec` terrain properties | UE geometry가 m를 요구하면 m로 변환합니다. |
| `timeLimitSec` | sec | `30`, `60`, `90` | episode 실행 제한 시간 | `WorldConfig.runtime.maxDurationSec`, `EpisodeSpec.run.time_limit_s` | simulation timeout 값입니다. |

## 4. Label 사용 규칙

label은 문서 설명, prompt, 사람이 읽는 해설에서만 사용할 수 있습니다. contract 값은 숫자여야 합니다.

예:

* narrow sidewalk = `sidewalkWidthCm=100 or 120`
* crowded path = `pedestrianCount=5`
* strong path blockage = `obstacleBlockingRatio=0.9`
* slow pedestrian = `pedestrianSpeedMps=0.8`

JSON contract에 `"low"`, `"middle"`, `"high"`를 값으로 저장하지 않습니다.

잘못된 표현:

* `pedestrianDensity: "high"`
* `sidewalkWidth: "narrow"`

올바른 표현:

* `pedestrianCount: 5`
* `sidewalkWidthCm: 120`

## 5. EpisodeSpec 변환 기준

* `sidewalkWidthCm=120` -> `EpisodeSpec` `shape.size_m` y 값 `1.2`
* `map.lengthCm=1200` -> `EpisodeSpec` `shape.size_m` x 값 `12.0`
* `robotSpeedKmh` -> UE movement field가 m/s를 요구할 때만 m/s로 변환
* `pedestrianSpeedMps` -> `EpisodeSpec` `movement.speed_mps`
* `obstacleBlockingRatio` -> `EpisodeSpec` `properties.blocking_ratio`
* `timeLimitSec` -> `EpisodeSpec` `run.time_limit_s`

## 6. 후속 단계

* Seed 기반 deterministic sampler를 WorldConfig generation constraints와 연결 완료
* DOE / Latin Hypercube / Sobol sampling
* Scenario matrix generation
* Repeated simulation run configuration
* UE controlled scenario batch test

## 7. Generation constraints 연결

`generationRequest.constraints.environmentSampling`으로 seed와 scenarioType을 전달하면 위 numeric 후보값 중 하나가 deterministic하게 선택됩니다.
선택된 값은 `Numeric Environment Constraints`로 prompt에 들어가며, deterministic post-processing에서도 LLM 출력보다 우선합니다.
low/middle/high는 설명 라벨로만 사용할 수 있고 JSON 값으로는 사용하지 않습니다.
