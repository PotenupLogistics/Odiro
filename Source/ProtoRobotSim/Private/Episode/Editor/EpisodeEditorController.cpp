
#include "Episode/Editor/EpisodeEditorController.h"
#include "Episode/Actors/EpisodeStaticObstacle.h"
#include "Episode/Components/EpisodePlaceableComponent.h"
#include "Episode/Editor/EpisodeAuthoringSubsystem.h"
#include "Episode/Editor/EpisodeEditorPawn.h"
#include "Episode/Editor/EpisodePlacementPreviewActor.h"
#include "Episode/Editor/EpisodeTransformGizmoActor.h"
#include "Episode/Widget/EpisodeEditorRootWidget.h"
#include "Episode/Widget/EpisodeEditorToolbarWidget.h"
#include "Episode/Actors/EpisodePedestrian.h"
#include "Camera/CameraComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

DEFINE_LOG_CATEGORY_STATIC(LogEpisodeEditorController, Log, All);

namespace
{
	bool IsRobotRouteMarkerPlaceable(const UEpisodePlaceableComponent* placeableComponent)
	{
		if (!placeableComponent) return false;

		return placeableComponent->AuthoringRole == EEpisodePlaceableAuthoringRole::RobotStartMarker
			|| placeableComponent->AuthoringRole == EEpisodePlaceableAuthoringRole::RobotGoalMarker;
	}

	bool IsTransformModeAllowedForPlaceable(
		const UEpisodePlaceableComponent* placeableComponent,
		EEpisodeTransformGizmoMode mode)
	{
		if (!placeableComponent) return false;

		switch (mode)
		{
		case EEpisodeTransformGizmoMode::Translate:
			return placeableComponent->bAuthoringAllowLocationEdit;
		case EEpisodeTransformGizmoMode::Rotate:
			return placeableComponent->bAuthoringAllowRotationEdit;
		case EEpisodeTransformGizmoMode::Scale:
			return placeableComponent->bAuthoringAllowScaleEdit;
		default:
			return false;
		}
	}

	bool IsTransformHandleAllowedForPlaceable(
		const UEpisodePlaceableComponent* placeableComponent,
		EEpisodeTransformGizmoHandle handle)
	{
		if (!placeableComponent) return false;

		switch (handle)
		{
		case EEpisodeTransformGizmoHandle::TranslateX:
		case EEpisodeTransformGizmoHandle::TranslateY:
		case EEpisodeTransformGizmoHandle::TranslateZ:
		case EEpisodeTransformGizmoHandle::TranslateXY:
		case EEpisodeTransformGizmoHandle::TranslateXZ:
		case EEpisodeTransformGizmoHandle::TranslateYZ:
			return placeableComponent->bAuthoringAllowLocationEdit;
		case EEpisodeTransformGizmoHandle::RotateX:
		case EEpisodeTransformGizmoHandle::RotateY:
		case EEpisodeTransformGizmoHandle::RotateZ:
			return placeableComponent->bAuthoringAllowRotationEdit;
		case EEpisodeTransformGizmoHandle::ScaleX:
		case EEpisodeTransformGizmoHandle::ScaleY:
		case EEpisodeTransformGizmoHandle::ScaleZ:
		case EEpisodeTransformGizmoHandle::ScaleXY:
		case EEpisodeTransformGizmoHandle::ScaleXZ:
		case EEpisodeTransformGizmoHandle::ScaleYZ:
		case EEpisodeTransformGizmoHandle::ScaleUniform:
			return placeableComponent->bAuthoringAllowScaleEdit;
		case EEpisodeTransformGizmoHandle::None:
		default:
			return false;
		}
	}

	bool IsTransformEditAllowedForPlaceable(
		const UEpisodePlaceableComponent* placeableComponent,
		const FTransform& currentTransform,
		const FTransform& requestedTransform,
		FString& outFailureReason)
	{
		if (!placeableComponent)
		{
			outFailureReason = TEXT("No selected placeable is editable.");
			return false;
		}

		if (!placeableComponent->bAuthoringAllowLocationEdit
			&& !currentTransform.GetLocation().Equals(requestedTransform.GetLocation(), KINDA_SMALL_NUMBER))
		{
			outFailureReason = TEXT("Selected placeable location cannot be edited.");
			return false;
		}
		if (!placeableComponent->bAuthoringAllowRotationEdit
			&& !currentTransform.GetRotation().Equals(requestedTransform.GetRotation(), KINDA_SMALL_NUMBER))
		{
			outFailureReason = TEXT("Selected placeable rotation cannot be edited.");
			return false;
		}
		if (!placeableComponent->bAuthoringAllowScaleEdit
			&& !currentTransform.GetScale3D().Equals(requestedTransform.GetScale3D(), KINDA_SMALL_NUMBER))
		{
			outFailureReason = TEXT("Selected placeable scale cannot be edited.");
			return false;
		}

		return true;
	}
}

AEpisodeEditorController::AEpisodeEditorController()
{
	PrimaryActorTick.bCanEverTick = true;
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	PlacementPreviewActorClass = AEpisodePlacementPreviewActor::StaticClass();
	TransformGizmoActorClass = AEpisodeTransformGizmoActor::StaticClass();
	EditorRootWidgetClass = UEpisodeEditorRootWidget::StaticClass();
	static ConstructorHelpers::FClassFinder<UEpisodeEditorRootWidget> editorRootWidgetFinder(
		TEXT("/Game/Widgets/WBP_EpisodeEditorRootWidget"));
	if (editorRootWidgetFinder.Succeeded())
	{
		EditorRootWidgetClass = editorRootWidgetFinder.Class;
	}
	EditorInputMappingContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/Input/IMC_Editor.IMC_Editor")));
	EditorMoveAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_EditorMove.IA_EditorMove")));
	EditorLookAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_EditorLook.IA_EditorLook")));
	EditorSelectionAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_Selection.IA_Selection")));
	EditorDeselectionAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_Deselection.IA_Deselection")));
	EditorTranslateAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_EditorTranslate.IA_EditorTranslate")));
	EditorRotateAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_EditorRotate.IA_EditorRotate")));
	EditorScaleAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_EditorScale.IA_EditorScale")));
}

void AEpisodeEditorController::BeginPlay()
{
	Super::BeginPlay();
	AddEditorInputMappingContext();
	EnsureAuthoringOutlineCustomDepthEnabled();
	SetObserverMode();
	ShowEditorRootWidget();
}

void AEpisodeEditorController::EndPlay(const EEndPlayReason::Type endPlayReason)
{
	RemoveEditorRootWidget();
	Super::EndPlay(endPlayReason);
}

void AEpisodeEditorController::Tick(float deltaSeconds)
{
	Super::Tick(deltaSeconds);

	switch (EditorMode)
	{
	case EEpisodeEditorControllerMode::Observer:
		if (bIsTransformGizmoDragging)
		{
			UpdateTransformGizmoDrag();
			SetHoveredPlaceable(nullptr);
		}
		else if (UpdateHoveredTransformGizmo())
		{
			SetHoveredPlaceable(nullptr);
		}
		else
		{
			UpdateHoveredPlaceable();
		}
		break;
	case EEpisodeEditorControllerMode::EditPlacement:
		UpdatePlacementPreview();
		break;
	default:
		break;
	}
}

void AEpisodeEditorController::SetupInputComponent()
{
	Super::SetupInputComponent();
	BindEditorInputActions();
}

void AEpisodeEditorController::SetObserverMode()
{
	EditorMode = EEpisodeEditorControllerMode::Observer;
	SelectedStaticObstaclePropId = NAME_None;
	SelectedPlacementItemType = EEpisodePaletteItemType::StaticObstacle;
	SelectedPlacementAssetId = NAME_None;
	bCurrentPlacementValid = false;
	CurrentPlacementFailureReason.Reset();
	bIsLookInputHeld = false;
	LookCaptureAccumulatedDelta = 0.0;
	PressedPlaceableComponent.Reset();
	ResetTransformGizmoDrag();
	SetHoveredPlaceable(nullptr);
	SetSelectedPlaceable(nullptr);
	DestroyPlacementPreview();
	ApplyInputMode();
}

void AEpisodeEditorController::RequestEditorWidgetInputMode(UWidget* focusWidget)
{
	if (!focusWidget) return;

	PruneEditorWidgetInputModeRequests();
	EditorWidgetInputModeRequesters.AddUnique(TWeakObjectPtr(focusWidget));
	ApplyInputMode();
}

void AEpisodeEditorController::ReleaseEditorWidgetInputMode(UWidget* focusWidget)
{
	if (!focusWidget) return;

	EditorWidgetInputModeRequesters.RemoveAll(
		[focusWidget](const TWeakObjectPtr<UWidget>& requester)
		{
			return !requester.IsValid() || requester.Get() == focusWidget;
		});
	ApplyInputMode();
}

bool AEpisodeEditorController::BeginStaticObstaclePlacement(FName propId)
{
	return BeginPalettePlacement(EEpisodePaletteItemType::StaticObstacle, propId);
}

