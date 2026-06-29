
#include "Scenario/Actors/ScenarioGroundRegion.h"

#include "ProceduralMeshComponent.h"
#include "Scenario/Components/ScenarioPlaceableComponent.h"
#include "Scenario/ScenarioCorridorGeometry.h"
#include "Scenario/Data/ScenarioCorridorSurfaceCatalog.h"
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
	if (regionSpec.ShapeType != EScenarioGroundShapeType::Rectangle)
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

	if (PlaceableComponent)
	{
		PlaceableComponent->InstanceId = RegionSpec.RegionId;
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

	RegionBoundsComponent->SetRelativeLocation(FVector(
		0.0,
		0.0,
		GetGroundRegionCollisionCenterZCm(RegionSpec.RegionType, safeCollisionHeightCm)));
	RegionBoundsComponent->SetRelativeScale3D(FVector(
		RegionSpec.Size.X / 100.0,
		RegionSpec.Size.Y / 100.0,
		safeCollisionHeightCm / 100.0));

	ApplyMaterialSettings();
	ApplyCollisionSettings();
	RebuildVisualMesh();
}

bool AScenarioGroundRegion::ContainsWorldLocation2D(const FVector& worldLocation) const
{
	const FVector localLocation = GetActorTransform().InverseTransformPosition(worldLocation);
	const FVector2D halfSize(RegionSpec.Size.X * 0.5, RegionSpec.Size.Y * 0.5);

	return FMath::Abs(localLocation.X) <= halfSize.X
		&& FMath::Abs(localLocation.Y) <= halfSize.Y;
}

void AScenarioGroundRegion::ApplyCollisionSettings()
{
	if (!RegionBoundsComponent) return;

	RegionBoundsComponent->SetVisibility(false);
	RegionBoundsComponent->SetHiddenInGame(true);
	RegionBoundsComponent->SetCollisionProfileName(
		FScenarioCorridorGeometry::ResolveRuntimeCollisionProfileName(RegionSpec.RegionType));
	RegionBoundsComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	RegionBoundsComponent->SetGenerateOverlapEvents(false);
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
	RegionVisualMeshComponent->SetVisibility(bRenderVisualMesh);
	RegionVisualMeshComponent->SetHiddenInGame(!bRenderVisualMesh);
	if (!bRenderVisualMesh)
	{
		return;
	}

	const double HalfLengthCm = RegionSpec.Size.X * 0.5;
	const double HalfWidthCm = RegionSpec.Size.Y * 0.5;
	const double LengthMeters = RegionSpec.Size.X / 100.0;
	const double WidthMeters = RegionSpec.Size.Y / 100.0;

	TArray<FVector> Vertices;
	Vertices.Reserve(4);
	Vertices.Add(FVector(-HalfLengthCm, -HalfWidthCm, 0.0));
	Vertices.Add(FVector(HalfLengthCm, -HalfWidthCm, 0.0));
	Vertices.Add(FVector(HalfLengthCm, HalfWidthCm, 0.0));
	Vertices.Add(FVector(-HalfLengthCm, HalfWidthCm, 0.0));

	TArray<int32> Triangles;
	Triangles.Reserve(6);
	Triangles.Add(3);
	Triangles.Add(1);
	Triangles.Add(0);
	Triangles.Add(3);
	Triangles.Add(2);
	Triangles.Add(1);

	TArray<FVector> Normals;
	Normals.Init(FVector::UpVector, 4);

	TArray<FVector2D> Uv0;
	Uv0.Reserve(4);
	Uv0.Add(FVector2D(0.0, 0.0));
	Uv0.Add(FVector2D(LengthMeters, 0.0));
	Uv0.Add(FVector2D(LengthMeters, WidthMeters));
	Uv0.Add(FVector2D(0.0, WidthMeters));

	TArray<FLinearColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;
	Tangents.Init(FProcMeshTangent(1.0f, 0.0f, 0.0f), 4);

	RegionVisualMeshComponent->CreateMeshSection_LinearColor(
		0,
		Vertices,
		Triangles,
		Normals,
		Uv0,
		VertexColors,
		Tangents,
		false);
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
