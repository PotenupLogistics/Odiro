
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shared/Struct/DeliveryBot/Path/DeliveryBotAStarInfo.h"
#include "Shared/Struct/DeliveryBot/Navigation/DeliveryBotNavigationConfigInfo.h"
#include "Shared/Struct/DeliveryBot/Path/DeliveryBotPathInfo.h"
#include "DeliveryBot_GlobalPathComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROTOROBOTSIM_API UDeliveryBot_GlobalPathComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeliveryBot_GlobalPathComponent();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
public:
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|GlobalPath")
	bool BuildPathByAStar(const FVector& startLocation, const FVector& goalLocation);

	void SetDrawDebugEnabled(bool bEnabled);

	// 현재 생성된 전역 경로 위치 배열을 반환한다.
	const TArray<FVector>& GetGlobalPath() const
	{
		return GlobalPath;
	}
	
	// 현재 생성된 방향 정보 포함 경로 포인트 배열을 반환한다.
	const TArray<FDeliveryBotPathPointInfo>& GetGlobalPathPointInfos() const
	{
		return GlobalPathPointInfos;
	}
	
	bool BuildPath(const FVector& startLocation, const FVector& goalLocation, const FDeliveryBotNavigationConfigInfo& navigationConfigInfo);

private:
	float GetHeuristicCost(const FIntPoint& fromGridIndex, const FIntPoint& toGridIndex) const;
	float GetMoveCost(const FIntPoint& fromGridIndex, const FIntPoint& toGridIndex) const;

	void BuildGlobalPathFromNodeMap(
		const TMap<FIntPoint, FDeliveryBotAStarInfo>& nodeMap,
		const FIntPoint& startGridIndex,
		const FIntPoint& goalGridIndex,
		const class UDeliveryBot_GridSubsystem* gridSubsystem);

	void DrawGlobalPath() const;
	void SmoothGlobalPath();
	bool CanConnectPathPoints(const FVector& fromLocation, const FVector& toLocation) const;
	
	void BuildGlobalPathPointInfosFromGlobalPath(EDeliveryBotMoveDirectionType moveDirectionType);
	
	FDeliveryBotHybridAStarConfigInfo NormalizeHybridAStarConfigInfo(const FDeliveryBotHybridAStarConfigInfo& hybridAStarConfigInfo) const;
	TArray<EDeliveryBotMoveDirectionType> GetHybridAStarMoveDirectionTypes(	const FDeliveryBotHybridAStarConfigInfo& hybridAStarConfigInfo) const;
	
	float GetHybridAStarDirectionCost(float baseMoveCost, EDeliveryBotMoveDirectionType previousMoveDirectionType,
		EDeliveryBotMoveDirectionType currentMoveDirectionType, const FDeliveryBotHybridAStarConfigInfo& hybridAStarConfigInfo,
		float turnDirectionSign, float previousTurnDirectionSign) const;
	
	const TCHAR* GetHybridAStarMotionModelName(	EDeliveryBotHybridAStarMotionModelType motionModelType) const;
	
	float NormalizeRadian(float angleRadian) const;

	int32 GetHybridAStarHeadingIndex(float headingRadian, const FDeliveryBotHybridAStarConfigInfo& hybridAStarConfigInfo) const;

	float GetHybridAStarHeadingRadianByIndex(int32 headingIndex, const FDeliveryBotHybridAStarConfigInfo& hybridAStarConfigInfo) const;

	float GetHybridAStarHeuristicCost(const FVector& fromLocationCm,const FVector& goalLocationCm) const;
	
	bool IsHybridAStarGoalReached(
		const FVector& currentLocationCm,
		float currentHeadingRadian,
		const FVector& goalLocationCm,
		float goalHeadingRadian,
		const FDeliveryBotHybridAStarConfigInfo& hybridAStarConfigInfo) const;
	
	float GetHybridAStarStartHeadingRadian(const FVector& startLocationCm, const FVector& goalLocationCm) const;

	float GetHybridAStarGoalHeadingRadian(const FVector& startLocationCm, const FVector& goalLocationCm) const;
	
	float GetHybridAStarMoveDirectionSign(EDeliveryBotMoveDirectionType moveDirectionType) const;

	TArray<float> GetHybridAStarTurnDirectionSigns() const;

	FVector CalculateHybridAStarNextLocation(
		const FVector& currentLocationCm,
		float currentHeadingRadian,
		EDeliveryBotMoveDirectionType moveDirectionType,
		float turnDirectionSign,
		const FDeliveryBotHybridAStarConfigInfo& hybridAStarConfigInfo,
		float& outNextHeadingRadian) const;

	bool CanUseHybridAStarSegment(const FVector& fromLocationCm,const FVector& toLocationCm,
					const class UDeliveryBot_GridSubsystem* gridSubsystem) const;



private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|GlobalPath", meta = (AllowPrivateAccess = "true"))
	TArray<FVector> GlobalPath;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|GlobalPath", meta = (AllowPrivateAccess = "true"))
	TArray<FDeliveryBotPathPointInfo> GlobalPathPointInfos;
	
private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeliveryBot|GlobalPath", meta = (AllowPrivateAccess = "true"))
	float PathSmoothingSampleDistance{ 50.f };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeliveryBot|GlobalPath", meta = (AllowPrivateAccess = "true"))
	bool bDrawDebug{ true };
	
	
};