bool AEpisodeEditorController::BeginPalettePlacement(EEpisodePaletteItemType itemType, FName assetId)
{
	bIsLookInputHeld = false;
	LookCaptureAccumulatedDelta = 0.0;
	PressedPlaceableComponent.Reset();
	ResetTransformGizmoDrag();
	SetHoveredPlaceable(nullptr);
	SetSelectedPlaceable(nullptr);

	UEpisodeAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem) return false;

	UWorld* world = GetWorld();
	if (!world || !PlacementPreviewActorClass) return false;

	if (!PlacementPreviewActor)
	{
		FActorSpawnParameters spawnParams;
		spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		PlacementPreviewActor = world->SpawnActor<AEpisodePlacementPreviewActor>(
			PlacementPreviewActorClass,
			FTransform::Identity,
			spawnParams);
	}

	SelectedPlacementItemType = itemType;
	SelectedPlacementAssetId = assetId;
	SelectedStaticObstaclePropId =
		itemType == EEpisodePaletteItemType::StaticObstacle ? assetId : NAME_None;

	if (!PlacementPreviewActor || !ConfigurePlacementPreviewForSelectedItem(authoringSubsystem))
	{
		UE_LOG(
			LogEpisodeEditorController,
			Warning,
			TEXT("Failed to begin placement preview | Type: %d | AssetId: %s | Reason: %s"),
			static_cast<int32>(SelectedPlacementItemType),
			*SelectedPlacementAssetId.ToString(),
			CurrentPlacementFailureReason.IsEmpty() ? TEXT("<none>") : *CurrentPlacementFailureReason);

		DestroyPlacementPreview();
		SelectedPlacementItemType = EEpisodePaletteItemType::StaticObstacle;
		SelectedPlacementAssetId = NAME_None;
		SelectedStaticObstaclePropId = NAME_None;
		return false;
	}

	EditorMode = EEpisodeEditorControllerMode::EditPlacement;
	ApplyInputMode();
	UpdatePlacementPreview();
	return true;
}

void AEpisodeEditorController::CancelPlacement()
{
	if (EditorMode == EEpisodeEditorControllerMode::EditPlacement)
	{
		SetObserverMode();
	}
}

bool AEpisodeEditorController::ConfirmPlacement()
{
	if (EditorMode != EEpisodeEditorControllerMode::EditPlacement || !bCurrentPlacementValid)
	{
		return false;
	}

	UEpisodeAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem) return false;

	bool bPlaced = false;
	FString failureReason;
	switch (SelectedPlacementItemType)
	{
	case EEpisodePaletteItemType::StaticObstacle:
	{
		FEpisodePlaceableInstanceSpec placedSpec;
		AEpisodeStaticObstacle* placedActor = nullptr;
		bPlaced = authoringSubsystem->AddStaticObstacleInternal(
			SelectedStaticObstaclePropId,
			CurrentPlacementTransform,
			placedSpec,
			placedActor);
		if (!bPlaced)
		{
			failureReason = TEXT("Static obstacle placement failed.");
		}
		break;
	}
	case EEpisodePaletteItemType::Pedestrian:
	{
		FEpisodeDynamicActorSpec placedSpec;
		AActor* placedActor = nullptr;
		bPlaced = authoringSubsystem->AddPedestrian(
			SelectedPlacementAssetId,
			CurrentPlacementTransform,
			placedSpec,
			placedActor,
			failureReason);
		break;
	}
	case EEpisodePaletteItemType::RobotStart:
	{
		FEpisodePlaceableInstanceSpec placedSpec;
		AActor* placedMarker = nullptr;
		bPlaced = authoringSubsystem->SetRobotStartLocation(
			SelectedPlacementAssetId,
			CurrentPlacementTransform,
			placedSpec,
			placedMarker,
			failureReason);
		break;
	}
	case EEpisodePaletteItemType::RobotGoal:
	{
		FEpisodePlaceableInstanceSpec placedSpec;
		AActor* placedMarker = nullptr;
		bPlaced = authoringSubsystem->SetRobotGoalLocation(
			CurrentPlacementTransform,
			placedSpec,
			placedMarker,
			failureReason);
		break;
	}
	default:
		failureReason = TEXT("Unknown palette placement item type.");
		break;
	}

	if (bPlaced)
	{
		SetObserverMode();
		return true;
	}

	CurrentPlacementFailureReason = failureReason;
	if (PlacementPreviewActor)
	{
		PlacementPreviewActor->SetPlacementValid(false);
	}

	UE_LOG(
		LogEpisodeEditorController,
		Warning,
		TEXT("Placement failed | Type: %d | AssetId: %s | Reason: %s"),
		static_cast<int32>(SelectedPlacementItemType),
		*SelectedPlacementAssetId.ToString(),
		*CurrentPlacementFailureReason);
	return false;
}

void AEpisodeEditorController::GetStaticObstaclePaletteEntries(TArray<FEpisodeStaticObstaclePropEntry>& outEntries) const
{
	outEntries.Reset();
	if (const UEpisodeAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem())
	{
		authoringSubsystem->GetStaticObstaclePaletteEntries(outEntries);
	}
}

bool AEpisodeEditorController::ExportAndValidateEpisodeSetupJsonString(
	FString& outJsonString,
	TArray<FString>& outDiagnostics) const
{
	outJsonString.Reset();
	outDiagnostics.Reset();

	const UEpisodeAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		outDiagnostics.Add(TEXT("Episode authoring subsystem is unavailable."));
		return false;
	}

	return authoringSubsystem->ExportAndValidateEpisodeSetupJsonString(outJsonString, outDiagnostics);
}

bool AEpisodeEditorController::LoadEpisodeSetupJsonFile(
	const FString& jsonFilePath,
	FString& outResolvedJsonFilePath,
	TArray<FString>& outDiagnostics)
{
	outResolvedJsonFilePath.Reset();
	outDiagnostics.Reset();

	UEpisodeAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		outDiagnostics.Add(TEXT("Episode authoring subsystem is unavailable."));
		return false;
	}

	CancelPlacement();
	return authoringSubsystem->LoadEpisodeSetupJsonFile(jsonFilePath, outResolvedJsonFilePath, outDiagnostics);
}

void AEpisodeEditorController::NewEpisodeDraft()
{
	CancelPlacement();
	if (UEpisodeAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem())
	{
		authoringSubsystem->NewDraft();
	}
}

bool AEpisodeEditorController::SaveEpisodeSetupJsonFile(
	const FString& jsonFilePath,
	FString& outResolvedJsonFilePath,
	TArray<FString>& outDiagnostics)
{
	outResolvedJsonFilePath.Reset();
	outDiagnostics.Reset();

	UEpisodeAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		outDiagnostics.Add(TEXT("Episode authoring subsystem is unavailable."));
		return false;
	}

	return authoringSubsystem->SaveEpisodeSetupJsonFile(jsonFilePath, outResolvedJsonFilePath, outDiagnostics);
}

FString AEpisodeEditorController::GetSourceEpisodeSetupJsonPath() const
{
	const UEpisodeAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	return authoringSubsystem ? authoringSubsystem->GetSourceEpisodeSetupJsonPath() : FString();
}

UEpisodeEditorToolbarWidget* AEpisodeEditorController::ShowToolbarWidget()
{
	UEpisodeEditorRootWidget* rootWidget = ShowEditorRootWidget();
	return rootWidget ? rootWidget->GetToolbarWidget() : nullptr;
}

