#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "Shared/Struct/DeliveryBot/Perception/DeliveryBotLidarSensorInfo.h"
#include "Shared/Struct/DeliveryBot/Setup/DeliveryBotSetupInfo.h"
#include "Shared/Struct/DeliveryBot/Drive/DeliveryBotMovementInfo.h"
#include "Shared/Struct/DeliveryBot/Policy/DeliveryBotPolicyInfo.h"
#include "Shared/Struct/DeliveryBot/Policy/DeliveryBotPolicyFailureInfo.h"
#include "Shared/Struct/DeliveryBot/Result/DeliveryBotSimulationFailureInfo.h"
#include "DeliveryBot_ChaosActor.generated.h"


class ADeliveryBot_ChaosActor;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDeliveryBotSimulationFailedSignature,	ADeliveryBot_ChaosActor*, DeliveryBotActor,
	const FDeliveryBotSimulationFailureInfo&, FailureInfo);


class UDeliveryBot_PolicyJudgmentComponent;
class UDeliveryBot_GridSubsystem;
class UDeliveryBot_LidarSensorComponent;
class UDeliveryBot_DriveComponent;
class UDeliveryBot_GlobalPathComponent;
class UDeliveryBot_PathFollowComponent;
class UEpisodePlaceableComponent;

/// Snapshot of Delivery Bot state used by measurement logging.
USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FDeliveryBotMeasurementSnapshot
{
	GENERATED_BODY()

	/// Last lidar scan captured by the robot actor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Measurement")
	FDeliveryBotLidarScanInfo LidarScanInfo{};

	/// Nearest front object from the last lidar scan.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Measurement")
	FDeliveryBotLidarDetectedObjectInfo FrontObjectInfo{};

	/// Last movement command applied to the drive component.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Measurement")
	FDeliveryBotMoveCommandInfo MoveCommandInfo{};

	/// Reason assigned to the last movement command.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Measurement")
	FString ActionReason{ TEXT("unknown") };

	/// True when the lidar scan has a front object.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Measurement")
	bool bHasFrontObject = false;

	/// True after at least one movement command has been applied.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Measurement")
	bool bHasMoveCommand = false;

	/// Number of lidar hits in the last scan.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Measurement")
	int32 LidarHitCount = 0;
};

