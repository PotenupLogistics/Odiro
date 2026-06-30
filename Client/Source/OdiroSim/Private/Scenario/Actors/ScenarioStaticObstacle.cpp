
#include "Scenario/Actors/ScenarioStaticObstacle.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Scenario/Components/ScenarioObstacleCollisionComponent.h"
#include "Scenario/Components/ScenarioPlaceableComponent.h"
#include "Scenario/Data/ScenarioStaticObstaclePropCatalog.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/BodySetup.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioStaticObstacle, Log, All);

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

	TSubclassOf<AScenarioStaticObstacle> ResolveStaticObstacleSpawnClass(
		TSubclassOf<AScenarioStaticObstacle> fallbackClass,
		const FScenarioStaticObstaclePropEntry& propEntry,
		FString& outFailureReason)
	{
		if (!propEntry.ObstacleActorClass.IsNull())
		{
			UClass* loadedClass = propEntry.ObstacleActorClass.LoadSynchronous();
			if (!loadedClass || !loadedClass->IsChildOf(AScenarioStaticObstacle::StaticClass()))
			{
				outFailureReason = FString::Printf(
					TEXT("Static obstacle prop '%s' actor class is invalid: %s"),
					*propEntry.PropId.ToString(),
					*propEntry.ObstacleActorClass.ToSoftObjectPath().ToString());
				return nullptr;
			}

			return loadedClass;
		}

		if (fallbackClass)
		{
			return fallbackClass;
		}

		return AScenarioStaticObstacle::StaticClass();
	}

}

AScenarioStaticObstacle* AScenarioStaticObstacle::SpawnConfigured(
	UWorld* world,
	TSubclassOf<AScenarioStaticObstacle> obstacleClass,
	const FTransform& transform,
	const FScenarioStaticObstaclePropEntry& propEntry,
	FString& outFailureReason)
{
	outFailureReason.Reset();
	if (!world)
	{
		outFailureReason = TEXT("World is unavailable.");
		return nullptr;
	}
	if (propEntry.PropId.IsNone())
	{
		outFailureReason = TEXT("Static obstacle prop entry is invalid.");
		return nullptr;
	}

	TSubclassOf<AScenarioStaticObstacle> spawnClass =
		ResolveStaticObstacleSpawnClass(obstacleClass, propEntry, outFailureReason);
	if (!spawnClass)
	{
		return nullptr;
	}

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AScenarioStaticObstacle* staticObstacle = world->SpawnActor<AScenarioStaticObstacle>(
		spawnClass,
		transform,
		spawnParams);
	if (!staticObstacle)
	{
		outFailureReason = TEXT("SpawnActor failed.");
		return nullptr;
	}

	if (!staticObstacle->ApplyPropEntry(propEntry))
	{
		outFailureReason = FString::Printf(TEXT("Failed to apply prop '%s'."), *propEntry.PropId.ToString());
		staticObstacle->Destroy();
		return nullptr;
	}

	return staticObstacle;
}

AScenarioStaticObstacle::AScenarioStaticObstacle()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MeshRoot = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshRoot"));
	MeshRoot->SetupAttachment(SceneRoot);
	MeshRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshRoot->SetCollisionResponseToAllChannels(ECR_Ignore);
	MeshRoot->SetGenerateOverlapEvents(false);

	CollisionBoundsComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBoundsComponent"));
	CollisionBoundsComponent->SetupAttachment(SceneRoot);
	CollisionBoundsComponent->SetHiddenInGame(true);
	CollisionBoundsComponent->SetVisibility(false);
	CollisionBoundsComponent->SetGenerateOverlapEvents(false);

	PlaceableComponent = CreateDefaultSubobject<UScenarioPlaceableComponent>(TEXT("PlaceableComponent"));
	ObstacleCollisionComponent = CreateDefaultSubobject<UScenarioObstacleCollisionComponent>(TEXT("ObstacleCollisionComponent"));

	StaticObstaclePropCatalog = UScenarioStaticObstaclePropCatalog::MakeDefaultCatalogReference();

	ApplyCollisionSettings();
}

void AScenarioStaticObstacle::OnConstruction(const FTransform& transform)
{
	Super::OnConstruction(transform);

	ApplyConfiguredStaticMesh();
	ApplyObjectTypeActorTag();
	ApplyCollisionSettings();
}

bool AScenarioStaticObstacle::SetStaticMeshAsset(TSoftObjectPtr<UStaticMesh> inStaticMeshAsset)
{
	StaticMeshAsset = inStaticMeshAsset;
	return ApplyConfiguredStaticMesh();
}

