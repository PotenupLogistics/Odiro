#pragma once

#include "CoreMinimal.h"
#include "DeliveryBotPythonSettings.generated.h"

USTRUCT(BlueprintType)
struct ODIROSIM_API FDeliveryBotPythonSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Python")
	bool bAutoLaunchPythonServer{ true };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Python")
	bool bStopServerOnShutdown{ true };


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Python")
	FString PythonExecutablePath{ TEXT("py") }; // Policy runtime을 실행할 Python launcher 또는 실행 파일

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Python")
	FString ServerScriptRelativePath{ TEXT("Resources/policy-runtime.py") };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Python")
	FString Host{ TEXT("127.0.0.1") };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Python")
	FString HealthEndpointPath{ TEXT("/health") };


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Python", meta = (ClampMin = "1"))
	int32 Port{ 18145 };


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Python", meta = (ClampMin = "0.1"))
	float StartupTimeoutSeconds{ 5.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Python", meta = (ClampMin = "0.05"))
	float HealthCheckIntervalSeconds{ 0.25f };


};