void AEpisodeEditorController::RemoveToolbarWidget()
{
	if (UEpisodeEditorRootWidget* rootWidget = GetEditorRootWidget())
	{
		if (UEpisodeEditorToolbarWidget* toolbarWidget = rootWidget->GetToolbarWidget())
		{
			toolbarWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

UEpisodeEditorRootWidget* AEpisodeEditorController::ShowEditorRootWidget()
{
	if (IsValid(EditorRootWidget))
	{
		if (!EditorRootWidget->IsInViewport())
		{
			EditorRootWidget->AddToViewport(EditorRootWidgetViewportZOrder);
		}
		return EditorRootWidget.Get();
	}

	if (!ensureMsgf(
			EditorRootWidgetClass,
			TEXT("EpisodeEditorController requires EditorRootWidgetClass to point to WBP_EditorRootWidget.")))
	{
		UE_LOG(LogEpisodeEditorController, Error, TEXT("EditorRootWidgetClass is not set."));
		return nullptr;
	}

	EditorRootWidget = CreateWidget<UEpisodeEditorRootWidget>(this, EditorRootWidgetClass);
	if (!EditorRootWidget)
	{
		UE_LOG(LogEpisodeEditorController, Error, TEXT("Failed to create editor root widget."));
		return nullptr;
	}

	EditorRootWidget->AddToViewport(EditorRootWidgetViewportZOrder);
	return EditorRootWidget.Get();
}

void AEpisodeEditorController::RemoveEditorRootWidget()
{
	if (IsValid(EditorRootWidget))
	{
		EditorRootWidget->RemoveFromParent();
	}

	EditorRootWidget = nullptr;
}

EEpisodeTransformGizmoOrientationMode AEpisodeEditorController::GetEffectiveTransformGizmoOrientationMode() const
{
	return GetEffectiveTransformGizmoOrientationModeForPlaceable(SelectedPlaceableComponent.Get());
}

bool AEpisodeEditorController::CanEditTransformGizmoOrientationForSelection() const
{
	const UEpisodePlaceableComponent* selectedPlaceable = SelectedPlaceableComponent.Get();
	return IsEditorSelectablePlaceable(selectedPlaceable)
		&& !IsRobotRouteMarkerPlaceable(selectedPlaceable);
}

void AEpisodeEditorController::SetTransformGizmoOrientationMode(
	EEpisodeTransformGizmoOrientationMode orientationMode)
{
	if (TransformGizmoOrientationMode == orientationMode)
	{
		return;
	}

	if (bIsTransformGizmoDragging)
	{
		EndTransformGizmoDrag();
	}

	TransformGizmoOrientationMode = orientationMode;
	UpdateTransformGizmoForSelection();
	UpdatePlaceableContextMenuForSelection(false);
}

bool AEpisodeEditorController::TryUpdateSelectedPlaceableTransform(
	const FTransform& transform,
	FString& outFailureReason)
{
	outFailureReason.Reset();

	UEpisodePlaceableComponent* selectedPlaceable = SelectedPlaceableComponent.Get();
	if (!IsEditorSelectablePlaceable(selectedPlaceable) || selectedPlaceable->InstanceId.IsEmpty())
	{
		outFailureReason = TEXT("No selected placeable is editable.");
		return false;
	}

	UEpisodeAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		outFailureReason = TEXT("Episode authoring subsystem is unavailable.");
		return false;
	}

	AActor* selectedActor = selectedPlaceable->GetOwner();
	if (!IsValid(selectedActor))
	{
		outFailureReason = TEXT("Selected placeable actor is unavailable.");
		return false;
	}
	const FTransform currentTransform = selectedActor->GetActorTransform();
	
	FTransform requestedTransform = transform;
	if (!selectedPlaceable->bAuthoringAllowRotationEdit)
	{
		requestedTransform.SetRotation(currentTransform.GetRotation());
	}
	if (!selectedPlaceable->bAuthoringAllowScaleEdit)
	{
		requestedTransform.SetScale3D(currentTransform.GetScale3D());
	}

	if (!IsTransformEditAllowedForPlaceable(selectedPlaceable, currentTransform, requestedTransform, outFailureReason))
	{
		return false;
	}

	bool bUpdated = false;
	if (selectedPlaceable->Category == EEpisodeActorCategory::StaticObstacle)
	{
		bUpdated = authoringSubsystem->UpdateStaticObstacleTransform(
			selectedPlaceable->InstanceId,
			requestedTransform,
			outFailureReason);
	}
	else if (selectedPlaceable->AuthoringRole == EEpisodePlaceableAuthoringRole::RobotStartMarker)
	{
		bUpdated = authoringSubsystem->UpdateRobotStartPointTransform(requestedTransform, outFailureReason);
	}
	else if (selectedPlaceable->AuthoringRole == EEpisodePlaceableAuthoringRole::RobotGoalMarker)
	{
		bUpdated = authoringSubsystem->UpdateRobotGoalPointTransform(requestedTransform, outFailureReason);
	}
	else
	{
		outFailureReason = TEXT("Selected placeable transform editing is not supported.");
	}

	if (!bUpdated)
	{
		return false;
	}

	UpdateTransformGizmoForSelection();
	UpdatePlaceableContextMenuForSelection(false);
	return true;
}

bool AEpisodeEditorController::TryRenameSelectedPlaceableInstanceId(
	const FString& newInstanceId,
	FString& outFailureReason)
{
	outFailureReason.Reset();

	UEpisodePlaceableComponent* selectedPlaceable = SelectedPlaceableComponent.Get();
	if (!IsEditorSelectablePlaceable(selectedPlaceable) || selectedPlaceable->InstanceId.IsEmpty())
	{
		outFailureReason = TEXT("No selected placeable is editable.");
		return false;
	}
	if (!selectedPlaceable->bAuthoringRenamable)
	{
		outFailureReason = TEXT("Selected placeable cannot be renamed.");
		return false;
	}

	UEpisodeAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		outFailureReason = TEXT("Episode authoring subsystem is unavailable.");
		return false;
	}

	const FString oldInstanceId = selectedPlaceable->InstanceId;
	if (!authoringSubsystem->RenameStaticObstacleInstanceId(oldInstanceId, newInstanceId, outFailureReason))
	{
		return false;
	}

	if (ActiveTransformGizmoInstanceId == oldInstanceId)
	{
		ActiveTransformGizmoInstanceId = selectedPlaceable->InstanceId;
	}

	UpdateTransformGizmoForSelection();
	UpdatePlaceableContextMenuForSelection(false);
	return true;
}

bool AEpisodeEditorController::DeleteSelectedPlaceable(FString& outFailureReason)
{
	outFailureReason.Reset();

	UEpisodePlaceableComponent* selectedPlaceable = SelectedPlaceableComponent.Get();
	if (!IsEditorSelectablePlaceable(selectedPlaceable) || selectedPlaceable->InstanceId.IsEmpty())
	{
		outFailureReason = TEXT("No selected placeable is editable.");
		return false;
	}
	if (!selectedPlaceable->bAuthoringDeletable)
	{
		outFailureReason = TEXT("Selected placeable cannot be deleted.");
		return false;
	}

	UEpisodeAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		outFailureReason = TEXT("Episode authoring subsystem is unavailable.");
		return false;
	}

	const FString instanceId = selectedPlaceable->InstanceId;
	selectedPlaceable->SetAuthoringSelected(false);
	if (!authoringSubsystem->RemoveStaticObstacle(instanceId, outFailureReason))
	{
		selectedPlaceable->SetAuthoringSelected(true);
		return false;
	}

	if (HoveredPlaceableComponent.Get() == selectedPlaceable)
	{
		HoveredPlaceableComponent.Reset();
	}
	if (PressedPlaceableComponent.Get() == selectedPlaceable)
	{
		PressedPlaceableComponent.Reset();
	}
	if (DraggedPlaceableComponent.Get() == selectedPlaceable)
	{
		ResetTransformGizmoDrag();
	}

	SelectedPlaceableComponent.Reset();
	HideTransformGizmo();
	HidePlaceableContextMenu();
	ApplyInputMode();
	return true;
}

void AEpisodeEditorController::HandleSelectionStartedInput()
{
	if (EditorMode == EEpisodeEditorControllerMode::EditPlacement)
	{
		ConfirmPlacement();
		return;
	}

	if (EditorMode == EEpisodeEditorControllerMode::Observer)
	{
		if (IsCursorOverEditorWidgetInputModeFocus())
		{
			return;
		}

		EEpisodeTransformGizmoHandle gizmoHandle = EEpisodeTransformGizmoHandle::None;
		FHitResult gizmoHit;
		if (TraceMouseTransformGizmo(gizmoHandle, gizmoHit)
			&& gizmoHandle != EEpisodeTransformGizmoHandle::None)
		{
			BeginTransformGizmoDrag(gizmoHandle, gizmoHit);
			return;
		}

		PressedPlaceableComponent = HoveredPlaceableComponent;
		BeginLookInputCapture();
	}
}

void AEpisodeEditorController::HandleSelectionCompletedInput()
{
	if (bIsTransformGizmoDragging)
	{
		EndTransformGizmoDrag();
		return;
	}

	const bool bShouldSelectPressedPlaceable =
		EditorMode == EEpisodeEditorControllerMode::Observer
		&& bIsLookInputHeld
		&& LookCaptureAccumulatedDelta <= SelectionClickLookDeltaThreshold;
	UEpisodePlaceableComponent* placeableToSelect = PressedPlaceableComponent.Get();

	EndLookInputCapture();
	PressedPlaceableComponent.Reset();

	if (bShouldSelectPressedPlaceable)
	{
		SetSelectedPlaceable(placeableToSelect);
	}
}

void AEpisodeEditorController::HandleCancelPlacementInput()
{
	if (EditorMode == EEpisodeEditorControllerMode::Observer)
	{
		if (bIsTransformGizmoDragging)
		{
			EndTransformGizmoDrag();
			return;
		}

		SetSelectedPlaceable(nullptr);
		return;
	}

	CancelPlacement();
}

void AEpisodeEditorController::HandleTranslateModeInput()
{
	SetTransformGizmoMode(EEpisodeTransformGizmoMode::Translate);
}

void AEpisodeEditorController::HandleRotateModeInput()
{
	SetTransformGizmoMode(EEpisodeTransformGizmoMode::Rotate);
}

void AEpisodeEditorController::HandleScaleModeInput()
{
	SetTransformGizmoMode(EEpisodeTransformGizmoMode::Scale);
}

void AEpisodeEditorController::HandleEditorMoveAction(const FInputActionValue& inputActionValue)
{
	if (EditorMode != EEpisodeEditorControllerMode::Observer)
	{
		return;
	}
	if (bIsTransformGizmoDragging)
	{
		return;
	}

	AEpisodeEditorPawn* editorPawn = GetEditorPawn();
	if (!editorPawn) return;

	float forwardValue = 0.0f;
	float rightValue = 0.0f;
	float upValue = 0.0f;

	switch (inputActionValue.GetValueType())
	{
	case EInputActionValueType::Axis3D:
	{
		const FVector moveValue = inputActionValue.Get<FVector>();
		forwardValue = moveValue.X;
		rightValue = moveValue.Y;
		upValue = moveValue.Z;
		break;
	}
	case EInputActionValueType::Axis2D:
	{
		const FVector2D moveValue = inputActionValue.Get<FVector2D>();
		forwardValue = moveValue.X;
		rightValue = -moveValue.Y;
		break;
	}
	case EInputActionValueType::Axis1D:
		forwardValue = inputActionValue.Get<float>();
		break;
	case EInputActionValueType::Boolean:
		forwardValue = inputActionValue.Get<bool>() ? 1.0f : 0.0f;
		break;
	default:
		break;
	}

	editorPawn->ApplyMoveInput(forwardValue, rightValue, upValue);
}

