#pragma once

#include "CoreMinimal.h"
#include "DeliveryBotBodyConfigInfo.generated.h"

// profile.json robot.body 필드를 런타임 vehicle spec 단위로 보관한다.
USTRUCT(BlueprintType)
struct FDeliveryBotBodyConfigInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasSetupBodyConfig{ false }; // JSON이 robot.body를 명시했는지 여부

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LengthM{ 0.6f }; // 로봇 전후 길이, m 단위

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float WidthM{ 0.9f }; // 로봇 좌우 폭, m 단위

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HeightM{ 0.5f }; // 로봇 높이, m 단위

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float WheelBaseM{ 0.42f }; // 앞뒤 바퀴 축 사이 거리, m 단위

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TurningRadiusM{ 3.0f }; // 최소 회전 반경, m 단위
};
