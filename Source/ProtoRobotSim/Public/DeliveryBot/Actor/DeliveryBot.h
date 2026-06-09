// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "WheeledVehiclePawn.h"
#include "Shared/Struct/DeliveryBot/Perception/DeliveryBotLidarSensorInfo.h"
#include "Shared/Struct/DeliveryBot/Drive/DeliveryBotMovementInfo.h"
#include "Shared/Struct/DeliveryBot/Setup/DeliveryBotSetupInfo.h"
#include "Shared/Struct/DeliveryBot/Observation/DeliveryBotObservationInfo.h"
#include "DeliveryBot.generated.h"

USTRUCT(BlueprintType)
struct FDeliveryBotSensorSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDeliveryBotLidarScanInfo LidarScanInfo{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FDeliveryBotLidarDetectedObjectInfo> DetectedObjects{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDeliveryBotLidarDetectedObjectInfo FrontObjectInfo{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasFrontObject{ false };
};

class UDeliveryBot_PolicyControllerComponent;
class UDeliveryBot_HttpPolicyComponent;
class UDeliveryBot_DriveComponent;
class UDeliveryBot_LidarSensorComponent;
class UEpisodePlaceableComponent;
UCLASS(Blueprintable)
class PROTOROBOTSIM_API ADeliveryBot : public AWheeledVehiclePawn
{
	GENERATED_BODY()

public:
	ADeliveryBot();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaTime) override;

public:
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Setup")
	void InitializeSetupInfo(const FDeliveryBotSetupInfo& setupInfo);

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Drive")
	void ApplyMoveCommand(const FDeliveryBotMoveCommandInfo& moveCommandInfo, float deltaTime);

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Drive")
	void ApplyParkingStop();

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Setup")
	void ApplyRuntimeDriveConfigInfo(const FDeliveryBotDriveConfigInfo& driveConfigInfo);

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Setup")
	void ApplyCurrentSetupInfoToRuntimeComponents();

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Policy")
	bool StartPolicyRunWithPolicySpecFileName(const FString& policySpecFileName);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "DeliveryBot|Debug")
	void SendCurrentRuntimeConfigUpdateToPolicyServerOnce();
	
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Sensor")
	void UpdateSensorSnapshot();

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Observation")
	FDeliveryBotObservationInfo BuildPolicyObservation();
	
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Sensor")
	bool GetSensorSnapshot(FDeliveryBotSensorSnapshot& outSnapshot) const;

	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Measurement")
	bool GetLastMoveCommandInfo(FDeliveryBotMoveCommandInfo& outMoveCommandInfo, FString& outActionReason) const;
	
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Observation")
	FDeliveryBotObservationInfo BuildObservation() const;
	
	TArray<FDeliveryBotLidarObservedObjectInfo> BuildObservedObjectsForPolicy() const;
	
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Observation")
	bool BuildObservationJson(const FDeliveryBotObservationInfo& observation, FString& outJson) const;
	
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool BuildEpisodeStartJson(FString& outJson) const;
	
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool BuildEpisodeConfigUpdateJson(FString& outJson) const;
	
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|HttpPolicy")
	bool SendPolicyObservationOnce();
	
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Policy")
	float GetMaxPolicySpeedKmh(EDeliveryBotMoveDirectionType moveDirectionType) const;
	
	
private:
	void ApplySetupInfo();

	void FillObservation(FDeliveryBotObservationInfo& observation) const;
	void DebugLogObservation(float deltaTime);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Component")
	TObjectPtr<UDeliveryBot_DriveComponent> DriveComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Component")
	TObjectPtr<UDeliveryBot_LidarSensorComponent> LidarSensorComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Component")
	TObjectPtr<UDeliveryBot_HttpPolicyComponent> HttpPolicyComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Component")
	TObjectPtr<UDeliveryBot_PolicyControllerComponent> PolicyControllerComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Component")
	TObjectPtr<UEpisodePlaceableComponent> PlaceableComponent;
	
	
	
protected:
	float DebugLogElapsedSeconds{ 0.f };
	int32 SensorSnapshotSequence{ 0 };
	int32 PolicyObservationSequence{ 0 };
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Setup")
	FDeliveryBotSetupInfo SetupInfo{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|VehicleSpec")
	FVector RobotBoxExtentCm{ 60.f, 90.f, 25.f }; // 길찾기 할 때 쓰이는 로봇의 충돌 박스 사이즈

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|VehicleSpec")
	float MinTurningRadiusCm{ 300.f }; //  최소 회전 반경

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Debug")
	bool bLogPolicyObservationRequests{ false };
	

private:
	FDeliveryBotSensorSnapshot LastSensorSnapshot{};
	FDeliveryBotMoveCommandInfo LastMoveCommandInfo{};
	FString LastActionReason{ TEXT("unknown") };
	bool bHasLastMoveCommand{ false };
	
};
