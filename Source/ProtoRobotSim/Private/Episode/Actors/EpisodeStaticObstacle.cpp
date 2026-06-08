
#include "Episode/Actors/EpisodeStaticObstacle.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Episode/Components/EpisodeObstacleCollisionComponent.h"
#include "Episode/Components/EpisodePlaceableComponent.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/BodySetup.h"

namespace
{
	const FString ObjectTypeActorTagPrefix{ TEXT("ObjectType.") };

	FEpisodeStaticObstaclePropEntry MakeStaticObstaclePropEntry(
		const TCHAR* propId,
		const TCHAR* semanticTypeId,
		const TCHAR* displayName,
		EEpisodeStaticObstaclePropCategory category,
		const TCHAR* meshPath,
		const FVector& fallbackBoxExtent,
		double safetyRadius,
		bool bUseMeshSimpleCollision = true,
		bool bUseFallbackBoxCollision = true)
	{
		FEpisodeStaticObstaclePropEntry entry;
		entry.PropId = FName(propId);
		entry.SemanticTypeId = FName(semanticTypeId);
		entry.DisplayName = FText::FromString(displayName);
		entry.Category = category;
		entry.StaticMeshAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(meshPath));
		entry.FallbackBoxExtent = fallbackBoxExtent;
		entry.SafetyRadius = safetyRadius;
		entry.bUseMeshSimpleCollision = bUseMeshSimpleCollision;
		entry.bUseFallbackBoxCollision = bUseFallbackBoxCollision;
		return entry;
	}

	bool StaticMeshHasSimpleCollision(UStaticMesh* staticMesh)
	{
		if (!staticMesh) return false;

		const UBodySetup* bodySetup = staticMesh->GetBodySetup();
		if (!bodySetup) return false;

		return bodySetup->AggGeom.GetElementCount() > 0;
	}

	FVector ClampCollisionBoxExtent(const FVector& boxExtent)
	{
		return FVector(
			FMath::Max(boxExtent.X, 1.0),
			FMath::Max(boxExtent.Y, 1.0),
			FMath::Max(boxExtent.Z, 1.0));
	}

	void ConfigureObstacleCollisionPrimitive(
		UPrimitiveComponent* primitiveComponent,
		ECollisionEnabled::Type collisionEnabled)
	{
		if (!primitiveComponent) return;

		primitiveComponent->SetCollisionEnabled(collisionEnabled);
		primitiveComponent->SetGenerateOverlapEvents(false);

		if (collisionEnabled == ECollisionEnabled::NoCollision)
		{
			primitiveComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
			return;
		}

		primitiveComponent->SetCollisionObjectType(ECC_WorldStatic);
		primitiveComponent->SetCollisionResponseToAllChannels(ECR_Block);
	}

	FName MakeObjectTypeActorTag(FName semanticTypeId)
	{
		if (semanticTypeId.IsNone())
		{
			return NAME_None;
		}

		return FName(*(ObjectTypeActorTagPrefix + semanticTypeId.ToString()));
	}
}

AEpisodeStaticObstacle::AEpisodeStaticObstacle()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshRoot = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshRoot"));
	SetRootComponent(MeshRoot);
	MeshRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshRoot->SetCollisionResponseToAllChannels(ECR_Ignore);
	MeshRoot->SetGenerateOverlapEvents(false);

	CollisionBoundsComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBoundsComponent"));
	CollisionBoundsComponent->SetupAttachment(MeshRoot);
	CollisionBoundsComponent->SetHiddenInGame(true);
	CollisionBoundsComponent->SetVisibility(false);
	CollisionBoundsComponent->SetGenerateOverlapEvents(false);

	PlaceableComponent = CreateDefaultSubobject<UEpisodePlaceableComponent>(TEXT("PlaceableComponent"));
	ObstacleCollisionComponent = CreateDefaultSubobject<UEpisodeObstacleCollisionComponent>(TEXT("ObstacleCollisionComponent"));

	ApplyCollisionSettings();
}

void AEpisodeStaticObstacle::OnConstruction(const FTransform& transform)
{
	Super::OnConstruction(transform);

	ApplyConfiguredStaticMesh();
	ApplyObjectTypeActorTag();
	ApplyCollisionSettings();
}

