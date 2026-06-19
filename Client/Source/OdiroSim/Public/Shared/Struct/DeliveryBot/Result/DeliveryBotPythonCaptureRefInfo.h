#pragma once

#include "CoreMinimal.h"
#include "DeliveryBotPythonCaptureRefInfo.generated.h"

// Python policy response가 반환한 capture artifact 참조를 저장한다.
USTRUCT(BlueprintType)
struct FDeliveryBotPythonCaptureRefInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString CaptureType{}; // capture artifact 종류

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SensorId{}; // capture를 만든 sensor id

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SensorSequence{ INDEX_NONE }; // capture 시점 sensor sequence

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SensorTimeSeconds{ 0.f }; // capture 시점 sensor 시간

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RunTimeSeconds{ 0.f }; // capture 시점 episode runtime

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Format{}; // capture 파일 포맷

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Path{}; // episode-relative capture artifact 경로
};