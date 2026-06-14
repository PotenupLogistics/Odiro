---
status: Draft
type: feature
specs:
  - Docs/JSON_Guide/ScenarioSetup_JSON_Guide.md
---

# Simulation Measurement Logging 계획

## 목표

PIE에서 실행 중인 시뮬레이션의 measurement를 JSONL 로그로 저장한다.

- 사용자가 PIE level 시작과 종료를 직접 제어
- logging system은 simulation 시작, 종료, fixed step, 종료 조건을 소유하지 않음
- 매 Tick마다 robot perception, robot action, moving actor transform 기록
- collision, near-miss, emergency stop 같은 event는 발생 시점에 즉시 기록
- 정적 환경 정보는 `Docs/JSON_Guide/ScenarioSetup_JSON_Guide.md`와 source JSON을 우선 참조하고 로그에는 중복 저장하지 않음
- AI 분석이 robot perception/action과 simulation truth를 구분해 읽을 수 있는 근거 생성

## 비목표

- fixed-step runtime 구현 또는 의존
- `StartTimeSeconds`, `EndTimeSeconds`, `CaptureHz` 설정
- deterministic replay, playback 입력, 재시뮬레이션 보장
- 모든 sensor raw data 저장
- SQLite, compression, seek index 도입
- LLM incident window export 구현
- runtime policy 자동 수정

## 용어

| 용어 | 의미 |
| --- | --- |
| Measurement log | PIE 실행 중 생성된 JSONL 로그 파일 |
| Tick record | 한 game Tick에서 기록한 `type:"tick"` record |
| Event record | runtime event 발생 시 기록한 `type:"event"` record |
| Actor table | `InstanceId`와 compact actor index를 매핑하는 header 정보 |
| Perception | robot이 해당 Tick에서 사용한 관측 snapshot |
| Action | robot이 해당 Tick에서 적용한 이동/정책 명령 snapshot |

## 데이터 카테고리

| Category | 의미 | 예시 | 기록 방식 |
| --- | --- | --- | --- |
| `perception` | robot이 알 수 있었던 관측값 | lidar hit, front obstacle, visible actor | 매 Tick |
| `action` | robot이 적용한 판단/명령 | target speed, steering, brake, stop/repath reason | 매 Tick |
| `truth` | simulation runtime state | robot/moving actor transform, velocity | 매 Tick |
| `event` | 사고나 이상 상황 | collision, near-miss, emergency stop | 발생 시 |
| `diagnostic` | logger 문제나 누락 | missing identity, writer failure | 발생 시 또는 footer |

규칙:
- `perception`에는 robot sensor나 policy 입력으로 실제 사용한 값만 기록
- `truth`는 사후 분석용이며 robot policy 입력으로 사용하지 않음
- event는 `tick` record 안에 중복 저장하지 않고 독립 `event` record로 저장
- 정적 환경 상세 geometry는 source scenario JSON으로 추적
- source JSON이 없는 PIE 수동 배치 run은 header actor table과 diagnostic으로만 보완

## 기록 대상

MVP 기준

| 대상 | 기록 방식 | 포함 payload |
| --- | --- | --- |
| Delivery Bot | 매 Tick | `perception`, `action`, `truth` |
| Moving pedestrian | 매 Tick | `truth` |
| Moving road vehicle / personal mobility | 매 Tick | `truth` |
| Static obstacle / floor / wall | header actor table만 기록 | identity, category, mobility |
| Evaluation event | 발생 시 | `event` |
| Logger diagnostic | 발생 시 또는 footer | `diagnostic` |

- `UEpisodePlaceableComponent`의 `InstanceId`, `AssetId`, `Category`, `MobilityMode`를 stable identity source로 사용
- actor `index`는 header `actors` 배열의 0-based contiguous index
- `MobilityMode == Moving` actor만 매 Tick transform 기록 대상
- `Static`, `Parked` actor는 source JSON과 actor table로 식별하고 매 Tick 반복 기록하지 않음
- Episode 실행 중 새로 발견된 actor는 MVP에서 actor table에 동적으로 추가하지 않고 diagnostic 기록

## 시간 처리

- logging은 fixed simulation clock을 만들지 않음
- Tick cadence는 PIE world의 variable Tick을 그대로 따른다
- `tickIndex`는 logger가 기록한 Tick record의 0-based sequential index
- `worldTimeSeconds`는 `UWorld::GetTimeSeconds()` 값이며 ordering과 사후 분석용 timestamp로만 사용
- `deltaSeconds`는 해당 Tick에서 logger가 받은 Tick delta
- start/end time 설정은 없음
- footer는 명시적 시간 구간 대신 record count와 close reason 중심으로 작성