void AEpisodeEditorController::HandleEditorLookAction(const FInputActionValue& inputActionValue)
{
	if (EditorMode != EEpisodeEditorControllerMode::Observer || !bIsLookInputHeld)
	{
		return;
	}

	AEpisodeEditorPawn* editorPawn = GetEditorPawn();
	if (!editorPawn) return;

	float yawValue = 0.0f;
	float pitchValue = 0.0f;

	switch (inputActionValue.GetValueType())
	{
	case EInputActionValueType::Axis2D:
	{
		const FVector2D lookValue = inputActionValue.Get<FVector2D>();
		yawValue = lookValue.X;
		pitchValue = lookValue.Y;
		break;
	}
	case EInputActionValueType::Axis1D:
		yawValue = inputActionValue.Get<float>();
		break;
	case EInputActionValueType::Boolean:
		yawValue = inputActionValue.Get<bool>() ? 1.0f : 0.0f;
		break;
	default:
		break;
	}

	LookCaptureAccumulatedDelta += FVector2D(yawValue, pitchValue).Size();
	editorPawn->ApplyLookInput(
		yawValue * MouseLookSensitivity,
		pitchValue * MouseLookSensitivity);
}

void AEpisodeEditorController::BeginLookInputCapture()
{
	if (bIsLookInputHeld)
	{
		return;
	}

	bIsLookInputHeld = true;
	LookCaptureAccumulatedDelta = 0.0;
	ApplyInputMode();
}

void AEpisodeEditorController::EndLookInputCapture()
{
	if (!bIsLookInputHeld)
	{
		return;
	}

	bIsLookInputHeld = false;
	ApplyInputMode();
}

void AEpisodeEditorController::UpdateHoveredPlaceable()
{
	if (bIsLookInputHeld || IsCursorOverEditorWidgetInputModeFocus())
	{
		SetHoveredPlaceable(nullptr);
		return;
	}

	UEpisodePlaceableComponent* hitPlaceableComponent = nullptr;
	FHitResult hit;
	if (TraceMouseSelectablePlaceable(hitPlaceableComponent, hit))
	{
		SetHoveredPlaceable(hitPlaceableComponent);
	}
	else
	{
		SetHoveredPlaceable(nullptr);
	}
}

bool AEpisodeEditorController::UpdateHoveredTransformGizmo()
{
	if (!IsValid(TransformGizmoActor))
	{
		return false;
	}

	if (bIsTransformGizmoDragging || bIsLookInputHeld || IsCursorOverEditorWidgetInputModeFocus())
	{
		TransformGizmoActor->SetHoveredHandle(EEpisodeTransformGizmoHandle::None);
		return false;
	}

	EEpisodeTransformGizmoHandle hoveredHandle = EEpisodeTransformGizmoHandle::None;
	FHitResult hit;
	const bool bHitGizmo = TraceMouseTransformGizmo(hoveredHandle, hit);
	TransformGizmoActor->SetHoveredHandle(hoveredHandle);
	return bHitGizmo && hoveredHandle != EEpisodeTransformGizmoHandle::None;
}

bool AEpisodeEditorController::TraceMouseTransformGizmo(
	EEpisodeTransformGizmoHandle& outHandle,
	FHitResult& outHit) const
{
	outHandle = EEpisodeTransformGizmoHandle::None;

	if (!IsValid(TransformGizmoActor) || TransformGizmoActor->IsHidden())
	{
		return false;
	}

	FVector worldOrigin = FVector::ZeroVector;
	FVector worldDirection = FVector::ForwardVector;
	if (!DeprojectMousePositionToWorld(worldOrigin, worldDirection)) return false;

	const FVector traceEnd = worldOrigin + worldDirection.GetSafeNormal() * PlacementTraceDistanceCm;
	
	FCollisionQueryParams queryParams(SCENE_QUERY_STAT(EpisodeEditorTransformGizmoTrace), true);
	queryParams.bReturnPhysicalMaterial = false;

	FHitResult hit;
	if (!TransformGizmoActor->ActorLineTraceSingle(
		hit,
		worldOrigin,
		traceEnd,
		PlacementTraceChannel,
		queryParams))
	{
		return false;
	}

	const EEpisodeTransformGizmoHandle handle =
		TransformGizmoActor->GetHandleForComponent(hit.GetComponent());
	if (handle == EEpisodeTransformGizmoHandle::None
		|| !TransformGizmoActor->IsHandleEnabled(handle))
	{
		return false;
	}

	outHit = hit;
	outHandle = handle;
	return true;
}

bool AEpisodeEditorController::BeginTransformGizmoDrag(
	EEpisodeTransformGizmoHandle handle,
	const FHitResult& hit)
{
	if (handle == EEpisodeTransformGizmoHandle::None)
	{
		return false;
	}
	if (!IsValid(TransformGizmoActor) || !TransformGizmoActor->IsHandleEnabled(handle))
	{
		return false;
	}

	UEpisodePlaceableComponent* selectedPlaceable = SelectedPlaceableComponent.Get();
	if (!IsEditorSelectablePlaceable(selectedPlaceable) || selectedPlaceable->InstanceId.IsEmpty())
	{
		return false;
	}

	AActor* selectedActor = selectedPlaceable->GetOwner();
	if (!selectedActor)
	{
		return false;
	}
	if (!IsTransformHandleAllowedForPlaceable(selectedPlaceable, handle))
	{
		return false;
	}

	bIsLookInputHeld = false;
	LookCaptureAccumulatedDelta = 0.0;
	PressedPlaceableComponent.Reset();
	SetHoveredPlaceable(nullptr);

	DraggedPlaceableComponent = selectedPlaceable;
	ActiveTransformGizmoHandle = handle;
	ActiveTransformGizmoInstanceId = selectedPlaceable->InstanceId;
	TransformGizmoDragStartTransform = selectedActor->GetActorTransform();
	ActiveTransformGizmoOrientationMode = GetEffectiveTransformGizmoOrientationModeForPlaceable(selectedPlaceable);
	LastTransformGizmoDragFailureReason.Reset();

	const FVector startLocation = TransformGizmoDragStartTransform.GetLocation();
	FVector startXAxis = FVector::ForwardVector;
	FVector startYAxis = FVector::RightVector;
	FVector startZAxis = FVector::UpVector;
	GetTransformGizmoBasis(
		TransformGizmoDragStartTransform,
		ActiveTransformGizmoOrientationMode,
		startXAxis,
		startYAxis,
		startZAxis);

	auto buildCameraFacingAxisPlaneNormal = [this](const FVector& axis)
	{
		FVector worldOrigin = FVector::ZeroVector;
		FVector worldDirection = FVector::ForwardVector;
		if (!DeprojectMousePositionToWorld(worldOrigin, worldDirection))
		{
			worldDirection = FVector::ForwardVector;
		}

		const FVector safeAxis = axis.GetSafeNormal();
		FVector planeNormal = FVector::CrossProduct(safeAxis, FVector::CrossProduct(worldDirection.GetSafeNormal(), safeAxis));
		if (!planeNormal.Normalize())
		{
			planeNormal = FVector::CrossProduct(safeAxis, FVector::UpVector);
			if (!planeNormal.Normalize())
			{
				planeNormal = FVector::CrossProduct(safeAxis, FVector::RightVector);
				planeNormal.Normalize();
			}
		}
		return planeNormal;
	};

	TransformGizmoDragAxis = FVector::ForwardVector;
	TransformGizmoDragPlaneNormal = FVector::UpVector;
	if (handle == EEpisodeTransformGizmoHandle::TranslateX || handle == EEpisodeTransformGizmoHandle::ScaleX)
	{
		TransformGizmoDragAxis = startXAxis;
	}
	else if (handle == EEpisodeTransformGizmoHandle::TranslateY || handle == EEpisodeTransformGizmoHandle::ScaleY)
	{
		TransformGizmoDragAxis = startYAxis;
	}
	else if (handle == EEpisodeTransformGizmoHandle::TranslateZ || handle == EEpisodeTransformGizmoHandle::ScaleZ)
	{
		TransformGizmoDragAxis = startZAxis;
		TransformGizmoDragPlaneNormal = buildCameraFacingAxisPlaneNormal(TransformGizmoDragAxis);
	}
	else if (handle == EEpisodeTransformGizmoHandle::TranslateXY || handle == EEpisodeTransformGizmoHandle::ScaleXY)
	{
		TransformGizmoDragPlaneNormal = startZAxis;
	}
	else if (handle == EEpisodeTransformGizmoHandle::TranslateXZ || handle == EEpisodeTransformGizmoHandle::ScaleXZ)
	{
		TransformGizmoDragPlaneNormal = startYAxis;
	}
	else if (handle == EEpisodeTransformGizmoHandle::TranslateYZ || handle == EEpisodeTransformGizmoHandle::ScaleYZ)
	{
		TransformGizmoDragPlaneNormal = startXAxis;
	}
	else if (handle == EEpisodeTransformGizmoHandle::RotateX)
	{
		TransformGizmoDragAxis = startXAxis;
		TransformGizmoDragPlaneNormal = startXAxis;
	}
	else if (handle == EEpisodeTransformGizmoHandle::RotateY)
	{
		TransformGizmoDragAxis = startYAxis;
		TransformGizmoDragPlaneNormal = startYAxis;
	}
	else if (handle == EEpisodeTransformGizmoHandle::RotateZ)
	{
		TransformGizmoDragAxis = startZAxis;
		TransformGizmoDragPlaneNormal = startZAxis;
	}

	if ((handle == EEpisodeTransformGizmoHandle::TranslateX || handle == EEpisodeTransformGizmoHandle::TranslateY)
		&& !TransformGizmoDragAxis.IsNearlyZero())
	{
		TransformGizmoDragAxis.Z = 0.0;
		if (!TransformGizmoDragAxis.Normalize())
		{
			TransformGizmoDragAxis = handle == EEpisodeTransformGizmoHandle::TranslateX
				? FVector::ForwardVector
				: FVector::RightVector;
		}
	}

	if (!TraceMouseToPlane(startLocation, TransformGizmoDragPlaneNormal, TransformGizmoDragStartPoint))
	{
		TransformGizmoDragStartPoint = hit.ImpactPoint;
		const double planeDistance = FVector::DotProduct(
			TransformGizmoDragStartPoint - startLocation,
			TransformGizmoDragPlaneNormal.GetSafeNormal());
		TransformGizmoDragStartPoint -= TransformGizmoDragPlaneNormal.GetSafeNormal() * planeDistance;
	}

	if (handle == EEpisodeTransformGizmoHandle::RotateX
		|| handle == EEpisodeTransformGizmoHandle::RotateY
		|| handle == EEpisodeTransformGizmoHandle::RotateZ)
	{
		TransformGizmoDragStartDirection = FVector::VectorPlaneProject(
			TransformGizmoDragStartPoint - startLocation,
			TransformGizmoDragAxis);
		if (!TransformGizmoDragStartDirection.Normalize())
		{
			TransformGizmoDragStartDirection = FVector::VectorPlaneProject(startXAxis, TransformGizmoDragAxis);
			if (!TransformGizmoDragStartDirection.Normalize())
			{
				TransformGizmoDragStartDirection = FVector::VectorPlaneProject(startYAxis, TransformGizmoDragAxis);
				TransformGizmoDragStartDirection.Normalize();
			}
		}
	}

	bIsTransformGizmoDragging = true;
	if (IsValid(TransformGizmoActor))
	{
		TransformGizmoActor->SetHoveredHandle(handle);
	}
	ApplyInputMode();
	return true;
}

