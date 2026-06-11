# DeliveryBot Python 길찾기/정책 구현 정리

## 목적

DeliveryBot Python Policy Server에서 전역 경로 탐색은 A*와 Hybrid A*로 명확히 구분하고, DWA는 별도의 local avoidance 정책으로 선택 적용한다.

목표는 Unreal 환경을 바꾸지 않고도 Python 쪽에서 다음을 수행하는 것이다.

- 정책 우선순위와 우선권 모드에 따라 정지, 감속, 재탐색, 우회 행동을 선택한다.
- A*와 Hybrid A*를 서버 실행 옵션 또는 PolicySpec에서 선택한다.
- DWA 사용 여부를 서버 실행 옵션으로 켜고 끈다.
- 가까운 장애물을 너무 늦게 피하는 문제를 줄이기 위해 DWA가 LiDAR hit뿐 아니라 가까운 grid blocked cell도 local obstacle로 본다.
- Hybrid A*는 후진 primitive, 후처리 shortcut/resample, 옵션형 footprint 충돌 검사를 제공한다.

## 실행 명령 4개

PowerShell 기준이다.

### 1. A* + DWA 미사용

```powershell
& 'C:\Users\user\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' Tools\PythonPolicyServer\server.py --policy-mode runtime --planner-mode astar --dwa-mode off
```

### 2. A* + DWA 사용

```powershell
& 'C:\Users\user\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' Tools\PythonPolicyServer\server.py --policy-mode runtime --planner-mode astar --dwa-mode on
```

### 3. Hybrid A* + DWA 미사용

```powershell
& 'C:\Users\user\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' Tools\PythonPolicyServer\server.py --policy-mode runtime --planner-mode hybrid-astar --dwa-mode off
```

### 4. Hybrid A* + DWA 사용

```powershell
& 'C:\Users\user\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' Tools\PythonPolicyServer\server.py --policy-mode runtime --planner-mode hybrid-astar --dwa-mode on
```

## 주요 구현

### Planner 선택

- `Tools/PythonPolicyServer/server.py`
  - `--planner-mode {auto, astar, hybrid-astar, hybrid_astar}` 지원.
  - `--dwa-mode {policy, on, off}` 지원.
  - `--right-of-way-mode {policy, pedestrian, robot}` 지원.
- `Tools/PythonPolicyServer/deliverybot_policy/planning.py`
  - A*와 Hybrid A* 결과를 `PlannedPathResult`로 통합.
  - DWA 정책은 `globalPlanner`로 A* 또는 Hybrid A*를 내부 선택 가능.
  - Hybrid A* 결과의 `direction`을 lookahead가 gear switch 너머로 넘어가지 않게 사용.

### Hybrid A*

- `Tools/PythonPolicyServer/deliverybot_policy/hybrid_astar.py`
  - 전진/후진 motion primitive.
  - 최소 회전 반경, heading bin, gear switch penalty, reverse penalty.
  - 연속 후진 거리 제한.
  - collision sampling과 clearance radius.
  - 후처리:
    - 중복 pose 제거.
    - 같은 gear 구간에서만 shortcut 적용.
    - shortcut 후 waypoint 간격을 다시 resample.
    - shortcut 충돌 검사 때 yaw를 보간해서 검사.
  - 옵션형 footprint 검사:
    - `footprintCheckEnabled`
    - `footprintHalfLengthCm`
    - `footprintHalfWidthCm`
    - `footprintPaddingCm`
    - `footprintSampleStepCm`

현재 UE grid는 이미 robot box extent로 cell collision을 분류하므로 footprint 검사는 기본 강제가 아니라 opt-in이다. 정책 또는 `vehicleSpec`/`configInfo.vehicleSpec`에 값이 들어오면 Python 쪽에서 사용할 수 있게 준비했다.

### DWA Local Avoidance

- `Tools/PythonPolicyServer/deliverybot_policy/policies/dwa_local_avoidance.py`
  - DWA는 전역 탐색기가 아니라 짧은 horizon의 회피 행동 선택 정책이다.
  - LiDAR obstacle point와 grid blocked cell obstacle point를 함께 평가한다.
  - 새 설정:
    - `includeGridObstacles`: 기본 `true`
    - `gridObstacleMaxDistanceM`: 기본 `3.5`
  - debug에 다음 값을 추가했다.
    - `dwaLidarObstacleCount`
    - `dwaGridObstacleCount`
    - `dwaGridObstacleMaxDistanceM`
  - Hybrid A* 경로가 후진으로 시작하면 DWA도 후진 후보를 평가하고 `direction: Reverse` action을 낼 수 있다.

