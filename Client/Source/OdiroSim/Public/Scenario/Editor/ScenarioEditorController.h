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
class UScenarioEditorRouteMarkerOverlayWidget;
class UMainMenuWidget;
class UScenarioPlaceableComponent;
class UInputAction;
class UInputMappingContext;
class UMaterialInterface;
class UWidget;
struct FInputActionValue;

// Native notification used by editor UI widgets to mirror viewport selection.
DECLARE_MULTICAST_DELEGATE_OneParam(FScenarioSelectedPlaceableChanged, const FString&);

UCLASS(BlueprintType)
class ODIROSIM_API AScenarioEditorController : public APlayerController
{
	GENERATED_BODY()

public:
	AScenarioEditorController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;
	virtual void Tick(float deltaSeconds) override;
	virtual void SetupInputComponent() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Input", meta = (ClampMin = "0.001"))
	float MouseLookSensitivity = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Input")
	TSoftObjectPtr<UInputMappingContext> EditorInputMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Input")
	TSoftObjectPtr<UInputAction> EditorMoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Input")
	TSoftObjectPtr<UInputAction> EditorLookAction;

	// Mouse-button action that gates camera look or top-down drag pan input.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Input")
	TSoftObjectPtr<UInputAction> EditorLookCaptureAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Input")
	TSoftObjectPtr<UInputAction> EditorSelectionAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Input")
	TSoftObjectPtr<UInputAction> EditorDeselectionAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Input")
	TSoftObjectPtr<UInputAction> EditorTranslateAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Input")
	TSoftObjectPtr<UInputAction> EditorRotateAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Input")
	TSoftObjectPtr<UInputAction> EditorScaleAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Input")
	TSoftObjectPtr<UInputAction> EditorViewModeToggleAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Input")
	TSoftObjectPtr<UInputAction> EditorZoomAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Input")
	int32 EditorInputMappingPriority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Placement", meta = (ClampMin = "1.0"))
	float PlacementTraceDistanceCm = 100000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Placement")
	TEnumAsByte<ECollisionChannel> PlacementTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Placement")
	bool bSnapPlacementToGrid = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Placement", meta = (ClampMin = "1.0"))
	double PlacementGridSizeCm = 50.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Placement", meta = (ClampMin = "0.0"))
	double PlacementGroundSnapToleranceCm = 5.0;

	// Z height of the plane used while drawing ground regions; corner traces are projected onto it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|RegionDraw")
	double GroundRegionDrawPlaneZCm = 0.0;

	// Minimum rectangle size required before a dragged ground-region box is committed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|RegionDraw", meta = (ClampMin = "0.0"))
	double RegionDrawMinSizeCm = 10.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Classes")
	TSubclassOf<AScenarioPlacementPreviewActor> PlacementPreviewActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Classes")
	TSubclassOf<AScenarioTransformGizmoActor> TransformGizmoActorClass;

	// Main control UI shown by ScenarioEditorMap.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Classes")
	TSubclassOf<UMainMenuWidget> MainMenuWidgetClass;

	// Viewport z-order used when ScenarioEditorMap attaches WBP_MainMenu.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|UI")
	int32 MainMenuWidgetViewportZOrder = 10;

	// Viewport z-order offset that keeps route markers below WBP_MainMenu and its editor panels.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|UI")
	int32 RouteMarkerOverlayViewportZOrderOffset = -1;

	// Screen-space route marker hit size matched to the root overlay's default 78:120 aspect.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Overlay")
	FVector2D RobotRouteMarkerOverlayHitSize = FVector2D(39.0, 60.0);

	// Normalized hit anchor; the marker tip is expected at the projected world point.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Overlay")
	FVector2D RobotRouteMarkerOverlayHitAnchor = FVector2D(0.5, 1.0);

	// Extra screen-space tolerance applied around the pin-shaped route marker hit mask.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Overlay", meta = (ClampMin = "0.0"))
	double RobotRouteMarkerOverlayHitPaddingPixels = 3.0;

	// Screen-space hit box size for corridor vertex overlay grips.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Overlay")
	FVector2D CorridorVertexHandleOverlayHitSize = FVector2D(20.0, 20.0);

	// Screen-space hit box size for corridor segment center grips.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Overlay")
	FVector2D CorridorSegmentHandleGripHitSize = FVector2D(18.0, 18.0);

	// Screen-space half thickness used when hit-testing corridor segment lines.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Overlay", meta = (ClampMin = "0.0"))
	double CorridorSegmentHandleLineHitHalfThicknessPixels = 6.0;

