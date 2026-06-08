# DeliveryBot Spawn 방식 변경 가이드

## 목적

현재 Episode 실행 시 로봇은 `ADeliveryBot_ChaosActor` 기준으로 소환된다.

새 기획 방향에서는 최종 로봇 Actor를 `ADeliveryBot`으로 유지하고, Python policy 서버가 pathfinding/action 판단을 담당해야 한다. 따라서 Episode JSON에서 읽은 Start/Goal/Drive/Lidar 설정을 기존 ChaosActor가 아니라 새 `ADeliveryBot`에 넣어 소환하는 경로가 필요하다.

이 문서는 spawn 담당자가 기존 Episode spawn 구조를 유지하면서 `ADeliveryBot` 소환 경로를 추가할 수 있도록 수정 위치와 권장 코드를 정리한다.

## 현재 구조 요약

| 항목 | 현재 상태 |
|---|---|
| Episode JSON compile | `actors.robot`에서 Start/Goal을 읽어 `FDeliveryBotSetupInfo.LocationSetupInfo`에 저장 |
| Robot 설정 JSON | `UDeliveryBotSetupCompiler`가 `drive`, `path_follow`, `lidar`를 `FDeliveryBotSetupInfo`로 컴파일 |
| Episode spawn | `UEpisodeSimulationSubsystem::SpawnRobotActor()`가 `ADeliveryBot_ChaosActor`를 소환 |
| 새 DeliveryBot | `ADeliveryBot::InitializeSetupInfo()`로 설정 주입 가능 |
| Python 연동 | `ADeliveryBot` 내부 `PolicyControllerComponent`가 `/episode/start`, `/policy/action`, `/episode/config/update` 처리 |

## 핵심 변경 방향

한 번에 기존 ChaosActor를 제거하지 말고, 먼저 Episode spawn에 새 `ADeliveryBot` 경로를 병렬로 추가하는 것을 권장한다.

권장 흐름:

```text
Episode JSON
 -> EpisodeCompiler
 -> FEpisodePlaceableInstanceSpec.DeliveryBot.SetupInfo
 -> EpisodeSimulationSubsystem::SpawnRobotActor()
 -> ADeliveryBot::InitializeSetupInfo(setupInfo)
 -> ADeliveryBot BeginPlay
 -> /episode/start
 -> Python policy loop
```

중요한 점:

- `ADeliveryBot::InitializeSetupInfo(setupInfo)`는 반드시 `FinishSpawningActor()` 전에 호출한다.
- 그래야 `BeginPlay()` 시점에 Drive/Lidar/Start/Goal 설정이 적용된 상태로 policy controller가 초기화된다.
- 기존 평가/측정 시스템은 아직 `ADeliveryBot_ChaosActor`에 의존하므로, spawn 교체와 평가/측정 교체는 분리해서 진행한다.

## 수정 대상 파일

| 파일 | 작업 |
|---|---|
| `Source/ProtoRobotSim/Public/Episode/EpisodeSimulationSubsystem.h` | 새 `ADeliveryBot` class forward declaration 및 spawn class property 추가 |
| `Source/ProtoRobotSim/Private/Episode/EpisodeSimulationSubsystem.cpp` | 새 `ADeliveryBot` include 추가, constructor 기본 class 설정, `SpawnRobotActor()` 분기 추가 |
| Blueprint 설정 | `DeliveryBotActorClass`에 새 `BP_DeliveryBot` 지정 |

## 1. Header 수정

대상:

```text
Source/ProtoRobotSim/Public/Episode/EpisodeSimulationSubsystem.h
```

현재는 기존 Actor만 forward declaration 되어 있다.

```cpp
class ADeliveryBot_ChaosActor;
```

아래처럼 새 Actor를 추가한다.

```cpp
class ADeliveryBot;
class ADeliveryBot_ChaosActor;
```

