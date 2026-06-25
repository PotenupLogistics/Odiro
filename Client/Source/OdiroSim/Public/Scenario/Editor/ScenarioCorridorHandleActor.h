#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "ScenarioCorridorHandleActor.generated.h"

class USceneComponent;
class UScenarioPlaceableComponent;
class USplineMeshComponent;
class UStaticMeshComponent;

// Editor-only invisible proxy that lets screen-space corridor handles reuse selection and gizmo flows.
UCLASS()
class ODIROSIM_API AScenarioCorridorHandleActor : public AActor
{
	GENERATED_BODY()

public:
	AScenarioCorridorHandleActor();

	// Root used by the transform gizmo as the editable world transform.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Corridor")
	TObjectPtr<USceneComponent> SceneRoot;

	// Disabled legacy mesh component kept so existing spawned handles remain valid proxy actors.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Corridor")
	TObjectPtr<UStaticMeshComponent> HandleMeshComponent;

	// Disabled legacy spline mesh component kept so existing spawned handles remain valid proxy actors.
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
	// Applies invisible proxy state and editable flags for the current handle type.
	void ApplyHandleVisual();
};