	// Fallback save path used when the draft was not opened from a user project scenario.json.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|UI")
	FString DefaultScenarioDraftSavePath = TEXT("Saved/UserProjects/ScenarioEditor/scenario.json");

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor")
	void SetObserverMode();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Input")
	void RequestEditorWidgetInputMode(UWidget* focusWidget);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Input")
	void ReleaseEditorWidgetInputMode(UWidget* focusWidget);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Placement")
	bool BeginStaticObstaclePlacement(FName propId);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Placement")
	bool BeginPalettePlacement(EScenarioPaletteItemType itemType, FName assetId);

	// Enters drag-out drawing mode for walkable, penalty, or blocked ground regions.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|RegionDraw")
	bool BeginGroundRegionDraw(EScenarioGroundRegionType regionType);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Placement")
	void CancelPlacement();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Placement")
	bool ConfirmPlacement();

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor")
	EScenarioEditorControllerMode GetEditorMode() const { return EditorMode; }

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor")
	EScenarioEditorViewMode GetEditorViewMode() const { return EditorViewMode; }

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor")
	void SetEditorViewMode(EScenarioEditorViewMode viewMode);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor")
	void ToggleEditorViewMode();

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Placement")
	bool IsPlacementSnapToGridEnabled() const { return bSnapPlacementToGrid; }

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Placement")
	void SetPlacementSnapToGridEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Placement")
	void TogglePlacementSnapToGrid();

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Gizmo")
	EScenarioTransformGizmoMode GetTransformGizmoMode() const { return TransformGizmoMode; }

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Gizmo")
	EScenarioTransformGizmoOrientationMode GetTransformGizmoOrientationMode() const { return TransformGizmoOrientationMode; }

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Gizmo")
	EScenarioTransformGizmoOrientationMode GetEffectiveTransformGizmoOrientationMode() const;

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Gizmo")
	bool CanEditTransformGizmoOrientationForSelection() const;

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Gizmo")
	void SetTransformGizmoOrientationMode(EScenarioTransformGizmoOrientationMode orientationMode);

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Placement")
	bool IsCurrentPlacementValid() const { return bCurrentPlacementValid; }

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Placement")
	FString GetCurrentPlacementFailureReason() const { return CurrentPlacementFailureReason; }

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Placement")
	FName GetSelectedStaticObstaclePropId() const { return SelectedStaticObstaclePropId; }

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Placement")
	EScenarioPaletteItemType GetSelectedPlacementItemType() const { return SelectedPlacementItemType; }

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Placement")
	FName GetSelectedPlacementAssetId() const { return SelectedPlacementAssetId; }

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Palette")
	void GetStaticObstaclePaletteEntries(TArray<FScenarioStaticObstaclePropEntry>& outEntries) const;

	// Exports the current editor draft as a validated user project scenario JSON string.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Export")
	bool ExportAndValidateProjectScenarioJsonString(FString& outJsonString, TArray<FString>& outDiagnostics) const;

	// Loads a user project scenario.json file into the editor draft.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Import")
	bool LoadProjectScenarioJsonFile(const FString& jsonFilePath, FString& outResolvedJsonFilePath, TArray<FString>& outDiagnostics);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Authoring")
	void NewScenarioDraft();

	// 현재 Draft를 저장하고 저장 결과와 독립적으로 프로젝트 Preview 생성을 시도한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Export")
	bool SaveProjectScenarioJsonFile(const FString& jsonFilePath, FString& outResolvedJsonFilePath, TArray<FString>& outDiagnostics);

	// Saves the current editor draft to its source scenario.json or a new fallback path.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Export")
	bool SaveCurrentScenarioDraft(FString& outResolvedJsonFilePath, TArray<FString>& outDiagnostics);

	// Returns the file path that currently owns the editor draft.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Authoring")
	FString GetSourceProjectScenarioJsonPath() const;

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|UI", meta = (DeprecatedFunction, DeprecationMessage = "Toolbar UI was removed. Use ShowEditorRootWidget and the root sidebar controls."))
	UScenarioEditorToolbarWidget* ShowToolbarWidget();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|UI", meta = (DeprecatedFunction, DeprecationMessage = "Toolbar UI was removed. Use ShowEditorRootWidget and the root sidebar controls."))
	void RemoveToolbarWidget();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|UI")
	UScenarioEditorRootWidget* ShowEditorRootWidget();

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|UI")
	void RemoveEditorRootWidget();

	// Shows the main control UI that wraps project controls and scenario editor UI.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|UI")
	UMainMenuWidget* ShowMainMenuWidget();

	// Removes the main control UI from the viewport.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|UI")
	void RemoveMainMenuWidget();

	// Registers the editor root child owned by WBP_MainMenu.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|UI")
	void RegisterEditorRootWidget(UScenarioEditorRootWidget* rootWidget);