bool AEpisodeStaticObstacle::SetStaticMeshAsset(TSoftObjectPtr<UStaticMesh> inStaticMeshAsset)
{
	StaticMeshAsset = inStaticMeshAsset;
	return ApplyConfiguredStaticMesh();
}

void AEpisodeStaticObstacle::SetStaticMesh(UStaticMesh* inStaticMesh)
{
	StaticMeshAsset = inStaticMesh;

	if (MeshRoot)
	{
		MeshRoot->SetStaticMesh(inStaticMesh);
	}
	ApplyCollisionSettings();
}

bool AEpisodeStaticObstacle::ApplyConfiguredStaticMesh()
{
	if (!MeshRoot) return false;

	if (StaticMeshAsset.IsNull())
	{
		ApplyCollisionSettings();
		return MeshRoot->GetStaticMesh() != nullptr;
	}

	UStaticMesh* loadedMesh = StaticMeshAsset.LoadSynchronous();
	if (!loadedMesh) return false;

	MeshRoot->SetStaticMesh(loadedMesh);
	ApplyCollisionSettings();
	return true;
}

bool AEpisodeStaticObstacle::ApplyPropEntry(const FEpisodeStaticObstaclePropEntry& propEntry)
{
	if (propEntry.PropId.IsNone()) return false;

	PropId = propEntry.PropId;
	SemanticTypeId = propEntry.SemanticTypeId;
	PropDisplayName = propEntry.DisplayName;
	PropCategory = propEntry.Category;
	StaticMeshAsset = propEntry.StaticMeshAsset;
	FallbackBoxExtent = propEntry.FallbackBoxExtent;
	bUseMeshSimpleCollision = propEntry.bUseMeshSimpleCollision;
	bUseFallbackBoxCollision = propEntry.bUseFallbackBoxCollision;

	if (ObstacleCollisionComponent)
	{
		ObstacleCollisionComponent->bUsePhysicalCollision = propEntry.bUsePhysicalCollision;
		ObstacleCollisionComponent->bUseSafetyQuery = propEntry.bUseSafetyQuery;
		ObstacleCollisionComponent->SafetyRadius = propEntry.SafetyRadius;
	}

	const bool bAppliedMesh = ApplyConfiguredStaticMesh();
	ApplyObjectTypeActorTag();
	ApplyCollisionSettings();
	return bAppliedMesh;
}

void AEpisodeStaticObstacle::ApplyObjectTypeActorTag()
{
	Tags.RemoveAll(
		[](const FName& tag)
		{
			return tag.ToString().StartsWith(ObjectTypeActorTagPrefix);
		});

	const FName objectTypeTag = MakeObjectTypeActorTag(SemanticTypeId);
	if (!objectTypeTag.IsNone())
	{
		Tags.AddUnique(objectTypeTag);
	}
}

void AEpisodeStaticObstacle::ApplyCollisionSettings()
{
	const bool bUsePhysicalCollision = !ObstacleCollisionComponent || ObstacleCollisionComponent->bUsePhysicalCollision;
	const bool bUseSafetyQuery = !ObstacleCollisionComponent || ObstacleCollisionComponent->bUseSafetyQuery;
	const ECollisionEnabled::Type collisionEnabled = bUsePhysicalCollision
		? ECollisionEnabled::QueryAndPhysics
		: (bUseSafetyQuery ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);

	const bool bUseMeshCollision = bUseMeshSimpleCollision
		&& MeshRoot
		&& StaticMeshHasSimpleCollision(MeshRoot->GetStaticMesh());
	const bool bUseFallbackCollision = !bUseMeshCollision && bUseFallbackBoxCollision;

	ConfigureObstacleCollisionPrimitive(
		MeshRoot,
		bUseMeshCollision ? collisionEnabled : ECollisionEnabled::NoCollision);

	if (!CollisionBoundsComponent)
	{
		return;
	}

	if (!bUseFallbackCollision)
	{
		ConfigureObstacleCollisionPrimitive(CollisionBoundsComponent, ECollisionEnabled::NoCollision);
		return;
	}

	const FVector boxExtent = ClampCollisionBoxExtent(FallbackBoxExtent);

	CollisionBoundsComponent->SetBoxExtent(boxExtent, false);
	CollisionBoundsComponent->SetRelativeLocation(FVector(0.0, 0.0, boxExtent.Z));
	ConfigureObstacleCollisionPrimitive(CollisionBoundsComponent, collisionEnabled);
}

