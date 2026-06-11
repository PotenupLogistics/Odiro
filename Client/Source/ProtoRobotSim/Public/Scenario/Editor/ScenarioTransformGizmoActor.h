#pragma once

#include "CoreMinimal.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "GameFramework/Actor.h"
#include "ScenarioTransformGizmoActor.generated.h"

class UMaterialInterface;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS(BlueprintType)
class PROTOROBOTSIM_API AScenarioTransformGizmoActor : public AActor
{
	GENERATED_BODY()

public:
	AScenarioTransformGizmoActor();

	virtual void Tick(float deltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Gizmo")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> TranslateXHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> TranslateYHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> TranslateZHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> TranslateXYHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> TranslateXZHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> TranslateYZHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> RotateXHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> RotateYHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> RotateZHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> ScaleXHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> ScaleYHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> ScaleZHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> ScaleXYHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> ScaleXZHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> ScaleYZHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> ScaleUniformHandleComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Gizmo", meta = (ClampMin = "0.0"))
	double ScreenScalePerDistanceCm = 0.0012;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Gizmo", meta = (ClampMin = "0.001"))
	double MinScreenScale = 0.35;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Gizmo", meta = (ClampMin = "0.001"))
	double MaxScreenScale = 3.0;

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Gizmo")
	void ShowForTarget(AActor* targetActor);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Gizmo")
	void HideGizmo();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Gizmo")
	void RefreshFromTarget();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Gizmo")
	void SetHoveredHandle(EScenarioTransformGizmoHandle handle);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Gizmo")
	void SetGizmoMode(EScenarioTransformGizmoMode mode);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Gizmo")
	void SetGizmoOrientationMode(EScenarioTransformGizmoOrientationMode orientationMode);

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Gizmo")
	EScenarioTransformGizmoHandle GetHoveredHandle() const { return HoveredHandle; }

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Gizmo")
	EScenarioTransformGizmoMode GetGizmoMode() const { return GizmoMode; }

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Gizmo")
	EScenarioTransformGizmoOrientationMode GetGizmoOrientationMode() const { return OrientationMode; }

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Gizmo")
	bool IsHandleEnabled(EScenarioTransformGizmoHandle handle) const;

	EScenarioTransformGizmoHandle GetHandleForComponent(const UPrimitiveComponent* component) const;

private:
	void ConfigureHandleComponent(
		UStaticMeshComponent* component,
		EScenarioTransformGizmoHandle handle,
		UMaterialInterface* material) const;
	void UpdateScreenScale();
	void ApplyHandleMaterials();
	void ApplyHandleMaterial(
		UStaticMeshComponent* component,
		EScenarioTransformGizmoHandle handle,
		UMaterialInterface* defaultMaterial) const;
	bool IsHandleVisibleInMode(EScenarioTransformGizmoHandle handle) const;
	void ApplyHandleState(UStaticMeshComponent* component, EScenarioTransformGizmoHandle handle) const;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> TargetActor;

	UPROPERTY(Transient)
	EScenarioTransformGizmoHandle HoveredHandle = EScenarioTransformGizmoHandle::None;

	UPROPERTY(Transient)
	EScenarioTransformGizmoMode GizmoMode = EScenarioTransformGizmoMode::Translate;

	UPROPERTY(Transient)
	EScenarioTransformGizmoOrientationMode OrientationMode = EScenarioTransformGizmoOrientationMode::Relative;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> XAxisMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> YAxisMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> XYAxisMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ZAxisMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> RotationXAxisMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> RotationYAxisMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> RotationZAxisMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> HoveredMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> GhostMaterial;
};
