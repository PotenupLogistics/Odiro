#pragma once

#include "CoreMinimal.h"

class APostProcessVolume;
class UCameraComponent;
class UMaterialInterface;
class UObject;
class USceneCaptureComponent2D;
class UWorld;

// Shared viewport presentation helpers used by editor, simulation, and preview capture cameras.
struct ODIROSIM_API FScenarioViewportPresentation
{
	// Returns the canonical grey background post-process asset for preload requests.
	static TSoftObjectPtr<UObject> MakeGreyBackgroundPreloadAsset();

	// Loads the canonical grey background post-process material.
	static UMaterialInterface* LoadGreyBackgroundPostProcessMaterial();

	// Applies a grey background post-process material to an editor or gameplay camera component.
	static bool ApplyGreyBackgroundPostProcess(
		UCameraComponent* cameraComponent,
		UMaterialInterface* postProcessMaterial,
		float blendWeight);

	// Applies the canonical grey background post-process material to an editor or gameplay camera component.
	static bool ApplyGreyBackgroundPostProcess(UCameraComponent* cameraComponent, float blendWeight);

	// Applies the canonical grey background post-process material to a transient preview capture component.
	static bool ApplyGreyBackgroundPostProcess(USceneCaptureComponent2D* captureComponent, float blendWeight);

	// Spawns an unbound post-process volume so runtime cameras and scene captures share the same background stencil.
	static APostProcessVolume* SpawnGreyBackgroundPostProcessVolume(UWorld* world, float blendWeight);
};
