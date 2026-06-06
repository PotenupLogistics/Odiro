# API 통신 방향성

## 목적

이 문서는 다른 프로젝트 에이전트가 DeliveryBot Python 통신 구조를 이해하고, 다음 구현 방향을 일관되게 잡기 위한 설계 메모다.

기준 클래스는 `ADeliveryBot`이다. 현재 Python 통신은 `UDeliveryBot_HttpPolicyComponent`와 `UDeliveryBot_PolicyControllerComponent`를 통해 `/policy/action`으로 observation을 보내고 action을 받는 구조다.

## 현재 문제의식

현재 `/policy/action` 요청에는 매 Tick 변하는 값과 거의 변하지 않는 값이 함께 들어간다.

예:

- 매 Tick 변하는 값: 현재 위치, yaw, 속도, 라이다 ray, 감지 객체
- 거의 변하지 않는 값: `maxSpeedKmh`, `maxReverseSpeedKmh`, `lidarScanRangeM`

초기 구현에서는 이 방식이 단순하다. 하지만 policy simulator로 확장하면 다음 문제가 생긴다.

- 매 요청 payload가 불필요하게 커진다.
- Python이 판단해야 하는 고정 스펙과 순간 observation의 경계가 흐려진다.
- 어린이 보호 구역, 임시 제한 속도, 공사 구역 같은 semi-static rule을 다루기 어렵다.
- Python 정책이 잘못된 action을 냈는지, Unreal이 잘못된 컨텍스트를 보냈는지 추적하기 어려워진다.

따라서 통신 데이터를 3종류로 나누는 방향이 좋다.

## 데이터 분류

| 구분 | 의미 | 예시 | 권장 전송 방식 |
| --- | --- | --- | --- |
| Static Spec | 한 세션 동안 거의 변하지 않는 로봇/센서/액션 한계 | `maxSpeedKmh`, `maxReverseSpeedKmh`, `robotBoxExtentCm`, `minTurningRadiusCm`, `lidarScanRangeM` | 세션 시작 시 1회 전송 |
| Semi-static Rule Context | 자주 바뀌지는 않지만 상황에 따라 갱신되는 규칙 | 어린이 보호 구역, 임시 속도 제한, 공사 구역, 위험 구역 | 변경 시 update API 호출 |
| Dynamic Observation | 매 Tick 또는 요청마다 변하는 관측값 | 위치, yaw, 속도, 라이다 ray, 감지 객체 | `/policy/action`마다 전송 |

## 권장 API 구조

```text
POST /policy/session/start
POST /policy/session/update
POST /policy/action
POST /policy/session/end
```

최소 구현은 `session/start`와 `policy/action`만 먼저 나누어도 충분하다. `session/update`는 어린이 보호 구역 같은 요구가 실제로 들어올 때 추가해도 된다.

## 1. Session Start

### 역할

Unreal이 Python 서버에 새 simulation/session이 시작됐음을 알리고, 고정 스펙을 한 번 전달한다.

Python은 `sessionId` 기준으로 이 값을 저장해두고 이후 `/policy/action` 판단에 사용한다.

### 요청 예시

```json
{
  "schemaVersion": "1.0.0",
  "sessionId": "episode-001-bot-001",
  "episodeId": "episode-001",
  "robotSpec": {
    "maxSpeedKmh": 10.0,
    "maxReverseSpeedKmh": 3.0,
    "robotBoxExtentCm": {
      "x": 60.0,
      "y": 90.0,
      "z": 25.0
    },
    "minTurningRadiusCm": 300.0
  },
  "sensorSpec": {
    "lidarMode": "TwoD",
    "lidarScanRangeM": 8.0,
    "angleStepDegree": 2.0,
    "frontHalfAngleDegree": 20.0
  },
  "actionLimit": {
    "steeringMin": -1.0,
    "steeringMax": 1.0,
    "throttleMin": 0.0,
    "throttleMax": 1.0,
    "brakeMin": 0.0,
    "brakeMax": 1.0
  },
  "contextVersion": 1,
  "ruleContext": {
    "zoneType": "Normal",
    "speedLimitKmh": 10.0,
    "reason": "Initial default rule"
  }
}
```

