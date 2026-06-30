
#include "Scenario/Actors/ScenarioGroundRegion.h"

#include "ProceduralMeshComponent.h"
#include "Scenario/Components/ScenarioPlaceableComponent.h"
#include "Scenario/ScenarioCorridorGeometry.h"
#include "Scenario/Data/ScenarioCorridorSurfaceCatalog.h"
#include "Algo/Reverse.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioGroundRegion, Log, All);

namespace
{
	const FName WalkableRegionCollisionProfileName{ TEXT("Walkable") };
	const FName PenaltyRegionCollisionProfileName{ TEXT("Penalty") };
	const FName BlockedRegionCollisionProfileName{ TEXT("Blocked") };
	// Generated road/curb visuals are owned by CityBuildings composite meshes while GroundRegions remain semantic proxies.
	const FString GeneratedCityRegionIdPrefix(TEXT("generated_city_"));
	// Road generated GroundRegions back curb and road semantics without rendering a generic flat surface.
	const FString GeneratedRoadSurfaceId(TEXT("road"));

	double GetGroundRegionCollisionCenterZCm(EScenarioGroundRegionType regionType, double collisionHeightCm)
	{
		if (regionType == EScenarioGroundRegionType::Blocked)
		{
			return collisionHeightCm * 0.5;
		}

		return -collisionHeightCm * 0.5;
	}

	bool IsSupportedGroundRegionShape(const FScenarioGroundRegionSpec& regionSpec)
	{
		if (regionSpec.ShapeType == EScenarioGroundShapeType::Rectangle)
		{
			return true;
		}

		return regionSpec.ShapeType == EScenarioGroundShapeType::ConvexPolygon
			&& regionSpec.PolygonVertices.Num() >= 3;
	}

	bool IsGeneratedCityRegionSpec(const FScenarioGroundRegionSpec& regionSpec)
	{
		return regionSpec.RegionId.StartsWith(GeneratedCityRegionIdPrefix);
	}

	double CalculateSignedArea2D(const TArray<FVector2D>& vertices)
	{
		double signedArea = 0.0;
		for (int32 Index = 0; Index < vertices.Num(); ++Index)
		{
			const FVector2D& Current = vertices[Index];
			const FVector2D& Next = vertices[(Index + 1) % vertices.Num()];
			signedArea += (Current.X * Next.Y) - (Next.X * Current.Y);
		}

		return signedArea * 0.5;
	}

	FBox2D CalculateLocalPolygonBounds(const TArray<FVector2D>& vertices)
	{
		FBox2D bounds(ForceInit);
		for (const FVector2D& vertex : vertices)
		{
			bounds += vertex;
		}

		return bounds;
	}

	bool IsPointInsideConvexPolygon2D(const TArray<FVector2D>& vertices, const FVector2D& point)
	{
		if (vertices.Num() < 3)
		{
			return false;
		}

		constexpr double EdgeToleranceCm = 0.1;
		double referenceSign = 0.0;
		for (int32 Index = 0; Index < vertices.Num(); ++Index)
		{
			const FVector2D& Current = vertices[Index];
			const FVector2D& Next = vertices[(Index + 1) % vertices.Num()];
			const FVector2D Edge = Next - Current;
			const FVector2D ToPoint = point - Current;
			const double CrossZ = (Edge.X * ToPoint.Y) - (Edge.Y * ToPoint.X);
			if (FMath::Abs(CrossZ) <= EdgeToleranceCm)
			{
				continue;
			}

			const double currentSign = CrossZ > 0.0 ? 1.0 : -1.0;
			if (referenceSign == 0.0)
			{
				referenceSign = currentSign;
			}
			else if (!FMath::IsNearlyEqual(referenceSign, currentSign))
			{
				return false;
			}
		}

		return true;
	}

	void BuildGroundRegionLocalVertices(const FScenarioGroundRegionSpec& regionSpec, TArray<FVector2D>& outVertices)
	{
		outVertices.Reset();
		if (regionSpec.ShapeType == EScenarioGroundShapeType::ConvexPolygon)
		{
			outVertices = regionSpec.PolygonVertices;
		}
		else
		{
			const FVector2D halfSize = regionSpec.Size * 0.5;
			outVertices.Reserve(4);
			outVertices.Add(FVector2D(-halfSize.X, -halfSize.Y));
			outVertices.Add(FVector2D(halfSize.X, -halfSize.Y));
			outVertices.Add(FVector2D(halfSize.X, halfSize.Y));
			outVertices.Add(FVector2D(-halfSize.X, halfSize.Y));
		}

		if (CalculateSignedArea2D(outVertices) < 0.0)
		{
			Algo::Reverse(outVertices);
		}
	}
}

