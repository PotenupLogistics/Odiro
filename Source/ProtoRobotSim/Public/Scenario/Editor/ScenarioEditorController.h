#pragma once

#include "CoreMinimal.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "Scenario/Widget/ScenarioEditorRootWidget.h"
#include "GameFramework/PlayerController.h"
#include "Shared/ScenarioCoreTypes.h"
#include "Shared/ScenarioSpecTypes.h"
#include "ScenarioEditorController.generated.h"

class AScenarioEditorPawn;
class AScenarioGroundRegion;
class AScenarioPlacementPreviewActor;
class AScenarioTransformGizmoActor;
class UScenarioAuthoringSubsystem;
class UScenarioEditorRootWidget;
class UScenarioEditorToolbarWidget;
class UScenarioPlaceableComponent;
class UScenarioPlaceableContextMenuWidget;
class UInputAction;
class UInputMappingContext;
class UMaterialInterface;
class UWidget;
struct FInputActionValue;

UCLASS(BlueprintType)
class PROTOROBOTSIM_API AScenarioEditorController : public APlayerController
{
	GENERATED_BODY()

public:
	AScenarioEditorController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;
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
	TSoftObjectPtr<UInputAction> EditorViewModeToggleAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Input")
	TSoftObjectPtr<UInputAction> EditorZoomAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Input")
	int32 EditorInputMappingPriority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Input", meta = (ClampMin = "0.0"))
	double SelectionClickLookDeltaThreshold = 4.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Placement", meta = (ClampMin = "1.0"))
	float PlacementTraceDistanceCm = 100000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Placement")
	TEnumAsByte<ECollisionChannel> PlacementTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Placement")
	bool bSnapPlacementToGrid = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Placement", meta = (ClampMin = "1.0"))
	double PlacementGridSizeCm = 50.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Placement", meta = (ClampMin = "0.0"))
	double PlacementGroundSnapToleranceCm = 5.0;

	// 지면 영역을 그리는 평면의 높이(cm). 코너 trace가 이 평면에 투영됨.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|RegionDraw")
	double GroundRegionDrawPlaneZCm = 0.0;

	// 드래그한 사각형의 가로/세로가 모두 이 값 이상이어야 커밋됨(짧은 클릭은 무시).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|RegionDraw", meta = (ClampMin = "0.0"))
	double RegionDrawMinSizeCm = 10.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Classes")
	TSubclassOf<AScenarioPlacementPreviewActor> PlacementPreviewActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Classes")
	TSubclassOf<AScenarioTransformGizmoActor> TransformGizmoActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Classes")
	TSubclassOf<UScenarioEditorRootWidget> EditorRootWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|UI")
	int32 EditorRootWidgetViewportZOrder = 2;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor")
	void SetObserverMode();

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Input")
	void RequestEditorWidgetInputMode(UWidget* focusWidget);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Input")
	void ReleaseEditorWidgetInputMode(UWidget* focusWidget);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Placement")
	bool BeginStaticObstaclePlacement(FName propId);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Placement")
	bool BeginPalettePlacement(EScenarioPaletteItemType itemType, FName assetId);

	// 지면 영역(walkable/penalty/blocked)을 drag-out으로 그리는 모드로 진입함.
	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|RegionDraw")
	bool BeginGroundRegionDraw(EScenarioGroundRegionType regionType);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Placement")
	void CancelPlacement();

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Placement")
	bool ConfirmPlacement();