void AEpisodeEditorController::UpdateTransformGizmoDrag()
{
	FTransform dragTransform;
	if (BuildTransformGizmoDragTransform(dragTransform))
	{
		ApplyTransformGizmoDragTransform(dragTransform);
	}

	if (IsValid(TransformGizmoActor))
	{
		TransformGizmoActor->SetHoveredHandle(ActiveTransformGizmoHandle);
		TransformGizmoActor->RefreshFromTarget();
	}
}

void AEpisodeEditorController::EndTransformGizmoDrag()
{
	if (!bIsTransformGizmoDragging)
	{
		return;
	}

	ResetTransformGizmoDrag();
	UpdateTransformGizmoForSelection();
	ApplyInputMode();
}

void AEpisodeEditorController::ResetTransformGizmoDrag()
{
	bIsTransformGizmoDragging = false;
	ActiveTransformGizmoHandle = EEpisodeTransformGizmoHandle::None;
	ActiveTransformGizmoInstanceId.Reset();
	TransformGizmoDragStartTransform = FTransform::Identity;
	TransformGizmoDragStartPoint = FVector::ZeroVector;
	TransformGizmoDragPlaneNormal = FVector::UpVector;
	TransformGizmoDragAxis = FVector::ForwardVector;
	TransformGizmoDragStartDirection = FVector::ForwardVector;
	ActiveTransformGizmoOrientationMode = EEpisodeTransformGizmoOrientationMode::Relative;
	LastTransformGizmoDragFailureReason.Reset();
	DraggedPlaceableComponent.Reset();

	if (IsValid(TransformGizmoActor))
	{
		TransformGizmoActor->SetHoveredHandle(EEpisodeTransformGizmoHandle::None);
	}
}

bool AEpisodeEditorController::BuildTransformGizmoDragTransform(FTransform& outTransform) const
{
	if (!bIsTransformGizmoDragging || ActiveTransformGizmoHandle == EEpisodeTransformGizmoHandle::None)
	{
		return false;
	}
	if (!IsEditorSelectablePlaceable(DraggedPlaceableComponent.Get()))
	{
		return false;
	}
	if (!IsTransformHandleAllowedForPlaceable(DraggedPlaceableComponent.Get(), ActiveTransformGizmoHandle))
	{
		return false;
	}

	const FVector startLocation = TransformGizmoDragStartTransform.GetLocation();
	FVector currentPoint = FVector::ZeroVector;
	if (!TraceMouseToPlane(startLocation, TransformGizmoDragPlaneNormal, currentPoint))
	{
		return false;
	}

	outTransform = TransformGizmoDragStartTransform;
	const FVector rawDelta = currentPoint - TransformGizmoDragStartPoint;
	const FVector planeDelta = FVector::VectorPlaneProject(rawDelta, TransformGizmoDragPlaneNormal);
	FVector startXAxis = FVector::ForwardVector;
	FVector startYAxis = FVector::RightVector;
	FVector startZAxis = FVector::UpVector;
	GetTransformGizmoBasis(
		TransformGizmoDragStartTransform,
		ActiveTransformGizmoOrientationMode,
		startXAxis,
		startYAxis,
		startZAxis);
	auto makeScaleFactor = [](double dragDistanceCm)
	{
		return FMath::Max(0.05, 1.0 + dragDistanceCm / 100.0);
	};

	switch (ActiveTransformGizmoHandle)
	{
	case EEpisodeTransformGizmoHandle::TranslateX:
	case EEpisodeTransformGizmoHandle::TranslateY:
	case EEpisodeTransformGizmoHandle::TranslateZ:
	{
		const double axisDistance = FVector::DotProduct(rawDelta, TransformGizmoDragAxis);
		outTransform.SetLocation(startLocation + TransformGizmoDragAxis * axisDistance);
		return true;
	}
	case EEpisodeTransformGizmoHandle::TranslateXY:
	case EEpisodeTransformGizmoHandle::TranslateXZ:
	case EEpisodeTransformGizmoHandle::TranslateYZ:
	{
		outTransform.SetLocation(startLocation + planeDelta);
		return true;
	}
	case EEpisodeTransformGizmoHandle::RotateX:
	case EEpisodeTransformGizmoHandle::RotateY:
	case EEpisodeTransformGizmoHandle::RotateZ:
	{
		FVector currentDirection = FVector::VectorPlaneProject(currentPoint - startLocation, TransformGizmoDragAxis);
		if (!currentDirection.Normalize())
		{
			return false;
		}

		const double deltaAngleRadians = FMath::Atan2(
			FVector::DotProduct(FVector::CrossProduct(TransformGizmoDragStartDirection, currentDirection), TransformGizmoDragAxis),
			FVector::DotProduct(TransformGizmoDragStartDirection, currentDirection));
		const FQuat deltaRotation(TransformGizmoDragAxis, deltaAngleRadians);
		outTransform.SetRotation(deltaRotation * TransformGizmoDragStartTransform.GetRotation());
		return true;
	}
	case EEpisodeTransformGizmoHandle::ScaleX:
	case EEpisodeTransformGizmoHandle::ScaleY:
	case EEpisodeTransformGizmoHandle::ScaleZ:
	{
		FVector scale = TransformGizmoDragStartTransform.GetScale3D();
		const double scaleFactor = makeScaleFactor(FVector::DotProduct(rawDelta, TransformGizmoDragAxis));
		if (ActiveTransformGizmoHandle == EEpisodeTransformGizmoHandle::ScaleX)
		{
			scale.X *= scaleFactor;
		}
		else if (ActiveTransformGizmoHandle == EEpisodeTransformGizmoHandle::ScaleY)
		{
			scale.Y *= scaleFactor;
		}
		else
		{
			scale.Z *= scaleFactor;
		}
		outTransform.SetScale3D(scale);
		return true;
	}
	case EEpisodeTransformGizmoHandle::ScaleXY:
	case EEpisodeTransformGizmoHandle::ScaleXZ:
	case EEpisodeTransformGizmoHandle::ScaleYZ:
	{
		FVector scale = TransformGizmoDragStartTransform.GetScale3D();
		if (ActiveTransformGizmoHandle == EEpisodeTransformGizmoHandle::ScaleXY)
		{
			scale.X *= makeScaleFactor(FVector::DotProduct(planeDelta, startXAxis));
			scale.Y *= makeScaleFactor(FVector::DotProduct(planeDelta, startYAxis));
		}
		else if (ActiveTransformGizmoHandle == EEpisodeTransformGizmoHandle::ScaleXZ)
		{
			scale.X *= makeScaleFactor(FVector::DotProduct(planeDelta, startXAxis));
			scale.Z *= makeScaleFactor(FVector::DotProduct(planeDelta, startZAxis));
		}
		else
		{
			scale.Y *= makeScaleFactor(FVector::DotProduct(planeDelta, startYAxis));
			scale.Z *= makeScaleFactor(FVector::DotProduct(planeDelta, startZAxis));
		}
		outTransform.SetScale3D(scale);
		return true;
	}
	case EEpisodeTransformGizmoHandle::ScaleUniform:
	{
		const FVector uniformDragAxis = (startXAxis + startYAxis + startZAxis).GetSafeNormal();
		const double scaleFactor = makeScaleFactor(FVector::DotProduct(rawDelta, uniformDragAxis));
		outTransform.SetScale3D(TransformGizmoDragStartTransform.GetScale3D() * scaleFactor);
		return true;
	}
	default:
		return false;
	}
}

