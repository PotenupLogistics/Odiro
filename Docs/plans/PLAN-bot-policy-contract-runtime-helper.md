# BotPolicy 공통 인터페이스 적용 작업 가이드

## 목적

이 문서는 채팅 기록 없이도 `BotPolicy` 공통 인터페이스 적용 작업을 이어갈 수 있게 하기 위한 기준 문서다.

대상 시스템은 `DeliveryBot` Unreal Client와 `Tools/PythonPolicyServer`다. 목표는 기존 HTTP 서버의 역할을 `BotPolicy` class contract로 분리하고, HTTP endpoint는 adapter/facade로 유지하는 것이다.

## 현재 구현 상태

추가된 Python 파일:

- `Tools/PythonPolicyServer/deliverybot_policy/bot_policy_contract.py`
  - 공통 contract dataclass와 `BotPolicy` Protocol 정의.
  - public method 이름은 lowerCamelCase:
    - `setConfig`
    - `setContext`
    - `initialize`
    - `decide`
  - JSON 호환 field는 lowerCamelCase로 유지.
  - Python type/class는 PascalCase로 정의.
- `Tools/PythonPolicyServer/deliverybot_policy/bot_policy_runtime.py`
  - 기존 runtime 정책 엔진을 감싸는 `RuntimeBotPolicy` adapter.
  - 기존 `build_runtime_policy_response()`를 재사용하므로 A*/Hybrid A*/DWA/정책 우선순위 동작을 유지한다.
  - legacy HTTP payload 변환 helper 포함:
    - `configFromLegacyPayload`
    - `episodeSetupFromLegacyPayload`
    - `contextFromLegacyGrid`
    - `contextFromLegacyObservation`
- `Tools/PythonPolicyServer/deliverybot_policy/bot_policy_loader.py`
  - 사용자 Python script를 dynamic import하는 loader.
  - script가 `BotPolicy` class를 export하는지 확인.
  - `setConfig`, `setContext`, `initialize`, `decide` 메서드 존재 여부 검증.
- `Tools/PythonPolicyServer/tests/test_bot_policy_contract.py`
  - legacy payload를 새 contract로 변환한 뒤 `RuntimeBotPolicy.decide()`가 정상 action을 반환하는지 검증.

기존 HTTP 서버 `Tools/PythonPolicyServer/server.py`는 아직 직접 교체하지 않았다. 현재는 새 contract/adapter가 추가된 상태이며, 기존 endpoint는 그대로 동작한다.

## 핵심 Contract

사용자 정책 script는 `BotPolicy` class를 export해야 한다.

```python
class BotPolicy:
    async def setConfig(self, config: Config) -> SetConfigResult: ...
    async def setContext(self, context: Context) -> SetContextResult: ...
    def initialize(self, setup: EpisodeSetup) -> InitializeResult: ...
    def decide(self, request: DecisionRequest) -> Decision: ...
```

호출 의도:

- `setConfig`: 차량, 센서, 제어, 정책 설정 반영
- `setContext`: sensor, grid, perception, compute artifact 갱신
- `initialize`: episode/run 시작 및 runtime state 초기화
- `decide`: 현재 context snapshot 기준 action 결정

## 기존 HTTP API와 새 호출 매핑

| 기존 API | 새 호출 |
| --- | --- |
| `POST /episode/start` | `setConfig(config)` + `initialize(episodeSetup)` + `setContext(grid context)` |
| `POST /episode/config/update` | `setConfig(config)` |
| `POST /grid/update` | `setContext(ContextItem kind=GRID_MAP)` |
| `POST /policy/action` | `setContext(observation context)` + `decide(decisionRequest)` |
| `POST /policy/spec/update` | `setConfig(Config(policy=policySpec))` |

## Version 규칙

| Version | 증가 시점 | Unreal 호환 |
| --- | --- | --- |
| `episodeVersion` | `initialize` 성공 | 필수 |
| `configVersion` | `setConfig` 성공 | 필수 |
| `contextVersion` | `setContext` 성공 | 신규 추적 |
| `gridVersion` | `GRID_MAP` context item 수락 | 필수 |

`Decision`은 Unreal 검증 호환을 위해 항상 `episodeVersion`, `configVersion`, `gridVersion`을 반환해야 한다.

## 다음 작업 순서

### 1. Python 테스트 확인

PowerShell:

