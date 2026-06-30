# RePath Agent Handoff

이 문서는 다른 채팅에서 RePath 최적화와 문제 해결 기록 작업을 그대로 이어가기 위한 인수인계 문서다. 이 문서만 먼저 읽고, 필요한 파일만 좁게 확인한다.

## 현재 목표

- Python RePath/A* 병목을 수치로 확인한다.
- 병목 원인이 확인되면 가장 작은 코드 변경으로 최적화한다.
- 의미 있는 분석, 수정, 검증 결과가 생길 때마다 `Client/Docs/TroubleshootingProcess.local.md`에 포트폴리오용 문제 해결 기록을 추가한다.

## 중요한 전제

- Python 정책 코드는 `static/templates/policy/demo`를 수정한다.
- 과거 `Tools/PythonAgent` 경로는 현재 이 작업의 수정 대상이 아니다.
- RePath 결정은 Python 응답의 `events`에서 나온다. C++은 이 이벤트를 받아 `events.jsonl`에 정규화해서 저장한다.
- runtime request/response 계약은 불필요하게 바꾸지 않는다. 지금은 debug/event 계측값 추가까지만 허용된 상태다.
- point cloud, replay, ray 시각화는 이 문서의 작업 범위가 아니다.

## 확인된 문제

확인 로그:

```text
C:\Users\user\Documents\OdiroProto1\runs\000002\episodes\000001
```

관찰된 수치:

- `actions.jsonl`: 133개 action
- 전체 action 로그 크기: 2,636,121 bytes
- action 1개 평균 크기: 약 19,820 bytes
- LiDAR: `TwoD`, `rays_2d=120`, `rays_3d=0`
- 첫 action 기록 시점: `run_time_seconds=6.3`
- 큰 응답 공백:
  - `15.1 -> 21.3`, 약 6.2초
  - `21.3 -> 27.5`, 약 6.2초
  - `27.5 -> 33.7`, 약 6.2초
  - `37.5 -> 43.5`, 약 6.0초
  - `43.5 -> 49.8`, 약 6.3초
- `events.jsonl`: RePath 6회
- `dynamic_blocked_cell_count`: `16 -> 48 -> 120 -> 152 -> 168 -> 168`

현재 판단:

- LiDAR를 2D 120개로 줄인 상태에서도 6초 공백이 생겼다.
- 따라서 1차 병목 후보는 ray 개수보다 RePath/A* 계산이다.
- RePath 횟수는 많지 않다. 한 번의 RePath가 동기 decide 요청을 오래 막는 구조일 가능성이 높다.
- 가장 의심되는 후보는 A* soft cost 계산이다.

## 현재 구현 상태

1단계 계측은 완료됐다.

Python A*가 다음 값을 기록한다.

- `pathfindTotalMs`
- `pathfindCellLookupMs`
- `pathfindSoftCostMs`
- `pathfindSearchMs`
- `pathfindSmoothMs`
- `pathfindGridCellCount`
- `pathfindBlockedCellCount`
- `pathfindSoftCostCellCount`
- `pathfindVisitedNodeCount`
- `pathfindNeighborCheckCount`
- `pathfindOpenPushCount`
- `pathfindPathCellCount`
- `pathfindReason`

Unreal `events.jsonl`에는 snake_case로 저장된다.

- `pathfind_total_ms`
- `pathfind_cell_lookup_ms`
- `pathfind_soft_cost_ms`
- `pathfind_search_ms`
- `pathfind_smooth_ms`
- `pathfind_grid_cell_count`
- `pathfind_blocked_cell_count`
- `pathfind_soft_cost_cell_count`
- `pathfind_visited_node_count`
- `pathfind_neighbor_check_count`
- `pathfind_open_push_count`

## 관련 파일

Python:

```text
static/templates/policy/demo/pathfinding/astar.py
static/templates/policy/demo/policies/repath_policy.py
static/templates/policy/demo/user_agent.py
static/templates/policy/demo/state.py
static/templates/policy/demo/debug_logger.py
```

C++ 이벤트 저장:

```text
Client/Source/OdiroSim/Public/Shared/Struct/DeliveryBot/Result/DeliveryBotPolicyEventSnapshot.h
Client/Source/OdiroSim/Private/DeliveryBot/Component/DeliveryBot_HttpPolicyComponent.cpp
Client/Source/OdiroSim/Private/Scenario/ScenarioEvaluationSubsystem.cpp
```

문제 해결 기록:

```text
Client/Docs/TroubleshootingProcess.local.md
```

## 다음 작업 순서

### 1. 계측값이 실제 로그에 남는지 확인

같은 시나리오를 다시 실행한 뒤 episode의 `events.jsonl`에서 RePath 이벤트를 본다.

필수 확인값:

- `pathfind_total_ms`
- `pathfind_soft_cost_ms`
- `pathfind_search_ms`
- `pathfind_grid_cell_count`
- `pathfind_blocked_cell_count`
- `pathfind_visited_node_count`

판단 기준:

- `pathfind_soft_cost_ms`가 크면 soft cost 계산 최적화가 1순위다.
- `pathfind_search_ms`가 크면 A* 탐색 범위와 grid 해상도 최적화가 1순위다.
- `pathfind_total_ms`가 낮은데 action 간격이 크면 Python 계산보다 HTTP, Unreal tick, 로그 저장, debug draw 갱신을 본다.