bool AEpisodeEditorController::ApplyTransformGizmoDragTransform(const FTransform& transform)
{
	if (ActiveTransformGizmoInstanceId.IsEmpty())
	{
		return false;
	}
	UEpisodePlaceableComponent* draggedPlaceable = DraggedPlaceableComponent.Get();
	if (!IsEditorSelectablePlaceable(draggedPlaceable))
	{
		return false;
	}

	UEpisodeAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		return false;
	}

	FString failureReason;
	AActor* draggedActor = draggedPlaceable->GetOwner();
	if (!IsValid(draggedActor))
	{
		return false;
	}
	const FTransform currentTransform = draggedActor->GetActorTransform();

	FTransform requestedTransform = transform;
	if (!draggedPlaceable->bAuthoringAllowRotationEdit)
	{
		requestedTransform.SetRotation(currentTransform.GetRotation());
	}
	if (!draggedPlaceable->bAuthoringAllowScaleEdit)
	{
		requestedTransform.SetScale3D(currentTransform.GetScale3D());
	}

	bool bUpdated = false;
	if (!IsTransformEditAllowedForPlaceable(draggedPlaceable, currentTransform, requestedTransform, failureReason))
	{
		bUpdated = false;
	}
	else if (draggedPlaceable->Category == EEpisodeActorCategory::StaticObstacle)
	{
		bUpdated = authoringSubsystem->UpdateStaticObstacleTransform(
			ActiveTransformGizmoInstanceId,
			requestedTransform,
			failureReason);
	}
	else if (draggedPlaceable->AuthoringRole == EEpisodePlaceableAuthoringRole::RobotStartMarker)
	{
		bUpdated = authoringSubsystem->UpdateRobotStartPointTransform(requestedTransform, failureReason);
	}
	else if (draggedPlaceable->AuthoringRole == EEpisodePlaceableAuthoringRole::RobotGoalMarker)
	{
		bUpdated = authoringSubsystem->UpdateRobotGoalPointTransform(requestedTransform, failureReason);
	}
	else
	{
		failureReason = TEXT("Selected placeable transform editing is not supported.");
	}

	if (bUpdated)
	{
		LastTransformGizmoDragFailureReason.Reset();
		UpdatePlaceableContextMenuForSelection(false);
		return true;
	}

	if (LastTransformGizmoDragFailureReason != failureReason)
	{
		LastTransformGizmoDragFailureReason = failureReason;
		UE_LOG(
			LogEpisodeEditorController,
			Verbose,
			TEXT("Transform gizmo drag rejected | InstanceId: %s, Reason: %s"),
			*ActiveTransformGizmoInstanceId,
			*failureReason);
	}

	return false;
}

bool AEpisodeEditorController::TraceMouseToPlane(
	const FVector& planeOrigin,
	const FVector& planeNormal,
	FVector& outPoint) const
{
	const FVector safePlaneNormal = planeNormal.GetSafeNormal();
	if (safePlaneNormal.IsNearlyZero())
	{
		return false;
	}

	FVector worldOrigin = FVector::ZeroVector;
	FVector worldDirection = FVector::ForwardVector;
	if (!DeprojectMousePositionToWorld(worldOrigin, worldDirection))
	{
		return false;
	}

	const FVector safeWorldDirection = worldDirection.GetSafeNormal();
	const double denominator = FVector::DotProduct(safeWorldDirection, safePlaneNormal);
	if (FMath::IsNearlyZero(denominator))
	{
		return false;
	}

	const double distance = FVector::DotProduct(planeOrigin - worldOrigin, safePlaneNormal) / denominator;
	if (distance < 0.0 || distance > PlacementTraceDistanceCm)
	{
		return false;
	}

	outPoint = worldOrigin + safeWorldDirection * distance;
	return true;
}

bool AEpisodeEditorController::TraceMouseSelectablePlaceable(
	UEpisodePlaceableComponent*& outPlaceableComponent,
	FHitResult& outHit) const
{
	outPlaceableComponent = nullptr;

	FVector worldOrigin = FVector::ZeroVector;
	FVector worldDirection = FVector::ForwardVector;
	if (!DeprojectMousePositionToWorld(worldOrigin, worldDirection)) return false;

	UWorld* world = GetWorld();
	if (!world) return false;

	FCollisionQueryParams queryParams(SCENE_QUERY_STAT(EpisodeEditorSelectableTrace), true);
	queryParams.bReturnPhysicalMaterial = false;
	if (PlacementPreviewActor)
	{
		queryParams.AddIgnoredActor(PlacementPreviewActor);
	}
	if (const APawn* pawn = GetPawn())
	{
		queryParams.AddIgnoredActor(pawn);
	}

	const FVector traceEnd = worldOrigin + worldDirection.GetSafeNormal() * PlacementTraceDistanceCm;
	if (!world->LineTraceSingleByChannel(
		outHit,
		worldOrigin,
		traceEnd,
		PlacementTraceChannel,
		queryParams))
	{
		return false;
	}

	AActor* hitActor = outHit.GetActor();
	if (!hitActor)
	{
		return false;
	}

	UEpisodePlaceableComponent* placeableComponent = hitActor->FindComponentByClass<UEpisodePlaceableComponent>();
	if (!IsEditorSelectablePlaceable(placeableComponent))
	{
		return false;
	}

	outPlaceableComponent = placeableComponent;
	return true;
}

bool AEpisodeEditorController::IsEditorSelectablePlaceable(const UEpisodePlaceableComponent* placeableComponent) const
{
	return placeableComponent
		&& placeableComponent->bAuthoringSelectable
		&& (placeableComponent->Category == EEpisodeActorCategory::StaticObstacle
			|| IsRobotRouteMarkerPlaceable(placeableComponent));
}

bool AEpisodeEditorController::IsCursorOverEditorWidgetInputModeFocus() const
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

	const FVector2D cursorScreenPosition = FSlateApplication::Get().GetCursorPos();
	for (int32 i = EditorWidgetInputModeRequesters.Num() - 1; i >= 0; --i)
	{
		UWidget* requester = EditorWidgetInputModeRequesters[i].Get();
		if (requester && requester->IsVisible() && requester->GetCachedGeometry().IsUnderLocation(cursorScreenPosition))
		{
			return true;
		}
	}

	return false;
}

void AEpisodeEditorController::SetHoveredPlaceable(UEpisodePlaceableComponent* placeableComponent)
{
	if (HoveredPlaceableComponent.Get() == placeableComponent)
	{
		return;
	}

	if (UEpisodePlaceableComponent* previousHoveredPlaceable = HoveredPlaceableComponent.Get())
	{
		previousHoveredPlaceable->SetAuthoringHovered(false);
	}

	HoveredPlaceableComponent = placeableComponent;
	if (placeableComponent)
	{
		ApplyAuthoringOutlinePostProcessMaterial(placeableComponent);
		placeableComponent->SetAuthoringHovered(true);
	}
}

void AEpisodeEditorController::SetSelectedPlaceable(UEpisodePlaceableComponent* placeableComponent)
{
	if (SelectedPlaceableComponent.Get() == placeableComponent)
	{
		UpdateTransformGizmoForSelection();
		UpdatePlaceableContextMenuForSelection();
		return;
	}

	if (UEpisodePlaceableComponent* previousSelectedPlaceable = SelectedPlaceableComponent.Get())
	{
		previousSelectedPlaceable->SetAuthoringSelected(false);
	}

	SelectedPlaceableComponent = placeableComponent;
	if (placeableComponent)
	{
		placeableComponent->SetAuthoringSelected(true);
	}

	UpdateTransformGizmoForSelection();
	UpdatePlaceableContextMenuForSelection();
}

void AEpisodeEditorController::ApplyAuthoringOutlinePostProcessMaterial(
	const UEpisodePlaceableComponent* placeableComponent)
{
	if (!placeableComponent)
	{
		return;
	}

	UMaterialInterface* outlinePostProcessMaterial =
		placeableComponent->AuthoringHoverOutlineMaterial.LoadSynchronous();
	if (!outlinePostProcessMaterial || ActiveAuthoringOutlinePostProcessMaterial.Get() == outlinePostProcessMaterial)
	{
		return;
	}

	AEpisodeEditorPawn* editorPawn = GetEditorPawn();
	if (!editorPawn || !editorPawn->CameraComponent)
	{
		return;
	}

	editorPawn->CameraComponent->PostProcessBlendWeight = 1.0f;
	editorPawn->CameraComponent->PostProcessSettings.AddBlendable(outlinePostProcessMaterial, 1.0f);
	ActiveAuthoringOutlinePostProcessMaterial = outlinePostProcessMaterial;
}

void AEpisodeEditorController::EnsureAuthoringOutlineCustomDepthEnabled() const
{
	IConsoleVariable* customDepthCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.CustomDepth"));
	if (customDepthCvar && customDepthCvar->GetInt() < 3)
	{
		customDepthCvar->Set(3, ECVF_SetByCode);
	}
}

void AEpisodeEditorController::AddEditorInputMappingContext()
{
	ULocalPlayer* localPlayer = GetLocalPlayer();
	if (!localPlayer) return;

	UEnhancedInputLocalPlayerSubsystem* enhancedInputSubsystem =
		localPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!enhancedInputSubsystem) return;

	UInputMappingContext* mappingContext = EditorInputMappingContext.LoadSynchronous();
	if (!mappingContext) return;

	enhancedInputSubsystem->AddMappingContext(mappingContext, EditorInputMappingPriority);
}

