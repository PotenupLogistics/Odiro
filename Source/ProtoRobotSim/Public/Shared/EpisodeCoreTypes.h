#pragma once

#include "CoreMinimal.h"
#include "EpisodeCoreTypes.generated.h"

// 에피소드 전반에서 공유하는 가장 작은 공통 타입들을 모아둔 파일임.

UENUM(BlueprintType)
enum class EEpisodeParamValueType : uint8
{
	None,
	Bool,
	Integer,
	Float,
	String,
	Vector
};

UENUM(BlueprintType)
enum class EEpisodeActorCategory : uint8
{
	StaticObstacle,
	Pedestrian,
	RoadVehicle,
	PersonalMobility
};

UENUM(BlueprintType)
enum class EEpisodeMobilityMode : uint8
{
	Static,
	Parked,
	Moving
};

UENUM(BlueprintType)
enum class EEpisodeRoadVehicleState : uint8
{
	Parked,
	Driving,
	PullingOut
};

UENUM(BlueprintType)
enum class EEpisodePersonalMobilityType : uint8
{
	Bicycle,
	PM,
	Scooter,
	Motorcycle
};

UENUM(BlueprintType)
enum class EEpisodePedestrianProfile : uint8
{
	Adult,
	Child,
	Elderly
};

// JSON으로 옮길 수 있는 파라미터 값을 담기 위한 작은 variant 타입임.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeParamValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	EEpisodeParamValueType Type = EEpisodeParamValueType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	bool BoolValue = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	int32 IntegerValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	double FloatValue = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FString StringValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FVector VectorValue = FVector::ZeroVector;
};

// Unreal FTransform을 외부 JSON 명세에 반영하기 위한 DTO임.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodeTransformDto
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	FVector Scale = FVector::OneVector;
};
