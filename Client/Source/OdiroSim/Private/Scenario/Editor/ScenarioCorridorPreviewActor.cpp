#include "Scenario/Editor/ScenarioCorridorPreviewActor.h"

#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
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
	// Curb-side lanes render lower than the walkway so the step boundary is visible.
	const double CurbSidePreviewDropCm = 15.0;
	// Blocked corridor surfaces use the same collision profile as blocked ground regions.
	const FName BlockedPreviewCollisionProfileName{ TEXT("Blocked") };

	double MeasurePreviewAxisLengthMeters(const TArray<FVector2D>& pointsMeters)
	{
		double lengthMeters = 0.0;
		for (int32 index = 0; index < pointsMeters.Num() - 1; ++index)
		{
			lengthMeters += FVector2D::Distance(pointsMeters[index], pointsMeters[index + 1]);
		}
		return lengthMeters;
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

	static ConstructorHelpers::FObjectFinder<UStaticMesh> laneStripMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (laneStripMeshAsset.Succeeded())
	{
		LaneStripMesh = laneStripMeshAsset.Object;
	}

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

	for (int32 segmentIndex = 0; segmentIndex < renderSegments.Num(); ++segmentIndex)
	{
		const FScenarioTemplateSegment& segment = renderSegments[segmentIndex];
		const FString segmentId = segment.SegmentId.IsEmpty()
			? FString::Printf(TEXT("segment_%03d"), segmentIndex)
			: segment.SegmentId;
		const FString walkwaySurfaceId = ResolvePreviewString(segment.ReplacedBySurfaceId, TEXT("sidewalk"));
		AddLaneStrip(
			corridor.Axis.PointsMeters,
			segment.AlongRangeMeters,
			FString::Printf(TEXT("%s_walkway"), *segmentId),
			walkwaySurfaceId,
			-halfWalkwayWidthMeters,
			halfWalkwayWidthMeters,
			0.0);

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
				corridor.Axis.PointsMeters,
				segment.AlongRangeMeters,
				index == 0
					? FString::Printf(TEXT("%s_building_edge"), *segmentId)
					: FString::Printf(TEXT("%s_building_%d"), *segmentId, index),
				laneRule.SurfaceId,
				buildingMaxOffsetMeters - widthMeters,
				buildingMaxOffsetMeters,
				0.0);
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
				corridor.Axis.PointsMeters,
				segment.AlongRangeMeters,
				index == 0
					? FString::Printf(TEXT("%s_curb_edge"), *segmentId)
					: FString::Printf(TEXT("%s_curb_%d"), *segmentId, index),
				laneRule.SurfaceId,
				curbMinOffsetMeters,
				curbMinOffsetMeters + widthMeters,
				-CurbSidePreviewDropCm);
			curbMinOffsetMeters += widthMeters;
		}
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
	double surfaceZOffsetCm)
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

	const double centerOffsetCm =
		((minOffsetMeters + maxOffsetMeters) * 0.5) * FScenarioCorridorGeometry::MetersToCentimeters;
	const double laneWidthCm = laneWidthMeters * FScenarioCorridorGeometry::MetersToCentimeters;
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
	const double laneHeightScale = laneHeightCm / 100.0;

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
	meshSpec.LaneStripMesh = LaneStripMesh.Get();
	meshSpec.Material = material;
	meshSpec.ComponentNameBase = FName(*FString::Printf(TEXT("Corridor_%s"), *laneId));
	meshSpec.AxisLocationsCm = MoveTemp(axisLocationsCm);
	meshSpec.AxisTangentsCm = MoveTemp(axisTangentsCm);
	meshSpec.CenterOffsetCm = centerOffsetCm;
	meshSpec.LaneWidthCm = laneWidthCm;
	meshSpec.LaneHeightScale = laneHeightScale;
	meshSpec.LaneCenterZCm = laneCenterZCm;
	meshSpec.SurfaceTopZCm = PreviewSurfaceTopZCm;
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