기존 `RobotActorClass`는 유지하고, 새 Actor class를 추가한다.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Classes")
TSubclassOf<ADeliveryBot> DeliveryBotActorClass;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Classes")
TSubclassOf<ADeliveryBot_ChaosActor> RobotActorClass;
```

역할:

- `DeliveryBotActorClass`: 새 최종 DeliveryBot 소환용
- `RobotActorClass`: 기존 ChaosActor fallback 유지용

기존 이름을 바로 바꾸지 않는 이유:

- `EpisodeEvaluationSubsystem`, `EpisodeMeasurementLogSubsystem` 등이 아직 `ADeliveryBot_ChaosActor`를 참조한다.
- 이름 변경까지 동시에 하면 수정 범위가 불필요하게 커진다.

## 2. CPP include 수정

대상:

```text
Source/ProtoRobotSim/Private/Episode/EpisodeSimulationSubsystem.cpp
```

기존 include 근처에 새 Actor include를 추가한다.

```cpp
#include "DeliveryBot/Actor/DeliveryBot.h"
#include "DeliveryBot/Actor/DeliveryBot_ChaosActor.h"
```

## 3. Constructor 기본 class 설정

대상:

```cpp
UEpisodeSimulationSubsystem::UEpisodeSimulationSubsystem()
```

기존에는 `BP_DeliveryBot_ChaosMesh`를 기본 로봇 class로 잡는다.

```cpp
static ConstructorHelpers::FClassFinder<ADeliveryBot_ChaosActor> robotBlueprintClass(
	TEXT("/Game/Blueprints/Vehicle/BP_DeliveryBot_ChaosMesh"));
```

새 Actor BP가 준비되어 있다면 아래 경로는 실제 프로젝트 BP 경로에 맞춰 지정한다.

```cpp
static ConstructorHelpers::FClassFinder<ADeliveryBot> deliveryBotBlueprintClass(
	TEXT("/Game/Blueprints/Vehicle/BP_DeliveryBot"));

if (deliveryBotBlueprintClass.Succeeded())
{
	DeliveryBotActorClass = deliveryBotBlueprintClass.Class;
}
else
{
	DeliveryBotActorClass = ADeliveryBot::StaticClass();
}
```

그리고 기존 ChaosActor class 설정은 fallback으로 유지한다.

```cpp
static ConstructorHelpers::FClassFinder<ADeliveryBot_ChaosActor> robotBlueprintClass(
	TEXT("/Game/Blueprints/Vehicle/BP_DeliveryBot_ChaosMesh"));

if (robotBlueprintClass.Succeeded())
{
	RobotActorClass = robotBlueprintClass.Class;
}
else
{
	RobotActorClass = ADeliveryBot_ChaosActor::StaticClass();
}
```

주의:

- `BP_DeliveryBot` 경로가 아직 다르면 실제 asset path로 바꿔야 한다.
- 새 BP가 아직 없다면 `ADeliveryBot::StaticClass()` fallback만으로 C++ spawn 테스트는 가능하다.

## 4. SpawnRobotActor 변경

대상:

```cpp
AActor* UEpisodeSimulationSubsystem::SpawnRobotActor(const FEpisodePlaceableInstanceSpec& placeableSpec)
```

현재 함수는 `setupInfo`, `bSpawnOnly`, `bHasGoal`, `goalLocation`을 계산한 뒤 `ADeliveryBot_ChaosActor`를 spawn한다.

계산부는 그대로 유지하고, 실제 Actor spawn 부분만 새 `ADeliveryBot` 우선으로 바꾼다.

### 새 DeliveryBot 우선 spawn 코드

`setupInfo.LocationSetupInfo.bAutoStartRoute` 계산이 끝난 뒤, 기존 `ADeliveryBot_ChaosActor* robotActor` spawn 블록 대신 아래 분기를 넣는다.

```cpp
if (DeliveryBotActorClass)
{
	ADeliveryBot* robotActor = world->SpawnActorDeferred<ADeliveryBot>(
		DeliveryBotActorClass,
		placeableSpec.Transform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn
	);

	if (!robotActor)
	{
		return nullptr;
	}

	robotActor->InitializeSetupInfo(setupInfo);

	UGameplayStatics::FinishSpawningActor(
		robotActor,
		placeableSpec.Transform
	);

	RegisterRuntimeActor(
		placeableSpec.InstanceId,
		placeableSpec.AssetId,
		placeableSpec.Category,
		EEpisodeMobilityMode::Moving,
		robotActor);

	// Goal/Start marker spawn 로직은 기존 코드와 동일하게 유지한다.
	if (!bSpawnOnly)
	{
		if (!bHasGoal)
		{
			UE_LOG(LogEpisodeSimulation, Warning, TEXT("Robot '%s' has no goal. Route marker spawn skipped."), *placeableSpec.InstanceId);
			return robotActor;
		}

		if (GoalPointClass)
		{
			FActorSpawnParameters spawnParams;
			spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			if (AActor* goalPointActor = world->SpawnActor<AActor>(GoalPointClass, FTransform(FRotator::ZeroRotator, goalLocation), spawnParams))
			{
				RuntimeActors.Add(goalPointActor);
			}
		}

		if (StartPointClass)
		{
			FActorSpawnParameters spawnParams;
			spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			if (AActor* startPointActor = world->SpawnActor<AActor>(StartPointClass, FTransform(placeableSpec.Transform), spawnParams))
			{
				RuntimeActors.Add(startPointActor);
			}
		}
	}

	return robotActor;
}
```

역할:

- 새 `ADeliveryBot`을 deferred spawn한다.
- `InitializeSetupInfo(setupInfo)`로 Episode JSON에서 온 Start/Goal/Drive/Lidar 설정을 주입한다.
- `FinishSpawningActor()` 이후 `BeginPlay()`가 정상 흐름으로 실행된다.
- 이후 `ADeliveryBot` 내부 policy controller가 Python 서버와 통신한다.

## 5. 기존 ChaosActor fallback 유지

새 `DeliveryBotActorClass`가 비어 있을 때는 기존 코드를 fallback으로 유지한다.

현재 기존 코드:

```cpp
ADeliveryBot_ChaosActor* robotActor{
	world->SpawnActorDeferred<ADeliveryBot_ChaosActor>(
		RobotActorClass,
		placeableSpec.Transform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn
	)
};
```

이 블록은 새 DeliveryBot 분기 뒤에 남겨둔다.

주의:

- 기존 `OnDeliveryBotSimulationFailed` evaluation binding은 `ADeliveryBot_ChaosActor` 전용이다.
- 새 `ADeliveryBot`에는 아직 동일 이벤트가 없으므로 새 Actor 분기에서는 이 binding을 하지 않는다.
- 평가/측정 시스템 교체는 별도 단계로 진행한다.

## 6. Blueprint 설정

새 BP가 있다면 아래처럼 설정한다.

```text
BP 또는 Subsystem 기본값
 -> Episode|Classes
 -> DeliveryBotActorClass = BP_DeliveryBot
