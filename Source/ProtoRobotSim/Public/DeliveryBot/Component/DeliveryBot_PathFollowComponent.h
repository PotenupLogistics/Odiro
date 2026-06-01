#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shared/Struct/DeliveryBotMovementInfo.h"
#include "Shared/Struct/DeliveryBotPathFollowConfigInfo.h"
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

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|PathFollow")
	FDeliveryBotPathFollowConfigInfo PathFollowConfigInfo{};

private:
	void UpdateCurrentPathIndex();
	FVector GetLookAheadLocation() const;
	float GetSteeringToLocation(const FVector& targetLocation) const;
	float GetDistance2D(const FVector& fromLocation, const FVector& toLocation) const;
	void DrawDebugPathFollow(const FVector& lookAheadLocation, float steering) const;
	
	
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|PathFollow", meta = (AllowPrivateAccess = "true"))
	TArray<FVector> PathPoints;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|PathFollow", meta = (AllowPrivateAccess = "true"))
	int32 CurrentPathIndex{ INDEX_NONE };

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|PathFollow", meta = (AllowPrivateAccess = "true"))
	bool bArrived{ false };
};