AScenarioGroundRegion* AScenarioGroundRegion::SpawnConfigured(
	UWorld* world,
	TSubclassOf<AScenarioGroundRegion> regionClass,
	const FScenarioGroundRegionSpec& regionSpec,
	FString& outFailureReason)
{
	outFailureReason.Reset();
	if (!world)
	{
		outFailureReason = TEXT("World is unavailable.");
		return nullptr;
	}
	if (regionSpec.RegionId.IsEmpty())
	{
		outFailureReason = TEXT("RegionId is empty.");
		return nullptr;
	}
	if (!IsSupportedGroundRegionShape(regionSpec))
	{
		outFailureReason = TEXT("Ground region shape is not supported.");
		return nullptr;
	}

	TSubclassOf<AScenarioGroundRegion> spawnClass = regionClass;
	if (!spawnClass)
	{
		spawnClass = AScenarioGroundRegion::StaticClass();
	}

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AScenarioGroundRegion* regionActor = world->SpawnActor<AScenarioGroundRegion>(
		spawnClass,
		FTransform::Identity,
		spawnParams);
	if (!regionActor)
	{
		outFailureReason = TEXT("SpawnActor failed.");
		return nullptr;
	}

	regionActor->ConfigureRegion(regionSpec);
	return regionActor;
}

AScenarioGroundRegion::AScenarioGroundRegion()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	RegionBoundsComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RegionBoundsComponent"));
	RegionBoundsComponent->SetupAttachment(SceneRoot);
	RegionBoundsComponent->SetVisibility(false);
	RegionBoundsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RegionBoundsComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	RegionBoundsComponent->SetGenerateOverlapEvents(false);
	RegionBoundsComponent->SetMobility(EComponentMobility::Movable);

	RegionVisualMeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RegionVisualMeshComponent"));
	RegionVisualMeshComponent->SetupAttachment(SceneRoot);
	RegionVisualMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RegionVisualMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	RegionVisualMeshComponent->SetGenerateOverlapEvents(false);
	RegionVisualMeshComponent->SetMobility(EComponentMobility::Movable);
	RegionVisualMeshComponent->SetCastShadow(false);
	RegionVisualMeshComponent->bUseAsyncCooking = true;

	PlaceableComponent = CreateDefaultSubobject<UScenarioPlaceableComponent>(TEXT("PlaceableComponent"));
	PlaceableComponent->Category = EScenarioActorCategory::GroundRegion;
	PlaceableComponent->AuthoringRole = EScenarioPlaceableAuthoringRole::Generic;
	PlaceableComponent->bAuthoringSelectable = true;
	PlaceableComponent->bAuthoringRenamable = false;
	PlaceableComponent->bAuthoringDeletable = true;
	PlaceableComponent->bAuthoringAllowLocationEdit = true;
	PlaceableComponent->bAuthoringAllowRotationEdit = true;
	PlaceableComponent->bAuthoringAllowScaleEdit = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> cubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (cubeMeshAsset.Succeeded())
	{
		RegionBoundsComponent->SetStaticMesh(cubeMeshAsset.Object);
	}

	SurfaceCatalog = UScenarioCorridorSurfaceCatalog::MakeDefaultCatalogReference();

	ApplyMaterialSettings();
}

void AScenarioGroundRegion::BeginPlay()
{
	Super::BeginPlay();

	ApplyMaterialSettings();
}