bool AEpisodeStaticObstacle::ApplyDefaultPropById(FName inPropId)
{
	FEpisodeStaticObstaclePropEntry propEntry;
	if (!FindDefaultPropEntryById(inPropId, propEntry)) return false;

	return ApplyPropEntry(propEntry);
}

bool AEpisodeStaticObstacle::FindDefaultPropEntryById(FName inPropId, FEpisodeStaticObstaclePropEntry& outPropEntry)
{
	if (inPropId.IsNone()) return false;

	for (const FEpisodeStaticObstaclePropEntry& propEntry : GetDefaultPropEntries())
	{
		if (propEntry.PropId == inPropId)
		{
			outPropEntry = propEntry;
			return true;
		}
	}

	return false;
}

const TArray<FEpisodeStaticObstaclePropEntry>& AEpisodeStaticObstacle::GetDefaultPropEntries()
{
	static const TArray<FEpisodeStaticObstaclePropEntry> propEntries =
	{
		MakeStaticObstaclePropEntry(TEXT("obstacle.bin"), TEXT("bin"), TEXT("Bin"), EEpisodeStaticObstaclePropCategory::StreetFurniture, TEXT("/Game/Models/Placeable/StaticMeshes/SM_Bin.SM_Bin"), FVector(45.0, 45.0, 90.0), 75.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.box_01"), TEXT("box"), TEXT("Box 01"), EEpisodeStaticObstaclePropCategory::DeliveryItem, TEXT("/Game/Models/Placeable/StaticMeshes/SM_Box_01.SM_Box_01"), FVector(45.0, 45.0, 45.0), 70.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.box_02"), TEXT("box"), TEXT("Box 02"), EEpisodeStaticObstaclePropCategory::DeliveryItem, TEXT("/Game/Models/Placeable/StaticMeshes/SM_Box_02.SM_Box_02"), FVector(45.0, 45.0, 45.0), 70.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.box_03"), TEXT("box"), TEXT("Box 03"), EEpisodeStaticObstaclePropCategory::DeliveryItem, TEXT("/Game/Models/Placeable/StaticMeshes/SM_Box_03.SM_Box_03"), FVector(45.0, 45.0, 45.0), 70.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.fire_hydrant"), TEXT("fire_hydrant"), TEXT("Fire Hydrant"), EEpisodeStaticObstaclePropCategory::Utility, TEXT("/Game/Models/Placeable/StaticMeshes/SM_FireHydrant.SM_FireHydrant"), FVector(35.0, 35.0, 80.0), 60.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.mailbox"), TEXT("mailbox"), TEXT("Mailbox"), EEpisodeStaticObstaclePropCategory::StreetFurniture, TEXT("/Game/Models/Placeable/StaticMeshes/SM_Mailbox.SM_Mailbox"), FVector(55.0, 45.0, 90.0), 80.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.manhole_01"), TEXT("manhole"), TEXT("Manhole 01"), EEpisodeStaticObstaclePropCategory::SurfaceObject, TEXT("/Game/Models/Placeable/StaticMeshes/SM_Manhole_01.SM_Manhole_01"), FVector(55.0, 55.0, 5.0), 65.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.manhole_02"), TEXT("manhole"), TEXT("Manhole 02"), EEpisodeStaticObstaclePropCategory::SurfaceObject, TEXT("/Game/Models/Placeable/StaticMeshes/SM_Manhole_02.SM_Manhole_02"), FVector(55.0, 55.0, 5.0), 65.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.manhole_03"), TEXT("manhole"), TEXT("Manhole 03"), EEpisodeStaticObstaclePropCategory::SurfaceObject, TEXT("/Game/Models/Placeable/StaticMeshes/SM_Manhole_03.SM_Manhole_03"), FVector(55.0, 55.0, 5.0), 65.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.manhole_04"), TEXT("manhole"), TEXT("Manhole 04"), EEpisodeStaticObstaclePropCategory::SurfaceObject, TEXT("/Game/Models/Placeable/StaticMeshes/SM_Manhole_04.SM_Manhole_04"), FVector(55.0, 55.0, 5.0), 65.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.road_cone_01"), TEXT("road_cone"), TEXT("Road Cone 01"), EEpisodeStaticObstaclePropCategory::TrafficControl, TEXT("/Game/Models/Placeable/StaticMeshes/SM_RoadCone_01.SM_RoadCone_01"), FVector(35.0, 35.0, 70.0), 55.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.road_cone_02"), TEXT("road_cone"), TEXT("Road Cone 02"), EEpisodeStaticObstaclePropCategory::TrafficControl, TEXT("/Game/Models/Placeable/StaticMeshes/SM_RoadCone_02.SM_RoadCone_02"), FVector(35.0, 35.0, 70.0), 55.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.road_barrier_01"), TEXT("road_barrier"), TEXT("Road Barrier 01"), EEpisodeStaticObstaclePropCategory::TrafficControl, TEXT("/Game/Models/Placeable/StaticMeshes/SM_Road_Barrier_01.SM_Road_Barrier_01"), FVector(120.0, 35.0, 60.0), 130.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.road_barrier_02"), TEXT("road_barrier"), TEXT("Road Barrier 02"), EEpisodeStaticObstaclePropCategory::TrafficControl, TEXT("/Game/Models/Placeable/StaticMeshes/SM_Road_Barrier_02.SM_Road_Barrier_02"), FVector(120.0, 35.0, 60.0), 130.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.street_bank"), TEXT("street_bank"), TEXT("Street Bank"), EEpisodeStaticObstaclePropCategory::StreetFurniture, TEXT("/Game/Models/Placeable/StaticMeshes/SM_StreetBank.SM_StreetBank"), FVector(100.0, 45.0, 60.0), 115.0),
		MakeStaticObstaclePropEntry(TEXT("obstacle.trash_bin"), TEXT("trash_bin"), TEXT("Trash Bin"), EEpisodeStaticObstaclePropCategory::StreetFurniture, TEXT("/Game/Models/Placeable/StaticMeshes/SM_Trash_Bin.SM_Trash_Bin"), FVector(45.0, 45.0, 90.0), 75.0)
	};

	return propEntries;
}

