// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeliveryBot_SimpleMesh.generated.h"

UCLASS()
class PROTOROBOTSIM_API ADeliveryBot_SimpleMesh : public AActor
{
	GENERATED_BODY()

public:
	ADeliveryBot_SimpleMesh();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	
	
	
	
};