void AScenarioGroundRegion::ConfigureRegion(const FScenarioGroundRegionSpec& inRegionSpec)
{
	RegionSpec = inRegionSpec;
	RegionSpec.Size.X = FMath::Max(RegionSpec.Size.X, 1.0);
	RegionSpec.Size.Y = FMath::Max(RegionSpec.Size.Y, 1.0);
	if (RegionSpec.ShapeType == EScenarioGroundShapeType::ConvexPolygon)
	{
		const FBox2D LocalBounds = CalculateLocalPolygonBounds(RegionSpec.PolygonVertices);
		if (LocalBounds.bIsValid)
		{
			const FVector2D BoundsSize = LocalBounds.GetSize();
			RegionSpec.Size.X = FMath::Max(BoundsSize.X, 1.0);
			RegionSpec.Size.Y = FMath::Max(BoundsSize.Y, 1.0);
		}
	}

	if (PlaceableComponent)
	{
		const bool bGeneratedCityRegion = IsGeneratedCityRegionSpec(RegionSpec);
		PlaceableComponent->InstanceId = RegionSpec.RegionId;
		PlaceableComponent->bAuthoringSelectable = !bGeneratedCityRegion;
		PlaceableComponent->bAuthoringDeletable = !bGeneratedCityRegion;
		PlaceableComponent->bAuthoringAllowLocationEdit = !bGeneratedCityRegion;
		PlaceableComponent->bAuthoringAllowRotationEdit = !bGeneratedCityRegion;
		PlaceableComponent->bAuthoringAllowScaleEdit = false;
		if (bGeneratedCityRegion)
		{
			PlaceableComponent->SetAuthoringHovered(false);
			PlaceableComponent->SetAuthoringSelected(false);
		}
	}

	SetActorLocation(RegionSpec.Center);
	SetActorRotation(FRotator(0.0, RegionSpec.YawDegrees, 0.0));

	if (!RegionSpec.CollisionTag.IsEmpty())
	{
		Tags.AddUnique(FName(*RegionSpec.CollisionTag));
	}

	const double collisionHeightCm = RegionSpec.RegionType == EScenarioGroundRegionType::Blocked
		? BlockedCollisionHeightCm
		: GroundCollisionThicknessCm;
	const double safeCollisionHeightCm = FMath::Max(collisionHeightCm, 1.0);
	const FBox2D LocalPolygonBounds = RegionSpec.ShapeType == EScenarioGroundShapeType::ConvexPolygon
		? CalculateLocalPolygonBounds(RegionSpec.PolygonVertices)
		: FBox2D(ForceInit);
	const FVector2D LocalBoundsCenter = LocalPolygonBounds.bIsValid
		? LocalPolygonBounds.GetCenter()
		: FVector2D::ZeroVector;

	RegionBoundsComponent->SetRelativeLocation(FVector(
		LocalBoundsCenter.X,
		LocalBoundsCenter.Y,
		GetGroundRegionCollisionCenterZCm(RegionSpec.RegionType, safeCollisionHeightCm)));
	RegionBoundsComponent->SetRelativeScale3D(FVector(
		RegionSpec.Size.X / 100.0,
		RegionSpec.Size.Y / 100.0,
		safeCollisionHeightCm / 100.0));

	ApplyMaterialSettings();
	RebuildVisualMesh();
	ApplyCollisionSettings();
}

bool AScenarioGroundRegion::ContainsWorldLocation2D(const FVector& worldLocation) const
{
	const FVector localLocation = GetActorTransform().InverseTransformPosition(worldLocation);
	if (RegionSpec.ShapeType == EScenarioGroundShapeType::ConvexPolygon)
	{
		return IsPointInsideConvexPolygon2D(
			RegionSpec.PolygonVertices,
			FVector2D(localLocation.X, localLocation.Y));
	}

	const FVector2D halfSize(RegionSpec.Size.X * 0.5, RegionSpec.Size.Y * 0.5);
	return FMath::Abs(localLocation.X) <= halfSize.X
		&& FMath::Abs(localLocation.Y) <= halfSize.Y;
}

void AScenarioGroundRegion::ApplyCollisionSettings()
{
	if (!RegionBoundsComponent || !RegionVisualMeshComponent) return;

	RegionBoundsComponent->SetVisibility(false);
	RegionBoundsComponent->SetHiddenInGame(true);
	RegionBoundsComponent->SetGenerateOverlapEvents(false);

	RegionVisualMeshComponent->SetCollisionProfileName(
		FScenarioCorridorGeometry::ResolveRuntimeCollisionProfileName(RegionSpec.RegionType));
	RegionVisualMeshComponent->SetGenerateOverlapEvents(false);

	if (RegionSpec.ShapeType == EScenarioGroundShapeType::ConvexPolygon)
	{
		RegionBoundsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		RegionBoundsComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		RegionVisualMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		return;
	}

	RegionBoundsComponent->SetCollisionProfileName(
		FScenarioCorridorGeometry::ResolveRuntimeCollisionProfileName(RegionSpec.RegionType));
	RegionBoundsComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	RegionVisualMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RegionVisualMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
}

void AScenarioGroundRegion::ApplyMaterialSettings()
{
	if (!RegionBoundsComponent) return;

	UMaterialInterface* selectedMaterial = ResolveSurfaceCatalogMaterial();

	if (selectedMaterial)
	{
		RegionBoundsComponent->SetMaterial(0, selectedMaterial);
		if (RegionVisualMeshComponent)
		{
			RegionVisualMeshComponent->SetMaterial(0, selectedMaterial);
		}
	}
}

