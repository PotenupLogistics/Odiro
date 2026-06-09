# DeliveryBot 다음 작업 인수인계 문서

이 문서는 다른 채팅창에서 DeliveryBot 작업을 그대로 이어가기 위한 인수인계 문서다.

현재 전제는 다음과 같다.

1. Episode 담당자가 추후 Object Semantic DataAsset을 만든다.
2. DataAsset이 만들어지기 전까지 DeliveryBot은 Lidar에 감지된 액터의 `ActorTags`를 우선 Python으로 보낸다.
3. Python이 길찾기, 정책 판단, 우회, 대기, 정지 판단을 담당한다.
4. Unreal은 관측 데이터 전송, 응답 검증, 물리 이동 실행만 담당한다.
5. Episode spawn 구조 변경은 다른 담당자 영역이므로 현재 작업에서는 직접 수정하지 않는다.

## 현재 구현 상태

| 영역 | 현재 상태 |
| --- | --- |
| DeliveryBot Actor | `ADeliveryBot` 기준으로 구현 진행 중 |
| Sensor | 1D, 2D Lidar 동작 확인 |
| Lidar 감지 데이터 | 감지된 액터 이름, 거리, ray 정보, `ActorTags` 저장 가능 |
| Observation | `/policy/action`으로 robot state, lidar rays, observed objects 전송 |
| Python 통신 | HTTP 기반으로 `/episode/start`, `/episode/config/update`, `/grid/update`, `/policy/action` 사용 |
| Grid | Collision Preset 기반으로 `Walkable`, `Penalty`, `Blocked` 생성 |
| Grid Trace | `GridTrace`를 `ECC_GameTraceChannel8`로 사용 |
| Action | Python이 `targetSpeedKmh`, `steering`, `brake`, `direction` 반환 |
| Throttle | Python 응답에는 남아 있으나 현재 주행 제어에서는 직접 사용하지 않음 |
| Version 검증 | `episodeVersion`, `configVersion`, `gridVersion` 검증 |
| Runtime Config Update | Details 패널에서 값 변경 후 CallInEditor 버튼으로 서버 반영 확인 |

## 주요 파일

| 파일 | 역할 |
| --- | --- |
| `Source/ProtoRobotSim/Public/DeliveryBot/Actor/DeliveryBot.h` | DeliveryBot Actor 공개 함수와 컴포넌트 선언 |
| `Source/ProtoRobotSim/Private/DeliveryBot/Actor/DeliveryBot.cpp` | observation, episode start, config update JSON 생성 |
| `Source/ProtoRobotSim/Public/DeliveryBot/Component/DeliveryBot_LidarSensorComponent.h` | Lidar 컴포넌트 선언 |
| `Source/ProtoRobotSim/Private/DeliveryBot/Component/DeliveryBot_LidarSensorComponent.cpp` | Lidar ray cast, hit actor 수집 |
| `Source/ProtoRobotSim/Public/Shared/Struct/DeliveryBot/Perception/DeliveryBotLidarSensorInfo.h` | Lidar ray/object 구조체 |
| `Source/ProtoRobotSim/Public/Shared/Struct/DeliveryBot/Observation/DeliveryBotObservationInfo.h` | Python으로 보낼 observation 구조체 |
| `Source/ProtoRobotSim/Public/DeliveryBot/Component/DeliveryBot_PolicyControllerComponent.h` | Policy loop, grid/config/episode 전송 제어 |
| `Source/ProtoRobotSim/Private/DeliveryBot/Component/DeliveryBot_PolicyControllerComponent.cpp` | 응답 검증, 실패 처리, policy loop 관리 |
| `Source/ProtoRobotSim/Public/DeliveryBot/Component/DeliveryBot_HttpPolicyComponent.h` | HTTP 요청/응답 컴포넌트 |
| `Source/ProtoRobotSim/Private/DeliveryBot/Component/DeliveryBot_HttpPolicyComponent.cpp` | Python 서버 HTTP 통신 |
| `Source/ProtoRobotSim/Public/DeliveryBot/Subsystem/DeliveryBot_GridSubsystem.h` | Grid 생성 Subsystem 선언 |
| `Source/ProtoRobotSim/Private/DeliveryBot/Subsystem/DeliveryBot_GridSubsystem.cpp` | Collision Preset 기반 grid 생성 |
| `Tools/PythonPolicyServer/server.py` | 테스트용 Python policy server |
| `Docs/DeliveryBot_Python_API_Current.md` | 현재 Python API 계약 문서 |
| `Docs/Episode데이터에셋.md` | Episode Object Semantic DataAsset 목표 문서 |

