// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeliveryBot_SimpleMesh.generated.h"

class UDeliveryBot_LocalAvoidanceComponent;
class UDeliveryBot_MovementComponent;
class UDeliveryBot_GlobalPathComponent;
UCLASS()
class PROTOROBOTSIM_API ADeliveryBot_SimpleMesh : public AActor
{
	GENERATED_BODY()

public:
	ADeliveryBot_SimpleMesh();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	
private:
	void RequestGlobalPathByPathPoints();
	bool GetPathPointLocations(FVector& outStartLocation, FVector& outGoalLocation) const;
	bool BuildGlobalPathAndStartMove(const FVector& startLocation, const FVector& goalLocation);
	
public:
	void RequestGlobalPathFromCurrentLocation();
	
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDeliveryBot_GlobalPathComponent> GlobalPathComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDeliveryBot_MovementComponent> MovementComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDeliveryBot_LocalAvoidanceComponent> LocalAvoidanceComponent;
	
private:
	FVector CachedGoalLocation{ FVector::ZeroVector };
	bool bHasCachedGoalLocation{ false };
};
