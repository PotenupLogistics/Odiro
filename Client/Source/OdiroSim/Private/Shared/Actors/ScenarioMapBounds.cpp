#include "Shared/Actors/ScenarioMapBounds.h"

#include "GameFramework/Actor.h"
#include "Shared/ScenarioSpecTypes.h"

// Surface actor와 DeliveryBot route를 합쳐 Padding이 적용된 최종 영역을 계산한다.
bool FScenarioMapBoundsResolver::TryResolve(
	const TArray<AActor*>& surfaceActors,
	const TArray<FScenarioPlaceableInstanceSpec>& placeables,
	const double paddingCm,
	FScenarioMapBounds& outBounds)
{
	outBounds = FScenarioMapBounds{};

	FBox2D surfaceXYBounds(ForceInit);
	double centerZSum = 0.0;
	int32 validActorCount = 0;

	for (const AActor* surfaceActor : surfaceActors)
	{
		AccumulateSurfaceActor(
			surfaceActor,
			surfaceXYBounds,
			centerZSum,
			validActorCount);
	}

	if (!surfaceXYBounds.bIsValid || validActorCount <= 0)
		return false;

	const double centerZ = centerZSum / static_cast<double>(validActorCount);

	return TryResolveFromSurfaceBounds(
		surfaceXYBounds,
		centerZ,
		placeables,
		paddingCm,
		outBounds);
}

// 이미 계산된 Surface Bounds에 DeliveryBot route와 Padding을 적용한다.
bool FScenarioMapBoundsResolver::TryResolveFromSurfaceBounds(
	const FBox2D& surfaceXYBounds,
	const double centerZ,
	const TArray<FScenarioPlaceableInstanceSpec>& placeables,
	const double paddingCm,
	FScenarioMapBounds& outBounds)
{
	outBounds = FScenarioMapBounds{};

	if (!surfaceXYBounds.bIsValid
		|| !FMath::IsFinite(centerZ)
		|| !FMath::IsFinite(paddingCm))
	{
		return false;
	}

	FBox2D resolvedXYBounds = surfaceXYBounds;

	for (const FScenarioPlaceableInstanceSpec& placeableSpec : placeables)
	{
		ExpandForDeliveryBotRoute(placeableSpec, resolvedXYBounds);
	}

	const double safePaddingCm = FMath::Max(paddingCm, 0.0);
	const FVector2D padding(safePaddingCm, safePaddingCm);

	outBounds.XYBounds = FBox2D(
		resolvedXYBounds.Min - padding,
		resolvedXYBounds.Max + padding);
	outBounds.CenterZ = centerZ;

	return outBounds.IsValid();
}

// 유효한 actor의 component bounds를 현재 Surface 영역에 누적한다.
void FScenarioMapBoundsResolver::AccumulateSurfaceActor(
	const AActor* actor,
	FBox2D& inOutXYBounds,
	double& inOutCenterZSum,
	int32& inOutValidActorCount)
{
	if (!IsValid(actor))
		return;

	const FBox componentBounds = actor->GetComponentsBoundingBox(true);
	if (!componentBounds.IsValid)
		return;

	inOutXYBounds += FVector2D(componentBounds.Min.X, componentBounds.Min.Y);
	inOutXYBounds += FVector2D(componentBounds.Max.X, componentBounds.Max.Y);

	inOutCenterZSum += componentBounds.GetCenter().Z;
	++inOutValidActorCount;
}

// DeliveryBot의 최종 Start와 Goal이 영역에 포함되도록 확장한다.
void FScenarioMapBoundsResolver::ExpandForDeliveryBotRoute(
	const FScenarioPlaceableInstanceSpec& placeableSpec,
	FBox2D& inOutXYBounds)
{
	if (placeableSpec.Category != EScenarioActorCategory::DeliveryBot)
		return;

	const FVector startLocation =
		placeableSpec.DeliveryBot.bHasStartLocation
			? placeableSpec.DeliveryBot.SetupInfo.LocationSetupInfo.StartLocationCm
			: placeableSpec.Transform.GetLocation();

	inOutXYBounds += FVector2D(startLocation.X, startLocation.Y);

	if (!placeableSpec.DeliveryBot.bHasGoalLocation)
		return;

	const FVector& goalLocation = placeableSpec.DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm;

	inOutXYBounds += FVector2D(goalLocation.X, goalLocation.Y);
}
