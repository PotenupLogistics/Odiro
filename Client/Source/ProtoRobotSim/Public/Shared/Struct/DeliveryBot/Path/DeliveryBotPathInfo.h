#pragma once

#include "CoreMinimal.h"
#include "DeliveryBotPathInfo.generated.h"

UENUM(BlueprintType)
enum class EDeliveryBotMoveDirectionType : uint8
{
	Forward,
	Reverse
};

USTRUCT(BlueprintType)
struct FDeliveryBotPathPointInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector LocationCm{ FVector::ZeroVector };// 이 경로점의 위치

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HeadingRadian{ 0.f };// 이 위치에서 로봇이 바라보는 방향

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EDeliveryBotMoveDirectionType MoveDirectionType{ EDeliveryBotMoveDirectionType::Forward };// 전진인지 후진인지
};	