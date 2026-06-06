#include "Episode/Components/EpisodePlaceableComponent.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"

UEpisodePlaceableComponent::UEpisodePlaceableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	AuthoringHoverOutlineMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
		TEXT("/Game/Materials/M_EditorOutline.M_EditorOutline")));
}

void UEpisodePlaceableComponent::SetAuthoringHovered(bool bHovered)
{
	if (bAuthoringHovered == bHovered)
	{
		return;
	}

	bAuthoringHovered = bHovered;
	if (bAuthoringHovered)
	{
		ApplyAuthoringHoverVisual();
	}
	else
	{
		ClearAuthoringHoverVisual();
	}
}

void UEpisodePlaceableComponent::SetAuthoringSelected(bool bSelected)
{
	bAuthoringSelected = bSelected;
}

void UEpisodePlaceableComponent::ApplyAuthoringHoverVisual()
{
	TArray<UMeshComponent*> meshComponents;
	CollectOwnerMeshComponents(meshComponents);
	for (UMeshComponent* meshComponent : meshComponents)
	{
		if (!meshComponent) continue;

		const TWeakObjectPtr<UMeshComponent> meshComponentKey(meshComponent);
		if (!CachedAuthoringCustomDepthStates.Contains(meshComponentKey))
		{
			FEpisodeAuthoringMeshCustomDepthState customDepthState;
			customDepthState.bRenderCustomDepth = meshComponent->bRenderCustomDepth;
			customDepthState.CustomDepthStencilValue = meshComponent->CustomDepthStencilValue;
			CachedAuthoringCustomDepthStates.Add(meshComponentKey, customDepthState);
		}

		meshComponent->SetRenderCustomDepth(true);
		meshComponent->SetCustomDepthStencilValue(AuthoringHoverCustomDepthStencilValue);
	}
}

void UEpisodePlaceableComponent::ClearAuthoringHoverVisual()
{
	for (const TPair<TWeakObjectPtr<UMeshComponent>, FEpisodeAuthoringMeshCustomDepthState>& cachedCustomDepthState
		: CachedAuthoringCustomDepthStates)
	{
		UMeshComponent* meshComponent = cachedCustomDepthState.Key.Get();
		if (!meshComponent) continue;

		meshComponent->SetRenderCustomDepth(cachedCustomDepthState.Value.bRenderCustomDepth);
		meshComponent->SetCustomDepthStencilValue(cachedCustomDepthState.Value.CustomDepthStencilValue);
	}

	CachedAuthoringCustomDepthStates.Reset();
}

void UEpisodePlaceableComponent::CollectOwnerMeshComponents(TArray<UMeshComponent*>& outMeshComponents) const
{
	outMeshComponents.Reset();

	AActor* owner = GetOwner();
	if (!owner) return;

	owner->GetComponents(outMeshComponents);
}
