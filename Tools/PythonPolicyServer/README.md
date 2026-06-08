# DeliveryBot Python Policy Server

`ADeliveryBot`의 `UDeliveryBot_HttpPolicyComponent`가 보내는 observation JSON을 받는 최소 HTTP 정책 서버다.

기본 정책은 `forward`이며, `targetSpeedKmh: 3.0`의 저속 전진 action을 반환한다.

## 필요 조건

Python 3.10 이상을 권장한다. 별도 패키지 설치는 필요 없다.

## 기본 실행

프로젝트 루트에서 실행한다.

```powershell
py -3 Tools\PythonPolicyServer\server.py --host 127.0.0.1 --port 8000
```

`py` 명령이 없으면 아래처럼 실행한다.

```powershell
python Tools\PythonPolicyServer\server.py --host 127.0.0.1 --port 8000
```

## 지연 응답 테스트

Unreal의 HTTP 요청 중복 방지와 action timeout을 확인할 때 사용한다.

```powershell
py -3 Tools\PythonPolicyServer\server.py --host 127.0.0.1 --port 8000 --response-delay-second 0.7
```

## Policy Mode

`--policy-mode`로 정상 주행/정지/검증 실패 응답을 바꿀 수 있다.

```powershell
py -3 Tools\PythonPolicyServer\server.py --host 127.0.0.1 --port 8000 --policy-mode runtime
py -3 Tools\PythonPolicyServer\server.py --host 127.0.0.1 --port 8000 --policy-mode forward
py -3 Tools\PythonPolicyServer\server.py --host 127.0.0.1 --port 8000 --policy-mode left
py -3 Tools\PythonPolicyServer\server.py --host 127.0.0.1 --port 8000 --policy-mode right
py -3 Tools\PythonPolicyServer\server.py --host 127.0.0.1 --port 8000 --policy-mode reverse
py -3 Tools\PythonPolicyServer\server.py --host 127.0.0.1 --port 8000 --policy-mode reverse-left
py -3 Tools\PythonPolicyServer\server.py --host 127.0.0.1 --port 8000 --policy-mode reverse-right
py -3 Tools\PythonPolicyServer\server.py --host 127.0.0.1 --port 8000 --policy-mode stop
py -3 Tools\PythonPolicyServer\server.py --host 127.0.0.1 --port 8000 --policy-mode invalid-speed
py -3 Tools\PythonPolicyServer\server.py --host 127.0.0.1 --port 8000 --policy-mode invalid-steering
py -3 Tools\PythonPolicyServer\server.py --host 127.0.0.1 --port 8000 --policy-mode invalid-brake
py -3 Tools\PythonPolicyServer\server.py --host 127.0.0.1 --port 8000 --policy-mode invalid-direction
py -3 Tools\PythonPolicyServer\server.py --host 127.0.0.1 --port 8000 --policy-mode missing-action
py -3 Tools\PythonPolicyServer\server.py --host 127.0.0.1 --port 8000 --policy-mode error-status
```

모드 의미:

- `runtime`: `policy_catalog.json`과 Unreal이 보낸 `policySpec` 기준으로 실제 정책 선택 구조 실행
- `forward`: 정상 저속 전진
- `left`: 전진 좌회전
- `right`: 전진 우회전
- `reverse`: 정상 후진
- `reverse-left`: 후진 좌회전
- `reverse-right`: 후진 우회전
- `stop`: 정상 정지
- `invalid-speed`: `targetSpeedKmh`를 차량 최대 속도보다 크게 반환
- `invalid-steering`: `steering`을 `2.0`으로 반환
- `invalid-brake`: `brake`를 `2.0`으로 반환
- `invalid-direction`: `direction`을 `Sideways`로 반환
- `missing-action`: `status: ok`이지만 `action` 객체를 누락
- `error-status`: `status: error` 반환

참고: 좌/우 조향 방향은 차량 세팅에 따라 반대로 보일 수 있다. 반대로 보이면 `left`와 `right`의 steering 부호만 바꾸면 된다.

## 상태 확인

```powershell
Invoke-RestMethod -Method Get -Uri http://127.0.0.1:8000/health
```

## Episode 시작 데이터 전송

기본 흐름은 Episode 시작 시 `/episode/start`로 정적 데이터와 초기 설정을 한 번에 보내는 것이다.
이 요청에는 grid, start, goal, vehicleSpec, lidarSpec, controlSpec처럼 Episode 시작에 필요한 전체 스냅샷을 담는다.

```powershell
Invoke-RestMethod `
  -Method Post `
  -Uri http://127.0.0.1:8000/episode/start `
  -ContentType "application/json" `
  -Body '{
    "episodeId":"episode_001",
    "robotInstanceId":"delivery_bot_01",
    "start":{"x":0,"y":0,"z":0,"yawDegree":0},
    "goal":{"hasGoal":true,"x":150,"y":150,"z":0,"acceptanceRadiusCm":100},
    "vehicleSpec":{"maxSpeedKmh":10,"maxReverseSpeedKmh":3},
    "lidarSpec":{"mode":"TwoD","scanRangeM":5},
    "controlSpec":{"mode":"TargetSpeed"},
    "grid":{
      "gridSizeX":2,
      "gridSizeY":2,
      "cellSizeCm":100,
      "cellCount":4,
      "originCm":{"x":0,"y":0,"z":0},
      "cells":[
        {"x":0,"y":0,"areaType":"Walkable","cost":1,"blocked":false,"sourceCollisionProfile":"Walkable"},
        {"x":1,"y":0,"areaType":"Penalty","cost":5,"blocked":false,"sourceCollisionProfile":"Penalty"},
        {"x":0,"y":1,"areaType":"Blocked","cost":9999,"blocked":true,"sourceCollisionProfile":"Blocked"},
        {"x":1,"y":1,"areaType":"Walkable","cost":1,"blocked":false,"sourceCollisionProfile":"Walkable"}
      ]
    }
  }'
```

