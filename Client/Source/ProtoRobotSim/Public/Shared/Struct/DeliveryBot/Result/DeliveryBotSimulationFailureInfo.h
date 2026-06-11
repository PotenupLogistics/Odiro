#pragma once

#include "CoreMinimal.h"
#include "Shared/Struct/DeliveryBot/Policy/DeliveryBotPolicyFailureInfo.h"
#include "DeliveryBotSimulationFailureInfo.generated.h"

class AActor;

UENUM(BlueprintType)
enum class EDeliveryBotSimulationFailureType : uint8
{
	None,
	PolicyRequestFailed, // Python 정책 서버 요청 실패, 응답 오류, timeout 등
	RobotTipOver, // 로봇 전복
	Collision, //  충돌
	PathFindingFailed, //  A* / Hybrid A* 등 경로 생성 실패
	Stuck, // 일정 시간 움직이지 못함
	Timeout // DeliveryBot 내부 제한 시간 초과
};

USTRUCT(BlueprintType)
struct FDeliveryBotSimulationFailureInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EDeliveryBotSimulationFailureType FailureType{ EDeliveryBotSimulationFailureType::None };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Message{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector LocationCm{ FVector::ZeroVector };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TimeSeconds{ 0.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<AActor> TargetActor{ nullptr };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpeedKmh{ 0.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasPolicyFailureInfo{ false };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDeliveryBotPolicyFailureInfo PolicyFailureInfo{};
};
