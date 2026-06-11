# Reeds-Shepp Analytic Expansion 구현 및 후속 작업

## 목적

Hybrid A*가 목표 근처에서 grid/motion primitive 확장만 반복하지 않고, 목표 pose까지 직접 연결 가능한 bounded-curvature path를 생성해보는 analytic expansion을 추가한다.

이번 Python 구현은 Reeds-Shepp 계열 shot을 Hybrid A* 내부에 붙이고, 생성된 shot이 grid/footprint 충돌 검사를 통과할 때만 최종 경로로 채택한다.

## Python 구현 완료 항목

### Reeds-Shepp shot 모듈

파일:

- `Tools/PythonPolicyServer/deliverybot_policy/reeds_shepp.py`

구현 내용:

- `ReedsSheppSegment`
- `ReedsSheppPose`
- `ReedsSheppPath`
- `find_reeds_shepp_path(...)`
- forward/reverse 방향 후보 생성
- CSC 계열 후보 생성:
  - `LSL`
  - `RSR`
  - `LSR`
  - `RSL`
- reverse 후보는 실제 후진 제어에 맞게 turn 방향을 반전한다.
- sample step 단위로 pose를 생성한다.
- endpoint position/yaw 오차를 검사한다.

현재 구현 범위는 Hybrid A* analytic expansion에 필요한 실용 shot이다. Reeds-Shepp 전체 family 48개를 모두 exhaustive하게 구현한 것은 아니다. 다만 전진/후진 bounded-curvature CSC shot을 제공하므로, 목표 근처에서 불필요한 노드 확장을 줄이는 목적에는 직접 사용 가능하다.

### Hybrid A* 연결

파일:

- `Tools/PythonPolicyServer/deliverybot_policy/hybrid_astar.py`

추가 옵션:

```json
{
  "analyticExpansionEnabled": true,
  "analyticExpansionMaxDistanceCm": 700.0,
  "analyticExpansionInterval": 1,
  "analyticExpansionSampleStepCm": 25.0,
  "analyticExpansionMaxLengthMultiplier": 4.0
}
```

동작:

1. Hybrid A* loop에서 현재 node와 goal 사이 거리가 `analyticExpansionMaxDistanceCm` 이내인지 확인한다.
2. `analyticExpansionInterval` 조건을 만족하면 Reeds-Shepp shot 생성을 시도한다.
3. 생성 path가 너무 우회하면 `analyticExpansionMaxLengthMultiplier` 기준으로 거부한다.
4. 연속 후진 거리 제한 `maxContinuousReverseDistanceCm`을 초과하면 거부한다.
5. 모든 sampled pose에 대해 기존 `is_pose_clear()` 충돌 검사를 수행한다.
6. 통과한 path만 기존 탐색 경로 뒤에 붙여 `HybridAStarResult`로 반환한다.

### PolicySpec 반영

파일:

- `Json/Input/PolicySpecs/PolicySpec_HybridAStarDefault.json`

`hybridAStar` 설정에 analytic expansion 옵션을 추가했다.

## 검증 완료 항목

테스트 파일:

- `Tools/PythonPolicyServer/tests/test_planner_modes.py`

추가 테스트:

- forward straight Reeds-Shepp shot 생성
- reverse straight Reeds-Shepp shot 생성
- Hybrid A*에서 analytic expansion으로 목표 직접 연결
- analytic expansion 비활성화 시 기존 max expanded node 제한 동작 확인

검증 명령:

```powershell
$env:PYTHONDONTWRITEBYTECODE='1'
& 'C:\Users\user\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' -m unittest discover -s Tools\PythonPolicyServer\tests
```

현재 결과:

- 21 tests 통과

## 현재 동작 방식

Hybrid A*는 다음 순서로 동작한다.

1. 현재 node가 goal acceptance 안이면 종료한다.
2. goal까지 거리가 analytic expansion 범위 안이면 Reeds-Shepp shot을 시도한다.
3. shot이 충돌 없이 유효하면 즉시 최종 경로로 사용한다.
4. shot이 실패하면 기존 motion primitive 확장을 계속한다.
5. 전진/후진 primitive, DWA, 정책 우선순위 로직은 기존과 동일하게 유지된다.

## Unreal 작업 필요 항목

현재 Python은 Reeds-Shepp analytic expansion 자체를 수행할 수 있다. Unreal 쪽 수정은 필수는 아니지만, 정확도와 디버깅을 위해 아래 작업을 해야 한다.

### 1. Observation JSON에 vehicleSpec 추가

