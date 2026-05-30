
#include "Episode/Actors/EpisodeStaticObstacle.h"
#include "Components/StaticMeshComponent.h"
#include "Episode/Components/EpisodeObstacleCollisionComponent.h"
#include "Episode/Components/EpisodePlaceableComponent.h"

namespace
{
	FEpisodeStaticObstaclePropEntry MakeStaticObstaclePropEntry(
		const TCHAR* PropId,
		const TCHAR* SemanticTypeId,
		const TCHAR* DisplayName,
		EEpisodeStaticObstaclePropCategory Category,
		const TCHAR* MeshPath,
		const FVector& FallbackBoxExtent,
		double SafetyRadius)
	{
		FEpisodeStaticObstaclePropEntry Entry;
		Entry.PropId = FName(PropId);
		Entry.SemanticTypeId = FName(SemanticTypeId);
		Entry.DisplayName = FText::FromString(DisplayName);
		Entry.Category = Category;
		Entry.StaticMeshAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(MeshPath));
		Entry.FallbackBoxExtent = FallbackBoxExtent;
		Entry.SafetyRadius = SafetyRadius;
		return Entry;
	}
}

AEpisodeStaticObstacle::AEpisodeStaticObstacle()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshRoot = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshRoot"));
	SetRootComponent(MeshRoot);

	PlaceableComponent = CreateDefaultSubobject<UEpisodePlaceableComponent>(TEXT("PlaceableComponent"));
	ObstacleCollisionComponent = CreateDefaultSubobject<UEpisodeObstacleCollisionComponent>(TEXT("ObstacleCollisionComponent"));
}

void AEpisodeStaticObstacle::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ApplyConfiguredStaticMesh();
}

bool AEpisodeStaticObstacle::SetStaticMeshAsset(TSoftObjectPtr<UStaticMesh> InStaticMeshAsset)
{
	StaticMeshAsset = InStaticMeshAsset;
	return ApplyConfiguredStaticMesh();
}

void AEpisodeStaticObstacle::SetStaticMesh(UStaticMesh* InStaticMesh)
{
	StaticMeshAsset = InStaticMesh;

	if (MeshRoot)
	{
		MeshRoot->SetStaticMesh(InStaticMesh);
	}
}

bool AEpisodeStaticObstacle::ApplyConfiguredStaticMesh()
{
	if (!MeshRoot)
	{
		return false;
	}

	if (StaticMeshAsset.IsNull())
	{
		return MeshRoot->GetStaticMesh() != nullptr;
	}

	UStaticMesh* LoadedMesh = StaticMeshAsset.LoadSynchronous();
	if (!LoadedMesh)
	{
		return false;
	}

	MeshRoot->SetStaticMesh(LoadedMesh);
	return true;
}

bool AEpisodeStaticObstacle::ApplyPropEntry(const FEpisodeStaticObstaclePropEntry& PropEntry)
{
	if (PropEntry.PropId.IsNone())
	{
		return false;
	}

	PropId = PropEntry.PropId;
	SemanticTypeId = PropEntry.SemanticTypeId;
	PropDisplayName = PropEntry.DisplayName;
	PropCategory = PropEntry.Category;
	StaticMeshAsset = PropEntry.StaticMeshAsset;
	FallbackBoxExtent = PropEntry.FallbackBoxExtent;

	if (ObstacleCollisionComponent)
	{
		ObstacleCollisionComponent->bUsePhysicalCollision = PropEntry.bUsePhysicalCollision;
		ObstacleCollisionComponent->bUseSafetyQuery = PropEntry.bUseSafetyQuery;
		ObstacleCollisionComponent->SafetyRadius = PropEntry.SafetyRadius;
	}

	return ApplyConfiguredStaticMesh();
}

bool AEpisodeStaticObstacle::ApplyDefaultPropById(FName InPropId)
{
	FEpisodeStaticObstaclePropEntry PropEntry;
	if (!FindDefaultPropEntryById(InPropId, PropEntry))
	{
		return false;
	}

	return ApplyPropEntry(PropEntry);
}