### 정책 판단

- `front_obstacle_stop`
  - 사람 우선이면 기본 대기 후 재탐색.
  - 로봇 우선이면 즉시 재탐색.
  - 재탐색 성공은 실패 attempt로 누적하지 않는다.
  - 후진 recovery 전에 후방 LiDAR clearance를 확인한다.
- `reroute_when_blocked`
  - 목적지까지 경로가 없을 때 stop 후보와 debug를 낸다.
- `normal_path_follow`
  - A*/Hybrid A* 공통 결과를 따라가며, Hybrid A*의 후진 direction을 action에 반영한다.

## PolicySpec 튜닝 예시

Hybrid A* 옵션 예:

```json
{
  "planner": "hybrid_astar",
  "hybridAStar": {
    "stepDistanceCm": 75.0,
    "minTurningRadiusCm": 300.0,
    "maxContinuousReverseDistanceCm": 350.0,
    "clearanceRadiusCm": 65.0,
    "postProcessEnabled": true,
    "shortcutEnabled": true,
    "resampleEnabled": true,
    "resampleDistanceCm": 50.0
  }
}
```

DWA grid obstacle 옵션 예:

```json
{
  "planner": "dwa",
  "globalPlanner": "hybrid_astar",
  "parameters": {
    "dwa": {
      "activationDistanceM": 5.0,
      "safetyDistanceM": 0.65,
      "includeGridObstacles": true,
      "gridObstacleMaxDistanceM": 3.5
    }
  }
}
```

## 검증 결과

- Python 단위 테스트: 16개 통과.
- 수정한 JSON 파일 파싱 확인 완료.
- `server.py --help` 실행 확인 완료.

실행한 테스트:

```powershell
$env:PYTHONDONTWRITEBYTECODE='1'
& 'C:\Users\user\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' -m unittest Tools\PythonPolicyServer\tests\test_planner_modes.py
```

## 전문가 관점 평가

현재 구조는 현업형 계층 분리에 가까워졌다.

- 전역 경로 탐색: A*, Hybrid A*
- 지역 회피: DWA
- 정책 판단: stop/slowdown/reroute/path-follow
- 실행 모드: CLI와 PolicySpec

이번 품질 개선에서 가장 실효성이 큰 부분은 DWA가 grid blocked cell을 미리 보는 것이다. 기존처럼 LiDAR hit만 기다리면 장애물이 라이다 정면에 충분히 가까워진 뒤에야 회피가 시작될 수 있는데, grid 기반 obstacle point를 함께 쓰면 정적 장애물에 대한 local horizon 판단이 더 일찍 시작된다.

남은 권장 개선은 Unreal observation JSON에 `vehicleSpec`을 포함시키는 것이다. Python은 이미 `vehicleSpec.robotBoxExtentCm`와 `minTurningRadiusCm`를 읽을 준비가 되어 있으므로, UE에서 이 값을 보내면 정책 JSON에 footprint 값을 반복해서 넣지 않아도 된다.

## Reeds-Shepp Analytic Expansion

Hybrid A*에 Reeds-Shepp 계열 analytic expansion을 추가했다.

- 구현 파일: `Tools/PythonPolicyServer/deliverybot_policy/reeds_shepp.py`
- 연결 파일: `Tools/PythonPolicyServer/deliverybot_policy/hybrid_astar.py`
- 상세 후속 작업 문서: `Docs/plans/PLAN-reeds-shepp-analytic-expansion.md`

현재 구현은 목표 근처에서 forward/reverse bounded-curvature CSC shot을 생성하고, 모든 sampled pose가 기존 grid/footprint collision check를 통과할 때만 경로에 붙인다.

PolicySpec 옵션:

```json
{
  "analyticExpansionEnabled": true,
  "analyticExpansionMaxDistanceCm": 700.0,
  "analyticExpansionInterval": 1,
  "analyticExpansionSampleStepCm": 25.0,
  "analyticExpansionMaxLengthMultiplier": 4.0
}
```

Unreal 쪽에서는 `vehicleSpec.minTurningRadiusCm`와 `vehicleSpec.robotBoxExtentCm`를 observation/config JSON에 포함시키는 작업이 필요하다. 이 값이 있어야 Python planner가 UE 차량의 실제 회전 반경과 차체 크기를 더 정확하게 반영한다.