Episode 상태는 아래처럼 확인한다.

```powershell
Invoke-RestMethod -Method Get -Uri http://127.0.0.1:8000/episode/status
```

## Episode 설정 갱신

차량 스펙, 라이다 스펙, 제어 모드처럼 거의 바뀌지 않지만 중간에 바뀔 가능성이 있는 값은 `/episode/config/update`로 부분 갱신한다.

```powershell
Invoke-RestMethod `
  -Method Post `
  -Uri http://127.0.0.1:8000/episode/config/update `
  -ContentType "application/json" `
  -Body '{"vehicleSpec":{"maxSpeedKmh":8,"maxReverseSpeedKmh":2.5}}'
```

## Grid 수신 상태 확인

기존 호환용으로 `/grid/update`도 유지한다. Unreal에서 생성한 Grid JSON은 매 observation마다 보내지 않고, 서버에 한 번 전달한다.
서버는 마지막으로 받은 grid를 메모리에 보관하고 `gridVersion`을 1씩 증가시킨다.

```powershell
Invoke-RestMethod -Method Get -Uri http://127.0.0.1:8000/grid/status
```

정상적으로 grid를 받은 뒤에는 아래처럼 `gridReceived: true`와 1 이상의 `gridVersion`이 보여야 한다.

```json
{
  "status": "ok",
  "gridReceived": true,
  "gridVersion": 1,
  "gridSizeX": 100,
  "gridSizeY": 100,
  "cellCount": 10000,
  "walkableCount": 7200,
  "penaltyCount": 1800,
  "blockedCount": 1000
}
```

## 요청 테스트

```powershell
Invoke-RestMethod `
  -Method Post `
  -Uri http://127.0.0.1:8000/policy/action `
  -ContentType "application/json" `
  -Body '{"sequence":1,"sensorSequence":10,"vehicleSpec":{"maxSpeedKmh":10,"maxReverseSpeedKmh":3},"lidarRays":[],"observedObjects":[]}'
```

Grid를 받은 뒤 `/policy/action` 응답의 `debug`에는 현재 로봇이 올라가 있는 grid cell 정보가 포함된다.

```json
{
  "robotGridStatus": "ok",
  "robotGridVersion": 1,
  "robotGridX": 12,
  "robotGridY": 34,
  "robotCellAreaType": "Walkable",
  "robotCellCost": 1.0,
  "robotCellBlocked": false,
  "robotCellSourceCollisionProfile": "Walkable"
}
```

`robotGridStatus`가 `outside_grid`이면 로봇 위치가 GridBoundsActor 범위 밖에 있다는 뜻이다.
`grid_not_received`이면 Unreal에서 `/grid/update`가 아직 성공하지 않은 상태다.

## Runtime Policy Catalog

패키징된 Unreal UI는 먼저 Python이 관리하는 정책 catalog 목록을 받아서 사용자에게 선택지를 보여준다.

```powershell
Invoke-RestMethod -Method Get -Uri http://127.0.0.1:8000/policy/catalog/sources
```

사용자가 catalog를 선택하면 Unreal은 `catalogId`를 Python에 보낸다. Python은 해당 catalog를 active catalog로 저장하고 catalog 내용을 반환한다.

```powershell
Invoke-RestMethod `
  -Method Post `
  -Uri http://127.0.0.1:8000/policy/catalog/source `
  -ContentType "application/json" `
  -Body '{"catalogId":"default_delivery"}'
```

현재 active catalog는 아래 API로 다시 받을 수 있다.

```powershell
Invoke-RestMethod -Method Get -Uri http://127.0.0.1:8000/policy/catalog
```

그 다음 사용자가 선택한 정책만 우선순위와 함께 Python에 보낸다.

```powershell
Invoke-RestMethod `
  -Method Post `
  -Uri http://127.0.0.1:8000/policy/spec/update `
  -ContentType "application/json" `
  -Body '{
    "policySpec": {
      "catalogVersion": 1,
      "catalogId": "default_delivery",
      "enabledPolicies": [
        {"policyId":"front_obstacle_stop","priority":10},
        {"policyId":"reroute_when_blocked","priority":20},
        {"policyId":"front_obstacle_slowdown","priority":30},
        {"policyId":"normal_path_follow","priority":100}
      ]
    }
  }'
```

`/episode/start` 요청에 같은 `policySpec`을 포함해도 된다. `runtime` 모드이거나 `policySpec`을 받은 상태라면 `/policy/action`은 enabled policy 후보를 만들고 priority가 가장 높은 후보 action을 반환한다.
