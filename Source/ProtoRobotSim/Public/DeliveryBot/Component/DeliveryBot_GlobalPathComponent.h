// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shared/Struct/DeliveryBotAStarInfo.h"
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
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	
	
public:
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|GlobalPath")
	bool BuildPathByAStar(const FVector& startLocation, const FVector& goalLocation);

	const TArray<FVector>& GetGlobalPath() const { return GlobalPath; }
	
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
	
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|GlobalPath", meta = (AllowPrivateAccess = "true"))
	TArray<FVector> GlobalPath;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeliveryBot|GlobalPath", meta = (AllowPrivateAccess = "true"))
	float PathSmoothingSampleDistance{ 50.f };
};
