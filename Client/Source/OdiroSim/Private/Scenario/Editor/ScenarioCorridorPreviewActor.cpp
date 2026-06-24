#include "Scenario/Editor/ScenarioCorridorPreviewActor.h"

#include "ProceduralMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Materials/MaterialInterface.h"
#include "Scenario/ScenarioCorridorGeometry.h"
#include "Scenario/Data/ScenarioCorridorSurfaceResolver.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Thin surface tops stay slightly above the ground to avoid z-fighting.
	const double PreviewSurfaceTopZCm = 1.0;
	// Non-blocking surfaces are thick enough to overlap 15cm side offsets without vertical holes.
	const double MinimumSurfacePreviewHeightCm = 20.0;
	// Blocked corridor surfaces match the legacy blocked ground-region collision height.
	const double BlockedPreviewHeightCm = 200.0;
	// Blocked corridor surfaces use the same collision profile as blocked ground regions.
	const FName BlockedPreviewCollisionProfileName{ TEXT("Blocked") };

	struct FPreviewCorridorVisualLaneSpec
	{
		FScenarioAlongRangeMeters AlongRangeMeters;
		FString LaneId;
		FString SurfaceId;
		double MinOffsetMeters = 0.0;
		double MaxOffsetMeters = 0.0;
		double SurfaceZOffsetCm = 0.0;
	};

	double MeasurePreviewAxisLengthMeters(const TArray<FVector2D>& pointsMeters)
	{
		double lengthMeters = 0.0;
		for (int32 index = 0; index < pointsMeters.Num() - 1; ++index)
		{
			lengthMeters += FVector2D::Distance(pointsMeters[index], pointsMeters[index + 1]);
		}
		return lengthMeters;
	}

	bool ArePreviewVisualLanesEquivalent(
		const FPreviewCorridorVisualLaneSpec& left,
		const FPreviewCorridorVisualLaneSpec& right)
	{
		return left.LaneId == right.LaneId
			&& left.SurfaceId == right.SurfaceId
			&& FMath::IsNearlyEqual(
				left.MinOffsetMeters,
				right.MinOffsetMeters,
				FScenarioCorridorGeometry::SurfaceQueryToleranceMeters)
			&& FMath::IsNearlyEqual(
				left.MaxOffsetMeters,
				right.MaxOffsetMeters,
				FScenarioCorridorGeometry::SurfaceQueryToleranceMeters)
			&& FMath::IsNearlyEqual(
				left.SurfaceZOffsetCm,
				right.SurfaceZOffsetCm,
				KINDA_SMALL_NUMBER);
	}

	void AddOrMergePreviewVisualLane(
		TArray<FPreviewCorridorVisualLaneSpec>& visualLaneSpecs,
		const FPreviewCorridorVisualLaneSpec& candidate)
	{
		for (FPreviewCorridorVisualLaneSpec& visualLaneSpec : visualLaneSpecs)
		{
			if (ArePreviewVisualLanesEquivalent(visualLaneSpec, candidate)
				&& FMath::IsNearlyEqual(
					visualLaneSpec.AlongRangeMeters.EndMeters,
					candidate.AlongRangeMeters.StartMeters,
					FScenarioCorridorGeometry::SurfaceQueryToleranceMeters))
			{
				visualLaneSpec.AlongRangeMeters.EndMeters = candidate.AlongRangeMeters.EndMeters;
				return;
			}
		}

		visualLaneSpecs.Add(candidate);
	}
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

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> walkableGroundMaterialAsset(
		TEXT("/Game/Materials/Scenario/M_ScenarioCorridorSidewalk.M_ScenarioCorridorSidewalk"));
	if (walkableGroundMaterialAsset.Succeeded())
	{
		WalkableGroundMaterial = walkableGroundMaterialAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> penaltyGroundMaterialAsset(
		TEXT("/Game/Materials/Scenario/M_ScenarioCorridorRoad.M_ScenarioCorridorRoad"));
	if (penaltyGroundMaterialAsset.Succeeded())
	{
		PenaltyGroundMaterial = penaltyGroundMaterialAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> blockedGroundMaterialAsset(
		TEXT("/Game/Materials/Scenario/M_ScenarioCorridorBuilding.M_ScenarioCorridorBuilding"));
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

	TArray<FScenarioTemplateSegment> renderSegments = corridor.Segments;
	if (renderSegments.IsEmpty())
	{
		FScenarioTemplateSegment fullAxisSegment;
		fullAxisSegment.SegmentId = TEXT("segment_000");
		fullAxisSegment.AlongRangeMeters.StartMeters = 0.0;
		fullAxisSegment.AlongRangeMeters.EndMeters = MeasurePreviewAxisLengthMeters(corridor.Axis.PointsMeters);
		renderSegments.Add(fullAxisSegment);
	}

	TArray<FPreviewCorridorVisualLaneSpec> visualLaneSpecs;
	for (int32 segmentIndex = 0; segmentIndex < renderSegments.Num(); ++segmentIndex)
	{
		const FScenarioTemplateSegment& segment = renderSegments[segmentIndex];
		const FString walkwaySurfaceId = ResolvePreviewString(segment.ReplacedBySurfaceId, TEXT("sidewalk"));
		AddOrMergePreviewVisualLane(
			visualLaneSpecs,
			FPreviewCorridorVisualLaneSpec{
				segment.AlongRangeMeters,
				TEXT("walkway"),
				walkwaySurfaceId,
				-halfWalkwayWidthMeters,
				halfWalkwayWidthMeters,
				0.0 });

		double buildingMaxOffsetMeters = -halfWalkwayWidthMeters;
		for (int32 index = 0; index < corridor.BuildingSide.Num(); ++index)
		{
			const FScenarioTemplateLaneRule& laneRule = corridor.BuildingSide[index];
			const double widthMeters = ResolvePreviewNumber(laneRule.WidthMeters, 0.0);
			if (widthMeters <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			AddOrMergePreviewVisualLane(
				visualLaneSpecs,
				FPreviewCorridorVisualLaneSpec{
					segment.AlongRangeMeters,
					index == 0 ? FString(TEXT("building_edge")) : FString::Printf(TEXT("building_%d"), index),
					laneRule.SurfaceId,
					buildingMaxOffsetMeters - widthMeters,
					buildingMaxOffsetMeters,
					0.0 });
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

			AddOrMergePreviewVisualLane(
				visualLaneSpecs,
				FPreviewCorridorVisualLaneSpec{
					segment.AlongRangeMeters,
					index == 0 ? FString(TEXT("curb_edge")) : FString::Printf(TEXT("curb_%d"), index),
					laneRule.SurfaceId,
					curbMinOffsetMeters,
					curbMinOffsetMeters + widthMeters,
					FScenarioCorridorGeometry::DefaultCurbSideSurfaceZOffsetCm });
			curbMinOffsetMeters += widthMeters;
		}
	}

	double cornerFilletRadiusMeters = 0.0;
	for (const FPreviewCorridorVisualLaneSpec& visualLaneSpec : visualLaneSpecs)
	{
		cornerFilletRadiusMeters = FMath::Max(
			cornerFilletRadiusMeters,
			FMath::Max(FMath::Abs(visualLaneSpec.MinOffsetMeters), FMath::Abs(visualLaneSpec.MaxOffsetMeters)));
	}

	for (const FPreviewCorridorVisualLaneSpec& visualLaneSpec : visualLaneSpecs)
	{
		AddLaneStrip(
			corridor.Axis.PointsMeters,
			visualLaneSpec.AlongRangeMeters,
			visualLaneSpec.LaneId,
			visualLaneSpec.SurfaceId,
			visualLaneSpec.MinOffsetMeters,
			visualLaneSpec.MaxOffsetMeters,
			cornerFilletRadiusMeters,
			visualLaneSpec.SurfaceZOffsetCm);
	}
}

bool AScenarioCorridorPreviewActor::HasRenderableCorridor() const
{
	return AxisSplineComponent && AxisSplineComponent->GetNumberOfSplinePoints() >= 2;
}

void AScenarioCorridorPreviewActor::ClearLaneMeshes()
{
	for (const TObjectPtr<UProceduralMeshComponent>& meshComponent : LaneMeshComponents)
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
		const FVector pointCm(
			pointMeters.X * FScenarioCorridorGeometry::MetersToCentimeters,
			pointMeters.Y * FScenarioCorridorGeometry::MetersToCentimeters,
			PreviewSurfaceTopZCm);
		AxisSplineComponent->AddSplinePoint(pointCm, ESplineCoordinateSpace::Local, false);
		AxisSplineComponent->SetSplinePointType(index, ESplinePointType::Curve, false);
	}
	AxisSplineComponent->SetClosedLoop(false, false);
	AxisSplineComponent->UpdateSpline();
}

void AScenarioCorridorPreviewActor::AddLaneStrip(
	const TArray<FVector2D>& axisPointsMeters,
	const FScenarioAlongRangeMeters& alongRangeMeters,
	const FString& laneId,
	const FString& surfaceId,
	double minOffsetMeters,
	double maxOffsetMeters,
	double cornerFilletRadiusMeters,
	double surfaceZOffsetCm)
{
	if (!HasRenderableCorridor())
	{
		return;
	}

	const double laneWidthMeters = maxOffsetMeters - minOffsetMeters;
	if (laneWidthMeters <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	FScenarioCorridorSurfaceEntry surfaceEntry;
	FScenarioCorridorSurfaceResolver::ResolveSurfaceEntry(surfaceId, SurfaceCatalog, surfaceEntry);
	const EScenarioGroundRegionType fallbackRegionType =
		FScenarioCorridorSurfaceResolver::ResolveFallbackRegionType(surfaceEntry.LaneType);
	UMaterialInterface* material = FScenarioCorridorSurfaceResolver::ResolveSurfaceMaterial(
		surfaceEntry,
		fallbackRegionType,
		WalkableGroundMaterial.Get(),
		PenaltyGroundMaterial.Get(),
		BlockedGroundMaterial.Get());
	const bool bBlockedSurface = FScenarioCorridorSurfaceResolver::IsBlockedSurface(surfaceEntry);
	const double laneTopZCm = PreviewSurfaceTopZCm + surfaceZOffsetCm;
	const double laneHeightCm = bBlockedSurface ? BlockedPreviewHeightCm : MinimumSurfacePreviewHeightCm;
	const double laneCenterZCm = bBlockedSurface
		? laneTopZCm + (laneHeightCm * 0.5)
		: laneTopZCm - (laneHeightCm * 0.5);

	FScenarioRuntimeCorridorSpec previewCorridorSpec;
	previewCorridorSpec.PointsMeters = axisPointsMeters;
	TArray<FVector> axisLocationsCm;
	if (!FScenarioCorridorGeometry::BuildRuntimeAxisLocationsForAlongRangeCm(
		previewCorridorSpec,
		alongRangeMeters,
		PreviewSurfaceTopZCm,
		axisLocationsCm))
	{
		return;
	}

	TArray<FVector> axisTangentsCm;
	axisTangentsCm.Reserve(axisLocationsCm.Num());
	for (int32 pointIndex = 0; pointIndex < axisLocationsCm.Num(); ++pointIndex)
	{
		axisTangentsCm.Add(FScenarioCorridorGeometry::ResolveCurveTangentCm(axisLocationsCm, pointIndex));
	}

	FScenarioCorridorLaneMeshBuildSpec meshSpec;
	meshSpec.Owner = this;
	meshSpec.AttachParent = SceneRoot;
	meshSpec.Material = material;
	meshSpec.ComponentNameBase = FName(*FString::Printf(TEXT("Corridor_%s"), *laneId));
	meshSpec.AxisLocationsCm = MoveTemp(axisLocationsCm);
	meshSpec.AxisTangentsCm = MoveTemp(axisTangentsCm);
	meshSpec.MinOffsetCm = minOffsetMeters * FScenarioCorridorGeometry::MetersToCentimeters;
	meshSpec.MaxOffsetCm = maxOffsetMeters * FScenarioCorridorGeometry::MetersToCentimeters;
	meshSpec.LaneHeightCm = laneHeightCm;
	meshSpec.LaneCenterZCm = laneCenterZCm;
	meshSpec.CornerFilletRadiusCm = FMath::Max(cornerFilletRadiusMeters, 0.0)
		* FScenarioCorridorGeometry::MetersToCentimeters;
	meshSpec.CollisionEnabled = bBlockedSurface ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision;
	meshSpec.CollisionProfileName = bBlockedSurface ? BlockedPreviewCollisionProfileName : NAME_None;
	FScenarioCorridorGeometry::AddLaneStripMeshes(meshSpec, LaneMeshComponents);
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

FString AScenarioCorridorPreviewActor::ResolvePreviewString(
	const FScenarioTemplateStringValue& value,
	const FString& defaultValue)
{
	if (!value.bIsSet)
	{
		return defaultValue;
	}

	if (value.Mode == EScenarioTemplateStringValueMode::Choices && !value.Choices.IsEmpty())
	{
		return value.Choices[0];
	}

	return value.FixedValue.IsEmpty() ? defaultValue : value.FixedValue;
}
