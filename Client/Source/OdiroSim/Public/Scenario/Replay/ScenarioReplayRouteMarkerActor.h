#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ScenarioReplayRouteMarkerActor.generated.h"

class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

// Replay-only actor that marks the route start and goal positions with camera-mode-specific meshes.
UCLASS()
class ODIROSIM_API AScenarioReplayRouteMarkerActor : public AActor
{
	GENERATED_BODY()

public:
	// Creates marker mesh components and loads default replay marker assets.
	AScenarioReplayRouteMarkerActor();

	// Applies route endpoint locations and hides missing endpoints.
	void ConfigureRouteMarkers(
		const FVector& StartLocationCm,
		bool bHasStartLocation,
		const FVector& GoalLocationCm,
		bool bHasGoalLocation);

	// Shows or hides all available route markers without destroying them.
	void SetRouteMarkersVisible(bool bVisible);

	// Selects the perspective pin mesh with yaw billboard rotation or the top-down target mesh.
	void SetBillboardMarkersEnabled(bool bEnabled);

	// Rotates perspective marker meshes to face the active replay camera.
	void FaceCameraLocation(const FVector& CameraLocationCm);

private:
	// Applies shared mesh, material, collision, and rendering settings to one marker component.
	void ConfigureMarkerMeshComponent(
		UStaticMeshComponent* MeshComponent,
		UMaterialInterface* MarkerMaterial,
		const FLinearColor& MarkerColor) const;

	// Applies the active camera-mode mesh and marker material to one marker component.
	void ApplyMarkerVisuals(
		UStaticMeshComponent* MeshComponent,
		UMaterialInterface* MarkerMaterial,
		const FLinearColor& MarkerColor) const;

	// Applies one alpha-1 dynamic marker material to every mesh material slot.
	void ApplyOpaqueMarkerMaterial(
		UStaticMeshComponent* MeshComponent,
		UMaterialInterface* MarkerMaterial,
		const FLinearColor& MarkerColor) const;

	// Rebuilds both marker components after the camera-mode mesh presentation changes.
	void RefreshMarkerPresentation();

	// Places one marker component at the route endpoint.
	void ApplyMarkerTransform(
		UStaticMeshComponent* MeshComponent,
		const FVector& WorldLocationCm,
		bool bEndpointAvailable) const;

	// Returns the mesh currently selected by the replay camera mode.
	UStaticMesh* GetActiveMarkerMesh() const;

	// Returns the largest target marker dimension for the active mesh mode.
	float GetActiveMarkerVisualSizeCm() const;

	// Returns the vertical placement offset for the active mesh mode.
	float GetActiveMarkerWorldZOffsetCm() const;

	// Returns a uniform scale that normalizes the authored mesh to the requested visual size.
	FVector ResolveMarkerScale() const;

	// Root component that owns the two replay marker meshes.
	UPROPERTY(VisibleAnywhere, Category = "Scenario|Replay")
	TObjectPtr<USceneComponent> SceneRoot;

	// Blue marker mesh that identifies the replay route start.
	UPROPERTY(VisibleAnywhere, Category = "Scenario|Replay")
	TObjectPtr<UStaticMeshComponent> StartMarkerComponent;

	// Red marker mesh that identifies the replay route goal.
	UPROPERTY(VisibleAnywhere, Category = "Scenario|Replay")
	TObjectPtr<UStaticMeshComponent> GoalMarkerComponent;

	// Static mesh used for top-down route endpoints.
	UPROPERTY(EditDefaultsOnly, Category = "Scenario|Replay")
	TObjectPtr<UStaticMesh> TopDownMarkerMesh;

	// Static mesh used for perspective route endpoints.
	UPROPERTY(EditDefaultsOnly, Category = "Scenario|Replay")
	TObjectPtr<UStaticMesh> BillboardMarkerMesh;

	// Material applied to the replay start marker.
	UPROPERTY(EditDefaultsOnly, Category = "Scenario|Replay")
	TObjectPtr<UMaterialInterface> StartMarkerMaterial;

	// Material applied to the replay goal marker.
	UPROPERTY(EditDefaultsOnly, Category = "Scenario|Replay")
	TObjectPtr<UMaterialInterface> GoalMarkerMaterial;

	// Desired largest top-down marker dimension in centimeters after uniform scaling.
	UPROPERTY(EditAnywhere, Category = "Scenario|Replay", meta = (ClampMin = "1.0"))
	float TopDownMarkerVisualSizeCm = 140.0f;

	// Desired largest perspective marker dimension in centimeters after uniform scaling.
	UPROPERTY(EditAnywhere, Category = "Scenario|Replay", meta = (ClampMin = "1.0"))
	float BillboardMarkerVisualSizeCm = 140.0f;

	// World-space lift applied to the top-down target mesh when it needs vertical adjustment.
	UPROPERTY(EditAnywhere, Category = "Scenario|Replay")
	float TopDownMarkerWorldZOffsetCm = 0.0f;

	// World-space lift applied to the perspective pin mesh when it needs vertical adjustment.
	UPROPERTY(EditAnywhere, Category = "Scenario|Replay")
	float BillboardMarkerWorldZOffsetCm = 0.0f;

	// Yaw correction for the authored pin mesh so its front face points at the camera.
	UPROPERTY(EditAnywhere, Category = "Scenario|Replay")
	float BillboardMarkerYawOffsetDegrees = 90.0f;

	// Whether the replay UI currently wants route markers visible.
	bool bRouteMarkersVisible = true;

	// Whether perspective camera modes should use the billboard pin mesh.
	bool bUseBillboardMarkerPresentation = false;

	// Whether the loaded replay scenario has a start endpoint.
	bool bHasStartMarker = false;

	// Whether the loaded replay scenario has a goal endpoint.
	bool bHasGoalMarker = false;

	// Cached replay start location used when marker presentation changes.
	FVector StartMarkerWorldLocationCm = FVector::ZeroVector;

	// Cached replay goal location used when marker presentation changes.
	FVector GoalMarkerWorldLocationCm = FVector::ZeroVector;
};
