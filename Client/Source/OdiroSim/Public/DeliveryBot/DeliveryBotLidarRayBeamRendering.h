#pragma once

#include "CoreMinimal.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;

// Configuration shared by LiDAR beam instanced mesh components.
struct ODIROSIM_API FDeliveryBotLidarRayBeamComponentOptions
{
	// Extra tag assigned to preview/replay-only beam components for capture filtering.
	FName ComponentTag = NAME_None;

	// Initial visibility state for the configured beam component.
	bool bVisible = false;

	// Initial game-hidden state for the configured beam component.
	bool bHiddenInGame = true;

	// Whether the beam component should cast shadows.
	bool bCastShadow = false;
};

// Shared LiDAR beam rendering helpers used by robot preview and replay review layers.
class ODIROSIM_API FDeliveryBotLidarRayBeamRendering
{
public:
	// Configures an instanced mesh component as transient LiDAR beam geometry.
	static void ConfigureBeamComponent(
		UInstancedStaticMeshComponent* Component,
		USceneComponent* Parent,
		UStaticMesh* Mesh,
		const FDeliveryBotLidarRayBeamComponentOptions& Options);

	// Applies one material to every available material slot on a beam component.
	static bool ApplyBeamMaterial(
		UInstancedStaticMeshComponent* Component,
		UMaterialInterface* Material);

	// Builds the transform that stretches the beam mesh between two centimeter-space points.
	static bool BuildBeamTransform(
		const FVector& StartLocationCm,
		const FVector& EndLocationCm,
		double BeamMeshLengthCm,
		double ThicknessScale,
		FTransform& OutTransform);

	// Adds one beam instance between two centimeter-space points.
	static bool AddBeamInstance(
		UInstancedStaticMeshComponent* Component,
		const FVector& StartLocationCm,
		const FVector& EndLocationCm,
		double BeamMeshLengthCm,
		double ThicknessScale,
		bool bWorldSpace);

	// Clears all beam instances from one component.
	static void ClearBeamInstances(UInstancedStaticMeshComponent* Component);

	// Applies matching visibility and hidden-in-game flags to one component.
	static void SetBeamComponentVisible(
		UInstancedStaticMeshComponent* Component,
		bool bVisible);

	// Marks one beam component dirty after batched instance writes.
	static void MarkBeamRenderStateDirty(UInstancedStaticMeshComponent* Component);
};