### 응답 예시

```json
{
  "sessionId": "episode-001-bot-001",
  "status": "ok",
  "acceptedSchemaVersion": "1.0.0",
  "contextVersion": 1
}
```

### 에이전트 구현 메모

- `sessionId`는 Python 캐시의 key다.
- Python이 재시작되어 session을 모르면 `/policy/action`에서 `status: "error"`를 반환해야 한다.
- Unreal은 Python의 session missing 응답을 받으면 `session/start`를 다시 보낼 수 있어야 한다.
- Static Spec의 원본 권한은 Unreal에 둔다.

## 2. Session Update

### 역할

어린이 보호 구역, 임시 제한 속도, 도로 공사 구역처럼 시뮬레이션 중 바뀌는 규칙을 Python 서버에 갱신한다.

`maxSpeedKmh` 자체를 바꾸기보다, 현재 구역의 제한 속도와 규칙 컨텍스트를 별도 `ruleContext`로 전달하는 방식이 좋다.

### 요청 예시

```json
{
  "schemaVersion": "1.0.0",
  "sessionId": "episode-001-bot-001",
  "contextVersion": 2,
  "ruleContext": {
    "zoneType": "SchoolZone",
    "speedLimitKmh": 3.0,
    "reason": "Entered school zone"
  }
}
```

### 응답 예시

```json
{
  "sessionId": "episode-001-bot-001",
  "status": "ok",
  "contextVersion": 2
}
```

### 에이전트 구현 메모

- `contextVersion`은 단조 증가시키는 것이 좋다.
- Python은 더 낮거나 같은 `contextVersion` update가 오면 무시하거나 stale로 처리한다.
- `/policy/action`에는 매번 `contextVersion`을 포함해 Python 캐시와 Unreal 상태가 같은지 확인한다.

## 3. Policy Action

### 역할

매 주기 또는 필요 시점마다 변하는 observation만 Python에 보낸다.

현재 `ADeliveryBot::BuildObservationJson()`은 `vehicleSpec`도 같이 보내고 있지만, 장기적으로는 static spec을 `session/start`로 옮기고 `/policy/action` payload를 줄이는 방향이 좋다.

### 권장 요청 예시

```json
{
  "schemaVersion": "1.0.0",
  "sessionId": "episode-001-bot-001",
  "sequence": 42,
  "sensorSequence": 108,
  "contextVersion": 2,
  "worldTimeSeconds": 12.35,
  "robotState": {
    "x": 120.0,
    "y": 40.0,
    "z": 0.0,
    "yawDegree": 90.0,
    "speedKmh": 4.2
  },
  "observedObjects": [
    {
      "actorName": "Obstacle_01",
      "closestDistanceM": 2.4,
      "closestRayYawDegree": -6.0,
      "totalHitRayCount": 5,
      "frontHitRayCount": 3,
      "inFront": true
    }
  ],
  "lidarRays": [
    {
      "hit": true,
      "rayIndex": 0,
      "rayYawDegree": -20.0,
      "distanceM": 2.7,
      "actorName": "Obstacle_01"
    }
  ]
}
```

### 현재 구현과의 차이

현재 구현의 `/policy/action` 요청에는 아래 값이 들어간다.

```json
{
  "vehicleSpec": {
    "maxSpeedKmh": 10.0,
    "maxReverseSpeedKmh": 3.0,
    "lidarScanRangeM": 8.0
  }
}
```

권장 구조에서는 이 값들을 `session/start`로 옮긴다.

다만 migration 기간에는 `/policy/action`에 `vehicleSpec`을 계속 넣어도 된다. Python 서버는 우선순위를 다음처럼 두면 안전하다.

```text
1. session cache의 robotSpec/sensorSpec/actionLimit
2. action 요청의 vehicleSpec
3. Python 기본값
```

### 응답 예시

```json
{
  "sessionId": "episode-001-bot-001",
  "sequence": 42,
  "contextVersion": 2,
  "status": "ok",
  "action": {
    "steering": 0.15,
    "throttle": 0.0,
    "brake": 0.0,
    "targetSpeedKmh": 3.0,
    "direction": "Forward"
  },
  "debug": {
    "policyName": "school_zone_policy",
    "reason": "Limited by school zone speed rule"
  }
}
```

