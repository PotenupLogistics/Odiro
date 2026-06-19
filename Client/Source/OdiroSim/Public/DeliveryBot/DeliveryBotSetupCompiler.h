#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Shared/ScenarioCompileTypes.h"
#include "Shared/Struct/DeliveryBot/Setup/DeliveryBotSetupInfo.h"
#include "UObject/Object.h"
#include "DeliveryBotSetupCompiler.generated.h"

class FJsonObject;

// DeliveryBotSetup JSON을 FDeliveryBotSetupInfo로 변환하는 컴파일러.
// ScenarioSetup이 담당하는 로봇 배치/목적지는 읽지 않고, 로봇 주행/센서/정책 설정만 다룬다.
USTRUCT(BlueprintType)
struct ODIROSIM_API FDeliveryBotSetupCompileResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Setup")
	bool bSuccess = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Setup")
	FDeliveryBotSetupInfo SetupInfo;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Setup")
	FString SpecHash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Setup")
	TArray<FScenarioCompileDiagnostic> Diagnostics;
};

UCLASS(BlueprintType)
class ODIROSIM_API UDeliveryBotSetupCompiler : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Setup")
	FDeliveryBotSetupCompileResult CompileDeliveryBotSetupFromJsonFile(const FString& jsonFilePath) const;

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Setup")
	FDeliveryBotSetupCompileResult CompileDeliveryBotSetupFromJsonString(const FString& jsonString) const;

private:
	static void AddDiagnostic(FDeliveryBotSetupCompileResult& result, EScenarioCompileDiagnosticSeverity severity, const FString& code, const FString& message);
	static bool HasErrors(const FDeliveryBotSetupCompileResult& result);

	static bool ReadOptionalFloatField(const FJsonObject& jsonObject, const FString& fieldName, const FString& path, FDeliveryBotSetupCompileResult& result, float& targetValue, float minValue, float maxValue = TNumericLimits<float>::Max());
	static bool ReadOptionalIntField(const FJsonObject& jsonObject, const FString& fieldName, const FString& path, FDeliveryBotSetupCompileResult& result, int32& targetValue, int32 minValue, int32 maxValue = TNumericLimits<int32>::Max());
	static bool ReadOptionalBoolField(const FJsonObject& jsonObject, const FString& fieldName, const FString& path, FDeliveryBotSetupCompileResult& result, bool& targetValue);
	static bool ReadOptionalNameArrayField(const FJsonObject& jsonObject, const FString& fieldName, const FString& path, FDeliveryBotSetupCompileResult& result, TArray<FName>& targetValue);
	static bool ReadOptionalCollisionChannelField(const FJsonObject& jsonObject, const FString& fieldName, const FString& path, FDeliveryBotSetupCompileResult& result, TEnumAsByte<ECollisionChannel>& targetValue);
	static bool ReadOptionalLidarModeField(const FJsonObject& jsonObject, const FString& fieldName, const FString& path, FDeliveryBotSetupCompileResult& result, EDeliveryBotLidarModeType& targetValue);
	
	static void CompileRobotObject(const FJsonObject& rootObject, FDeliveryBotSetupCompileResult& result);
	static void CompileDrive(const FJsonObject& robotObject, FDeliveryBotSetupCompileResult& result, FDeliveryBotDriveConfigInfo& driveConfigInfo);
	static void WarnDeprecatedPathFollow(const FJsonObject& robotObject, FDeliveryBotSetupCompileResult& result);
	static void CompileLidar(const FJsonObject& robotObject, FDeliveryBotSetupCompileResult& result, FDeliveryBotLidarSensorConfigInfo& lidarSensorConfigInfo, FDeliveryBotPointCloudCaptureConfigInfo& pointCloudCaptureConfigInfo);
	static void CompilePointCloudCapture(const FJsonObject& lidarObject, FDeliveryBotSetupCompileResult& result, FDeliveryBotPointCloudCaptureConfigInfo& pointCloudCaptureConfigInfo);
	static bool ReadOptionalStringField(const FJsonObject& jsonObject, const FString& fieldName, const FString& path, FDeliveryBotSetupCompileResult& result, FString& targetValue);
	
	static void CompilePolicy(const FJsonObject& robotObject, FDeliveryBotSetupCompileResult& result, FString& startupPolicySpecFileName);
};