프로젝트 정책 충돌:
- 이번 요구사항은 every Tick logging이므로 simulation authority가 아니라 measurement capture에 한해 Tick을 사용한다.

## Runtime 구조

- `UEpisodeMeasurementLogSubsystem`
  - PIE/game world에서 logger lifecycle 소유
  - log file open, Tick capture, event append, footer write, file close 담당
  - simulation 시작/종료를 제어하지 않고 world BeginPlay/EndPlay 또는 explicit Blueprint call에 반응

- `UEpisodeLogSubjectRegistry`
  - `UEpisodePlaceableComponent` 보유 actor 수집
  - duplicate `InstanceId`, missing identity, unsupported category diagnostic 생성
  - actor table과 moving actor 목록 생성

- `UEpisodeJsonlMeasurementWriter`
  - JSONL streaming writer
  - `header`, `tick`, `event`, `footer` record 직렬화
  - 외부 분석 도구가 읽을 public file contract 유지

- `UEpisodeRobotMeasurementAdapter`
  - `ADeliveryBot_ChaosActor`에서 latest lidar perception과 latest move command snapshot 조회
  - robot transform, velocity, front obstacle, action command를 Tick payload로 변환
  - robot 내부 상태 노출은 logging 전용 read-only API로 제한

- `UEpisodeEvaluationSubsystem` event bridge
  - `FEpisodeEvaluationEvent`가 추가되는 시점에 logger가 event record를 받을 수 있는 delegate 또는 callback 제공
  - 기존 `OnEpisodeEnded` footer성 결과와 별도로 event 발생 시점 기록 지원

## 작업 과정

1. PIE world가 시작되면 logger 활성화 여부를 확인
2. output directory와 JSONL file 생성
3. `UEpisodePlaceableComponent` 기준으로 actor table 생성
4. source JSON path, spec hash, map name, units, actor table을 포함한 header record 작성
5. 매 Tick마다 Delivery Bot adapter에서 perception/action/truth snapshot 수집
6. 매 Tick마다 moving actor transform과 velocity 수집
7. 같은 Tick의 measurement를 `type:"tick"` record 하나로 작성
8. EvaluationSubsystem 또는 collision/emergency stop 경로가 event를 emit하면 `type:"event"` record 즉시 작성
9. PIE 종료나 logger stop 시 footer record 작성 후 file handle close
10. writer failure, missing actor, invalid payload는 diagnostic으로 기록

## JSONL 계약

Record는 `type` discriminator를 사용한다.

```json
{"type":"header","version":1,"logId":"...","mapName":"ScenarioEditorMap","sourceJsonPath":"Json/Input/ScenarioSetupSample.json","specHash":"...","units":{"position":"cm","velocity":"cm/s","rotation":"quat_xyzw","axes":"UE_XForward_YRight_ZUp"},"categories":["perception","action","truth","event","diagnostic"],"actors":[{"index":0,"id":"robot_01","assetId":"delivery_bot","actorCategory":"DeliveryBot","mobility":"Moving"},{"index":1,"id":"ped_01","assetId":"adult_pedestrian","actorCategory":"Pedestrian","mobility":"Moving"}]}
{"type":"tick","tickIndex":0,"worldTimeSeconds":12.345,"deltaSeconds":0.0167,"robot":{"id":"robot_01","truth":{"p":[0,0,0],"q":[0,0,0,1],"v":[0,0,0]},"perception":{"lidar":{"hasFrontObject":true,"frontObjectId":"ped_01","frontDistanceM":2.4,"frontYawDegree":-5.0,"hitCount":8}},"action":{"targetSpeedKmh":4.0,"steering":0.1,"brake":0.0,"brakeApplied":false,"reason":"path_follow"}},"movingActors":[{"i":1,"p":[120,40,0],"q":[0,0,0,1],"v":[0,80,0]}]}
{"type":"event","eventIndex":0,"worldTimeSeconds":13.2,"kind":"pedestrian_near_miss","severity":"warning","subjectId":"robot_01","targetId":"ped_01","location":[130,42,0],"value":-3.0,"properties":{"minDistanceM":0.35,"durationSeconds":0.7}}
{"type":"footer","ticks":1800,"events":1,"closeReason":"pie_end","diagnostics":[]}
```

계약 규칙:
- 새 필드는 optional로 추가
- 기존 필드 타입 변경 금지
- 호환 불가 변경은 `version` 증가와 migration note 필요
- unknown `type`은 warning 후 skip 가능
- malformed record는 structured diagnostic으로 보고
- 외부 분석 도구는 JSONL을 untrusted input으로 검증
- input ScenarioSetup JSON은 snake_case, output JSONL은 camelCase 사용
- `p`는 UE cm 위치 `[x,y,z]`
- `q`는 quaternion `[x,y,z,w]`
- `v`는 UE cm/s 선속도 `[x,y,z]`
- `worldTimeSeconds`는 start/end control이 아니라 record timestamp