void AScenarioStaticObstacle::SetStaticMesh(UStaticMesh* inStaticMesh)
{
	StaticMeshAsset = inStaticMesh;

	if (MeshRoot)
	{
		MeshRoot->SetStaticMesh(inStaticMesh);
	}
	ApplyMeshGroundAlignment();
	ApplyCollisionSettings();
}

bool AScenarioStaticObstacle::ApplyConfiguredStaticMesh()
{
	if (!MeshRoot) return false;

	if (StaticMeshAsset.IsNull())
	{
		ApplyMeshGroundAlignment();
		ApplyCollisionSettings();
		return MeshRoot->GetStaticMesh() != nullptr;
	}

	UStaticMesh* loadedMesh = StaticMeshAsset.LoadSynchronous();
	if (!loadedMesh) return false;

	MeshRoot->SetStaticMesh(loadedMesh);
	ApplyMeshGroundAlignment();
	ApplyCollisionSettings();
	return true;
}

bool AScenarioStaticObstacle::ApplyPropEntry(const FScenarioStaticObstaclePropEntry& propEntry)
{
	if (propEntry.PropId.IsNone()) return false;

	PropId = propEntry.PropId;
	SemanticTypeId = propEntry.SemanticTypeId;
	PropDisplayName = propEntry.DisplayName;
	PropCategory = propEntry.Category;
	ObstacleActorClass = propEntry.ObstacleActorClass;
	StaticMeshAsset = propEntry.StaticMeshAsset;
	BoundsSizeMeters = propEntry.BoundsSizeMeters;
	BoundsCenterOffsetMeters = propEntry.BoundsCenterOffsetMeters;
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
	return bAppliedMesh || HasConfiguredVisualMesh();
}

void AScenarioStaticObstacle::ApplyObjectTypeActorTag()
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

void AScenarioStaticObstacle::ApplyCollisionSettings()
{
	const bool bUsePhysicalCollision = !ObstacleCollisionComponent || ObstacleCollisionComponent->bUsePhysicalCollision;
	const bool bUseSafetyQuery = !ObstacleCollisionComponent || ObstacleCollisionComponent->bUseSafetyQuery;
	const ECollisionEnabled::Type collisionEnabled = bUsePhysicalCollision
		? ECollisionEnabled::QueryAndPhysics
		: (bUseSafetyQuery ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);

	const bool bUseAuthoredBoundsCollision = HasAuthoredBoundsSize();
	const bool bUseMeshCollision = !bUseAuthoredBoundsCollision
		&& bUseMeshSimpleCollision
		&& MeshRoot
		&& StaticMeshHasSimpleCollision(MeshRoot->GetStaticMesh());
	const bool bUseBoundsCollision = bUseFallbackBoxCollision
		&& (bUseAuthoredBoundsCollision || !bUseMeshCollision);

	ApplyVisualPrimitiveCollisionSettings(bUseMeshCollision ? collisionEnabled : ECollisionEnabled::NoCollision);

	if (!CollisionBoundsComponent)
	{
		return;
	}

	if (!bUseBoundsCollision)
	{
		ConfigureObstacleCollisionPrimitive(CollisionBoundsComponent, ECollisionEnabled::NoCollision);
		return;
	}

	const FVector boxExtent = ClampCollisionBoxExtent(ResolveBoundsExtentCm());

	CollisionBoundsComponent->SetBoxExtent(boxExtent, false);
	CollisionBoundsComponent->SetRelativeLocation(ResolveBoundsCenterOffsetCm());
	ConfigureObstacleCollisionPrimitive(CollisionBoundsComponent, collisionEnabled);
}

void AScenarioStaticObstacle::ApplyVisualPrimitiveCollisionSettings(
	ECollisionEnabled::Type meshRootCollisionEnabled)
{
	TArray<UPrimitiveComponent*> primitiveComponents;
	GetComponents<UPrimitiveComponent>(primitiveComponents);
	for (UPrimitiveComponent* primitiveComponent : primitiveComponents)
	{
		if (!primitiveComponent || primitiveComponent == CollisionBoundsComponent)
		{
			continue;
		}

		const bool bAllowMeshRootCollision =
			primitiveComponent == MeshRoot && meshRootCollisionEnabled != ECollisionEnabled::NoCollision;
		ConfigureObstacleCollisionPrimitive(
			primitiveComponent,
			bAllowMeshRootCollision ? meshRootCollisionEnabled : ECollisionEnabled::NoCollision);
	}
}

bool AScenarioStaticObstacle::ApplyDefaultPropById(FName inPropId)
{
	FScenarioStaticObstaclePropEntry propEntry;
	if (!TryFindConfiguredPropEntry(inPropId, propEntry)) return false;

	return ApplyPropEntry(propEntry);
}