void AEpisodeEditorController::BindEditorInputActions()
{
	UEnhancedInputComponent* enhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!enhancedInputComponent) return;

	if (UInputAction* moveAction = EditorMoveAction.LoadSynchronous())
	{
		enhancedInputComponent->BindAction(
			moveAction,
			ETriggerEvent::Triggered,
			this,
			&AEpisodeEditorController::HandleEditorMoveAction);
	}

	if (UInputAction* lookAction = EditorLookAction.LoadSynchronous())
	{
		enhancedInputComponent->BindAction(
			lookAction,
			ETriggerEvent::Triggered,
			this,
			&AEpisodeEditorController::HandleEditorLookAction);
	}

	if (UInputAction* selectionAction = EditorSelectionAction.LoadSynchronous())
	{
		enhancedInputComponent->BindAction(
			selectionAction,
			ETriggerEvent::Started,
			this,
			&AEpisodeEditorController::HandleSelectionStartedInput);

		enhancedInputComponent->BindAction(
			selectionAction,
			ETriggerEvent::Completed,
			this,
			&AEpisodeEditorController::HandleSelectionCompletedInput);

		enhancedInputComponent->BindAction(
			selectionAction,
			ETriggerEvent::Canceled,
			this,
			&AEpisodeEditorController::HandleSelectionCompletedInput);
	}

	if (UInputAction* deselectionAction = EditorDeselectionAction.LoadSynchronous())
	{
		enhancedInputComponent->BindAction(
			deselectionAction,
			ETriggerEvent::Started,
			this,
			&AEpisodeEditorController::HandleCancelPlacementInput);
	}

	if (UInputAction* translateAction = EditorTranslateAction.LoadSynchronous())
	{
		enhancedInputComponent->BindAction(
			translateAction,
			ETriggerEvent::Started,
			this,
			&AEpisodeEditorController::HandleTranslateModeInput);
	}

	if (UInputAction* rotateAction = EditorRotateAction.LoadSynchronous())
	{
		enhancedInputComponent->BindAction(
			rotateAction,
			ETriggerEvent::Started,
			this,
			&AEpisodeEditorController::HandleRotateModeInput);
	}

	if (UInputAction* scaleAction = EditorScaleAction.LoadSynchronous())
	{
		enhancedInputComponent->BindAction(
			scaleAction,
			ETriggerEvent::Started,
			this,
			&AEpisodeEditorController::HandleScaleModeInput);
	}
}

void AEpisodeEditorController::UpdatePlacementPreview()
{
	if (!PlacementPreviewActor) return;

	FHitResult hit;
	if (!TraceMousePlacement(hit))
	{
		bCurrentPlacementValid = false;
		CurrentPlacementFailureReason = TEXT("No placement surface under cursor.");
		PlacementPreviewActor->SetPlacementValid(false);
		return;
	}

	CurrentPlacementTransform = BuildPlacementTransform(hit.ImpactPoint);

	UEpisodeAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		bCurrentPlacementValid = false;
		CurrentPlacementFailureReason = TEXT("Episode authoring subsystem is unavailable.");
	}
	else
	{
		bCurrentPlacementValid = ValidatePlacementForSelectedItem(
			authoringSubsystem,
			CurrentPlacementFailureReason);
	}

	PlacementPreviewActor->SetActorTransform(CurrentPlacementTransform);
	PlacementPreviewActor->SetPlacementValid(bCurrentPlacementValid);
}

bool AEpisodeEditorController::ConfigurePlacementPreviewForSelectedItem(
	UEpisodeAuthoringSubsystem* authoringSubsystem)
{
	if (!PlacementPreviewActor || !authoringSubsystem)
	{
		CurrentPlacementFailureReason = TEXT("Episode authoring subsystem is unavailable.");
		return false;
	}

	switch (SelectedPlacementItemType)
	{
	case EEpisodePaletteItemType::StaticObstacle:
	{
		FString failureReason;
		if (!authoringSubsystem->CanPlaceStaticObstacle(
				SelectedStaticObstaclePropId,
				FTransform::Identity,
				failureReason)
			&& failureReason.StartsWith(TEXT("Unknown static obstacle prop")))
		{
			CurrentPlacementFailureReason = failureReason;
			return false;
		}

		FEpisodeStaticObstaclePropEntry propEntry;
		if (!authoringSubsystem->TryGetStaticObstaclePropEntry(SelectedStaticObstaclePropId, propEntry)
			|| !PlacementPreviewActor->ConfigureStaticObstaclePropEntry(propEntry))
		{
			CurrentPlacementFailureReason = FString::Printf(
				TEXT("Failed to configure static obstacle preview '%s'."),
				*SelectedStaticObstaclePropId.ToString());
			return false;
		}
		return true;
	}
	case EEpisodePaletteItemType::Pedestrian:
	{
		TSubclassOf<AActor> pedestrianPreviewClass = authoringSubsystem->PedestrianVisualizationActorClass;
		if (!pedestrianPreviewClass)
		{
			pedestrianPreviewClass = authoringSubsystem->PedestrianClass.Get();
		}
		if (!pedestrianPreviewClass)
		{
			CurrentPlacementFailureReason = TEXT("Pedestrian preview class is unavailable.");
			return false;
		}

		if (!PlacementPreviewActor->ConfigureActorPreviewClass(pedestrianPreviewClass))
		{
			CurrentPlacementFailureReason = FString::Printf(
				TEXT("Failed to configure pedestrian preview class '%s'."),
				*pedestrianPreviewClass->GetPathName());
			return false;
		}
		return true;
	}
	case EEpisodePaletteItemType::RobotStart:
		if (!PlacementPreviewActor->ConfigureActorPreviewClass(authoringSubsystem->StartPointClass))
		{
			CurrentPlacementFailureReason = TEXT("Failed to configure robot start preview.");
			return false;
		}
		return true;
	case EEpisodePaletteItemType::RobotGoal:
		if (!PlacementPreviewActor->ConfigureActorPreviewClass(authoringSubsystem->GoalPointClass))
		{
			CurrentPlacementFailureReason = TEXT("Failed to configure robot goal preview.");
			return false;
		}
		return true;
	default:
		CurrentPlacementFailureReason = TEXT("Unknown palette placement item type.");
		return false;
	}
}

bool AEpisodeEditorController::ValidatePlacementForSelectedItem(
	const UEpisodeAuthoringSubsystem* authoringSubsystem,
	FString& outFailureReason) const
{
	outFailureReason.Reset();
	if (!authoringSubsystem)
	{
		outFailureReason = TEXT("Episode authoring subsystem is unavailable.");
		return false;
	}

	switch (SelectedPlacementItemType)
	{
	case EEpisodePaletteItemType::StaticObstacle:
		return authoringSubsystem->CanPlaceStaticObstacle(
			SelectedStaticObstaclePropId,
			CurrentPlacementTransform,
			outFailureReason);
	case EEpisodePaletteItemType::Pedestrian:
	case EEpisodePaletteItemType::RobotStart:
		return authoringSubsystem->CanPlaceEditorGroundActor(
			CurrentPlacementTransform,
			outFailureReason);
	case EEpisodePaletteItemType::RobotGoal:
		if (!HasAuthoredRobotStart(authoringSubsystem))
		{
			outFailureReason = TEXT("Robot start point must be placed before a goal point.");
			return false;
		}
		return authoringSubsystem->CanPlaceEditorGroundActor(
			CurrentPlacementTransform,
			outFailureReason);
	default:
		outFailureReason = TEXT("Unknown palette placement item type.");
		return false;
	}
}

bool AEpisodeEditorController::HasAuthoredRobotStart(
	const UEpisodeAuthoringSubsystem* authoringSubsystem) const
{
	if (!authoringSubsystem)
	{
		return false;
	}

	const FEpisodeWorldSpec draftWorldSpec = authoringSubsystem->GetDraftWorldSpec();
	for (const FEpisodePlaceableInstanceSpec& spec : draftWorldSpec.Placeables)
	{
		if ((spec.Category == EEpisodeActorCategory::DeliveryBot
				|| spec.Category == EEpisodeActorCategory::RoadVehicle)
			&& spec.DeliveryBot.bHasStartLocation)
		{
			return true;
		}
	}

	return false;
}

bool AEpisodeEditorController::TraceMousePlacement(FHitResult& outHit) const
{
	FVector worldOrigin = FVector::ZeroVector;
	FVector worldDirection = FVector::ForwardVector;
	if (!DeprojectMousePositionToWorld(worldOrigin, worldDirection)) return false;

	UWorld* world = GetWorld();
	if (!world) return false;

	FCollisionQueryParams queryParams(SCENE_QUERY_STAT(EpisodeEditorPlacementTrace), true);
	queryParams.bReturnPhysicalMaterial = false;
	if (PlacementPreviewActor)
	{
		queryParams.AddIgnoredActor(PlacementPreviewActor);
	}
	if (const APawn* pawn = GetPawn())
	{
		queryParams.AddIgnoredActor(pawn);
	}
	if (UEpisodeAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem())
	{
		TArray<AActor*> ignoredActors;
		authoringSubsystem->GetEditorPlacementIgnoredActors(ignoredActors);
		for (const AActor* ignoredActor : ignoredActors)
		{
			queryParams.AddIgnoredActor(ignoredActor);
		}
	}

	const FVector traceEnd = worldOrigin + worldDirection.GetSafeNormal() * PlacementTraceDistanceCm;
	return world->LineTraceSingleByChannel(
		outHit,
		worldOrigin,
		traceEnd,
		PlacementTraceChannel,
		queryParams);
}

