# Episode Object Semantic DataAsset 구현 목표

이 문서는 Episode에서 배치되는 맵 오브젝트, 장애물, 환경 액터에 의미 정보를 부여하기 위한 DataAsset 구현 목표를 정리한다.

Episode는 액터가 어떤 종류의 오브젝트인지 식별 가능한 데이터를 제공한다.  
DeliveryBot은 Lidar에 감지된 액터에서 이 의미 정보를 읽어 Python 서버로 전달한다.  
Python Policy는 전달받은 의미 정보와 거리, 위치, grid, goal 정보를 바탕으로 행동을 결정한다.

## 구현 구조

| 구분 | 구현 내용 |
| --- | --- |
| Episode DataAsset | 오브젝트 의미 정보를 정의한다. |
| Actor Tag | 액터와 DataAsset을 연결하는 키를 제공한다. |
| Episode 배치 도구 | 배치되는 액터에 Tag를 자동으로 추가한다. |
| DeliveryBot observation | 감지된 액터의 semantic 정보를 Python으로 전달한다. |

## DataAsset 클래스

Episode 오브젝트 의미 정보를 담는 DataAsset을 만든다.

파일 위치:

```text
Source/ProtoRobotSim/Public/Shared/DataAsset/Episode/EpisodeObjectSemanticDataAsset.h
```

클래스 이름:

```cpp
UEpisodeObjectSemanticDataAsset
```

### Enum

```cpp
UENUM(BlueprintType)
enum class EEpisodeObjectCategory : uint8
{
	Unknown,
	Human,
	Vehicle,
	PersonalMobility,
	StaticObject,
	GroundObject
};

UENUM(BlueprintType)
enum class EEpisodeObjectMobilityType : uint8
{
	Unknown,
	Static,
	Dynamic
};
```

### DataAsset 필드

```cpp
UCLASS(BlueprintType)
class PROTOROBOTSIM_API UEpisodeObjectSemanticDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName SemanticTypeId{ NAME_None };

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName PythonObjectType{ NAME_None };

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EEpisodeObjectCategory ObjectCategory{ EEpisodeObjectCategory::Unknown };

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EEpisodeObjectMobilityType MobilityType{ EEpisodeObjectMobilityType::Unknown };
};
```

## 필드 역할

| 필드 | 예시 | 역할 |
| --- | --- | --- |
| `SemanticTypeId` | `road_cone` | Episode 내부에서 DataAsset을 찾기 위한 고유 식별자 |
| `PythonObjectType` | `Cone` | Python 서버로 전달되는 오브젝트 타입 |
| `ObjectCategory` | `StaticObject` | 큰 분류 정보 |
| `MobilityType` | `Static` | 정적 오브젝트인지 동적 오브젝트인지 구분 |

## Actor Tag 규칙

액터의 `Tags` 배열에 DataAsset을 찾기 위한 태그를 넣는다.

형식:

```text
ObjectType.<SemanticTypeId>
```

예시:

```text
ObjectType.road_cone
ObjectType.road_barricade
ObjectType.pedestrian
ObjectType.vehicle
```

DeliveryBot은 Lidar에 감지된 액터의 `Tags`에서 `ObjectType.` 접두사를 가진 값을 찾고, 뒤의 값을 `SemanticTypeId`로 사용한다.

## 기본 DataAsset 목록

Episode에서 아래 DataAsset을 먼저 만든다.

| Asset 이름 | SemanticTypeId | PythonObjectType | ObjectCategory | MobilityType |
| --- | --- | --- | --- | --- |
| `DA_EpisodeObject_Unknown` | `unknown` | `Unknown` | `Unknown` | `Unknown` |
| `DA_EpisodeObject_RoadCone` | `road_cone` | `Cone` | `StaticObject` | `Static` |
| `DA_EpisodeObject_RoadBarricade` | `road_barricade` | `Barricade` | `StaticObject` | `Static` |
| `DA_EpisodeObject_Box` | `box` | `Box` | `StaticObject` | `Static` |
| `DA_EpisodeObject_TrashBin` | `trash_bin` | `TrashBin` | `StaticObject` | `Static` |
| `DA_EpisodeObject_Manhole` | `manhole` | `Manhole` | `GroundObject` | `Static` |
| `DA_EpisodeObject_Pedestrian` | `pedestrian` | `Pedestrian` | `Human` | `Dynamic` |
| `DA_EpisodeObject_Vehicle` | `vehicle` | `Vehicle` | `Vehicle` | `Dynamic` |
| `DA_EpisodeObject_Bicycle` | `bicycle` | `Bicycle` | `PersonalMobility` | `Dynamic` |
| `DA_EpisodeObject_Kickboard` | `kickboard` | `Kickboard` | `PersonalMobility` | `Dynamic` |

