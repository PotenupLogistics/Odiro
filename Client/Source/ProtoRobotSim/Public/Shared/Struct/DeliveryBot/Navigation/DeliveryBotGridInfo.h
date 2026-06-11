#pragma once

#include "CoreMinimal.h"
#include "DeliveryBotGridInfo.generated.h"

UENUM(BlueprintType)
enum class EDeliveryBotGridAreaType : uint8
{
	Walkable,
	Penalty,
	Blocked
};
USTRUCT(BlueprintType)
struct FDeliveryBotGridCollisionRuleInfo
{
	GENERATED_BODY()

	// 이 Collision Preset을 가진 컴포넌트를 만났을 때 적용할 규칙
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName CollisionProfileName{ NAME_None };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EDeliveryBotGridAreaType AreaType{ EDeliveryBotGridAreaType::Walkable };

	// Walkable은 보통 1.0, Penalty는 더 큰 값, Blocked는 BIG_NUMBER로 사용
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Cost{ 1.f };

	// true면 AreaType이 무엇이든 최종적으로 Blocked로 처리한다
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bBlocksMovement{ false };

};