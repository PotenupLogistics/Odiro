
#include "Episode/Actors/EpisodeStaticObstacle.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Episode/Components/EpisodeObstacleCollisionComponent.h"
#include "Episode/Components/EpisodePlaceableComponent.h"
#include "Episode/Data/EpisodeStaticObstaclePropCatalog.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/BodySetup.h"

DEFINE_LOG_CATEGORY_STATIC(LogEpisodeStaticObstacle, Log, All);

namespace
{
	const FString ObjectTypeActorTagPrefix{ TEXT("ObjectType.") };

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

	StaticObstaclePropCatalog = UEpisodeStaticObstaclePropCatalog::MakeDefaultCatalogReference();

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
	if (!TryFindConfiguredPropEntry(inPropId, propEntry)) return false;

	return ApplyPropEntry(propEntry);
}

bool AEpisodeStaticObstacle::TryFindConfiguredPropEntry(
	FName inPropId,
	FEpisodeStaticObstaclePropEntry& outPropEntry) const
{
	if (inPropId.IsNone()) return false;

	const UEpisodeStaticObstaclePropCatalog* propCatalog = StaticObstaclePropCatalog.LoadSynchronous();
	if (!IsValid(propCatalog))
	{
		UE_LOG(
			LogEpisodeStaticObstacle,
			Warning,
			TEXT("Episode static obstacle prop catalog is not configured or failed to load: %s"),
			*StaticObstaclePropCatalog.ToSoftObjectPath().ToString());
		return false;
	}

	return propCatalog->FindPropEntryById(inPropId, outPropEntry);
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