bool AScenarioStaticObstacle::TryFindConfiguredPropEntry(
	FName inPropId,
	FScenarioStaticObstaclePropEntry& outPropEntry) const
{
	if (inPropId.IsNone()) return false;

	const UScenarioStaticObstaclePropCatalog* propCatalog = StaticObstaclePropCatalog.LoadSynchronous();
	if (!IsValid(propCatalog))
	{
		UE_LOG(
			LogScenarioStaticObstacle,
			Warning,
			TEXT("Scenario static obstacle prop catalog is not configured or failed to load: %s"),
			*StaticObstaclePropCatalog.ToSoftObjectPath().ToString());
		return false;
	}

	return propCatalog->FindPropEntryById(inPropId, outPropEntry);
}

void AScenarioStaticObstacle::ApplyMeshGroundAlignment()
{
	if (!MeshRoot)
	{
		return;
	}

	UStaticMesh* staticMesh = MeshRoot->GetStaticMesh();
	if (!staticMesh)
	{
		MeshRoot->SetRelativeLocation(FVector::ZeroVector);
		return;
	}

	const FBox localBounds = staticMesh->GetBoundingBox();
	const double relativeScaleZ = MeshRoot->GetRelativeScale3D().Z;
	MeshRoot->SetRelativeLocation(FVector(0.0, 0.0, -localBounds.Min.Z * relativeScaleZ));
}

bool AScenarioStaticObstacle::GetPlacementBounds(
	FVector& outOrigin,
	FVector& outBoxExtent,
	FVector2D& outHalfSize2D,
	double& outRadius2D) const
{
	outOrigin = GetActorLocation();
	outBoxExtent = FVector::ZeroVector;
	outHalfSize2D = FVector2D::ZeroVector;
	outRadius2D = 0.0;

	if (HasAuthoredBoundsSize())
	{
		outBoxExtent = ResolveBoundsExtentCm();
		outOrigin = GetActorTransform().TransformPosition(ResolveBoundsCenterOffsetCm());
	}
	else if (MeshRoot && MeshRoot->GetStaticMesh())
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

double AScenarioStaticObstacle::GetPlacementRadius2D() const
{
	FVector origin = FVector::ZeroVector;
	FVector boxExtent = FVector::ZeroVector;
	FVector2D halfSize2D = FVector2D::ZeroVector;
	double radius2D = 0.0;

	GetPlacementBounds(origin, boxExtent, halfSize2D, radius2D);
	return radius2D;
}

bool AScenarioStaticObstacle::HasConfiguredVisualMesh() const
{
	TArray<UStaticMeshComponent*> staticMeshComponents;
	GetComponents<UStaticMeshComponent>(staticMeshComponents);
	for (const UStaticMeshComponent* staticMeshComponent : staticMeshComponents)
	{
		if (staticMeshComponent && staticMeshComponent->GetStaticMesh())
		{
			return true;
		}
	}

	TArray<USkeletalMeshComponent*> skeletalMeshComponents;
	GetComponents<USkeletalMeshComponent>(skeletalMeshComponents);
	for (const USkeletalMeshComponent* skeletalMeshComponent : skeletalMeshComponents)
	{
		if (skeletalMeshComponent && skeletalMeshComponent->GetSkeletalMeshAsset())
		{
			return true;
		}
	}

	return false;
}

bool AScenarioStaticObstacle::HasAuthoredBoundsSize() const
{
	return BoundsSizeMeters.X > KINDA_SMALL_NUMBER
		&& BoundsSizeMeters.Y > KINDA_SMALL_NUMBER
		&& BoundsSizeMeters.Z > KINDA_SMALL_NUMBER;
}

FVector AScenarioStaticObstacle::ResolveBoundsExtentCm() const
{
	if (HasAuthoredBoundsSize())
	{
		return FVector(
			FMath::Max(BoundsSizeMeters.X * 50.0, 0.0),
			FMath::Max(BoundsSizeMeters.Y * 50.0, 0.0),
			FMath::Max(BoundsSizeMeters.Z * 50.0, 0.0));
	}

	return FVector(
		FMath::Max(FallbackBoxExtent.X, 0.0),
		FMath::Max(FallbackBoxExtent.Y, 0.0),
		FMath::Max(FallbackBoxExtent.Z, 0.0));
}

FVector AScenarioStaticObstacle::ResolveBoundsCenterOffsetCm() const
{
	const FVector boundsExtentCm = ResolveBoundsExtentCm();
	FVector centerOffsetCm = BoundsCenterOffsetMeters * 100.0;
	if (centerOffsetCm.IsNearlyZero())
	{
		centerOffsetCm.Z = boundsExtentCm.Z;
	}
	return centerOffsetCm;
}
