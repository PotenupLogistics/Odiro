// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeliveryBot_MovementComponent.generated.h"

class UDeliveryBot_LocalAvoidanceComponent;
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROTOROBOTSIM_API UDeliveryBot_MovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeliveryBot_MovementComponent();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	
public:
	void SetPath(const TArray<FVector>& pathPoints);
	void StopMove();

private:
	void MoveAlongPath(float deltaTime);
	void MarkObstacleAsDynamicBlocked(const FHitResult& obstacleHitResult) const;
	void RequestOwnerReroute();
	bool CanRequestReroute() const;
	
	bool IsArrivedAtCurrentPathPoint() const;
	bool GetGroundLocationByWorldLocation(const FVector& worldLocation, FVector& outGroundLocation) const;

private:
	UPROPERTY()
	TObjectPtr<UDeliveryBot_LocalAvoidanceComponent> LocalAvoidanceComponent;
	
	
private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Move", meta = (AllowPrivateAccess = "true"))
	float MoveSpeed{ 200.f };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Move", meta = (AllowPrivateAccess = "true"))
	float AcceptanceRadius{ 30.f };

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Move", meta = (AllowPrivateAccess = "true"))
	TArray<FVector> PathPoints;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Move", meta = (AllowPrivateAccess = "true"))
	int32 CurrentPathIndex{ 0 };

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Move", meta = (AllowPrivateAccess = "true"))
	bool bMoving{ false };
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Move", meta = (AllowPrivateAccess = "true"))
	float BodyHalfHeight{ 25.f };
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Move", meta = (AllowPrivateAccess = "true"))
	float RerouteCooldownTime{ 1.f };

	float LastRerouteRequestTime{ -1000.f };
};