```

만약 이 값이 비어 있으면 C++ fallback인 `ADeliveryBot::StaticClass()` 또는 기존 ChaosActor fallback을 사용한다.

## 7. 테스트 순서

Python 서버 실행:

```powershell
py -3 Tools\PythonPolicyServer\server.py --host 127.0.0.1 --port 8000 --policy-mode forward
```

Episode 실행 후 확인할 Unreal 로그:

```text
Policy controller initialized
Episode start request sent
Episode start response | Success: true
Expected policy versions updated
Policy loop started
Policy response ... "targetSpeedKmh": ...
Valid policy action
```

Python 서버 로그:

```text
POST /episode/start
observation sequence=...
POST /policy/action
```

성공 기준:

| 확인 항목 | 기대 결과 |
|---|---|
| Actor class | `BP_DeliveryBot` 또는 `ADeliveryBot`이 spawn됨 |
| Start/Goal | Episode JSON의 위치가 `/episode/start`에 포함됨 |
| Grid | `gridReceived=true` |
| Policy | `/policy/action` 반복 호출 |
| Action | `Valid policy action` 로그 출력 |
| 이동 | Python 응답의 `targetSpeedKmh`, `steering`, `direction` 기준으로 이동 |

## 8. 아직 건드리지 말아야 할 영역

다음 시스템은 현재 기존 ChaosActor 의존성이 크다.

| 시스템 | 현재 의존 |
|---|---|
| `EpisodeEvaluationSubsystem` | `ADeliveryBot_ChaosActor`, `OnDeliveryBotSimulationFailed` |
| `EpisodeMeasurementLogSubsystem` | `ADeliveryBot_ChaosActor` 검색 및 snapshot |
| `EpisodeRobotMeasurementAdapter` | `ADeliveryBot_ChaosActor` 기반 snapshot |

권장 순서:

1. Episode spawn에서 새 `ADeliveryBot` 소환 확인
2. Python policy 주행 확인
3. Start/Goal/grid/config 정상 전송 확인
4. 그 다음 평가/측정 시스템을 `ADeliveryBot` 기준으로 점진 교체

## 결론

이번 변경의 핵심은 `EpisodeSimulationSubsystem::SpawnRobotActor()`에서 기존 `ADeliveryBot_ChaosActor` 대신 새 `ADeliveryBot`을 우선 소환하도록 만드는 것이다.

단, 기존 평가/측정 시스템이 아직 ChaosActor를 참조하므로, 기존 Actor class는 fallback으로 남기는 것이 안전하다. 새 DeliveryBot spawn과 Python policy 주행이 안정화된 뒤 평가/측정 시스템을 별도로 마이그레이션한다.
