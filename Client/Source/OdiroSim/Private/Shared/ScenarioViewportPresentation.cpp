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
}

TSoftObjectPtr<UObject> FScenarioViewportPresentation::MakeGreyBackgroundPreloadAsset()
{
	return TSoftObjectPtr<UObject>(FSoftObjectPath(GreyBackgroundPostProcessMaterialPath));
}

UMaterialInterface* FScenarioViewportPresentation::LoadGreyBackgroundPostProcessMaterial()
{
	return LoadObject<UMaterialInterface>(nullptr, GreyBackgroundPostProcessMaterialPath);
}

bool FScenarioViewportPresentation::ApplyGreyBackgroundPostProcess(
	UCameraComponent* cameraComponent,
	UMaterialInterface* postProcessMaterial,
	const float blendWeight)
{
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
		LoadGreyBackgroundPostProcessMaterial(),
		blendWeight);
}

bool FScenarioViewportPresentation::ApplyGreyBackgroundPostProcess(
	USceneCaptureComponent2D* captureComponent,
	const float blendWeight)
{
	UMaterialInterface* postProcessMaterial = LoadGreyBackgroundPostProcessMaterial();
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
	UMaterialInterface* postProcessMaterial = LoadGreyBackgroundPostProcessMaterial();
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