```powershell
$env:PYTHONDONTWRITEBYTECODE='1'
& 'C:\Users\user\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' -m unittest discover -s Tools\PythonPolicyServer\tests
```

통과 기준:

- 기존 pathfinding/policy 테스트 통과
- `test_bot_policy_contract.py` 통과

### 2. HTTP server.py를 adapter 호출 방식으로 점진 전환

권장 순서:

1. `main()`에서 `RuntimeBotPolicy` instance를 생성해 `server.bot_policy`에 저장한다.
2. 기존 server 상태 필드는 당장 제거하지 않는다.
3. 각 endpoint handler에서 기존 상태 갱신과 새 adapter 호출을 병행한다.
4. 응답 JSON은 기존 Unreal 호환 필드를 유지한다.

전환 예:

```python
server.bot_policy = RuntimeBotPolicy(
    policyCatalog=policy_catalog,
    plannerMode=server.planner_mode,
    dwaMode=server.dwa_mode,
    rightOfWayMode=server.right_of_way_mode,
)
```

`/policy/action` 전환 예:

```python
await policy.setContext(contextFromLegacyObservation(observation))
decision = policy.decide(
    DecisionRequest(
        sequence=observation["sequence"],
        worldTimeSeconds=observation.get("worldTimeSeconds", 0.0),
    )
)
```

HTTP handler는 sync이므로 다음 중 하나를 선택한다.

- 단순 적용: `asyncio.run(policy.setContext(...))`
- 장기 운영형: event loop worker/thread를 runtime helper로 분리

장기 구조에서는 HTTP server가 직접 async orchestration을 갖지 않고 runtime helper가 담당하는 편이 낫다.

### 3. 사용자 Python script dynamic import

runtime helper에서 수행할 작업:

1. script path를 안전하게 검증한다.
2. `importlib.util.spec_from_file_location()`로 module load.
3. module에 `BotPolicy` class가 있는지 확인.
4. instance 생성.
5. `setConfig`, `setContext`, `initialize`, `decide` method 존재 여부 검증.
6. 반환값을 `bot_policy_contract.py` dataclass 또는 dict로 검증/정규화.

주의:

- 사용자 script 반환값은 trust boundary data다.
- Unreal로 보내기 전 `Decision.action` range를 반드시 검증한다.
- 기존 C++ validation도 유지한다.

### 4. Unreal observation에 vehicleSpec 추가

Python은 이미 `vehicleSpec.robotBoxExtentCm`, `minTurningRadiusCm`를 읽을 준비가 되어 있다.

권장 UE 작업:

- `ADeliveryBot::BuildObservationJson()`에 `vehicleSpec` object 추가.
- 포함 필드:
  - `maxSpeedKmh`
  - `maxReverseSpeedKmh`
  - `robotBoxExtentCm`
  - `minTurningRadiusCm`
  - `lidarModeType`
  - `lidarScanRangeM`

이 작업이 들어가면 Hybrid A* footprint option과 최소 회전 반경 runtime 반영이 더 정확해진다.

## 정상 동작 확인 체크리스트

- 서버 실행 옵션 4개가 유지된다.
  - `--planner-mode astar --dwa-mode off`
  - `--planner-mode astar --dwa-mode on`
  - `--planner-mode hybrid-astar --dwa-mode off`
  - `--planner-mode hybrid-astar --dwa-mode on`
- `/episode/start` 후 `episodeVersion`, `configVersion`, `gridVersion`이 정상 증가한다.
- `/policy/action` 응답에 기존 Unreal 필수 필드가 있다.
  - `sequence`
  - `status`
  - `episodeVersion`
  - `configVersion`
  - `gridVersion`
  - `action`
  - `debug.policyName`
  - `debug.reason`
- goal 도달 시 `debug.reason == "goal_reached"`를 유지한다.
- action 검증:
  - `-1.0 <= steering <= 1.0`
  - `0.0 <= brake <= 1.0`
  - `targetSpeedKmh >= 0.0`
  - `direction in {"Forward", "Reverse"}`

## 현재 판단

현재 단계에서는 contract와 adapter를 추가한 상태가 적절하다. 바로 `server.py`를 전면 교체하면 Unreal 호환 응답과 기존 테스트가 동시에 흔들릴 수 있다. 다음 단계는 adapter 병행 호출로 version/result parity를 확인한 뒤, endpoint별로 기존 상태 갱신 코드를 제거하는 순서가 안전하다.