### 현재 응답 형식과의 호환

현재 `UDeliveryBot_HttpPolicyComponent::TryParsePolicyResponseJson()`은 아래 필드를 읽는다.

```json
{
  "sequence": 42,
  "status": "ok",
  "action": {
    "steering": 0.15,
    "throttle": 0.0,
    "brake": 0.0,
    "targetSpeedKmh": 3.0,
    "direction": "Forward"
  },
  "debug": {
    "policyName": "school_zone_policy",
    "reason": "Limited by school zone speed rule"
  }
}
```

따라서 `sessionId`와 `contextVersion`을 응답에 추가해도 기존 파서는 무시할 수 있다. 나중에 검증을 강화하려면 Unreal 쪽 response struct와 parser에 두 필드를 추가하면 된다.

## 4. Session End

### 역할

simulation이 끝나거나 DeliveryBot이 destroy될 때 Python 캐시를 정리한다.

필수는 아니지만, batch simulation을 많이 돌릴수록 session cache 누수를 막는 데 유리하다.

### 요청 예시

```json
{
  "schemaVersion": "1.0.0",
  "sessionId": "episode-001-bot-001",
  "reason": "SimulationFinished"
}
```

### 응답 예시

```json
{
  "sessionId": "episode-001-bot-001",
  "status": "ok"
}
```

## 안전 검증 원칙

Python이 spec과 rule context를 저장해서 판단하더라도, 최종 안전 검증은 Unreal에서 다시 수행해야 한다.

예:

- Python이 `targetSpeedKmh: 10.0`을 반환
- 현재 `SchoolZone`의 `speedLimitKmh`가 `3.0`

이때 Unreal은 두 방식 중 하나를 선택해야 한다.

| 방식 | 설명 | 정책 평가 시 추천 |
| --- | --- | --- |
| 실패 처리 | Python action이 계약을 위반했다고 기록하고 정지/실패 처리 | 추천 |
| 강제 제한 | Unreal이 `targetSpeedKmh`를 3.0으로 clamp | 실제 안전 시스템에 가까움 |

이 프로젝트가 정책 성능을 비교하는 시뮬레이터라면 기본값은 실패 처리가 좋다. 조용히 clamp하면 Python policy의 잘못된 판단이 성공처럼 보일 수 있다.

## 단계별 구현 추천

### 1단계

현재 `/policy/action` 구조를 유지하되 문서화한다.

- 이미 구현된 `sequence`, `status`, `action`, `debug` 계약 유지
- Python 서버 샘플을 현재 형식에 맞춘다.

### 2단계

`/policy/session/start`를 추가한다.

- `robotSpec`
- `sensorSpec`
- `actionLimit`
- 초기 `ruleContext`
- `sessionId`

이후 `/policy/action`에서 `vehicleSpec` 제거를 준비한다.

### 3단계

`/policy/action`에 `sessionId`, `contextVersion`을 추가한다.

- Python은 session cache를 사용한다.
- session이 없으면 error를 반환한다.
- Unreal은 session missing 시 `session/start` 재전송을 고려한다.

### 4단계

`/policy/session/update`를 추가한다.

- 어린이 보호 구역
- 임시 제한 속도
- 위험 구역
- 공사 구역

같은 semi-static rule을 갱신한다.

### 5단계

Unreal action validator에 rule context 검증을 추가한다.

- Python action이 현재 제한 속도를 넘으면 invalid action으로 기록
- 실패 로그 또는 episode 결과에 reason 저장

## 다른 에이전트가 지켜야 할 결정

- `ADeliveryBot` 기준으로 작업한다.
- 매 Tick observation과 세션 스펙을 분리하는 방향으로 확장한다.
- Python cache의 key는 `sessionId`다.
- rule 변경 동기화는 `contextVersion`으로 한다.
- Python 응답 action은 Unreal에서 다시 검증한다.
- policy 평가 목적에서는 조용한 clamp보다 실패 기록이 우선이다.
- `/policy/action`의 기존 응답 형식은 당장 깨지지 않게 유지한다.