bool AEpisodeStaticObstacle::FindDefaultPropEntryById(FName InPropId, FEpisodeStaticObstaclePropEntry& OutPropEntry)
{
	if (InPropId.IsNone())
	{
		return false;
	}

	for (const FEpisodeStaticObstaclePropEntry& PropEntry : GetDefaultPropEntries())
	{
		if (PropEntry.PropId == InPropId)
		{
			OutPropEntry = PropEntry;
			return true;
		}
	}

	return false;
}

const TArray<FEpisodeStaticObstaclePropEntry>& AEpisodeStaticObstacle::GetDefaultPropEntries()
{
	static const TArray<FEpisodeStaticObstaclePropEntry> PropEntries =
	{
		MakeStaticObstaclePropEntry(TEXT("obstacle.bin"), TEXT("bin"), TEXT("Bin"), EEpisodeStaticObstaclePropCategory::StreetFurniture, TEXT("/Game/Episode/Mesh/SM_Bin.SM_Bin"), FVector(45.0, 45.0, 90.0), 75.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.box_01"), TEXT("box"), TEXT("Box 01"), EEpisodeStaticObstaclePropCategory::DeliveryItem, TEXT("/Game/Episode/Mesh/SM_Box_01.SM_Box_01"), FVector(45.0, 45.0, 45.0), 70.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.box_02"), TEXT("box"), TEXT("Box 02"), EEpisodeStaticObstaclePropCategory::DeliveryItem, TEXT("/Game/Episode/Mesh/SM_Box_02.SM_Box_02"), FVector(45.0, 45.0, 45.0), 70.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.box_03"), TEXT("box"), TEXT("Box 03"), EEpisodeStaticObstaclePropCategory::DeliveryItem, TEXT("/Game/Episode/Mesh/SM_Box_03.SM_Box_03"), FVector(45.0, 45.0, 45.0), 70.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.fire_hydrant"), TEXT("fire_hydrant"), TEXT("Fire Hydrant"), EEpisodeStaticObstaclePropCategory::Utility, TEXT("/Game/Episode/Mesh/SM_FireHydrant.SM_FireHydrant"), FVector(35.0, 35.0, 80.0), 60.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.mailbox"), TEXT("mailbox"), TEXT("Mailbox"), EEpisodeStaticObstaclePropCategory::StreetFurniture, TEXT("/Game/Episode/Mesh/SM_Mailbox.SM_Mailbox"), FVector(55.0, 45.0, 90.0), 80.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.manhole_01"), TEXT("manhole"), TEXT("Manhole 01"), EEpisodeStaticObstaclePropCategory::SurfaceObject, TEXT("/Game/Episode/Mesh/SM_Manhole_01.SM_Manhole_01"), FVector(55.0, 55.0, 5.0), 65.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.manhole_02"), TEXT("manhole"), TEXT("Manhole 02"), EEpisodeStaticObstaclePropCategory::SurfaceObject, TEXT("/Game/Episode/Mesh/SM_Manhole_02.SM_Manhole_02"), FVector(55.0, 55.0, 5.0), 65.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.manhole_03"), TEXT("manhole"), TEXT("Manhole 03"), EEpisodeStaticObstaclePropCategory::SurfaceObject, TEXT("/Game/Episode/Mesh/SM_Manhole_03.SM_Manhole_03"), FVector(55.0, 55.0, 5.0), 65.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.manhole_04"), TEXT("manhole"), TEXT("Manhole 04"), EEpisodeStaticObstaclePropCategory::SurfaceObject, TEXT("/Game/Episode/Mesh/SM_Manhole_04.SM_Manhole_04"), FVector(55.0, 55.0, 5.0), 65.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.road_cone_01"), TEXT("road_cone"), TEXT("Road Cone 01"), EEpisodeStaticObstaclePropCategory::TrafficControl, TEXT("/Game/Episode/Mesh/SM_RoadCone_01.SM_RoadCone_01"), FVector(35.0, 35.0, 70.0), 55.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.road_cone_02"), TEXT("road_cone"), TEXT("Road Cone 02"), EEpisodeStaticObstaclePropCategory::TrafficControl, TEXT("/Game/Episode/Mesh/SM_RoadCone_02.SM_RoadCone_02"), FVector(35.0, 35.0, 70.0), 55.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.road_barrier_01"), TEXT("road_barrier"), TEXT("Road Barrier 01"), EEpisodeStaticObstaclePropCategory::TrafficControl, TEXT("/Game/Episode/Mesh/SM_Road_Barrier_01.SM_Road_Barrier_01"), FVector(120.0, 35.0, 60.0), 130.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.road_barrier_02"), TEXT("road_barrier"), TEXT("Road Barrier 02"), EEpisodeStaticObstaclePropCategory::TrafficControl, TEXT("/Game/Episode/Mesh/SM_Road_Barrier_02.SM_Road_Barrier_02"), FVector(120.0, 35.0, 60.0), 130.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.street_bank"), TEXT("street_bank"), TEXT("Street Bank"), EEpisodeStaticObstaclePropCategory::StreetFurniture, TEXT("/Game/Episode/Mesh/SM_StreetBank.SM_StreetBank"), FVector(100.0, 45.0, 60.0), 115.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.trash_bin"), TEXT("trash_bin"), TEXT("Trash Bin"), EEpisodeStaticObstaclePropCategory::StreetFurniture, TEXT("/Game/Episode/Mesh/SM_Trash_Bin.SM_Trash_Bin"), FVector(45.0, 45.0, 90.0), 75.0)
	};

	return PropEntries;
}

