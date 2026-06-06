#pragma once

#include "CoreMinimal.h"
#include "Episode/Editor/EpisodeEditorTypes.h"
#include "GameFramework/PlayerController.h"
#include "Shared/EpisodeCoreTypes.h"
#include "EpisodeEditorController.generated.h"

class AEpisodeEditorPawn;
class AEpisodePlacementPreviewActor;
class AEpisodeTransformGizmoActor;
class UEpisodeAuthoringSubsystem;
class UEpisodePlaceableComponent;
class UEpisodePlaceableContextMenuWidget;
class UInputAction;
class UInputMappingContext;
class UMaterialInterface;
class UWidget;
struct FInputActionValue;

UCLASS(BlueprintType)
class PROTOROBOTSIM_API AEpisodeEditorController : public APlayerController
{
	GENERATED_BODY()

public:
	AEpisodeEditorController();

	virtual void BeginPlay() override;
	virtual void Tick(float deltaSeconds) override;
	virtual void SetupInputComponent() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Input", meta = (ClampMin = "0.001"))
	float MouseLookSensitivity = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Input")
	TSoftObjectPtr<UInputMappingContext> EditorInputMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Input")
	TSoftObjectPtr<UInputAction> EditorMoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Input")
	TSoftObjectPtr<UInputAction> EditorLookAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Input")
	TSoftObjectPtr<UInputAction> EditorSelectionAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Input")
	TSoftObjectPtr<UInputAction> EditorDeselectionAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Input")
	TSoftObjectPtr<UInputAction> EditorTranslateAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Input")
	TSoftObjectPtr<UInputAction> EditorRotateAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Input")
	TSoftObjectPtr<UInputAction> EditorScaleAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Input")
	int32 EditorInputMappingPriority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Input", meta = (ClampMin = "0.0"))
	double SelectionClickLookDeltaThreshold = 4.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Placement", meta = (ClampMin = "1.0"))
	float PlacementTraceDistanceCm = 100000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Placement")
	TEnumAsByte<ECollisionChannel> PlacementTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Placement")
	bool bSnapPlacementToGrid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Placement", meta = (ClampMin = "1.0"))
	double PlacementGridSizeCm = 50.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Placement", meta = (ClampMin = "0.0"))
	double PlacementGroundSnapToleranceCm = 5.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Classes")
	TSubclassOf<AEpisodePlacementPreviewActor> PlacementPreviewActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Classes")
	TSubclassOf<AEpisodeTransformGizmoActor> TransformGizmoActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Classes")
	TSubclassOf<UEpisodePlaceableContextMenuWidget> PlaceableContextMenuWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|UI")
	int32 PlaceableContextMenuViewportZOrder = 10;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor")
	void SetObserverMode();

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Input")
	void RequestEditorWidgetInputMode(UWidget* focusWidget);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Input")
	void ReleaseEditorWidgetInputMode(UWidget* focusWidget);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Placement")
	bool BeginStaticObstaclePlacement(FName propId);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Placement")
	void CancelPlacement();

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Placement")
	bool ConfirmPlacement();

