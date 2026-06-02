#pragma once
#include "CoreMinimal.h"

#include "DeliveryBotPolicyConfigInfo.generated.h"

USTRUCT(BlueprintType)
struct FDeliveryBotPolicyConfigInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bUseRemotePolicy{ false };  // true 면 api 통신

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString PolicyServerUrl{ TEXT("http://127.0.0.1:8000/policy/decision") };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RequestTimeoutSecond{ 1.0f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinRequestIntervalSecond{ 0.5f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bUseSlowDownPolicy{ true };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bUseStopPolicy{ true };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bUseRepathPolicy{ true };
};