#pragma once

#include "CoreMinimal.h"
#include "Shared/Types/DeliveryBotPolicyActionType.h"
#include "DeliveryBotPolicyInfo.generated.h"


USTRUCT(BlueprintType)
struct FDeliveryBotPolicyContextInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasFrontObject{ false };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FrontObjectDistanceM{ 0.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StopDistanceM{ 1.2f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SlowDownDistanceM{ 3.5f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CurrentSpeedKmh{ 0.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxSpeedKmh{ 10.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bInRepathMoveGraceTime{ false };  // 바로 재탐색 안하고 약간 움직일 수 있는 시간 주기
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCanRepath{ true };
};


USTRUCT(BlueprintType)
struct FDeliveryBotPolicyDecisionInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EDeliveryBotPolicyActionType ActionType{ EDeliveryBotPolicyActionType::None };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bFromRemoteApi{ false };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Reason{};
};