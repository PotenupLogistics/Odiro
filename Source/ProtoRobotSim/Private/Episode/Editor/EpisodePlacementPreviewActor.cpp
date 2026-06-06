#include "Episode/Editor/EpisodePlacementPreviewActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Episode/Actors/EpisodeStaticObstacle.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

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
	if (!AEpisodeStaticObstacle::FindDefaultPropEntryById(propId, propEntry))
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

	PreviewMeshComponent->SetStaticMesh(loadedMesh);
	PreviewMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMeshComponent->SetGenerateOverlapEvents(false);
	SetActorHiddenInGame(false);
	SetPlacementValid(true);
	return true;
}

void AEpisodePlacementPreviewActor::SetPlacementValid(bool bCanPlace)
{
	ApplyPreviewMaterial(bCanPlace ? ValidPlacementMaterial.Get() : InvalidPlacementMaterial.Get());
	PreviewMeshComponent->SetRenderCustomDepth(!bCanPlace);
}

void AEpisodePlacementPreviewActor::ApplyPreviewMaterial(UMaterialInterface* material)
{
	if (!PreviewMeshComponent || !material) return;

	const int32 materialCount = FMath::Max(PreviewMeshComponent->GetNumMaterials(), 1);
	for (int32 index = 0; index < materialCount; ++index)
	{
		PreviewMeshComponent->SetMaterial(index, material);
	}
}
