#include "Episode/Editor/EpisodePlacementPreviewActor.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Episode/Data/EpisodeStaticObstaclePropCatalog.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogEpisodePlacementPreview, Log, All);

AEpisodePlacementPreviewActor::AEpisodePlacementPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PreviewMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewMeshComponent"));
	PreviewMeshComponent->SetupAttachment(SceneRoot);
	PreviewMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMeshComponent->SetGenerateOverlapEvents(false);
	PreviewMeshComponent->SetCastShadow(false);
	PreviewMeshComponent->SetMobility(EComponentMobility::Movable);

	PreviewSkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PreviewSkeletalMeshComponent"));
	PreviewSkeletalMeshComponent->SetupAttachment(SceneRoot);
	PreviewSkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewSkeletalMeshComponent->SetGenerateOverlapEvents(false);
	PreviewSkeletalMeshComponent->SetCastShadow(false);
	PreviewSkeletalMeshComponent->SetMobility(EComponentMobility::Movable);

	StaticObstaclePropCatalog = UEpisodeStaticObstaclePropCatalog::MakeDefaultCatalogReference();

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> validPlacementMaterial(
		TEXT("/Game/Models/Placeable/Materials/MI_EpisodePlaceable.MI_EpisodePlaceable"));
	if (validPlacementMaterial.Succeeded())
	{
		ValidPlacementMaterial = validPlacementMaterial.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> invalidPlacementMaterial(
		TEXT("/Game/Models/Placeable/Materials/MI_EpisodeNonPlaceable.MI_EpisodeNonPlaceable"));
	if (invalidPlacementMaterial.Succeeded())
	{
		InvalidPlacementMaterial = invalidPlacementMaterial.Object;
	}

	SetActorHiddenInGame(true);
}

bool AEpisodePlacementPreviewActor::ConfigureStaticObstacleProp(FName propId)
{
	FEpisodeStaticObstaclePropEntry propEntry;
	const UEpisodeStaticObstaclePropCatalog* propCatalog = StaticObstaclePropCatalog.LoadSynchronous();
	if (!IsValid(propCatalog) || !propCatalog->FindPropEntryById(propId, propEntry))
	{
		SetActorHiddenInGame(true);
		PreviewPropId = NAME_None;
		PlacementRadius2D = 0.0;
		return false;
	}

	return ConfigureStaticObstaclePropEntry(propEntry);
}

bool AEpisodePlacementPreviewActor::ConfigureStaticObstaclePropEntry(const FEpisodeStaticObstaclePropEntry& propEntry)
{
	if (propEntry.PropId.IsNone() || !PreviewMeshComponent)
	{
		SetActorHiddenInGame(true);
		return false;
	}

	UStaticMesh* loadedMesh = propEntry.StaticMeshAsset.LoadSynchronous();
	if (!loadedMesh)
	{
		SetActorHiddenInGame(true);
		return false;
	}

	PreviewPropId = propEntry.PropId;
	PlacementRadius2D = propEntry.SafetyRadius > 0.0
		? propEntry.SafetyRadius
		: FMath::Sqrt(FMath::Square(propEntry.FallbackBoxExtent.X) + FMath::Square(propEntry.FallbackBoxExtent.Y));

	SetStaticMeshPreview(loadedMesh);
	SetActorHiddenInGame(false);
	SetPlacementValid(true);
	return true;
}

bool AEpisodePlacementPreviewActor::ConfigureActorPreviewClass(TSubclassOf<AActor> actorClass)
{
	if (!actorClass)
	{
		ClearPreviewMeshes();
		SetActorHiddenInGame(true);
		return false;
	}

	if (ConfigureActorPreviewFromActor(actorClass->GetDefaultObject<AActor>(), actorClass->GetFName()))
	{
		return true;
	}

	if (ConfigureActorPreviewFromSpawnedActor(actorClass))
	{
		return true;
	}

	UE_LOG(
		LogEpisodePlacementPreview,
		Warning,
		TEXT("Failed to configure actor preview mesh | Class: %s"),
		*actorClass->GetPathName());

	ClearPreviewMeshes();
	SetActorHiddenInGame(true);
	return false;
}