## 리스크

- every Tick JSONL은 파일 크기가 빠르게 증가할 수 있음
- variable Tick 기반이므로 서로 다른 PIE 실행의 Tick count와 timestamp가 다를 수 있음
- robot adapter가 내부 state를 과하게 노출하면 policy와 logging 경계가 흐려질 수 있음
- perception/action snapshot 시점이 실제 command 적용 시점과 어긋나면 분석이 왜곡될 수 있음
- source JSON 없이 수동 배치한 정적 환경은 로그만으로 geometry 복원이 어렵다
- event bridge가 footer 결과만 사용하면 event 발생 시점 기록을 놓칠 수 있음

## 검증 전략

- PIE 시작 후 `Saved/AnalysisLogs/MeasurementLog_<YYYYMMDD_HHMMSS>_<MapName>.jsonl` 생성 확인
- header에 source JSON path, spec hash, units, actor table 포함 확인
- fixed-step 필드 `simStep`, `fixedDeltaSeconds`, `captureHz`, `startTimeSeconds`, `endTimeSeconds` 미포함 확인
- Tick마다 `tickIndex`가 1씩 증가하고 `worldTimeSeconds`가 단조 증가하는지 확인
- Delivery Bot Tick record에 lidar perception과 latest move command 포함 확인
- moving pedestrian transform과 velocity가 매 Tick 기록되는지 확인
- static obstacle이 매 Tick 반복 기록되지 않는지 확인
- near-miss 발생 시 footer 전 독립 event record 생성 확인
- PIE 종료 시 footer 작성과 file close 확인
- 전체 JSONL을 line-by-line parse해 malformed record 없음 확인

## Tasks

### T01 Log contract foundation [x]

목표: fixed-step 없는 measurement JSONL public contract 정의

상세 동작:
- `FEpisodeMeasurementLogSettings`, actor info, tick, event, footer DTO 정의
- JSONL field 이름과 단위 규칙 문서 계약에 맞춤
- `StartTimeSeconds`, `EndTimeSeconds`, `CaptureHz`, stride validation 제거
- writer diagnostic schema는 `severity`, `code`, `message`, `worldTimeSeconds` 사용

검증:
- DTO 기본값이 PIE Tick logging 기준과 일치
- JSON serialization에서 fixed-step 필드가 생성되지 않음
- malformed DTO 입력이 structured diagnostic으로 변환

### T02 Subject registry [x]

목표: PIE world actor를 stable actor table로 변환

상세 동작:
- `UEpisodePlaceableComponent` 보유 actor 수집
- `InstanceId`, `AssetId`, `Category`, `MobilityMode` 누락 검사
- duplicate `InstanceId`를 error diagnostic으로 보고
- `MobilityMode == Moving` actor 목록 생성
- runtime 중 새 actor 발견 시 dynamic actor diagnostic 기록

검증:
- robot, pedestrian, static obstacle이 actor table에 들어감
- moving actor만 Tick transform 대상이 됨
- actor `index`가 0-based contiguous
- duplicate `InstanceId` fixture가 실패 diagnostic 생성

### T03 JSONL writer and subsystem [x]

목표: PIE lifecycle 동안 JSONL streaming write

상세 동작:
- `UEpisodeMeasurementLogSubsystem` 추가
- output 기본 위치는 `Saved/AnalysisLogs`
- 기본 파일명은 `MeasurementLog_<YYYYMMDD_HHMMSS>_<MapName>.jsonl`
- logger start 시 header record 작성
- Tick마다 tick record 작성
- event 발생 시 event record 작성
- EndPlay 또는 explicit stop 시 footer record 작성 후 close

검증:
- PIE 시작 후 파일 생성
- PIE 종료 후 footer 존재
- writer failure가 diagnostic으로 기록
- 파일 handle이 종료 후 닫힘

### T04 Robot perception/action adapter [x]

목표: Delivery Bot의 매 Tick perception/action snapshot 기록

상세 동작:
- `ADeliveryBot_ChaosActor` 또는 logging 전용 adapter에 read-only snapshot API 추가
- latest lidar scan에서 front object, distance, yaw, hit count 기록
- latest `FDeliveryBotMoveCommandInfo`에서 target speed, steering, brake, bBrake를 읽고 log action의 `brakeApplied`에 매핑
- stop, slowdown, repath, path_follow 같은 action reason 기록
- robot transform과 velocity를 truth payload로 기록