### 2. soft cost가 병목이면 먼저 고칠 것

현재 의심 구조:

- `build_obstacle_soft_costs()`가 grid 전체와 blocked cell 목록을 반복 비교할 수 있다.
- grid cell 수와 blocked cell 수가 커질수록 비용이 급격히 증가한다.

권장 최적화:

- static blocked cell soft cost는 `/scenario/start` 이후 캐시한다.
- RePath 때는 dynamic blocked cell 주변 soft cost만 새로 계산한다.
- 전체 grid scan 대신 blocked cell 주변 radius만 확장한다.
- static cost와 dynamic cost는 cell별 `max()`로 합친다.

구현 후보:

```text
AStarPathfinder
├─ staticSoftCostCache
├─ make_grid_cache_key(grid)
├─ build_static_obstacle_soft_costs(grid, cell_lookup)
├─ build_dynamic_obstacle_soft_costs(grid, dynamic_blocked_cells)
└─ merge_soft_costs(static_costs, dynamic_costs)
```

주의:

- 처음부터 복잡한 비동기 구조로 가지 않는다.
- 현재 병목이 soft cost인지 수치로 확인한 뒤 캐시를 넣는다.
- 캐시 invalidation 기준은 grid origin, cell size, width/height, blocked cell fingerprint다.

### 3. search가 병목이면 그 다음 고칠 것

검토 후보:

- RePath 목표를 최종 goal이 아니라 기존 path의 앞쪽 lookahead 지점으로 제한한다.
- A* 탐색 bounding box를 start, target, 기존 corridor 주변으로 제한한다.
- grid 해상도 또는 dynamic obstacle 확장 반경을 조정한다.
- path가 이미 유효하면 전체 재계산 대신 부분 경로만 교체한다.

주의:

- search 최적화는 주행 품질에 영향을 줄 수 있다.
- soft cost보다 먼저 건드리지 않는다.

### 4. 계산 시간이 낮으면 통신/기록 병목을 본다

확인 후보:

- `/scenario/decide` 요청 간격
- Python 응답 생성 시간
- action 로그 저장 시간
- Unreal debug draw lifetime
- 같은 sensor sequence가 반복되는지 여부

이 경우 A*를 더 고쳐도 debug path가 사라지는 문제는 해결되지 않을 수 있다.

## TroubleshootingProcess.local.md 업데이트 규칙

다음 중 하나라도 생기면 `Client/Docs/TroubleshootingProcess.local.md`에 짧게 추가한다.

- 새 run 로그를 분석해서 수치가 나왔다.
- 병목 원인이 하나로 좁혀졌다.
- 최적화 전후 수치를 비교했다.
- 구현 방향을 바꿨다.
- 검증 결과가 성공 또는 실패로 나왔다.

기록 형식:

```text
## YYYY-MM-DD - 제목

### 상황
- 어떤 run을 봤는지
- 어떤 증상이 있었는지

### 수치
- 변경 전/후 시간
- action 간격
- RePath 횟수
- grid/blocked/visited node 수

### 판단
- 무엇이 병목인지
- 왜 그렇게 판단했는지

### 조치
- 어떤 코드를 바꿨는지
- 왜 그 방식이 최소 변경인지

### 결과
- 검증 명령
- 성공/실패
- 다음 작업
```

## 검증 명령

Python 문법 확인:

```powershell
& "C:\Users\user\AppData\Local\Programs\Python\Python311\python.exe" -m py_compile `
  "static\templates\policy\demo\pathfinding\astar.py" `
  "static\templates\policy\demo\state.py" `
  "static\templates\policy\demo\user_agent.py" `
  "static\templates\policy\demo\debug_logger.py" `
  "static\templates\policy\demo\policies\repath_policy.py"
```

중요:

- `py_compile` 후 `static/templates/policy/demo` 아래에 `__pycache__`가 생기면 삭제한다.
- policy preset은 `__pycache__`가 있으면 실행을 거부한다.

C++ 빌드:

```powershell
.\task-build.bat -Target client
```

한글 문서 파일이 untracked 상태일 때 UnrealBuildTool이 `git status` working set 계산에서 죽으면, 그 빌드 프로세스에만 아래 옵션을 준다.

```powershell
$env:GIT_CONFIG_COUNT='1'
$env:GIT_CONFIG_KEY_0='core.quotePath'
$env:GIT_CONFIG_VALUE_0='false'
.\task-build.bat -Target client
Remove-Item Env:\GIT_CONFIG_COUNT, Env:\GIT_CONFIG_KEY_0, Env:\GIT_CONFIG_VALUE_0 -ErrorAction SilentlyContinue
```

diff 확인:

```powershell
git diff --check
```

## 완료 기준

1단계 완료 기준:

- 새 run의 RePath 이벤트에 pathfind 계측값이 저장된다.
- `pathfind_soft_cost_ms`, `pathfind_search_ms`, `pathfind_total_ms` 중 어디가 병목인지 판단할 수 있다.
- 판단 내용을 `Client/Docs/TroubleshootingProcess.local.md`에 추가한다.

최적화 완료 기준:

- RePath가 발생해도 action 간격이 눈에 띄게 줄어든다.
- Debug Draw 경로가 반복적으로 사라지지 않는다.
- 기존 주행 정책 결과가 더 나빠지지 않는다.
- Python 문법 검사와 client 빌드가 통과한다.
