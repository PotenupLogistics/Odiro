#include "Scenario/Editor/ScenarioCorridorPreviewActor.h"

#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const double MetersToCentimeters = 100.0;
	const double PreviewSurfaceZCm = 1.0;
	const double PreviewSurfaceThicknessScale = 0.015;
}

AScenarioCorridorPreviewActor::AScenarioCorridorPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	AxisSplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("AxisSplineComponent"));
	AxisSplineComponent->SetupAttachment(SceneRoot);
	AxisSplineComponent->SetMobility(EComponentMobility::Movable);
	AxisSplineComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SurfaceCatalog = UScenarioCorridorSurfaceCatalog::MakeDefaultCatalogReference();

	static ConstructorHelpers::FObjectFinder<UStaticMesh> laneStripMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (laneStripMeshAsset.Succeeded())
	{
		LaneStripMesh = laneStripMeshAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> walkableGroundMaterialAsset(
		TEXT("/Game/Materials/M_ScenarioGroundWalkable.M_ScenarioGroundWalkable"));
	if (walkableGroundMaterialAsset.Succeeded())
	{
		WalkableGroundMaterial = walkableGroundMaterialAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> penaltyGroundMaterialAsset(
		TEXT("/Game/Materials/M_ScenarioGroundPenalty.M_ScenarioGroundPenalty"));
	if (penaltyGroundMaterialAsset.Succeeded())
	{
		PenaltyGroundMaterial = penaltyGroundMaterialAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> blockedGroundMaterialAsset(
		TEXT("/Game/Materials/M_ScenarioGroundBlock.M_ScenarioGroundBlock"));
	if (blockedGroundMaterialAsset.Succeeded())
	{
		BlockedGroundMaterial = blockedGroundMaterialAsset.Object;
	}
}

void AScenarioCorridorPreviewActor::ConfigureFromCorridor(const FScenarioTemplateCorridor& corridor)
{
	ClearLaneMeshes();
	RebuildAxisSpline(corridor.Axis.PointsMeters);

	if (!HasRenderableCorridor())
	{
		return;
	}

	const double walkwayWidthMeters = ResolvePreviewNumber(corridor.WalkwayWidthMeters, 3.0);
	const double halfWalkwayWidthMeters = FMath::Max(walkwayWidthMeters, 0.0) * 0.5;
	AddLaneStrip(TEXT("walkway"), TEXT("sidewalk"), -halfWalkwayWidthMeters, halfWalkwayWidthMeters);

	double buildingMaxOffsetMeters = -halfWalkwayWidthMeters;
	for (int32 index = 0; index < corridor.BuildingSide.Num(); ++index)
	{
		const FScenarioTemplateLaneRule& laneRule = corridor.BuildingSide[index];
		const double widthMeters = ResolvePreviewNumber(laneRule.WidthMeters, 0.0);
		if (widthMeters <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		AddLaneStrip(
			index == 0 ? TEXT("building_edge") : FString::Printf(TEXT("building_%d"), index),
			laneRule.SurfaceId,
			buildingMaxOffsetMeters - widthMeters,
			buildingMaxOffsetMeters);
		buildingMaxOffsetMeters -= widthMeters;
	}

	double curbMinOffsetMeters = halfWalkwayWidthMeters;
	for (int32 index = 0; index < corridor.CurbSide.Num(); ++index)
	{
		const FScenarioTemplateLaneRule& laneRule = corridor.CurbSide[index];
		const double widthMeters = ResolvePreviewNumber(laneRule.WidthMeters, 0.0);
		if (widthMeters <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		AddLaneStrip(
			index == 0 ? TEXT("curb_edge") : FString::Printf(TEXT("curb_%d"), index),
			laneRule.SurfaceId,
			curbMinOffsetMeters,
			curbMinOffsetMeters + widthMeters);
		curbMinOffsetMeters += widthMeters;
	}
}

bool AScenarioCorridorPreviewActor::HasRenderableCorridor() const
{
	return AxisSplineComponent && AxisSplineComponent->GetNumberOfSplinePoints() >= 2;
}

void AScenarioCorridorPreviewActor::ClearLaneMeshes()
{
	for (const TObjectPtr<USplineMeshComponent>& meshComponent : LaneMeshComponents)
	{
		if (IsValid(meshComponent))
		{
			meshComponent->DestroyComponent();
		}
	}

	LaneMeshComponents.Reset();
}

void AScenarioCorridorPreviewActor::RebuildAxisSpline(const TArray<FVector2D>& pointsMeters)
{
	if (!AxisSplineComponent)
	{
		return;
	}

	AxisSplineComponent->ClearSplinePoints(false);
	for (int32 index = 0; index < pointsMeters.Num(); ++index)
	{
		const FVector2D& pointMeters = pointsMeters[index];
		const FVector pointCm(pointMeters.X * MetersToCentimeters, pointMeters.Y * MetersToCentimeters, PreviewSurfaceZCm);
		AxisSplineComponent->AddSplinePoint(pointCm, ESplineCoordinateSpace::Local, false);
		AxisSplineComponent->SetSplinePointType(index, ESplinePointType::Curve, false);
	}
	AxisSplineComponent->SetClosedLoop(false, false);
	AxisSplineComponent->UpdateSpline();
}

void AScenarioCorridorPreviewActor::AddLaneStrip(
	const FString& laneId,
	const FString& surfaceId,
	double minOffsetMeters,
	double maxOffsetMeters)
{
	if (!LaneStripMesh || !HasRenderableCorridor())
	{
		return;
	}

	const double laneWidthMeters = maxOffsetMeters - minOffsetMeters;
	if (laneWidthMeters <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const double centerOffsetCm = ((minOffsetMeters + maxOffsetMeters) * 0.5) * MetersToCentimeters;
	const double laneWidthCm = laneWidthMeters * MetersToCentimeters;
	FScenarioCorridorSurfaceEntry surfaceEntry;
	ResolveSurfaceEntry(surfaceId, surfaceEntry);
	UMaterialInterface* material = ResolveSurfaceMaterial(surfaceEntry);
	const int32 lastPointIndex = AxisSplineComponent->GetNumberOfSplinePoints() - 1;
	for (int32 pointIndex = 0; pointIndex < lastPointIndex; ++pointIndex)
	{
		const FVector startLocation = AxisSplineComponent->GetLocationAtSplinePoint(
			pointIndex,
			ESplineCoordinateSpace::Local);
		const FVector endLocation = AxisSplineComponent->GetLocationAtSplinePoint(
			pointIndex + 1,
			ESplineCoordinateSpace::Local);
		const FVector startTangent = AxisSplineComponent->GetTangentAtSplinePoint(
			pointIndex,
			ESplineCoordinateSpace::Local);
		const FVector endTangent = AxisSplineComponent->GetTangentAtSplinePoint(
			pointIndex + 1,
			ESplineCoordinateSpace::Local);
		const FVector startDirection = startTangent.GetSafeNormal();
		const FVector endDirection = endTangent.GetSafeNormal();
		if (startDirection.IsNearlyZero() || endDirection.IsNearlyZero())
		{
			continue;
		}

		const FVector startRight = FVector::CrossProduct(FVector::UpVector, startDirection).GetSafeNormal();
		const FVector endRight = FVector::CrossProduct(FVector::UpVector, endDirection).GetSafeNormal();
		const FName componentName = MakeUniqueObjectName(
			this,
			USplineMeshComponent::StaticClass(),
			FName(*FString::Printf(TEXT("Corridor_%s_%02d"), *laneId, pointIndex)));
		USplineMeshComponent* meshComponent = NewObject<USplineMeshComponent>(this, componentName);
		if (!meshComponent)
		{
			continue;
		}

		meshComponent->SetMobility(EComponentMobility::Movable);
		meshComponent->SetupAttachment(SceneRoot);
		meshComponent->SetStaticMesh(LaneStripMesh);
		meshComponent->SetForwardAxis(ESplineMeshAxis::X, false);
		meshComponent->SetStartAndEnd(
			startLocation + startRight * centerOffsetCm,
			startTangent,
			endLocation + endRight * centerOffsetCm,
			endTangent,
			false);
		meshComponent->SetStartScale(FVector2D(laneWidthCm / 100.0, PreviewSurfaceThicknessScale), false);
		meshComponent->SetEndScale(FVector2D(laneWidthCm / 100.0, PreviewSurfaceThicknessScale), false);
		meshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		meshComponent->SetGenerateOverlapEvents(false);
		meshComponent->SetCastShadow(false);
		if (material)
		{
			meshComponent->SetMaterial(0, material);
		}
		meshComponent->RegisterComponent();
		meshComponent->UpdateMesh();
		LaneMeshComponents.Add(meshComponent);
	}
}

double AScenarioCorridorPreviewActor::ResolvePreviewNumber(
	const FScenarioTemplateNumberValue& value,
	double defaultValue)
{
	if (!value.bIsSet)
	{
		return defaultValue;
	}

	if (value.Mode == EScenarioTemplateNumberValueMode::Range)
	{
		return (value.MinValue + value.MaxValue) * 0.5;
	}

	return value.FixedValue;
}

bool AScenarioCorridorPreviewActor::ResolveSurfaceEntry(
	const FString& surfaceId,
	FScenarioCorridorSurfaceEntry& outSurfaceEntry) const
{
	outSurfaceEntry = FScenarioCorridorSurfaceEntry();
	const FName surfaceName(*surfaceId);
	if (const UScenarioCorridorSurfaceCatalog* loadedCatalog = SurfaceCatalog.LoadSynchronous())
	{
		if (loadedCatalog->FindSurfaceEntryById(surfaceName, outSurfaceEntry))
		{
			return true;
		}
	}

	if (UScenarioCorridorSurfaceCatalog::FindDefaultSurfaceEntryById(surfaceName, outSurfaceEntry))
	{
		return true;
	}

	outSurfaceEntry.SurfaceId = surfaceName;
	outSurfaceEntry.DisplayName = FText::FromString(surfaceId.IsEmpty() ? TEXT("Unknown Surface") : surfaceId);
	outSurfaceEntry.LaneType = EScenarioSampleLaneType::Walkable;
	outSurfaceEntry.GroundRegionType = EScenarioGroundRegionType::Walkable;
	outSurfaceEntry.TraversabilityScore = 1.0;
	return false;
}

UMaterialInterface* AScenarioCorridorPreviewActor::ResolveSurfaceMaterial(
	const FScenarioCorridorSurfaceEntry& surfaceEntry) const
{
	if (UMaterialInterface* catalogMaterial = surfaceEntry.PreviewMaterial.LoadSynchronous())
	{
		return catalogMaterial;
	}

	return ResolveFallbackSurfaceMaterial(surfaceEntry.LaneType);
}

UMaterialInterface* AScenarioCorridorPreviewActor::ResolveFallbackSurfaceMaterial(EScenarioSampleLaneType laneType) const
{
	if (laneType == EScenarioSampleLaneType::Blocked)
	{
		return BlockedGroundMaterial ? BlockedGroundMaterial.Get() : WalkableGroundMaterial.Get();
	}

	if (laneType == EScenarioSampleLaneType::Penalty)
	{
		return PenaltyGroundMaterial ? PenaltyGroundMaterial.Get() : WalkableGroundMaterial.Get();
	}

	return WalkableGroundMaterial.Get();
}