	// Clears the registered editor root child if it matches the widget being destroyed.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|UI")
	void ClearRegisteredEditorRootWidget(UScenarioEditorRootWidget* rootWidget);

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|UI")
	UScenarioEditorRootWidget* GetEditorRootWidget() const { return EditorRootWidget.Get(); }

	// Returns the main control UI currently owned by this controller.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|UI")
	UMainMenuWidget* GetMainMenuWidget() const { return MainMenuWidget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Selection")
	UScenarioPlaceableComponent* GetSelectedPlaceableComponent() const { return SelectedPlaceableComponent.Get(); }

	// Selects a spawned editor placeable by scenario instance_id.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Selection")
	bool SelectPlaceableByInstanceId(const FString& instanceId);

	// Clears viewport placeable selection and hides placeable details.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Selection")
	void ClearSelectedPlaceable();

	// Returns the native selection change delegate for root/sidebar synchronization.
	FScenarioSelectedPlaceableChanged& OnSelectedPlaceableChanged() { return SelectedPlaceableChanged; }

	// Returns whether corridor handles should currently be visible and screen-space selectable.
	bool ShouldShowCorridorHandleOverlay() const;

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Selection")
	bool TryUpdateSelectedPlaceableTransform(const FTransform& transform, FString& outFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Selection")
	bool TryRenameSelectedPlaceableInstanceId(const FString& newInstanceId, FString& outFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Selection")
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
	void HandleLookCaptureStartedInput();
	void HandleLookCaptureCompletedInput();
	void HandleViewModeToggleInput();
	void HandleEditorZoomAction(const FInputActionValue& inputActionValue);
	// Resolves fixed keyboard navigation so Q/E remain pure world-Z movement even if Axis3D mapping values are polluted.
	bool TryResolveEditorKeyboardMoveInput(float& outForwardValue, float& outRightValue, float& outUpValue) const;
	void BeginLookInputCapture();
	void EndLookInputCapture();
	void UpdateHoveredPlaceable();
	bool UpdateHoveredTransformGizmo();
	bool TraceMouseTransformGizmo(EScenarioTransformGizmoHandle& outHandle, FHitResult& outHit) const;
	// Resolves screen-space route marker hits before world traces can select geometry behind the marker.
	bool TraceMouseRobotRouteMarkerOverlay(UScenarioPlaceableComponent*& outPlaceableComponent, FHitResult& outHit) const;
	// Resolves screen-space corridor handle hits before world traces can select geometry behind the handle.
	bool TraceMouseCorridorHandleOverlay(UScenarioPlaceableComponent*& outPlaceableComponent, FHitResult& outHit) const;
	// Checks whether a viewport point is inside the route marker's approximate pin-shaped hit mask.
	bool IsScreenPointInsideRobotRouteMarkerOverlay(
		const FVector2D& screenPoint,
		const FVector2D& markerAnchorPoint) const;
	// Checks whether a viewport point is inside one corridor vertex grip hit box.
	bool IsScreenPointInsideCorridorVertexHandleOverlay(
		const FVector2D& screenPoint,
		const FVector2D& gripCenterPoint) const;
	// Checks whether a viewport point is inside one corridor segment line or center grip.
	bool IsScreenPointInsideCorridorSegmentHandleOverlay(
		const FVector2D& screenPoint,
		const FVector2D& segmentStartPoint,
		const FVector2D& segmentEndPoint,
		const FVector2D& gripCenterPoint,
		double& outDistanceSquared) const;
	bool BeginTransformGizmoDrag(EScenarioTransformGizmoHandle handle, const FHitResult& hit);
	void UpdateTransformGizmoDrag();
	void EndTransformGizmoDrag();
	void ResetTransformGizmoDrag();
	bool BuildTransformGizmoDragTransform(FTransform& outTransform) const;
	bool ApplyTransformGizmoDragTransform(const FTransform& transform);
	bool TraceMouseToPlane(const FVector& planeOrigin, const FVector& planeNormal, FVector& outPoint) const;
	bool TraceMouseSelectablePlaceable(UScenarioPlaceableComponent*& outPlaceableComponent, FHitResult& outHit) const;
	bool IsEditorSelectablePlaceable(const UScenarioPlaceableComponent* placeableComponent) const;
	UScenarioPlaceableComponent* FindSelectablePlaceableByInstanceId(const FString& instanceId) const;
	FString ResolveCurrentScenarioDraftSavePath() const;
	// 저장된 scenario.json과 현재 Editor Preview로 프로젝트 대표 PNG를 생성한다.
	void CaptureScenarioPreviewAfterSave(
		const FString& resolvedJsonFilePath,
		UScenarioAuthoringSubsystem* authoringSubsystem);
	bool IsCursorOverEditorWidgetInputModeFocus() const;
	void SetHoveredPlaceable(UScenarioPlaceableComponent* placeableComponent);
	void SetSelectedPlaceable(UScenarioPlaceableComponent* placeableComponent);
	void ApplyAuthoringOutlinePostProcessMaterial(const UScenarioPlaceableComponent* placeableComponent);
	void EnsureAuthoringOutlineCustomDepthEnabled() const;
	void AddEditorInputMappingContext();
	void BindEditorInputActions();
	// Ensures the route marker visual overlay is present below the main editor UI.
	void ShowRouteMarkerOverlayWidget(UScenarioEditorRootWidget* rootWidget);
	// Removes the route marker visual overlay owned by this controller.
	void RemoveRouteMarkerOverlayWidget();
	void UpdatePlacementPreview();
	bool ConfigurePlacementPreviewForSelectedItem(UScenarioAuthoringSubsystem* authoringSubsystem);
	bool ValidatePlacementForSelectedItem(
		const UScenarioAuthoringSubsystem* authoringSubsystem,
		FString& outFailureReason) const;
	bool HasAuthoredRobotStart(const UScenarioAuthoringSubsystem* authoringSubsystem) const;
	// Returns true when palette placement should use Corridor schema projection instead of mesh collision hits.
	bool ShouldUseSchemaPlacementPlane() const;
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
	void UpdatePlaceableDetailsForSelection();
	void HidePlaceableDetails();
	AScenarioEditorPawn* GetEditorPawn() const;
	UScenarioAuthoringSubsystem* GetAuthoringSubsystem() const;

	UPROPERTY(VisibleInstanceOnly, Category = "Scenario|Editor")
	EScenarioEditorControllerMode EditorMode = EScenarioEditorControllerMode::Observer;

	UPROPERTY(VisibleInstanceOnly, Category = "Scenario|Editor")
	EScenarioEditorViewMode EditorViewMode = EScenarioEditorViewMode::Perspective;

	UPROPERTY(VisibleInstanceOnly, Category = "Scenario|Editor|Gizmo")
	EScenarioTransformGizmoMode TransformGizmoMode = EScenarioTransformGizmoMode::Translate;

	UPROPERTY(VisibleInstanceOnly, Category = "Scenario|Editor|Gizmo")
	EScenarioTransformGizmoOrientationMode TransformGizmoOrientationMode =
		EScenarioTransformGizmoOrientationMode::Relative;

	UPROPERTY(VisibleInstanceOnly, Category = "Scenario|Editor|Placement")
	FName SelectedStaticObstaclePropId;

	UPROPERTY(VisibleInstanceOnly, Category = "Scenario|Editor|Placement")
	EScenarioPaletteItemType SelectedPlacementItemType = EScenarioPaletteItemType::StaticObstacle;

	UPROPERTY(VisibleInstanceOnly, Category = "Scenario|Editor|Placement")
	FName SelectedPlacementAssetId;

	UPROPERTY(VisibleInstanceOnly, Category = "Scenario|Editor|RegionDraw")
	EScenarioGroundRegionType PendingGroundRegionType = EScenarioGroundRegionType::Walkable;

	UPROPERTY(Transient)
	TObjectPtr<AScenarioGroundRegion> RegionDrawPreviewActor;

	UPROPERTY(Transient)
	TObjectPtr<AScenarioPlacementPreviewActor> PlacementPreviewActor;

	UPROPERTY(Transient)
	TObjectPtr<AScenarioTransformGizmoActor> TransformGizmoActor;

	UPROPERTY(Transient)
	TObjectPtr<UScenarioEditorRootWidget> EditorRootWidget;

	// Viewport-level route marker overlay rendered beneath WBP_MainMenu.
	UPROPERTY(Transient)
	TObjectPtr<UScenarioEditorRouteMarkerOverlayWidget> RouteMarkerOverlayWidget;

	// MainMenu widget created by ScenarioEditorMap controller.
	UPROPERTY(Transient)
	TObjectPtr<UMainMenuWidget> MainMenuWidget;

	UPROPERTY(VisibleInstanceOnly, Category = "Scenario|Editor|Placement")
	FTransform CurrentPlacementTransform = FTransform::Identity;

	UPROPERTY(VisibleInstanceOnly, Category = "Scenario|Editor|Placement")
	bool bCurrentPlacementValid = false;

	UPROPERTY(VisibleInstanceOnly, Category = "Scenario|Editor|Placement")
	FString CurrentPlacementFailureReason;

	bool bIsLookInputHeld = false;
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
	FScenarioSelectedPlaceableChanged SelectedPlaceableChanged;
};
