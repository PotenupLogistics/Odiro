// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeliveryBot_LocalAvoidanceComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROTOROBOTSIM_API UDeliveryBot_LocalAvoidanceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeliveryBot_LocalAvoidanceComponent();

protected:
	virtual void BeginPlay() override;

public:
	bool HasObstacleAhead(const FVector& moveDirection, FHitResult& outHitResult) const;
private:
	bool IsIgnoredActor(const AActor* actor) const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeliveryBot|LocalAvoidance", meta = (AllowPrivateAccess = "true"))
	float ObstacleTraceDistance{ 300.f };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeliveryBot|LocalAvoidance", meta = (AllowPrivateAccess = "true"))
	FVector ObstacleBoxHalfExtent{ 45.f, 35.f, 25.f };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeliveryBot|LocalAvoidance", meta = (AllowPrivateAccess = "true"))
	float TraceHeightOffset{ 35.f };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeliveryBot|LocalAvoidance", meta = (AllowPrivateAccess = "true"))
	bool bDrawDebugTrace{ true };
	
};