	UFUNCTION(BlueprintPure, Category = "Episode|Editor")
	EScenarioEditorControllerMode GetEditorMode() const { return EditorMode; }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor")
	EScenarioEditorViewMode GetEditorViewMode() const { return EditorViewMode; }

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor")
	void SetEditorViewMode(EScenarioEditorViewMode viewMode);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor")
	void ToggleEditorViewMode();

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Placement")
	bool IsPlacementSnapToGridEnabled() const { return bSnapPlacementToGrid; }

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Placement")
	void SetPlacementSnapToGridEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Placement")
	void TogglePlacementSnapToGrid();

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Gizmo")
	EScenarioTransformGizmoMode GetTransformGizmoMode() const { return TransformGizmoMode; }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Gizmo")
	EScenarioTransformGizmoOrientationMode GetTransformGizmoOrientationMode() const { return TransformGizmoOrientationMode; }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Gizmo")
	EScenarioTransformGizmoOrientationMode GetEffectiveTransformGizmoOrientationMode() const;

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Gizmo")
	bool CanEditTransformGizmoOrientationForSelection() const;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Gizmo")
	void SetTransformGizmoOrientationMode(EScenarioTransformGizmoOrientationMode orientationMode);

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Placement")
	bool IsCurrentPlacementValid() const { return bCurrentPlacementValid; }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Placement")
	FString GetCurrentPlacementFailureReason() const { return CurrentPlacementFailureReason; }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Placement")
	FName GetSelectedStaticObstaclePropId() const { return SelectedStaticObstaclePropId; }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Placement")
	EScenarioPaletteItemType GetSelectedPlacementItemType() const { return SelectedPlacementItemType; }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Placement")
	FName GetSelectedPlacementAssetId() const { return SelectedPlacementAssetId; }

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Palette")
	void GetStaticObstaclePaletteEntries(TArray<FScenarioStaticObstaclePropEntry>& outEntries) const;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Export")
	bool ExportAndValidateEpisodeSetupJsonString(FString& outJsonString, TArray<FString>& outDiagnostics) const;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Import")
	bool LoadEpisodeSetupJsonFile(const FString& jsonFilePath, FString& outResolvedJsonFilePath, TArray<FString>& outDiagnostics);

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Authoring")
	void NewEpisodeDraft();

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Export")
	bool SaveEpisodeSetupJsonFile(const FString& jsonFilePath, FString& outResolvedJsonFilePath, TArray<FString>& outDiagnostics);

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Authoring")
	FString GetSourceEpisodeSetupJsonPath() const;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|UI")
	UScenarioEditorToolbarWidget* ShowToolbarWidget();

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|UI")
	void RemoveToolbarWidget();

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|UI")
	UScenarioEditorRootWidget* ShowEditorRootWidget();

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|UI")
	void RemoveEditorRootWidget();

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|UI")
	UScenarioEditorRootWidget* GetEditorRootWidget() const { return EditorRootWidget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Selection")
	UScenarioPlaceableComponent* GetSelectedPlaceableComponent() const { return SelectedPlaceableComponent.Get(); }

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
	void HandleViewModeToggleInput();
	void HandleEditorZoomAction(const FInputActionValue& inputActionValue);
	void BeginLookInputCapture();
	void EndLookInputCapture();
	void UpdateHoveredPlaceable();
	bool UpdateHoveredTransformGizmo();
	bool TraceMouseTransformGizmo(EScenarioTransformGizmoHandle& outHandle, FHitResult& outHit) const;
	bool BeginTransformGizmoDrag(EScenarioTransformGizmoHandle handle, const FHitResult& hit);
	void UpdateTransformGizmoDrag();
	void EndTransformGizmoDrag();
	void ResetTransformGizmoDrag();
	bool BuildTransformGizmoDragTransform(FTransform& outTransform) const;
	bool ApplyTransformGizmoDragTransform(const FTransform& transform);
	bool TraceMouseToPlane(const FVector& planeOrigin, const FVector& planeNormal, FVector& outPoint) const;
	bool TraceMouseSelectablePlaceable(UScenarioPlaceableComponent*& outPlaceableComponent, FHitResult& outHit) const;
	bool IsEditorSelectablePlaceable(const UScenarioPlaceableComponent* placeableComponent) const;
	bool IsCursorOverEditorWidgetInputModeFocus() const;
	void SetHoveredPlaceable(UScenarioPlaceableComponent* placeableComponent);
	void SetSelectedPlaceable(UScenarioPlaceableComponent* placeableComponent);
	void ApplyAuthoringOutlinePostProcessMaterial(const UScenarioPlaceableComponent* placeableComponent);
	void EnsureAuthoringOutlineCustomDepthEnabled() const;
	void AddEditorInputMappingContext();
	void BindEditorInputActions();
	void UpdatePlacementPreview();
	bool ConfigurePlacementPreviewForSelectedItem(UScenarioAuthoringSubsystem* authoringSubsystem);
	bool ValidatePlacementForSelectedItem(
		const UScenarioAuthoringSubsystem* authoringSubsystem,
		FString& outFailureReason) const;
	bool HasAuthoredRobotStart(const UScenarioAuthoringSubsystem* authoringSubsystem) const;
	bool TraceMousePlacement(FHitResult& outHit) const;
	FTransform BuildPlacementTransform(const FVector& location) const;
	FVector SnapLocationIfNeeded(const FVector& location) const;
	void UpdateRegionDrawPreview();
	void BeginRegionDrag();
	void FinalizeRegionDrag();
	bool TraceMouseToGroundRegionPlane(FVector& outPoint) const;
	void ComputeRegionRectFromCorners(
		const FVector& cornerA,
		const FVector& cornerB,
		FVector& outCenter,
		FVector2D& outSize) const;
	void ConfigureRegionDrawPreview(const FVector& center, const FVector2D& size);
	AScenarioGroundRegion* EnsureRegionDrawPreviewActor();
	void DestroyRegionDrawPreview();
	void ApplyInputMode();
	void PruneEditorWidgetInputModeRequests();
	UWidget* FindEditorWidgetInputModeFocus() const;
	void DestroyPlacementPreview();
	AScenarioTransformGizmoActor* EnsureTransformGizmoActor();
	void UpdateTransformGizmoForSelection();
	void HideTransformGizmo();
	void SetTransformGizmoMode(EScenarioTransformGizmoMode mode);
	EScenarioTransformGizmoOrientationMode GetEffectiveTransformGizmoOrientationModeForPlaceable(
		const UScenarioPlaceableComponent* placeableComponent) const;
	void GetTransformGizmoBasis(
		const FTransform& transform,
		EScenarioTransformGizmoOrientationMode orientationMode,
		FVector& outXAxis,
		FVector& outYAxis,
		FVector& outZAxis) const;
	UScenarioPlaceableContextMenuWidget* EnsurePlaceableContextMenuWidget();
	void UpdatePlaceableContextMenuForSelection(bool bRepositionToMouse = true);
	void HidePlaceableContextMenu();
	AScenarioEditorPawn* GetEditorPawn() const;
	UScenarioAuthoringSubsystem* GetAuthoringSubsystem() const;

	UPROPERTY(VisibleInstanceOnly, Category = "Episode|Editor")
	EScenarioEditorControllerMode EditorMode = EScenarioEditorControllerMode::Observer;

	UPROPERTY(VisibleInstanceOnly, Category = "Episode|Editor")
	EScenarioEditorViewMode EditorViewMode = EScenarioEditorViewMode::Perspective;

	UPROPERTY(VisibleInstanceOnly, Category = "Episode|Editor|Gizmo")
	EScenarioTransformGizmoMode TransformGizmoMode = EScenarioTransformGizmoMode::Translate;

	UPROPERTY(VisibleInstanceOnly, Category = "Episode|Editor|Gizmo")
	EScenarioTransformGizmoOrientationMode TransformGizmoOrientationMode =
		EScenarioTransformGizmoOrientationMode::Relative;

	UPROPERTY(VisibleInstanceOnly, Category = "Episode|Editor|Placement")
	FName SelectedStaticObstaclePropId;

	UPROPERTY(VisibleInstanceOnly, Category = "Episode|Editor|Placement")
	EScenarioPaletteItemType SelectedPlacementItemType = EScenarioPaletteItemType::StaticObstacle;

	UPROPERTY(VisibleInstanceOnly, Category = "Episode|Editor|Placement")
	FName SelectedPlacementAssetId;

	UPROPERTY(VisibleInstanceOnly, Category = "Episode|Editor|RegionDraw")
	EScenarioGroundRegionType PendingGroundRegionType = EScenarioGroundRegionType::Walkable;

	UPROPERTY(Transient)
	TObjectPtr<AScenarioGroundRegion> RegionDrawPreviewActor;

	UPROPERTY(Transient)
	TObjectPtr<AScenarioPlacementPreviewActor> PlacementPreviewActor;

	UPROPERTY(Transient)
	TObjectPtr<AScenarioTransformGizmoActor> TransformGizmoActor;

	UPROPERTY(Transient)
	TObjectPtr<UScenarioEditorRootWidget> EditorRootWidget;

	UPROPERTY(VisibleInstanceOnly, Category = "Episode|Editor|Placement")
	FTransform CurrentPlacementTransform = FTransform::Identity;

	UPROPERTY(VisibleInstanceOnly, Category = "Episode|Editor|Placement")
	bool bCurrentPlacementValid = false;

	UPROPERTY(VisibleInstanceOnly, Category = "Episode|Editor|Placement")
	FString CurrentPlacementFailureReason;

	bool bIsLookInputHeld = false;
	double LookCaptureAccumulatedDelta = 0.0;
	bool bIsRegionDragging = false;
	FVector RegionDragStartWorld = FVector::ZeroVector;
	bool bIsTransformGizmoDragging = false;
	EScenarioTransformGizmoHandle ActiveTransformGizmoHandle = EScenarioTransformGizmoHandle::None;
	FString ActiveTransformGizmoInstanceId;
	FTransform TransformGizmoDragStartTransform = FTransform::Identity;
	FVector TransformGizmoDragStartPoint = FVector::ZeroVector;
	FVector TransformGizmoDragPlaneNormal = FVector::UpVector;
	FVector TransformGizmoDragAxis = FVector::ForwardVector;
	FVector TransformGizmoDragStartDirection = FVector::ForwardVector;
	EScenarioTransformGizmoOrientationMode ActiveTransformGizmoOrientationMode =
		EScenarioTransformGizmoOrientationMode::Relative;
	FString LastTransformGizmoDragFailureReason;
	TWeakObjectPtr<UScenarioPlaceableComponent> HoveredPlaceableComponent;
	TWeakObjectPtr<UScenarioPlaceableComponent> SelectedPlaceableComponent;
	TWeakObjectPtr<UScenarioPlaceableComponent> PressedPlaceableComponent;
	TWeakObjectPtr<UScenarioPlaceableComponent> DraggedPlaceableComponent;
	TWeakObjectPtr<UMaterialInterface> ActiveAuthoringOutlinePostProcessMaterial;
	TArray<TWeakObjectPtr<UWidget>> EditorWidgetInputModeRequesters;
};
