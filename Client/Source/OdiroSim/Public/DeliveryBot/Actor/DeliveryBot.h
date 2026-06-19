
#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "Shared/Struct/DeliveryBot/Perception/DeliveryBotLidarSensorInfo.h"
#include "Shared/Struct/DeliveryBot/Drive/DeliveryBotMovementInfo.h"
#include "Shared/Struct/DeliveryBot/Setup/DeliveryBotSetupInfo.h"
#include "Shared/Struct/DeliveryBot/Observation/DeliveryBotObservationInfo.h"
#include "Shared/Struct/DeliveryBot/Result/DeliveryBotPythonCaptureRefInfo.h"
#include "Shared/Struct/DeliveryBot/Result/DeliveryBotPolicyDecisionResultInfo.h"
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

	// 이 센서 snapshot이 만들어진 고정 시뮬레이션 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SimulationTimeSeconds{ 0.f };
};

class UDeliveryBot_HttpPolicyComponent;
class UDeliveryBot_DriveComponent;
class UDeliveryBot_LidarSensorComponent;
class UScenarioPlaceableComponent;
class UPrimitiveComponent;
struct FHitResult;

UCLASS(Blueprintable)
class ODIROSIM_API ADeliveryBot : public AWheeledVehiclePawn
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

	void ConfigureProjectActionLogging(const FString& projectOutputEpisodeId); // project actions.jsonl 기록 대상 output episode를 전달한다

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Observation")
	FDeliveryBotObservationInfo BuildPolicyObservation();

	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Sensor")
	bool GetSensorSnapshot(FDeliveryBotSensorSnapshot& outSnapshot) const;

	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Measurement")
	bool GetLastMoveCommandInfo(FDeliveryBotMoveCommandInfo& outMoveCommandInfo, FString& outActionReason) const;

	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Observation")
	FDeliveryBotObservationInfo BuildObservation() const;

	TArray<FDeliveryBotLidarObservedObjectInfo> BuildObservedObjectsForPolicy() const;

	// 현재 로봇 setup 정보를 읽기 전용으로 반환한다.
	const FDeliveryBotSetupInfo& GetSetupInfo() const { return SetupInfo; }

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Python")
	void NotifyGoalReachedByEvaluation(); // 평가 시스템이 목표 도착을 알렸을 때 Python 서버에 종료를 요청한다

	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Python")
	FString GetLastPythonScenarioResultJson() const; // Python 서버에서 받은 마지막 scenario result JSON을 반환한다

	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Python")
	FDeliveryBotPolicyDecisionResultInfo GetLastPolicyDecisionResult() const; // Python policy의 마지막 decide 결과를 반환한다
	
	void GetLastPythonCaptureRefs(TArray<FDeliveryBotPythonCaptureRefInfo>& outCaptureRefs) const; // Python policy가 마지막으로 반환한 capture refs를 복사한다

private:
	void ApplySetupInfo();
	void ApplyBodyConfigInfo(const FDeliveryBotBodyConfigInfo& bodyConfigInfo); // profile robot.body를 actor vehicle spec 값으로 적용한다.

	void FillObservation(FDeliveryBotObservationInfo& observation) const;
	void DebugLogObservation(float deltaTime);

	void RefreshSensorSnapshot(); // 현재 LiDAR 센서 관측값을 LastSensorSnapshot에 저장한다
	void BindCollisionStopHitDelegates();
	bool IsCollisionStopActor(const AActor* otherActor) const;
	void ResetCollisionStopState();

	UFUNCTION()
	void HandleCollisionStopHit(
		UPrimitiveComponent* hitComponent,
		AActor* otherActor,
		UPrimitiveComponent* otherComp,
		FVector normalImpulse,
		const FHitResult& hit);


private:  // tick/Hz 관련 함수
	void UpdateFixedSimulation(float deltaTime);
	void StepFixedSimulation(float fixedDeltaSeconds);
	void UpdateFixedSensor(float fixedDeltaSeconds);
	void UpdateFixedPolicy(float fixedDeltaSeconds);
	void ApplyLatestMoveCommand(float fixedDeltaSeconds);
	float GetFixedTickIntervalSeconds() const;


protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Component")
	TObjectPtr<UDeliveryBot_DriveComponent> DriveComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Component")
	TObjectPtr<UDeliveryBot_LidarSensorComponent> LidarSensorComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Component")
	TObjectPtr<UDeliveryBot_HttpPolicyComponent> HttpPolicyComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Component")
	TObjectPtr<UScenarioPlaceableComponent> PlaceableComponent;



protected:
	float DebugLogElapsedSeconds{ 0.f };
	int32 SensorSnapshotSequence{ 0 };
	int32 PolicyObservationSequence{ 0 };

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Setup")
	FDeliveryBotSetupInfo SetupInfo{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|VehicleSpec")
	float WheelBaseCm{ 42.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|VehicleSpec")
	FVector RobotBoxExtentCm{ 30.f, 45.f, 25.f }; // 길찾기 할 때 쓰이는 로봇의 충돌 박스 사이즈

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|VehicleSpec")
	float MinTurningRadiusCm{ 300.f }; //  최소 회전 반경

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Debug")
	bool bLogPolicyObservationRequests{ false };


private:
	FDeliveryBotSensorSnapshot LastSensorSnapshot{};
	FDeliveryBotMoveCommandInfo LastMoveCommandInfo{};

	FString CollisionStopActorName{};
	// Scenario semantic id for the current collision stop target.
	FString CollisionStopTargetId{};
	FString LastActionReason{ TEXT("unknown") };

	bool bHasLastMoveCommand{ false };
	bool bCollisionStopActive{ false };

	TArray<FName> CollisionStopActorTags{};
	// Scenario semantic tags for the current collision stop target.
	TArray<FName> CollisionStopTargetTags{};


protected:   // tick/Hz 관련 변수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|FixedTick", meta = (AllowPrivateAccess = "true"))
	float FixedTickRateHz{ 30.f };

private:
	// 고정 시뮬레이션 틱 누적 시간.
	float FixedTickElapsedSeconds{ 0.f };
	// 고정 시뮬레이션이 진행한 총 시간.
	float FixedSimulationTimeSeconds{ 0.f };
	// LiDAR scan rate를 맞추기 위한 누적 시간.
	float SensorElapsedSeconds{ 0.f };
};
