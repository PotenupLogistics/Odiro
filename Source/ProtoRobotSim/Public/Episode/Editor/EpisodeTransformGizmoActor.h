#pragma once

#include "CoreMinimal.h"
#include "Episode/Editor/EpisodeEditorTypes.h"
#include "GameFramework/Actor.h"
#include "EpisodeTransformGizmoActor.generated.h"

class UMaterialInterface;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS(BlueprintType)
class PROTOROBOTSIM_API AEpisodeTransformGizmoActor : public AActor
{
	GENERATED_BODY()

public:
	AEpisodeTransformGizmoActor();

	virtual void Tick(float deltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode|Editor|Gizmo")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> TranslateXHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> TranslateYHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> TranslateZHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> TranslateXYHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> TranslateXZHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> TranslateYZHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> RotateXHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> RotateYHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> RotateZHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> ScaleXHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> ScaleYHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> ScaleZHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> ScaleXYHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> ScaleXZHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> ScaleYZHandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode|Editor|Gizmo")
	TObjectPtr<UStaticMeshComponent> ScaleUniformHandleComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Gizmo", meta = (ClampMin = "0.0"))
	double ScreenScalePerDistanceCm = 0.0012;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Gizmo", meta = (ClampMin = "0.001"))
	double MinScreenScale = 0.35;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Gizmo", meta = (ClampMin = "0.001"))
	double MaxScreenScale = 3.0;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Gizmo")
	void ShowForTarget(AActor* targetActor);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Gizmo")
	void HideGizmo();

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Gizmo")
	void RefreshFromTarget();

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Gizmo")
	void SetHoveredHandle(EEpisodeTransformGizmoHandle handle);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Gizmo")
	void SetGizmoMode(EEpisodeTransformGizmoMode mode);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Gizmo")
	void SetGizmoOrientationMode(EEpisodeTransformGizmoOrientationMode orientationMode);

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Gizmo")
	EEpisodeTransformGizmoHandle GetHoveredHandle() const { return HoveredHandle; }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Gizmo")
	EEpisodeTransformGizmoMode GetGizmoMode() const { return GizmoMode; }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Gizmo")
	EEpisodeTransformGizmoOrientationMode GetGizmoOrientationMode() const { return OrientationMode; }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Gizmo")
	bool IsHandleEnabled(EEpisodeTransformGizmoHandle handle) const;

	EEpisodeTransformGizmoHandle GetHandleForComponent(const UPrimitiveComponent* component) const;

private:
	void ConfigureHandleComponent(
		UStaticMeshComponent* component,
		EEpisodeTransformGizmoHandle handle,
		UMaterialInterface* material) const;
	void UpdateScreenScale();
	void ApplyHandleMaterials();
	void ApplyHandleMaterial(
		UStaticMeshComponent* component,
		EEpisodeTransformGizmoHandle handle,
		UMaterialInterface* defaultMaterial) const;
	bool IsHandleVisibleInMode(EEpisodeTransformGizmoHandle handle) const;
	void ApplyHandleState(UStaticMeshComponent* component, EEpisodeTransformGizmoHandle handle) const;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> TargetActor;

	UPROPERTY(Transient)
	EEpisodeTransformGizmoHandle HoveredHandle = EEpisodeTransformGizmoHandle::None;

	UPROPERTY(Transient)
	EEpisodeTransformGizmoMode GizmoMode = EEpisodeTransformGizmoMode::Translate;

	UPROPERTY(Transient)
	EEpisodeTransformGizmoOrientationMode OrientationMode = EEpisodeTransformGizmoOrientationMode::Relative;

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
