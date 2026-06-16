#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "ScenarioCorridorHandleActor.generated.h"

class UMaterialInterface;
class USceneComponent;
class UScenarioPlaceableComponent;
class USplineMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

// Editor-only visual handle for authoring the scenario_template corridor axis.
UCLASS()
class ODIROSIM_API AScenarioCorridorHandleActor : public AActor
{
	GENERATED_BODY()

public:
	AScenarioCorridorHandleActor();

	// Root used by the transform gizmo as the editable world transform.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Corridor")
	TObjectPtr<USceneComponent> SceneRoot;

	// Sphere mesh that provides visible feedback and selectable trace geometry for vertex handles.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Corridor")
	TObjectPtr<UStaticMeshComponent> HandleMeshComponent;

	// Spline-deformed cylinder mesh that provides visible feedback and selectable trace geometry for segment handles.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Corridor")
	TObjectPtr<USplineMeshComponent> SegmentSplineMeshComponent;

	// Placeable identity that routes selection and gizmo updates back to the authoring subsystem.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Corridor")
	TObjectPtr<UScenarioPlaceableComponent> PlaceableComponent;

	// Current handle role inside the corridor axis editor.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Scenario|Editor|Corridor")
	EScenarioCorridorHandleType HandleType = EScenarioCorridorHandleType::Vertex;

	// Vertex index for vertex handles; INDEX_NONE for non-vertex handles.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Scenario|Editor|Corridor")
	int32 VertexIndex = INDEX_NONE;

	// Polyline edge index for segment handles; INDEX_NONE for non-segment handles.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Scenario|Editor|Corridor")
	int32 SegmentIndex = INDEX_NONE;

	// Configure this actor as a draggable corridor vertex handle.
	void ConfigureVertexHandle(int32 InVertexIndex, const FString& InInstanceId, const FTransform& InTransform);

	// Configure this actor as a draggable corridor polyline segment handle.
	void ConfigureSegmentHandle(
		int32 InSegmentIndex,
		const FString& InInstanceId,
		const FTransform& InTransform,
		double InSegmentLengthCm);

private:
	// Mesh loaded from engine content for vertex handles.
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> VertexMesh;

	// Mesh loaded from engine content for segment handles.
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> SegmentMesh;

	// Material loaded from engine content for lightweight editor visualization.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> HandleMaterial;

	// Current segment handle length used when rebuilding the spline cylinder.
	double SegmentLengthCm = 0.0;

	// Applies mesh, material, and editable flags for the current handle type.
	void ApplyHandleVisual();
};