대상:

- `ADeliveryBot::BuildObservationJson()`

추가할 JSON field:

```json
{
  "vehicleSpec": {
    "maxSpeedKmh": 8.0,
    "maxReverseSpeedKmh": 3.0,
    "robotBoxExtentCm": {
      "x": 60.0,
      "y": 90.0,
      "z": 25.0
    },
    "minTurningRadiusCm": 300.0,
    "lidarModeType": "All",
    "lidarScanRangeM": 10.0
  }
}
```

이유:

- Python `planning.py`는 `vehicleSpec.minTurningRadiusCm`를 읽어 Hybrid A* 최소 회전 반경에 반영할 수 있다.
- `vehicleSpec.robotBoxExtentCm`가 있으면 footprint collision option을 runtime 값으로 채울 수 있다.
- Reeds-Shepp shot은 최소 회전 반경에 민감하므로 UE 차량 설정과 Python planner 설정이 같아야 한다.

### 2. Episode start/config update JSON에도 vehicleSpec 또는 planningSpec 추가

대상:

- `ADeliveryBot::BuildEpisodeStartJson()`
- `ADeliveryBot::BuildEpisodeConfigUpdateJson()`

권장 추가 field:

```json
{
  "vehicleSpec": {
    "robotBoxExtentCm": {
      "x": 60.0,
      "y": 90.0,
      "z": 25.0
    },
    "minTurningRadiusCm": 300.0
  }
}
```

또는 planner 전용으로 분리하려면:

```json
{
  "planningSpec": {
    "minTurningRadiusCm": 300.0,
    "robotBoxExtentCm": {
      "x": 60.0,
      "y": 90.0,
      "z": 25.0
    }
  }
}
```

현재 Python은 `vehicleSpec` 우선으로 읽는 구조이므로, 우선은 `vehicleSpec` 사용을 권장한다.

### 3. PolicySpec JSON에 analytic expansion 옵션 노출

기본 예:

```json
{
  "hybridAStar": {
    "analyticExpansionEnabled": true,
    "analyticExpansionMaxDistanceCm": 700.0,
    "analyticExpansionInterval": 1,
    "analyticExpansionSampleStepCm": 25.0,
    "analyticExpansionMaxLengthMultiplier": 4.0
  }
}
```

튜닝 기준:

- 좁은 맵에서 부자연스러운 직접 연결이 많으면 `analyticExpansionMaxDistanceCm`를 낮춘다.
- CPU 부담이 크면 `analyticExpansionInterval`을 3~5로 올린다.
- 경로 샘플이 거칠어 충돌 누락이 의심되면 `analyticExpansionSampleStepCm`를 10~20으로 낮춘다.
- 너무 긴 우회 shot이 채택되면 `analyticExpansionMaxLengthMultiplier`를 2~3으로 낮춘다.

### 4. Unreal debug 표시 권장

Python 응답 debug에 다음 field를 추가하는 후속 작업을 권장한다.

- `analyticExpansionUsed`
- `analyticExpansionFamily`
- `analyticExpansionLengthCm`

그 후 Unreal debug draw나 log에 표시한다.

현재 Python `HybridAStarResult`에는 이 debug field가 아직 없다. 다음 작업으로 추가하면 UE에서 Reeds-Shepp shot 채택 여부를 명확히 확인할 수 있다.

## 주의 사항

Reeds-Shepp analytic expansion은 장애물을 무시하고 바로 쓰면 위험하다. 현재 구현은 반드시 기존 grid/footprint collision check를 통과한 경우에만 채택한다.

현재 UE grid는 robot box extent 기반으로 cell을 분류한다. Python footprint check를 동시에 강하게 켜면 보수성이 과해질 수 있다. 운영 튜닝 시 다음 순서를 권장한다.

1. `clearanceRadiusCm`와 UE grid collision만으로 테스트
2. 장애물 스침이 있으면 `footprintCheckEnabled`를 켜고 padding을 낮게 시작
3. Reeds-Shepp shot 거리와 sample step을 조정

## 다음 구현 우선순위

1. `ADeliveryBot::BuildObservationJson()`에 `vehicleSpec` 추가
2. `BuildEpisodeStartJson()`/`BuildEpisodeConfigUpdateJson()`에 `vehicleSpec` 추가
3. Python `HybridAStarResult`와 debug response에 analytic expansion 사용 여부 추가
4. UE log/debug draw에서 analytic expansion path와 일반 Hybrid A* primitive path를 구분 표시
5. 필요 시 Reeds-Shepp full family 확장