FTransform AEpisodeEditorController::BuildPlacementTransform(const FVector& location) const
{
	FVector snappedLocation = SnapLocationIfNeeded(location);
	if (FMath::Abs(snappedLocation.Z) <= PlacementGroundSnapToleranceCm)
	{
		snappedLocation.Z = 0.0;
	}

	return FTransform(FRotator::ZeroRotator, snappedLocation, FVector::OneVector);
}

FVector AEpisodeEditorController::SnapLocationIfNeeded(const FVector& location) const
{
	if (!bSnapPlacementToGrid || PlacementGridSizeCm <= KINDA_SMALL_NUMBER)
	{
		return location;
	}

	return FVector(
		FMath::GridSnap(location.X, PlacementGridSizeCm),
		FMath::GridSnap(location.Y, PlacementGridSizeCm),
		location.Z);
}

void AEpisodeEditorController::ApplyInputMode()
{
	if (EditorMode == EEpisodeEditorControllerMode::Observer && bIsTransformGizmoDragging)
	{
		bShowMouseCursor = true;
		bEnableClickEvents = true;
		bEnableMouseOverEvents = true;

		FInputModeGameAndUI inputMode;
		inputMode.SetHideCursorDuringCapture(false);
		inputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(inputMode);
	}
	else if (EditorMode == EEpisodeEditorControllerMode::Observer && bIsLookInputHeld)
	{
		bShowMouseCursor = false;
		bEnableClickEvents = false;
		bEnableMouseOverEvents = false;

		FInputModeGameOnly inputMode;
		SetInputMode(inputMode);
	}
	else if (EditorMode == EEpisodeEditorControllerMode::EditPlacement)
	{
		bShowMouseCursor = true;
		bEnableClickEvents = true;
		bEnableMouseOverEvents = true;

		FInputModeGameAndUI inputMode;
		inputMode.SetHideCursorDuringCapture(false);
		SetInputMode(inputMode);
	}
	else if (UWidget* focusWidget = FindEditorWidgetInputModeFocus())
	{
		bShowMouseCursor = true;
		bEnableClickEvents = true;
		bEnableMouseOverEvents = true;

		FInputModeGameAndUI inputMode;
		const TSharedRef<SWidget> slateFocusWidget = focusWidget->TakeWidget();
		if (slateFocusWidget->SupportsKeyboardFocus())
		{
			inputMode.SetWidgetToFocus(slateFocusWidget);
		}
		inputMode.SetHideCursorDuringCapture(false);
		inputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(inputMode);
	}
	else
	{
		bShowMouseCursor = false;
		bEnableClickEvents = false;
		bEnableMouseOverEvents = false;

		FInputModeGameOnly inputMode;
		SetInputMode(inputMode);
	}
}

void AEpisodeEditorController::PruneEditorWidgetInputModeRequests()
{
	EditorWidgetInputModeRequesters.RemoveAll(
		[](const TWeakObjectPtr<UWidget>& requester)
		{
			return !requester.IsValid();
		});
}

UWidget* AEpisodeEditorController::FindEditorWidgetInputModeFocus() const
{
	for (int32 i = EditorWidgetInputModeRequesters.Num() - 1; i >= 0; --i)
	{
		UWidget* requester = EditorWidgetInputModeRequesters[i].Get();
		if (requester && requester->IsVisible())
		{
			return requester;
		}
	}

	return nullptr;
}

void AEpisodeEditorController::DestroyPlacementPreview()
{
	if (IsValid(PlacementPreviewActor))
	{
		PlacementPreviewActor->Destroy();
	}

	PlacementPreviewActor = nullptr;
}

AEpisodeTransformGizmoActor* AEpisodeEditorController::EnsureTransformGizmoActor()
{
	if (IsValid(TransformGizmoActor))
	{
		return TransformGizmoActor;
	}

	UWorld* world = GetWorld();
	if (!world || !TransformGizmoActorClass)
	{
		return nullptr;
	}

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TransformGizmoActor = world->SpawnActor<AEpisodeTransformGizmoActor>(
		TransformGizmoActorClass,
		FTransform::Identity,
		spawnParams);
	return TransformGizmoActor;
}

void AEpisodeEditorController::UpdateTransformGizmoForSelection()
{
	UEpisodePlaceableComponent* selectedPlaceable = SelectedPlaceableComponent.Get();
	if (!IsEditorSelectablePlaceable(selectedPlaceable))
	{
		HideTransformGizmo();
		return;
	}

	AActor* selectedActor = selectedPlaceable->GetOwner();
	if (!selectedActor)
	{
		HideTransformGizmo();
		return;
	}

	AEpisodeTransformGizmoActor* gizmoActor = EnsureTransformGizmoActor();
	if (!gizmoActor)
	{
		return;
	}

	if (!IsTransformModeAllowedForPlaceable(selectedPlaceable, TransformGizmoMode))
	{
		TransformGizmoMode = EEpisodeTransformGizmoMode::Translate;
	}

	gizmoActor->ShowForTarget(selectedActor);
	gizmoActor->SetGizmoOrientationMode(GetEffectiveTransformGizmoOrientationModeForPlaceable(selectedPlaceable));
	gizmoActor->SetGizmoMode(TransformGizmoMode);
}

void AEpisodeEditorController::HideTransformGizmo()
{
	if (IsValid(TransformGizmoActor))
	{
		TransformGizmoActor->HideGizmo();
	}
}

void AEpisodeEditorController::SetTransformGizmoMode(EEpisodeTransformGizmoMode mode)
{
	if (UEpisodePlaceableComponent* selectedPlaceable = SelectedPlaceableComponent.Get();
		IsEditorSelectablePlaceable(selectedPlaceable)
		&& !IsTransformModeAllowedForPlaceable(selectedPlaceable, mode))
	{
		mode = EEpisodeTransformGizmoMode::Translate;
	}

	if (TransformGizmoMode == mode)
	{
		return;
	}

	if (bIsTransformGizmoDragging)
	{
		EndTransformGizmoDrag();
	}

	TransformGizmoMode = mode;
	if (IsValid(TransformGizmoActor))
	{
		TransformGizmoActor->SetGizmoMode(TransformGizmoMode);
	}
}

EEpisodeTransformGizmoOrientationMode AEpisodeEditorController::GetEffectiveTransformGizmoOrientationModeForPlaceable(
	const UEpisodePlaceableComponent* placeableComponent) const
{
	return IsRobotRouteMarkerPlaceable(placeableComponent)
		? EEpisodeTransformGizmoOrientationMode::World
		: TransformGizmoOrientationMode;
}

void AEpisodeEditorController::GetTransformGizmoBasis(
	const FTransform& transform,
	EEpisodeTransformGizmoOrientationMode orientationMode,
	FVector& outXAxis,
	FVector& outYAxis,
	FVector& outZAxis) const
{
	if (orientationMode == EEpisodeTransformGizmoOrientationMode::World)
	{
		outXAxis = FVector::ForwardVector;
		outYAxis = FVector::RightVector;
		outZAxis = FVector::UpVector;
		return;
	}

	outXAxis = transform.GetUnitAxis(EAxis::X).GetSafeNormal();
	outYAxis = transform.GetUnitAxis(EAxis::Y).GetSafeNormal();
	outZAxis = transform.GetUnitAxis(EAxis::Z).GetSafeNormal();
	if (outXAxis.IsNearlyZero()) outXAxis = FVector::ForwardVector;
	if (outYAxis.IsNearlyZero()) outYAxis = FVector::RightVector;
	if (outZAxis.IsNearlyZero()) outZAxis = FVector::UpVector;
}

UEpisodePlaceableContextMenuWidget* AEpisodeEditorController::EnsurePlaceableContextMenuWidget()
{
	UEpisodeEditorRootWidget* rootWidget = ShowEditorRootWidget();
	return rootWidget ? rootWidget->GetPlaceableContextMenuWidget() : nullptr;
}

void AEpisodeEditorController::UpdatePlaceableContextMenuForSelection(bool bRepositionToMouse)
{
	(void)bRepositionToMouse;

	UEpisodePlaceableComponent* selectedPlaceable = SelectedPlaceableComponent.Get();
	if (!IsEditorSelectablePlaceable(selectedPlaceable))
	{
		HidePlaceableContextMenu();
		return;
	}

	UEpisodeEditorRootWidget* rootWidget = ShowEditorRootWidget();
	if (!rootWidget || !rootWidget->ShowPlaceableContextMenu(selectedPlaceable))
	{
		return;
	}
}

void AEpisodeEditorController::HidePlaceableContextMenu()
{
	if (UEpisodeEditorRootWidget* rootWidget = GetEditorRootWidget())
	{
		rootWidget->HidePlaceableContextMenu();
	}
}

AEpisodeEditorPawn* AEpisodeEditorController::GetEditorPawn() const
{
	return Cast<AEpisodeEditorPawn>(GetPawn());
}

UEpisodeAuthoringSubsystem* AEpisodeEditorController::GetAuthoringSubsystem() const
{
	UWorld* world = GetWorld();
	return world ? world->GetSubsystem<UEpisodeAuthoringSubsystem>() : nullptr;
}