void AScenarioGroundRegion::RebuildVisualMesh()
{
	if (!RegionVisualMeshComponent)
	{
		return;
	}

	RegionVisualMeshComponent->ClearAllMeshSections();
	const bool bRenderVisualMesh = ShouldRenderVisualMesh();
	const bool bNeedsMeshCollision = RegionSpec.ShapeType == EScenarioGroundShapeType::ConvexPolygon;
	RegionVisualMeshComponent->SetVisibility(bRenderVisualMesh);
	RegionVisualMeshComponent->SetHiddenInGame(!bRenderVisualMesh);
	if (!bRenderVisualMesh && !bNeedsMeshCollision)
	{
		return;
	}

	TArray<FVector2D> LocalVertices2D;
	BuildGroundRegionLocalVertices(RegionSpec, LocalVertices2D);
	if (LocalVertices2D.Num() < 3)
	{
		return;
	}

	const FBox2D LocalBounds = CalculateLocalPolygonBounds(LocalVertices2D);
	const FVector2D LocalMin = LocalBounds.bIsValid ? LocalBounds.Min : FVector2D::ZeroVector;

	TArray<int32> Triangles;
	Triangles.Reserve((LocalVertices2D.Num() - 2) * 3);
	for (int32 Index = 1; Index < LocalVertices2D.Num() - 1; ++Index)
	{
		Triangles.Add(0);
		Triangles.Add(Index + 1);
		Triangles.Add(Index);
	}

	TArray<FVector> Vertices;
	Vertices.Reserve(LocalVertices2D.Num());
	for (const FVector2D& LocalVertex : LocalVertices2D)
	{
		Vertices.Add(FVector(LocalVertex.X, LocalVertex.Y, 0.0));
	}

	TArray<FVector> Normals;
	Normals.Init(FVector::UpVector, Vertices.Num());

	TArray<FVector2D> Uv0;
	Uv0.Reserve(LocalVertices2D.Num());
	for (const FVector2D& LocalVertex : LocalVertices2D)
	{
		Uv0.Add((LocalVertex - LocalMin) / 100.0);
	}

	TArray<FLinearColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;
	Tangents.Init(FProcMeshTangent(1.0f, 0.0f, 0.0f), Vertices.Num());

	RegionVisualMeshComponent->CreateMeshSection_LinearColor(
		0,
		Vertices,
		Triangles,
		Normals,
		Uv0,
		VertexColors,
		Tangents,
		bNeedsMeshCollision);
}

bool AScenarioGroundRegion::ShouldRenderVisualMesh() const
{
	return !(RegionSpec.RegionId.StartsWith(GeneratedCityRegionIdPrefix)
		&& RegionSpec.SurfaceId.Equals(GeneratedRoadSurfaceId, ESearchCase::IgnoreCase));
}

UMaterialInterface* AScenarioGroundRegion::ResolveSurfaceCatalogMaterial() const
{
	if (RegionSpec.SurfaceId.IsEmpty())
	{
		return nullptr;
	}

	FScenarioCorridorSurfaceEntry surfaceEntry;
	const FName surfaceName(*RegionSpec.SurfaceId);
	bool bResolvedSurfaceEntry = false;
	if (const UScenarioCorridorSurfaceCatalog* loadedCatalog = SurfaceCatalog.LoadSynchronous())
	{
		bResolvedSurfaceEntry = loadedCatalog->FindSurfaceEntryById(surfaceName, surfaceEntry);
	}
	else if (!SurfaceCatalog.IsNull())
	{
		UE_LOG(
			LogScenarioGroundRegion,
			Warning,
			TEXT("Corridor surface catalog could not be loaded for runtime ground region. Region: %s | Surface: %s | Path: %s"),
			*RegionSpec.RegionId,
			*RegionSpec.SurfaceId,
			*SurfaceCatalog.ToSoftObjectPath().ToString());
	}

	if (!bResolvedSurfaceEntry)
	{
		bResolvedSurfaceEntry = UScenarioCorridorSurfaceCatalog::FindDefaultSurfaceEntryById(surfaceName, surfaceEntry);
	}

	if (!bResolvedSurfaceEntry)
	{
		UE_LOG(
			LogScenarioGroundRegion,
			Warning,
			TEXT("Unknown Corridor surface for runtime ground region; using region-type material. Region: %s | Surface: %s"),
			*RegionSpec.RegionId,
			*RegionSpec.SurfaceId);
		return nullptr;
	}

	if (UMaterialInterface* catalogMaterial = surfaceEntry.PreviewMaterial.LoadSynchronous())
	{
		return catalogMaterial;
	}

	if (!surfaceEntry.PreviewMaterial.IsNull())
	{
		UE_LOG(
			LogScenarioGroundRegion,
			Warning,
			TEXT("Corridor surface material failed to load for runtime ground region; no material applied. Region: %s | Surface: %s | Path: %s"),
			*RegionSpec.RegionId,
			*RegionSpec.SurfaceId,
			*surfaceEntry.PreviewMaterial.ToSoftObjectPath().ToString());
	}

	return nullptr;
}
