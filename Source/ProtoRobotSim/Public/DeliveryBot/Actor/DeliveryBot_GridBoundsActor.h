// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeliveryBot_GridBoundsActor.generated.h"

class UBoxComponent;

UCLASS()
class PROTOROBOTSIM_API ADeliveryBot_GridBoundsActor : public AActor
{
	GENERATED_BODY()

public:
	ADeliveryBot_GridBoundsActor();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	

	
	
public:
	UBoxComponent* GetBoundsBox() const { return BoundsBox; }
	
	float GetCellSize() const { return CellSize; }
	float GetMaxWalkableSlopeDegree() const { return MaxWalkableSlopeDegree; }
	
	FVector GetRobotBoxExtent() const { return RobotBoxExtent; }
	
	
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Grid", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UBoxComponent> BoundsBox;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Grid", meta = (AllowPrivateAccess = "true"))
	float CellSize{ 100.f };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Grid", meta = (AllowPrivateAccess = "true"))
	FVector RobotBoxExtent{ 60.f, 90.f, 25.f };
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Grid", meta = (AllowPrivateAccess = "true"))
	float MaxWalkableSlopeDegree{ 60.f };
};