UCLASS(Blueprintable)
class PROTOROBOTSIM_API ADeliveryBot_ChaosActor : public AWheeledVehiclePawn
{
	GENERATED_BODY()
public: // 생성자와 UE Actor 생명주기
	ADeliveryBot_ChaosActor(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void NotifyHit(
		UPrimitiveComponent* MyComp,
		AActor* Other,
		UPrimitiveComponent* OtherComp,
		bool bSelfMoved,
		FVector HitLocation,
		FVector HitNormal,
		FVector NormalImpulse,
		const FHitResult& Hit) override;

public: // 외부에서 호출하는 설정/디버그/상태 API
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Setup")
	void InitializeSetupInfo(const FDeliveryBotSetupInfo& setupInfo);

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|Debug")
	void SetDrawDebugEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Simulation")
	bool HasSimulationFailed() const { return bSimulationFailed; }

public: // 시뮬레이션 실패를 외부에 알리는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "DeliveryBot|Simulation")
	FDeliveryBotSimulationFailedSignature OnDeliveryBotSimulationFailed;

	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Simulation")
	bool HasSimulationFailed() const
	{
		return bSimulationFailed;
	}

	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Measurement")
	bool GetMeasurementSnapshot(FDeliveryBotMeasurementSnapshot& OutSnapshot) const;
	
private: // 설정 적용과 종료 시 런타임 정리
	void ApplySetupInfo();
	void CleanupSimulationRuntimeState();

private: // 전역 경로 생성과 path follow 주행
	void BuildGlobalPathAndStartFollow();
	void ApplyPathFollowMoveCommand(float deltaTime);
	void ApplyStopCommand(FDeliveryBotMoveCommandInfo& moveCommandInfo) const;
	void AlignRotationToPathStart();

private: // 전방 장애물 감지와 재경로 탐색
	bool TryRequestRepathByFrontObject(const FDeliveryBotLidarDetectedObjectInfo& frontObjectInfo);
	bool IsInRepathMoveGraceTime() const;
	void ApplyFrontObstacleSlowDown(FDeliveryBotMoveCommandInfo& moveCommandInfo, const FDeliveryBotLidarDetectedObjectInfo& frontObjectInfo) const;

private: // 라이다 스캔과 grid dynamic blocked cell 관리
	void UpdateLidarScan();
	void DebugFrontLidarObject() const;
	int32 SetLidarDetectedActorsAsDynamicBlocked(UDeliveryBot_GridSubsystem* gridSubsystem,	AActor* requiredFrontActor) const;
	void ClearLidarDynamicBlockedCells();

private: // 정책 판단 context 생성과 decision 적용
	FDeliveryBotPolicyContextInfo BuildPolicyContextInfo(bool bHasFrontObject, const FDeliveryBotLidarDetectedObjectInfo& frontObjectInfo) const;

	FString ApplyPolicyDecisionToMoveCommand(
		FDeliveryBotMoveCommandInfo& moveCommandInfo,
		const FDeliveryBotPolicyDecisionInfo& decisionInfo,
		const FDeliveryBotLidarDetectedObjectInfo& frontObjectInfo,
		float deltaTime);

	UFUNCTION()
	void HandlePolicyFailed(const FDeliveryBotPolicyFailureInfo& failureInfo);

private: // 시뮬레이션 실패 처리와 실패 정보 생성
	void ApplyFailureStopCommand(float deltaTime);
	void FailSimulation(const FDeliveryBotSimulationFailureInfo& failureInfo);
	void CheckTipOverFailure();

	FDeliveryBotSimulationFailureInfo BuildPolicySimulationFailureInfo(const FDeliveryBotPolicyFailureInfo& policyFailureInfo) const;

	FDeliveryBotSimulationFailureInfo BuildSimulationFailureInfo(EDeliveryBotSimulationFailureType failureType,
		const FString& message,	AActor* targetActor = nullptr) const;

	float GetCurrentSpeedKmh() const;

private: // 충돌 실패 판정과 충돌 실패 정보 생성
	bool ShouldFailByCollision(AActor* otherActor, const FHitResult& hit) const;
	FDeliveryBotSimulationFailureInfo BuildCollisionSimulationFailureInfo(AActor* otherActor, const FHitResult& hit) const;

protected: // DeliveryBot을 구성하는 주요 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Component")
	TObjectPtr<UDeliveryBot_DriveComponent> ChaosDriveComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Component")
	TObjectPtr<UDeliveryBot_GlobalPathComponent> GlobalPathComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Component")
	TObjectPtr<UDeliveryBot_PathFollowComponent> PathFollowComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Component", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDeliveryBot_LidarSensorComponent> LidarSensorComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Component")
	TObjectPtr<UDeliveryBot_PolicyJudgmentComponent> PolicyJudgmentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Component")
	TObjectPtr<UEpisodePlaceableComponent> PlaceableComponent;
	
protected: // 에피소드/스폰 시 전달되는 초기 설정과 센서 캐시
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Setup")
	FDeliveryBotSetupInfo SetupInfo{};

	FDeliveryBotLidarScanInfo LastLidarScanInfo{};
	FDeliveryBotMoveCommandInfo LastMoveCommandInfo{};
	FString LastActionReason{ TEXT("unknown") };
	bool bHasLastMoveCommand{ false };

protected: // 디버그 표시와 정책 로그 옵션
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Debug")
	bool bDrawDebug{ true };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Debug")
	bool bLogPolicyDecision{ false };

protected: // 전방 장애물 기반 재경로 탐색 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Repath")
	bool bUseFrontObstacleRepath{ true };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Repath")
	float RepathCooldownSeconds{ 2.0f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Repath")
	float RepathMoveGraceSeconds{ 2.5f };

protected: // 재경로 탐색과 dynamic blocked cell 런타임 상태
	float LastRepathRequestTimeSeconds{ -1000.f };
	float LastSuccessfulRepathTimeSeconds{ -1000.f };
	bool bHasLidarDynamicBlockedCells{ false };
	bool bLastAppliedDrawDebug{ true };

protected: // 전복/충돌 실패 판정 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Failure")
	bool bFailOnTipOver{ true };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Failure")
	float TipOverFailureAngleDegree{ 60.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Failure")
	float TipOverFailureHoldSecond{ 0.5f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Failure")
	bool bFailOnCollision{ true };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Failure")
	float CollisionFailureMinSpeedKmh{ 0.5f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Failure")
	float GroundHitNormalZThreshold{ 0.7f };

private: // 현재 시뮬레이션 실패 상태
	bool bSimulationFailed{ false };
	float TipOverStartTimeSeconds{ -1.f };
	FDeliveryBotSimulationFailureInfo LastSimulationFailureInfo{};
};