## DataAsset Registry

`SemanticTypeId`로 DataAsset을 찾을 수 있는 Registry를 만든다.

파일 위치:

```text
Source/ProtoRobotSim/Public/Shared/DataAsset/Episode/EpisodeObjectSemanticRegistryDataAsset.h
```

클래스 이름:

```cpp
UEpisodeObjectSemanticRegistryDataAsset
```

필드:

```cpp
UCLASS(BlueprintType)
class PROTOROBOTSIM_API UEpisodeObjectSemanticRegistryDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FName, TObjectPtr<UEpisodeObjectSemanticDataAsset>> SemanticDataAssets;
};
```

Registry 예시:

| Key | Value |
| --- | --- |
| `road_cone` | `DA_EpisodeObject_RoadCone` |
| `road_barricade` | `DA_EpisodeObject_RoadBarricade` |
| `pedestrian` | `DA_EpisodeObject_Pedestrian` |

## Episode 배치 규칙

Episode에서 액터를 배치할 때 해당 액터에 `ObjectType.<SemanticTypeId>` 태그를 추가한다.

예시:

| 배치 액터 | 추가할 Tag |
| --- | --- |
| 콘 | `ObjectType.road_cone` |
| 바리케이드 | `ObjectType.road_barricade` |
| 박스 | `ObjectType.box` |
| 보행자 | `ObjectType.pedestrian` |
| 차량 | `ObjectType.vehicle` |

StaticObstacle 배치 기능이 있다면, 오브젝트 종류를 선택했을 때 Actor Tag가 자동으로 들어가게 만든다.

## DeliveryBot으로 전달될 데이터 형태

Lidar가 액터를 감지하면 DeliveryBot observation에 다음 semantic 정보를 포함한다.

```json
{
  "actorName": "BP_RoadCone_C_12",
  "distanceM": 2.8,
  "semanticTypeId": "road_cone",
  "objectType": "Cone",
  "objectCategory": "StaticObject",
  "mobilityType": "Static"
}
```

## 구현 순서

| 순서 | 작업 | 결과 |
| --- | --- | --- |
| 1 | `UEpisodeObjectSemanticDataAsset` 생성 | 오브젝트 의미 정보를 담는 DataAsset 클래스가 생긴다. |
| 2 | `EEpisodeObjectCategory`, `EEpisodeObjectMobilityType` 생성 | Python에 전달할 분류 체계가 생긴다. |
| 3 | 기본 DataAsset 생성 | Cone, Barricade, Pedestrian 등 기본 오브젝트 타입을 등록한다. |
| 4 | `UEpisodeObjectSemanticRegistryDataAsset` 생성 | `SemanticTypeId`로 DataAsset을 찾을 수 있다. |
| 5 | Episode 배치 액터에 Tag 부여 | 감지된 액터에서 `SemanticTypeId`를 얻을 수 있다. |
| 6 | StaticObstacle 배치 시 Tag 자동 추가 | 수동 입력 없이 태그가 유지된다. |
| 7 | DeliveryBot observation에 semantic 필드 추가 | Python 서버가 오브젝트 의미 정보를 받을 수 있다. |

## 완료 기준

1. Episode DataAsset으로 오브젝트 의미 정보를 정의할 수 있다.
2. Actor Tag에서 `SemanticTypeId`를 읽을 수 있다.
3. `SemanticTypeId`로 Registry에서 DataAsset을 찾을 수 있다.
4. Lidar에 감지된 액터의 semantic 정보가 observation JSON에 포함된다.
5. Python 서버 로그에서 `semanticTypeId`, `objectType`, `objectCategory`, `mobilityType`을 확인할 수 있다.