TArray<FName> AEpisodeStaticObstacle::GetDefaultPropIds()
{
	TArray<FName> propIds;
	propIds.Reserve(GetDefaultPropEntries().Num());

	for (const FEpisodeStaticObstaclePropEntry& propEntry : GetDefaultPropEntries())
	{
		if (!propEntry.PropId.IsNone())
		{
			propIds.Add(propEntry.PropId);
		}
	}

	return propIds;
}

bool AEpisodeStaticObstacle::GetPlacementBounds(
	FVector& outOrigin,
	FVector& outBoxExtent,
	FVector2D& outHalfSize2D,
	double& outRadius2D) const
{
	outOrigin = GetActorLocation();
	outBoxExtent = FVector::ZeroVector;
	outHalfSize2D = FVector2D::ZeroVector;
	outRadius2D = 0.0;

	if (MeshRoot && MeshRoot->GetStaticMesh())
	{
		const FBoxSphereBounds meshBounds = MeshRoot->Bounds;
		outOrigin = meshBounds.Origin;
		outBoxExtent = meshBounds.BoxExtent;
	}
	else if (bUseFallbackBoundsWhenMeshMissing)
	{
		outBoxExtent = FVector(
			FMath::Max(FallbackBoxExtent.X, 0.0),
			FMath::Max(FallbackBoxExtent.Y, 0.0),
			FMath::Max(FallbackBoxExtent.Z, 0.0));
	}
	else
	{
		return false;
	}

	outHalfSize2D = FVector2D(outBoxExtent.X, outBoxExtent.Y);
	outRadius2D = FMath::Sqrt(FMath::Square(outHalfSize2D.X) + FMath::Square(outHalfSize2D.Y));
	return outRadius2D > KINDA_SMALL_NUMBER;
}

double AEpisodeStaticObstacle::GetPlacementRadius2D() const
{
	FVector origin = FVector::ZeroVector;
	FVector boxExtent = FVector::ZeroVector;
	FVector2D halfSize2D = FVector2D::ZeroVector;
	double radius2D = 0.0;

	GetPlacementBounds(origin, boxExtent, halfSize2D, radius2D);
	return radius2D;
}