bool AEpisodePlacementPreviewActor::ConfigureActorPreviewFromActor(AActor* actor, FName previewId)
{
	if (!actor)
	{
		return false;
	}

	TArray<UStaticMeshComponent*> staticMeshComponents;
	actor->GetComponents(staticMeshComponents);
	for (const UStaticMeshComponent* staticMeshComponent : staticMeshComponents)
	{
		if (staticMeshComponent && staticMeshComponent->GetStaticMesh())
		{
			PreviewPropId = previewId;
			PlacementRadius2D = 0.0;
			SetStaticMeshPreview(staticMeshComponent->GetStaticMesh());
			SetActorHiddenInGame(false);
			SetPlacementValid(true);
			return true;
		}
	}

	TArray<USkeletalMeshComponent*> skeletalMeshComponents;
	actor->GetComponents(skeletalMeshComponents);
	for (const USkeletalMeshComponent* skeletalMeshComponent : skeletalMeshComponents)
	{
		if (skeletalMeshComponent && skeletalMeshComponent->GetSkeletalMeshAsset())
		{
			PreviewPropId = previewId;
			PlacementRadius2D = 0.0;
			SetSkeletalMeshPreview(skeletalMeshComponent->GetSkeletalMeshAsset());
			SetActorHiddenInGame(false);
			SetPlacementValid(true);
			return true;
		}
	}

	return false;
}

bool AEpisodePlacementPreviewActor::ConfigureActorPreviewFromSpawnedActor(TSubclassOf<AActor> actorClass)
{
	UWorld* world = GetWorld();
	if (!world || !actorClass)
	{
		return false;
	}

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	spawnParams.ObjectFlags |= RF_Transient;

	AActor* sourceActor = world->SpawnActor<AActor>(actorClass, FTransform::Identity, spawnParams);
	if (!sourceActor)
	{
		return false;
	}

	sourceActor->SetActorHiddenInGame(true);
	sourceActor->SetActorEnableCollision(false);

	const bool bConfigured = ConfigureActorPreviewFromActor(sourceActor, actorClass->GetFName());
	sourceActor->Destroy();
	return bConfigured;
}

void AEpisodePlacementPreviewActor::ClearPreviewMeshes()
{
	PreviewPropId = NAME_None;
	PlacementRadius2D = 0.0;
	if (PreviewMeshComponent)
	{
		PreviewMeshComponent->SetStaticMesh(nullptr);
		PreviewMeshComponent->SetVisibility(false);
	}
	if (PreviewSkeletalMeshComponent)
	{
		PreviewSkeletalMeshComponent->SetSkeletalMesh(nullptr);
		PreviewSkeletalMeshComponent->SetVisibility(false);
	}
}

void AEpisodePlacementPreviewActor::SetStaticMeshPreview(UStaticMesh* staticMesh)
{
	if (PreviewMeshComponent)
	{
		PreviewMeshComponent->SetStaticMesh(staticMesh);
		PreviewMeshComponent->SetVisibility(staticMesh != nullptr);
	}
	if (PreviewSkeletalMeshComponent)
	{
		PreviewSkeletalMeshComponent->SetSkeletalMesh(nullptr);
		PreviewSkeletalMeshComponent->SetVisibility(false);
	}
}

void AEpisodePlacementPreviewActor::SetSkeletalMeshPreview(USkeletalMesh* skeletalMesh)
{
	if (PreviewSkeletalMeshComponent)
	{
		PreviewSkeletalMeshComponent->SetSkeletalMesh(skeletalMesh);
		PreviewSkeletalMeshComponent->SetVisibility(skeletalMesh != nullptr);
	}
	if (PreviewMeshComponent)
	{
		PreviewMeshComponent->SetStaticMesh(nullptr);
		PreviewMeshComponent->SetVisibility(false);
	}
}

void AEpisodePlacementPreviewActor::SetPlacementValid(bool bCanPlace)
{
	ApplyPreviewMaterial(bCanPlace ? ValidPlacementMaterial.Get() : InvalidPlacementMaterial.Get());
	if (PreviewMeshComponent)
	{
		PreviewMeshComponent->SetRenderCustomDepth(!bCanPlace);
	}
	if (PreviewSkeletalMeshComponent)
	{
		PreviewSkeletalMeshComponent->SetRenderCustomDepth(!bCanPlace);
	}
}

void AEpisodePlacementPreviewActor::ApplyPreviewMaterial(UMaterialInterface* material)
{
	if (!material) return;

	if (PreviewMeshComponent && PreviewMeshComponent->IsVisible())
	{
		const int32 materialCount = FMath::Max(PreviewMeshComponent->GetNumMaterials(), 1);
		for (int32 index = 0; index < materialCount; ++index)
		{
			PreviewMeshComponent->SetMaterial(index, material);
		}
	}
	if (PreviewSkeletalMeshComponent && PreviewSkeletalMeshComponent->IsVisible())
	{
		const int32 materialCount = FMath::Max(PreviewSkeletalMeshComponent->GetNumMaterials(), 1);
		for (int32 index = 0; index < materialCount; ++index)
		{
			PreviewSkeletalMeshComponent->SetMaterial(index, material);
		}
	}
}