TArray<FName> AEpisodeStaticObstacle::GetDefaultPropIds()
{
	TArray<FName> PropIds;
	PropIds.Reserve(GetDefaultPropEntries().Num());

	for (const FEpisodeStaticObstaclePropEntry& PropEntry : GetDefaultPropEntries())
	{
		if (!PropEntry.PropId.IsNone())
		{
			PropIds.Add(PropEntry.PropId);
		}
	}

	return PropIds;
}

bool AEpisodeStaticObstacle::GetPlacementBounds(
	FVector& OutOrigin,
	FVector& OutBoxExtent,
	FVector2D& OutHalfSize2D,
	double& OutRadius2D) const
{
	OutOrigin = GetActorLocation();
	OutBoxExtent = FVector::ZeroVector;
	OutHalfSize2D = FVector2D::ZeroVector;
	OutRadius2D = 0.0;

	if (MeshRoot && MeshRoot->GetStaticMesh())
	{
		const FBoxSphereBounds MeshBounds = MeshRoot->Bounds;
		OutOrigin = MeshBounds.Origin;
		OutBoxExtent = MeshBounds.BoxExtent;
	}
	else if (bUseFallbackBoundsWhenMeshMissing)
	{
		OutBoxExtent = FVector(
			FMath::Max(FallbackBoxExtent.X, 0.0),
			FMath::Max(FallbackBoxExtent.Y, 0.0),
			FMath::Max(FallbackBoxExtent.Z, 0.0));
	}
	else
	{
		return false;
	}

	OutHalfSize2D = FVector2D(OutBoxExtent.X, OutBoxExtent.Y);
	OutRadius2D = FMath::Sqrt(FMath::Square(OutHalfSize2D.X) + FMath::Square(OutHalfSize2D.Y));
	return OutRadius2D > KINDA_SMALL_NUMBER;
}

double AEpisodeStaticObstacle::GetPlacementRadius2D() const
{
	FVector Origin = FVector::ZeroVector;
	FVector BoxExtent = FVector::ZeroVector;
	FVector2D HalfSize2D = FVector2D::ZeroVector;
	double Radius2D = 0.0;

	GetPlacementBounds(Origin, BoxExtent, HalfSize2D, Radius2D);
	return Radius2D;
}
