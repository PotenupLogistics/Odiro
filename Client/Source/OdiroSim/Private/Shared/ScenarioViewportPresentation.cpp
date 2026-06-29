#include "Shared/ScenarioViewportPresentation.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const TCHAR* GreyBackgroundPostProcessMaterialPath =
		TEXT("/Game/Materials/Scenario/M_PP_ScenarioEditorGreyBackground.M_PP_ScenarioEditorGreyBackground");
	const TCHAR* EditorOutlinePostProcessMaterialPath =
		TEXT("/Game/Materials/M_EditorOutline.M_EditorOutline");

	bool IsValidGreyBackgroundRequest(const float blendWeight)
	{
		return blendWeight > 0.0f;
	}

	void AddGreyBackgroundBlendable(
		FPostProcessSettings& postProcessSettings,
		UMaterialInterface* postProcessMaterial,
		const float blendWeight)
	{
		postProcessSettings.AddBlendable(postProcessMaterial, blendWeight);
	}

	// Resolves material residency without triggering sync load after map travel.
	UMaterialInterface* ResolveLoadedMaterialFromPath(const FSoftObjectPath& materialPath)
	{
		if (!materialPath.IsValid())
		{
			return nullptr;
		}

		return Cast<UMaterialInterface>(materialPath.ResolveObject());
	}

	// Keeps legacy callers working when the preload gate was skipped or failed.
	UMaterialInterface* ResolveOrLoadMaterialFromPath(const FSoftObjectPath& materialPath)
	{
		if (UMaterialInterface* loadedMaterial = ResolveLoadedMaterialFromPath(materialPath))
		{
			return loadedMaterial;
		}

		return Cast<UMaterialInterface>(materialPath.TryLoad());
	}
}

// Defaults scenario maps to the neutral grey background unless a caller opts out before viewport setup.
bool FScenarioViewportPresentation::bUseGreyBackgroundPostProcess = false;

TSoftObjectPtr<UObject> FScenarioViewportPresentation::MakeGreyBackgroundPreloadAsset()
{
	if (!bUseGreyBackgroundPostProcess)
	{
		return TSoftObjectPtr<UObject>();
	}

	return TSoftObjectPtr<UObject>(FSoftObjectPath(GreyBackgroundPostProcessMaterialPath));
}

TArray<TSoftObjectPtr<UObject>> FScenarioViewportPresentation::MakeScenarioMapPreloadAssets()
{
	TArray<TSoftObjectPtr<UObject>> preloadAssets;
	if (TSoftObjectPtr<UObject> greyBackgroundAsset = MakeGreyBackgroundPreloadAsset();
		!greyBackgroundAsset.IsNull())
	{
		preloadAssets.Add(greyBackgroundAsset);
	}
	preloadAssets.Add(TSoftObjectPtr<UObject>(FSoftObjectPath(EditorOutlinePostProcessMaterialPath)));
	return preloadAssets;
}

UMaterialInterface* FScenarioViewportPresentation::ResolveLoadedMaterial(
	const TSoftObjectPtr<UMaterialInterface>& materialReference)
{
	return ResolveLoadedMaterialFromPath(materialReference.ToSoftObjectPath());
}

UMaterialInterface* FScenarioViewportPresentation::ResolveOrLoadMaterial(
	const TSoftObjectPtr<UMaterialInterface>& materialReference)
{
	return ResolveOrLoadMaterialFromPath(materialReference.ToSoftObjectPath());
}

UMaterialInterface* FScenarioViewportPresentation::ResolveLoadedGreyBackgroundPostProcessMaterial()
{
	if (!bUseGreyBackgroundPostProcess)
	{
		return nullptr;
	}

	return ResolveLoadedMaterialFromPath(FSoftObjectPath(GreyBackgroundPostProcessMaterialPath));
}

UMaterialInterface* FScenarioViewportPresentation::ResolveOrLoadGreyBackgroundPostProcessMaterial()
{
	if (!bUseGreyBackgroundPostProcess)
	{
		return nullptr;
	}

	return ResolveOrLoadMaterialFromPath(FSoftObjectPath(GreyBackgroundPostProcessMaterialPath));
}

bool FScenarioViewportPresentation::ApplyGreyBackgroundPostProcess(
	UCameraComponent* cameraComponent,
	UMaterialInterface* postProcessMaterial,
	const float blendWeight)
{
	if (!bUseGreyBackgroundPostProcess)
	{
		return false;
	}

	if (!cameraComponent || !postProcessMaterial || !IsValidGreyBackgroundRequest(blendWeight))
	{
		return false;
	}

	cameraComponent->PostProcessBlendWeight = 1.0f;
	AddGreyBackgroundBlendable(cameraComponent->PostProcessSettings, postProcessMaterial, blendWeight);
	return true;
}

bool FScenarioViewportPresentation::ApplyGreyBackgroundPostProcess(
	UCameraComponent* cameraComponent,
	const float blendWeight)
{
	return ApplyGreyBackgroundPostProcess(
		cameraComponent,
		ResolveOrLoadGreyBackgroundPostProcessMaterial(),
		blendWeight);
}

bool FScenarioViewportPresentation::ApplyGreyBackgroundPostProcess(
	USceneCaptureComponent2D* captureComponent,
	const float blendWeight)
{
	if (!bUseGreyBackgroundPostProcess)
	{
		return false;
	}

	UMaterialInterface* postProcessMaterial = ResolveOrLoadGreyBackgroundPostProcessMaterial();
	if (!captureComponent || !postProcessMaterial || !IsValidGreyBackgroundRequest(blendWeight))
	{
		return false;
	}

	captureComponent->PostProcessBlendWeight = 1.0f;
	AddGreyBackgroundBlendable(captureComponent->PostProcessSettings, postProcessMaterial, blendWeight);
	return true;
}

APostProcessVolume* FScenarioViewportPresentation::SpawnGreyBackgroundPostProcessVolume(
	UWorld* world,
	const float blendWeight)
{
	if (!bUseGreyBackgroundPostProcess)
	{
		return nullptr;
	}

	UMaterialInterface* postProcessMaterial = ResolveOrLoadGreyBackgroundPostProcessMaterial();
	if (!IsValid(world) || !postProcessMaterial || !IsValidGreyBackgroundRequest(blendWeight))
	{
		return nullptr;
	}

	FActorSpawnParameters spawnParameters;
	spawnParameters.ObjectFlags |= RF_Transient;
	spawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	APostProcessVolume* postProcessVolume =
		world->SpawnActor<APostProcessVolume>(
			APostProcessVolume::StaticClass(),
			FTransform::Identity,
			spawnParameters);
	if (!IsValid(postProcessVolume))
	{
		return nullptr;
	}

	postProcessVolume->bEnabled = true;
	postProcessVolume->bUnbound = true;
	postProcessVolume->BlendWeight = blendWeight;
	AddGreyBackgroundBlendable(postProcessVolume->Settings, postProcessMaterial, blendWeight);
	return postProcessVolume;
}
