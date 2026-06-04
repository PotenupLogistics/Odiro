#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shared/Struct/DeliveryBot/Drive/DeliveryBotMovementInfo.h"
#include "Shared/Struct/DeliveryBot/Path/DeliveryBotPathFollowConfigInfo.h"
#include "Shared/Struct/DeliveryBot/Navigation/DeliveryBotNavigationConfigInfo.h"
#include "Shared/Struct/DeliveryBot/Path/DeliveryBotPathInfo.h"
#include "DeliveryBot_PathFollowComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROTOROBOTSIM_API UDeliveryBot_PathFollowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeliveryBot_PathFollowComponent();

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|PathFollow")
	void InitializePathFollow(const FDeliveryBotPathFollowConfigInfo& pathFollowConfigInfo);

	void SetPath(const TArray<FVector>& pathPoints);
	void ClearPath();

	bool HasPath() const;
	bool HasArrived() const;

	FDeliveryBotMoveCommandInfo BuildMoveCommand(float deltaTime);

	FDeliveryBotMoveCommandInfo BuildDriveCommand(float deltaTime, const FDeliveryBotNavigationConfigInfo& navigationConfigInfo);
	
	void SetPathPointInfos(const TArray<FDeliveryBotPathPointInfo>& pathPointInfos);
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|PathFollow")
	FDeliveryBotPathFollowConfigInfo PathFollowConfigInfo{};

private:
	void UpdateCurrentPathIndex();
	FVector GetLookAheadLocation() const;
	float GetSteeringToLocation(const FVector& targetLocation,	EDeliveryBotMoveDirectionType moveDirectionType) const;
	float GetSteeringToLocation(const FVector& targetLocation) const;
	float GetDistance2D(const FVector& fromLocation, const FVector& toLocation) const;
	void DrawDebugPathFollow(const FVector& lookAheadLocation, float steering) const;
	FVector GetPathPointLocation(int32 pathIndex) const;
	EDeliveryBotMoveDirectionType GetCurrentMoveDirectionType() const;
	int32 GetClosestPathSegmentIndex() const;
	
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|PathFollow", meta = (AllowPrivateAccess = "true"))
	TArray<FVector> PathPoints;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|PathFollow", meta = (AllowPrivateAccess = "true"))
	TArray<FDeliveryBotPathPointInfo> PathPointInfos;
	
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|PathFollow", meta = (AllowPrivateAccess = "true"))
	int32 CurrentPathIndex{ INDEX_NONE };

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|PathFollow", meta = (AllowPrivateAccess = "true"))
	bool bArrived{ false };
};