	UFUNCTION(BlueprintPure, Category = "Episode|Editor")
	EEpisodeEditorControllerMode GetEditorMode() const { return EditorMode; }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Gizmo")
	EEpisodeTransformGizmoMode GetTransformGizmoMode() const { return TransformGizmoMode; }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Placement")
	bool IsCurrentPlacementValid() const { return bCurrentPlacementValid; }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Placement")
	FString GetCurrentPlacementFailureReason() const { return CurrentPlacementFailureReason; }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Placement")
	FName GetSelectedStaticObstaclePropId() const { return SelectedStaticObstaclePropId; }

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Palette")
	void GetStaticObstaclePaletteEntries(TArray<FEpisodeStaticObstaclePropEntry>& outEntries) const;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Export")
	bool ExportAndValidateEpisodeSetupJsonString(FString& outJsonString, TArray<FString>& outDiagnostics) const;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Import")
	bool LoadEpisodeSetupJsonFile(const FString& jsonFilePath, FString& outResolvedJsonFilePath, TArray<FString>& outDiagnostics);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Authoring")
	void NewEpisodeDraft();

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Export")
	bool SaveEpisodeSetupJsonFile(const FString& jsonFilePath, FString& outResolvedJsonFilePath, TArray<FString>& outDiagnostics);

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Selection")
	UEpisodePlaceableComponent* GetSelectedPlaceableComponent() const { return SelectedPlaceableComponent.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Selection")
	bool TryUpdateSelectedPlaceableTransform(const FTransform& transform, FString& outFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Selection")
	bool TryRenameSelectedPlaceableInstanceId(const FString& newInstanceId, FString& outFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Selection")
	bool DeleteSelectedPlaceable(FString& outFailureReason);

private:
	void HandleSelectionStartedInput();
	void HandleSelectionCompletedInput();
	void HandleCancelPlacementInput();
	void HandleTranslateModeInput();
	void HandleRotateModeInput();
	void HandleScaleModeInput();
	void HandleEditorMoveAction(const FInputActionValue& inputActionValue);
	void HandleEditorLookAction(const FInputActionValue& inputActionValue);
	void BeginLookInputCapture();
	void EndLookInputCapture();
	void UpdateHoveredPlaceable();
	bool UpdateHoveredTransformGizmo();
	bool TraceMouseTransformGizmo(EEpisodeTransformGizmoHandle& outHandle, FHitResult& outHit) const;
	bool BeginTransformGizmoDrag(EEpisodeTransformGizmoHandle handle, const FHitResult& hit);
	void UpdateTransformGizmoDrag();
	void EndTransformGizmoDrag();
	void ResetTransformGizmoDrag();
	bool BuildTransformGizmoDragTransform(FTransform& outTransform) const;
	bool ApplyTransformGizmoDragTransform(const FTransform& transform);
	bool TraceMouseToPlane(const FVector& planeOrigin, const FVector& planeNormal, FVector& outPoint) const;
	bool TraceMouseSelectablePlaceable(UEpisodePlaceableComponent*& outPlaceableComponent, FHitResult& outHit) const;
	bool IsEditorSelectablePlaceable(const UEpisodePlaceableComponent* placeableComponent) const;
	bool IsCursorOverEditorWidgetInputModeFocus() const;
	void SetHoveredPlaceable(UEpisodePlaceableComponent* placeableComponent);
	void SetSelectedPlaceable(UEpisodePlaceableComponent* placeableComponent);
	void ApplyAuthoringOutlinePostProcessMaterial(const UEpisodePlaceableComponent* placeableComponent);
	void EnsureAuthoringOutlineCustomDepthEnabled() const;
	void AddEditorInputMappingContext();
	void BindEditorInputActions();
	void UpdatePlacementPreview();
	bool TraceMousePlacement(FHitResult& outHit) const;
	FTransform BuildPlacementTransform(const FVector& location) const;
	FVector SnapLocationIfNeeded(const FVector& location) const;
	void ApplyInputMode();
	void PruneEditorWidgetInputModeRequests();
	UWidget* FindEditorWidgetInputModeFocus() const;
	void DestroyPlacementPreview();
	AEpisodeTransformGizmoActor* EnsureTransformGizmoActor();
	void UpdateTransformGizmoForSelection();
	void HideTransformGizmo();
	void SetTransformGizmoMode(EEpisodeTransformGizmoMode mode);
	UEpisodePlaceableContextMenuWidget* EnsurePlaceableContextMenuWidget();
	void UpdatePlaceableContextMenuForSelection(bool bRepositionToMouse = true);
	void HidePlaceableContextMenu();
	AEpisodeEditorPawn* GetEditorPawn() const;
	UEpisodeAuthoringSubsystem* GetAuthoringSubsystem() const;

	UPROPERTY(VisibleInstanceOnly, Category = "Episode|Editor")
	EEpisodeEditorControllerMode EditorMode = EEpisodeEditorControllerMode::Observer;

	UPROPERTY(VisibleInstanceOnly, Category = "Episode|Editor|Gizmo")
	EEpisodeTransformGizmoMode TransformGizmoMode = EEpisodeTransformGizmoMode::Translate;

	UPROPERTY(VisibleInstanceOnly, Category = "Episode|Editor|Placement")
	FName SelectedStaticObstaclePropId;

	UPROPERTY(Transient)
	TObjectPtr<AEpisodePlacementPreviewActor> PlacementPreviewActor;

	UPROPERTY(Transient)
	TObjectPtr<AEpisodeTransformGizmoActor> TransformGizmoActor;

	UPROPERTY(Transient)
	TObjectPtr<UEpisodePlaceableContextMenuWidget> PlaceableContextMenuWidget;

	UPROPERTY(VisibleInstanceOnly, Category = "Episode|Editor|Placement")
	FTransform CurrentPlacementTransform = FTransform::Identity;

	UPROPERTY(VisibleInstanceOnly, Category = "Episode|Editor|Placement")
	bool bCurrentPlacementValid = false;

	UPROPERTY(VisibleInstanceOnly, Category = "Episode|Editor|Placement")
	FString CurrentPlacementFailureReason;

	bool bIsLookInputHeld = false;

	double LookCaptureAccumulatedDelta = 0.0;

	bool bIsTransformGizmoDragging = false;

	EEpisodeTransformGizmoHandle ActiveTransformGizmoHandle = EEpisodeTransformGizmoHandle::None;

	FString ActiveTransformGizmoInstanceId;

	FTransform TransformGizmoDragStartTransform = FTransform::Identity;

	FVector TransformGizmoDragStartPoint = FVector::ZeroVector;

	FVector TransformGizmoDragPlaneNormal = FVector::UpVector;

	FVector TransformGizmoDragAxis = FVector::ForwardVector;

	FVector TransformGizmoDragStartDirection = FVector::ForwardVector;

	FString LastTransformGizmoDragFailureReason;

	TWeakObjectPtr<UEpisodePlaceableComponent> HoveredPlaceableComponent;

	TWeakObjectPtr<UEpisodePlaceableComponent> SelectedPlaceableComponent;

	TWeakObjectPtr<UEpisodePlaceableComponent> PressedPlaceableComponent;

	TWeakObjectPtr<UEpisodePlaceableComponent> DraggedPlaceableComponent;

	TWeakObjectPtr<UMaterialInterface> ActiveAuthoringOutlinePostProcessMaterial;

	TArray<TWeakObjectPtr<UWidget>> EditorWidgetInputModeRequesters;
};