검증:
- obstacle 없음 Tick에서 perception payload가 빈 값 또는 null-friendly shape로 기록
- front obstacle 감지 Tick에서 actor id와 distance 기록
- stop command Tick에서 `brakeApplied=true`, `targetSpeedKmh=0` 기록
- repath 성공 Tick에서 action reason이 `repath` 계열로 기록

### T05 Moving actor tick capture [x]

목표: 이동 obstacle/actor transform을 매 Tick 기록

상세 동작:
- moving actor 목록에서 transform과 velocity 수집
- invalid actor는 해당 Tick에서 제외하고 diagnostic 기록
- static/parked actor는 Tick payload에서 제외
- actor id 대신 compact actor index `i` 사용 가능

검증:
- moving pedestrian 위치가 Tick마다 기록
- static obstacle이 Tick payload에 없음
- invalid moving actor가 crash 없이 diagnostic 생성

### T06 Event bridge [x]

목표: 사고와 이상 상황을 발생 시점 event record로 기록

상세 동작:
- `UEpisodeEvaluationSubsystem`이 event 추가 시 delegate 또는 callback emit
- pedestrian near-miss event를 JSONL event로 변환
- collision, emergency stop event entrypoint 추가
- footer의 final result에 의존하지 않고 발생 시점에 기록

검증:
- near-miss fixture에서 독립 event record 생성
- event `worldTimeSeconds`가 발생 시점과 일치
- footer 전에도 event record가 파일에 존재
- unknown event kind는 writer에서 warning diagnostic으로 처리

## Progress

- [x] fixed-step 의존 제거 방향 확정
- [x] PIE Tick measurement logging 목표 반영
- [x] JSONL 계약 개정
- [x] task breakdown 개정
- [x] T01 Log contract foundation
- [x] T02 Subject registry
- [x] T03 JSONL writer and subsystem
- [x] T04 Robot perception/action adapter
- [x] T05 Moving actor tick capture
- [x] T06 Event bridge

## Verification

- T01: `FEpisodeMeasurementLogSettings`, actor/tick/event/footer DTO와 JSONL serializer 추가
- T01: fixed-step 관련 시작/종료/cadence field 미포함 확인
- T01/T02: UE 5.7 `ProtoRobotSimEditor Win64 Development` build 성공
- T02: `UEpisodeLogSubjectRegistry`가 placeable actor table, moving actor 목록, duplicate/dynamic diagnostic 생성
- T02: `UEpisodeSimulationSubsystem` spawn metadata가 `UEpisodePlaceableComponent.MobilityMode`까지 설정
- T02: `ProtoRobotSim.MeasurementLog.SubjectRegistry.*` automation 3건 성공
- T03: `FEpisodeJsonlMeasurementWriter`와 `UEpisodeMeasurementLogSubsystem` 추가
- T03: header/tick/event/footer를 LF-only JSONL로 streaming write하고 stop/endplay에서 footer 후 close
- T03: `ProtoRobotSim.MeasurementLog.JsonlWriter.Streaming` automation 성공
- T04: `ADeliveryBot_ChaosActor` logging snapshot API와 `FEpisodeRobotMeasurementAdapter` 추가
- T04: latest lidar front object, latest move command, `brakeApplied`, action reason, robot truth를 Tick payload로 변환
- T04: Delivery Bot에 `UEpisodePlaceableComponent` 추가해 actor table identity에 포함
- T05: moving actor transform/velocity를 compact actor index로 Tick payload에 기록
- T05: static/parked actor와 robot 중복 truth를 Tick `movingActors`에서 제외
- T05: runtime dynamic subject와 invalid moving actor diagnostic 기록
- T06: `UEpisodeEvaluationSubsystem.OnEvaluationEvent` delegate 추가
- T06: near-miss/timeout evaluation event를 발생 시점 JSONL event로 즉시 기록
- T06: collision/emergency stop event entrypoint 추가
- T06: footer 작성 전 event delegate unbind로 footer 이후 event append 방지
- MeasurementLog 전체 automation 7건 성공, report `Saved/Automation/MeasurementLogT06Final`

## 인계 메모

- 구현은 PIE Tick logging 기준으로 시작
- simulation clock, episode lifecycle, end condition은 logger 범위 밖
- source JSON이 있는 run은 static environment를 JSON에서 추적
- source JSON이 없는 수동 PIE run은 static geometry 재구성을 MVP 범위 밖으로 둠
- file output 기본 위치는 `Saved/AnalysisLogs`
- 기본 파일명은 `MeasurementLog_<YYYYMMDD_HHMMSS>_<MapName>.jsonl`
