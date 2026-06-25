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

	// Returns visual assets that should be resident before entering scenario editor or simulation maps.
	static TArray<TSoftObjectPtr<UObject>> MakeScenarioMapPreloadAssets();

	// Returns an already-loaded material without triggering synchronous asset load.
	static UMaterialInterface* ResolveLoadedMaterial(const TSoftObjectPtr<UMaterialInterface>& materialReference);

	// Returns a material from memory first, falling back to synchronous load only when preload did not run.
	static UMaterialInterface* ResolveOrLoadMaterial(const TSoftObjectPtr<UMaterialInterface>& materialReference);

	// Returns the canonical grey background material from memory without triggering synchronous asset load.
	static UMaterialInterface* ResolveLoadedGreyBackgroundPostProcessMaterial();

	// Returns the canonical grey background material, falling back to synchronous load only when preload did not run.
	static UMaterialInterface* ResolveOrLoadGreyBackgroundPostProcessMaterial();

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
