#include "Scenario/Editor/ScenarioCorridorPreviewActor.h"

#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

// Corridor preview surface/material 해석 상태를 추적하는 로그 카테고리.
DEFINE_LOG_CATEGORY_STATIC(LogScenarioCorridorPreview, Log, All);

namespace
{
	// Template coordinates are stored in meters while Unreal actors use centimeters.
	const double MetersToCentimeters = 100.0;
	// Thin surface tops stay slightly above the ground to avoid z-fighting.
	const double PreviewSurfaceTopZCm = 1.0;
	// Non-blocking surfaces are thick enough to overlap 15cm side offsets without vertical holes.
	const double MinimumSurfacePreviewHeightCm = 20.0;
	// Blocked corridor surfaces match the legacy blocked ground-region collision height.
	const double BlockedPreviewHeightCm = 200.0;
	// curb_side lane은 walkway보다 낮은 노면으로 보여 계단식 경계가 드러나게 한다.
	const double CurbSidePreviewDropCm = 15.0;
	// Blocked corridor surfaces use the same collision profile as blocked ground regions.
	const FName BlockedPreviewCollisionProfileName{ TEXT("Blocked") };

	// Returns true when a resolved surface should behave like a physical blocked volume.
	bool IsBlockedCorridorSurface(const FScenarioCorridorSurfaceEntry& surfaceEntry)
	{
		return surfaceEntry.GroundRegionType == EScenarioGroundRegionType::Blocked
			|| surfaceEntry.LaneType == EScenarioSampleLaneType::Blocked;
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
	AddLaneStrip(TEXT("walkway"), TEXT("sidewalk"), -halfWalkwayWidthMeters, halfWalkwayWidthMeters, 0.0);

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
			index == 0 ? TEXT("curb_edge") : FString::Printf(TEXT("curb_%d"), index),
			laneRule.SurfaceId,
			curbMinOffsetMeters,
			curbMinOffsetMeters + widthMeters,
			-CurbSidePreviewDropCm);
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
		const FVector pointCm(
			pointMeters.X * MetersToCentimeters,
			pointMeters.Y * MetersToCentimeters,
			PreviewSurfaceTopZCm);
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

	const double centerOffsetCm = ((minOffsetMeters + maxOffsetMeters) * 0.5) * MetersToCentimeters;
	const double laneWidthCm = laneWidthMeters * MetersToCentimeters;
	FScenarioCorridorSurfaceEntry surfaceEntry;
	ResolveSurfaceEntry(surfaceId, surfaceEntry);
	UMaterialInterface* material = ResolveSurfaceMaterial(surfaceEntry);
	const bool bBlockedSurface = IsBlockedCorridorSurface(surfaceEntry);
	const double laneTopZCm = PreviewSurfaceTopZCm + surfaceZOffsetCm;
	const double laneHeightCm = bBlockedSurface ? BlockedPreviewHeightCm : MinimumSurfacePreviewHeightCm;
	const double laneCenterZCm = bBlockedSurface
		? laneTopZCm + (laneHeightCm * 0.5)
		: laneTopZCm - (laneHeightCm * 0.5);
	const double laneHeightScale = laneHeightCm / 100.0;
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
		const FVector laneHeightOffset(0.0, 0.0, laneCenterZCm - PreviewSurfaceTopZCm);
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
			startLocation + startRight * centerOffsetCm + laneHeightOffset,
			startTangent,
			endLocation + endRight * centerOffsetCm + laneHeightOffset,
			endTangent,
			false);
		meshComponent->SetStartScale(FVector2D(laneWidthCm / 100.0, laneHeightScale), false);
		meshComponent->SetEndScale(FVector2D(laneWidthCm / 100.0, laneHeightScale), false);
		if (bBlockedSurface)
		{
			meshComponent->SetCollisionProfileName(BlockedPreviewCollisionProfileName);
			meshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
		else
		{
			meshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
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
			UE_LOG(
				LogScenarioCorridorPreview,
				Log,
				TEXT("Resolved Corridor surface '%s' from catalog/defaults. Catalog: %s"),
				*surfaceName.ToString(),
				*loadedCatalog->GetPathName());
			return true;
		}
	}
	else if (!SurfaceCatalog.IsNull())
	{
		UE_LOG(
			LogScenarioCorridorPreview,
			Warning,
			TEXT("Corridor surface catalog could not be loaded. Path: %s"),
			*SurfaceCatalog.ToSoftObjectPath().ToString());
	}

	if (UScenarioCorridorSurfaceCatalog::FindDefaultSurfaceEntryById(surfaceName, outSurfaceEntry))
	{
		UE_LOG(
			LogScenarioCorridorPreview,
			Log,
			TEXT("Resolved Corridor surface '%s' from built-in defaults."),
			*surfaceName.ToString());
		return true;
	}

	UE_LOG(
		LogScenarioCorridorPreview,
		Warning,
		TEXT("Unknown Corridor surface '%s'; using walkable fallback metadata."),
		surfaceId.IsEmpty() ? TEXT("<empty>") : *surfaceId);
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
		UE_LOG(
			LogScenarioCorridorPreview,
			Log,
			TEXT("Using Corridor surface preview material. Surface: %s | Material: %s"),
			*surfaceEntry.SurfaceId.ToString(),
			*catalogMaterial->GetPathName());
		return catalogMaterial;
	}

	if (!surfaceEntry.PreviewMaterial.IsNull())
	{
		UE_LOG(
			LogScenarioCorridorPreview,
			Warning,
			TEXT("Corridor surface preview material failed to load. Surface: %s | Path: %s"),
			*surfaceEntry.SurfaceId.ToString(),
			*surfaceEntry.PreviewMaterial.ToSoftObjectPath().ToString());
	}

	UMaterialInterface* fallbackMaterial = ResolveFallbackSurfaceMaterial(surfaceEntry.LaneType);
	UE_LOG(
		LogScenarioCorridorPreview,
		Log,
		TEXT("Using Corridor surface fallback material. Surface: %s | Material: %s"),
		*surfaceEntry.SurfaceId.ToString(),
		fallbackMaterial ? *fallbackMaterial->GetPathName() : TEXT("<null>"));
	return fallbackMaterial;
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
