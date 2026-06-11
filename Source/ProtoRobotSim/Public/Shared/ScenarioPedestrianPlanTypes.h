#pragma once

#include "CoreMinimal.h"
#include "Shared/ScenarioSpecTypes.h"
#include "ScenarioPedestrianPlanTypes.generated.h"

USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FScenarioPedestrianPlanPoint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan")
	FVector Direction = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan", meta = (ClampMin = "0.0", Units = "cm"))
	double DistanceCm = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan", meta = (ClampMin = "0.0", Units = "s"))
	double TimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan", meta = (ClampMin = "0.0", Units = "cm/s"))
	double SpeedCmPerSecond = 120.0;
};

USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FScenarioPedestrianPlanReservation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan")
	FString ReservationId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan")
	FVector Center = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan", meta = (ClampMin = "0.0", Units = "s"))
	double StartTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan", meta = (ClampMin = "0.0", Units = "s"))
	double EndTimeSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FScenarioPedestrianBehaviorParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianBehavior", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Cooperation = 0.5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianBehavior", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Evasiveness = 0.35;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianBehavior", meta = (ClampMin = "0.0", Units = "cm"))
	double PersonalSpaceCm = 80.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianBehavior", meta = (ClampMin = "0.0", Units = "s"))
	double AwarenessHorizonSeconds = 2.5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianBehavior", meta = (ClampMin = "0.0", Units = "s"))
	double MaxYieldWaitSeconds = 4.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianBehavior", meta = (ClampMin = "0.0", Units = "cm"))
	double SidestepDistanceCm = 60.0;
};

USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FScenarioPedestrianPathShapeParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan", meta = (ClampMin = "0.0", Units = "cm"))
	double CurveOffsetCm = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan", meta = (ClampMin = "1.0", Units = "cm"))
	double CurveSampleSpacingCm = 50.0;
};

USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FScenarioPedestrianPlan
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan")
	FString InstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan")
	FString PlanId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan")
	FString SourceSpecHash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan")
	FString ResolvedFootprintHash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan")
	FString SemanticNavigationHash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan")
	FString PlanHash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan")
	FString BehaviorHash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan")
	FString PedestrianScenarioHash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan")
	FScenarioPedestrianBehaviorParams BehaviorParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan")
	FScenarioPedestrianPathShapeParams PathShapeParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan", meta = (ClampMin = "0.0", Units = "s"))
	double NominalDurationSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan")
	TArray<FScenarioPedestrianPlanPoint> Points;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan")
	TArray<FScenarioPedestrianPlanReservation> Reservations;
};

USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FScenarioPedestrianObstacleFootprint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan")
	FString InstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan")
	FString AssetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan")
	FVector Center = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan", meta = (ClampMin = "0.0", Units = "cm"))
	FVector Extent = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FScenarioPedestrianPlanBuildContext
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan")
	FString SourceSpecHash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan")
	FString SemanticNavigationHash = TEXT("default");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan", meta = (ClampMin = "0.0", Units = "cm"))
	double StaticObstacleClearanceCm = 80.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan")
	TArray<FScenarioPedestrianObstacleFootprint> StaticObstacleFootprints;
};

USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FScenarioPedestrianPlanBuildResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan")
	bool bSuccess = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan")
	FString ResolvedFootprintHash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan")
	TArray<FScenarioPedestrianPlan> Plans;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|PedestrianPlan")
	TArray<FString> Diagnostics;
};