## 다음 작업 우선순위

| 우선순위 | 작업 | DataAsset 필요 여부 | 목표 |
| --- | --- | --- | --- |
| 1 | Observation JSON에 `actorTags` 포함 | 필요 없음 | Python이 현재 태그 기반으로 오브젝트 의미를 임시 확인 가능 |
| 2 | Python 서버 로그에 observed object 요약 추가 | 필요 없음 | Lidar 감지 객체와 태그가 정상 전달되는지 확인 |
| 3 | SetupInfo에 policy 선택 정보 추가 | 필요 없음 | 사용자가 Python policy 종류와 우선순위를 선택 가능 |
| 4 | Python policy 구조 분리 | 필요 없음 | policy mode별 함수가 아니라 policy registry/strategy 구조로 전환 |
| 5 | Policy 응답 debug 확장 | 필요 없음 | 어떤 policy가 어떤 이유로 action을 반환했는지 추적 |
| 6 | Episode DataAsset 연결 | Episode 담당자 작업 후 | `ActorTags`를 semantic 정보로 변환 |
| 7 | Observation JSON에 semantic 필드 추가 | Episode 담당자 작업 후 | Python이 `objectType`, `objectCategory`, `mobilityType` 사용 가능 |
| 8 | Spawn 교체 연동 | Episode spawn 담당자 작업 후 | 기존 Chaos Actor spawn을 DeliveryBot spawn으로 교체 |
| 9 | 3D Lidar / point cloud 고도화 | 후순위 | 센서 표현력 확장 |

## 1단계 작업: Observation JSON에 actorTags 포함

현재 `FDeliveryBotLidarRayInfo`, `FDeliveryBotLidarDetectedObjectInfo`, `FDeliveryBotLidarObservedObjectInfo`에는 `ActorTags`가 있다.  
하지만 `ADeliveryBot::BuildObservationJson()`에서 JSON으로 내보내는 부분에는 `actorTags`가 빠져 있다.

작업 위치:

```text
Source/ProtoRobotSim/Private/DeliveryBot/Actor/DeliveryBot.cpp
```

수정 함수:

```cpp
bool ADeliveryBot::BuildObservationJson(const FDeliveryBotObservationInfo& observation, FString& outJson) const
```

수정 목표:

1. `observedObjects` 각 object에 `actorTags` 배열을 추가한다.
2. `lidarRays` 각 ray에도 `actorTags` 배열을 추가한다.

예상 JSON:

```json
{
  "observedObjects": [
    {
      "actorName": "BP_RoadCone_C_12",
      "actorTags": ["ObjectType.road_cone"],
      "closestDistanceM": 2.8,
      "closestRayYawDegree": 0.0,
      "totalHitRayCount": 3,
      "frontHitRayCount": 2,
      "inFront": true
    }
  ]
}
```

확인 방법:

1. 액터 Details에서 `Tags`에 `ObjectType.road_cone` 추가
2. Python 서버 실행
3. PIE 실행
4. Python 로그 또는 Unreal HTTP body 로그에서 `actorTags` 확인

## 2단계 작업: Python 서버에서 observed object 로그 강화

작업 위치:

```text
Tools/PythonPolicyServer/server.py
```

수정 목표:

1. `/policy/action` 요청을 받을 때 `observedObjects` 개수를 출력한다.
2. 첫 번째 또는 가까운 object의 `actorName`, `actorTags`, `closestDistanceM`, `inFront`를 출력한다.
3. 태그가 없으면 빈 배열로 처리한다.

로그 예시:

