#pragma once

#include "CoreMinimal.h"
#include "Shared/EpisodeSpecTypes.h"
#include "EpisodePedestrianPlanTypes.generated.h"

USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodePedestrianPlanPoint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan")
	FVector Direction = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan", meta = (ClampMin = "0.0", Units = "cm"))
	double DistanceCm = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan", meta = (ClampMin = "0.0", Units = "s"))
	double TimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan", meta = (ClampMin = "0.0", Units = "cm/s"))
	double SpeedCmPerSecond = 120.0;
};

USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodePedestrianPlanReservation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan")
	FString ReservationId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan")
	FVector Center = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan", meta = (ClampMin = "0.0", Units = "s"))
	double StartTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan", meta = (ClampMin = "0.0", Units = "s"))
	double EndTimeSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodePedestrianBehaviorParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianBehavior", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Cooperation = 0.5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianBehavior", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Evasiveness = 0.35;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianBehavior", meta = (ClampMin = "0.0", Units = "cm"))
	double PersonalSpaceCm = 80.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianBehavior", meta = (ClampMin = "0.0", Units = "s"))
	double AwarenessHorizonSeconds = 2.5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianBehavior", meta = (ClampMin = "0.0", Units = "s"))
	double MaxYieldWaitSeconds = 4.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianBehavior", meta = (ClampMin = "0.0", Units = "cm"))
	double SidestepDistanceCm = 60.0;
};

USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodePedestrianPathShapeParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan", meta = (ClampMin = "0.0", Units = "cm"))
	double CurveOffsetCm = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan", meta = (ClampMin = "1.0", Units = "cm"))
	double CurveSampleSpacingCm = 50.0;
};

USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodePedestrianPlan
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan")
	FString InstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan")
	FString PlanId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan")
	FString SourceSpecHash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan")
	FString ResolvedFootprintHash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan")
	FString SemanticNavigationHash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan")
	FString PlanHash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan")
	FString BehaviorHash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan")
	FString PedestrianScenarioHash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan")
	FEpisodePedestrianBehaviorParams BehaviorParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan")
	FEpisodePedestrianPathShapeParams PathShapeParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan", meta = (ClampMin = "0.0", Units = "s"))
	double SpawnTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan", meta = (ClampMin = "0.0", Units = "s"))
	double NominalDurationSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan")
	TArray<FEpisodePedestrianPlanPoint> Points;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan")
	TArray<FEpisodePedestrianPlanReservation> Reservations;
};

USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodePedestrianObstacleFootprint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan")
	FString InstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan")
	FString AssetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan")
	FVector Center = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan", meta = (ClampMin = "0.0", Units = "cm"))
	FVector Extent = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodePedestrianPlanBuildContext
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan")
	FString SourceSpecHash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan")
	FString SemanticNavigationHash = TEXT("default");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan", meta = (ClampMin = "0.0", Units = "cm"))
	double StaticObstacleClearanceCm = 80.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan")
	TArray<FEpisodePedestrianObstacleFootprint> StaticObstacleFootprints;
};

USTRUCT(BlueprintType)
struct PROTOROBOTSIM_API FEpisodePedestrianPlanBuildResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan")
	bool bSuccess = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan")
	FString ResolvedFootprintHash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan")
	TArray<FEpisodePedestrianPlan> Plans;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|PedestrianPlan")
	TArray<FString> Diagnostics;
};
