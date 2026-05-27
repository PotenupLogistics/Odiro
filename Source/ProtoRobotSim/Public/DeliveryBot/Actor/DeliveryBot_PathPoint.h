// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TargetPoint.h"
#include "DeliveryBot_PathPoint.generated.h"

UENUM(BlueprintType)
enum class EDeliveryBotPathPointType : uint8
{
	Start,
	Goal
};


UCLASS()
class PROTOROBOTSIM_API ADeliveryBot_PathPoint : public ATargetPoint
{
	GENERATED_BODY()

public:
	ADeliveryBot_PathPoint();
	
public:
	EDeliveryBotPathPointType GetPathPointType() const { return PathPointType; }

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeliveryBot|Path", meta = (AllowPrivateAccess = "true"))
	EDeliveryBotPathPointType PathPointType{ EDeliveryBotPathPointType::Start };
};