```text
observation sequence=12 sensorSequence=130 policyMode=forward rays=23 objects=2 robotGridStatus=ok robotGrid=(40,47)
nearest object actor=BP_RoadCone_C_12 tags=['ObjectType.road_cone'] distanceM=2.80 inFront=True
```

이 작업은 DataAsset 없이도 바로 가능하다.

## 3단계 작업: SetupInfo에 policy 선택 정보 추가

사용자가 어떤 Python policy를 사용할지 Unreal SetupInfo에서 선택할 수 있어야 한다.

추천 구조:

```cpp
USTRUCT(BlueprintType)
struct FDeliveryBotPolicySetupInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName PolicyName{ TEXT("forward_test_policy") };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> EnabledPolicyRules{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> PolicyPriorityOrder{};
};
```

추천 위치:

```text
Source/ProtoRobotSim/Public/Shared/Struct/DeliveryBot/Policy/DeliveryBotPolicySetupInfo.h
```

그 다음 `FDeliveryBotSetupInfo`에 추가한다.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite)
FDeliveryBotPolicySetupInfo PolicySetupInfo{};
```

`ADeliveryBot::BuildEpisodeStartJson()`에서 `policySpec`으로 전송한다.

예상 JSON:

```json
{
  "policySpec": {
    "policyName": "pedestrian_priority_policy",
    "enabledPolicyRules": [
      "pedestrian_slowdown",
      "pedestrian_stop",
      "reroute_when_blocked"
    ],
    "policyPriorityOrder": [
      "emergency_stop",
      "communication_failure",
      "pedestrian_stop",
      "reroute_when_blocked",
      "normal_drive"
    ]
  }
}
```

## 4단계 작업: Python policy 구조 분리

현재 Python server는 테스트 mode 중심이다.  
앞으로는 `policyName`을 받아서 해당 policy가 판단하도록 분리한다.

추천 구조:

```text
Tools/PythonPolicyServer/
  server.py
  deliverybot_policy/
    __init__.py
    registry.py
    context.py
    actions.py
    policies/
      __init__.py
      forward_test_policy.py
      pedestrian_priority_policy.py
      cautious_static_obstacle_policy.py
```

역할:

| 파일 | 역할 |
| --- | --- |
| `registry.py` | policy name으로 policy 함수 찾기 |
| `context.py` | episode, config, grid, observation을 policy가 쓰기 좋은 형태로 정리 |
| `actions.py` | action 생성 helper |
| `forward_test_policy.py` | 현재 forward 테스트 정책 유지 |
| `pedestrian_priority_policy.py` | 보행자 우선 정책 |
| `cautious_static_obstacle_policy.py` | 정적 장애물 보수 정책 |

이 단계의 목표는 실제 고급 길찾기가 아니라 policy 교체 구조를 만드는 것이다.

## 5단계 작업: Policy debug 확장

Python 응답의 `debug`에 판단 근거를 더 넣는다.

예상 응답:

```json
{
  "debug": {
    "policyName": "pedestrian_priority_policy",
    "selectedRule": "pedestrian_stop",
    "reason": "front pedestrian inside stop distance",
    "nearestObjectType": "Pedestrian",
    "nearestObjectDistanceM": 1.4,
    "robotCellAreaType": "Walkable",
    "goalGridStatus": "ok"
  }
}
```

Unreal은 이 값을 주행 판단에 쓰지 않고 로그 분석용으로만 사용한다.

## 6단계 작업: Episode DataAsset 연결 후 DeliveryBot 확장

이 단계는 Episode 담당자가 DataAsset을 만든 뒤 진행한다.

전제:

1. `UEpisodeObjectSemanticDataAsset`이 존재한다.
2. `UEpisodeObjectSemanticRegistryDataAsset`이 존재한다.
3. 액터 Tags에 `ObjectType.<SemanticTypeId>` 형식의 태그가 들어간다.

DeliveryBot 쪽 작업:

1. `FDeliveryBotLidarObservedObjectInfo`에 semantic 필드 추가
2. `ActorTags`에서 `SemanticTypeId` 추출
3. Registry에서 DataAsset 조회
4. 조회 결과를 observation JSON에 포함

추가될 JSON 예시:

```json
{
  "actorName": "BP_RoadCone_C_12",
  "actorTags": ["ObjectType.road_cone"],
  "semanticTypeId": "road_cone",
  "objectType": "Cone",
  "objectCategory": "StaticObject",
  "mobilityType": "Static",
  "closestDistanceM": 2.8
}
```

## 7단계 작업: Spawn 교체 연동

이 작업은 Episode spawn 담당자와 맞춰 진행한다.

목표:

1. Episode JSON의 start, goal, setup 정보를 DeliveryBot에 전달한다.
2. 기존 Chaos Actor spawn 위치에 `ADeliveryBot` 또는 DeliveryBot Blueprint를 spawn한다.
3. spawn 직후 `InitializeSetupInfo()`를 호출한다.
4. `BeginPlay()` 이후 `/episode/start`가 정상 전송되는지 확인한다.

관련 문서:

```text
Docs/spawn방식변경.md
```

해당 문서가 현재 브랜치에 없다면 새로 만들어서 Episode 담당자에게 전달한다.

## 8단계 작업: 3D Lidar / Point Cloud 고도화

이 작업은 policy 구조와 semantic 정보 전달이 안정화된 뒤 진행한다.

목표:

1. `EDeliveryBotLidarModeType::ThreeD` 실제 구현
2. vertical layer, horizontal ray, point cloud 데이터 구조 추가
3. point cloud debug 시각화
4. Python으로 point cloud 요약 또는 원본 전송 방식 결정

이 단계 전에는 1D/2D Lidar와 object-level observation을 안정화하는 것이 우선이다.

## 테스트 순서

### 기본 주행 테스트

```powershell
py -3 Tools\PythonPolicyServer\server.py --host 127.0.0.1 --port 8000 --policy-mode forward
```

확인 로그:

```text
POST /episode/start
POST /policy/action
Valid policy action
```

### Runtime config update 테스트

1. PIE 실행
2. DeliveryBot Details에서 `MaxSpeedKmh` 변경
3. `SendCurrentRuntimeConfigUpdateToPolicyServerOnce()` CallInEditor 버튼 실행
4. Python 응답의 `configVersion` 증가 확인
5. 이후 `/policy/action` 응답의 `targetSpeedKmh`가 변경된 최대 속도를 반영하는지 확인

### Grid 테스트

확인 로그:

```text
DeliveryBot Grid Built | Size: 80 x 80, Cells: 6400, Walkable: ..., Penalty: ..., Blocked: ...
Grid upload response | Success: true, Code: 200
```

### ActorTags 테스트

1. 테스트 액터에 `ObjectType.road_cone` 태그 추가
2. Lidar가 해당 액터를 바라보게 배치
3. `/policy/action` JSON에서 `actorTags` 확인

## 다음 채팅에서 시작할 때 사용할 문장

다음 채팅에서는 아래 내용을 그대로 붙여 넣고 시작하면 된다.

```text
현재 DeliveryBot 작업은 Docs/DeliveryBot_Next_Work_For_New_Chat.md 기준으로 이어간다.
Episode 담당자가 추후 Object Semantic DataAsset을 만들어준다는 전제다.
우선 DataAsset 없이 가능한 1단계 작업부터 진행한다.
첫 작업은 ADeliveryBot::BuildObservationJson()에서 observedObjects와 lidarRays에 actorTags를 JSON으로 포함시키는 것이다.
코드 중심으로 함수 단위로 알려줘.
```

## 현재 가장 먼저 할 일

가장 먼저 할 일은 `ADeliveryBot::BuildObservationJson()`에 `actorTags`를 포함시키는 작업이다.

이유:

1. 이미 Lidar 구조체에는 `ActorTags`가 존재한다.
2. DataAsset이 없어도 바로 테스트할 수 있다.
3. 나중에 DataAsset 연결 시 `ObjectType.<SemanticTypeId>` 태그를 그대로 재사용할 수 있다.
4. Python policy 구조를 바꾸기 전에 observation 계약을 먼저 안정화할 수 있다.
