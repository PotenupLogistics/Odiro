
#include "Scenario/Editor/ScenarioEditorController.h"
#include "Scenario/Actors/ScenarioGroundRegion.h"
#include "Scenario/Actors/ScenarioStaticObstacle.h"
#include "Scenario/Components/ScenarioPlaceableComponent.h"
#include "Scenario/Editor/ScenarioAuthoringSubsystem.h"
#include "Scenario/Editor/ScenarioEditorPawn.h"
#include "Scenario/Editor/ScenarioPlacementPreviewActor.h"
#include "Scenario/Editor/ScenarioTransformGizmoActor.h"
#include "Scenario/Widget/ScenarioEditorRouteMarkerOverlayWidget.h"
#include "Scenario/Widget/ScenarioEditorRootWidget.h"
#include "Scenario/Widget/ScenarioEditorToolbarWidget.h"
#include "Scenario/Actors/ScenarioPedestrian.h"
#include "Scenario/ViewModel/ScenarioEditorShellViewModel.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/CameraComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PawnMovementComponent.h"
#include "InputCoreTypes.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Platform/PlatformUiDeveloperSettings.h"
#include "Platform/Widget/PlatformRootWidget.h"
#include "Shared/Actors/ScenarioPreviewFraming.h"
#include "Shared/ScenarioViewportPresentation.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioEditorController, Log, All);

namespace
{
	bool IsRobotRouteMarkerPlaceable(const UScenarioPlaceableComponent* placeableComponent)
	{
		if (!placeableComponent) return false;

		return placeableComponent->AuthoringRole == EScenarioPlaceableAuthoringRole::RobotStartMarker
			|| placeableComponent->AuthoringRole == EScenarioPlaceableAuthoringRole::RobotGoalMarker;
	}

	// Tests the triangular tail of the screen-space route marker hit shape.
	bool IsPointInsideTriangle2D(
		const FVector2D& point,
		const FVector2D& triangleA,
		const FVector2D& triangleB,
		const FVector2D& triangleC)
	{
		const auto sign = [](const FVector2D& pointA, const FVector2D& pointB, const FVector2D& pointC)
		{
			return (pointA.X - pointC.X) * (pointB.Y - pointC.Y)
				- (pointB.X - pointC.X) * (pointA.Y - pointC.Y);
		};

		const double d1 = sign(point, triangleA, triangleB);
		const double d2 = sign(point, triangleB, triangleC);
		const double d3 = sign(point, triangleC, triangleA);
		const bool bHasNegative = d1 < 0.0 || d2 < 0.0 || d3 < 0.0;
		const bool bHasPositive = d1 > 0.0 || d2 > 0.0 || d3 > 0.0;
		return !(bHasNegative && bHasPositive);
	}

	double GetPointToSegmentDistanceSquared2D(
		const FVector2D& point,
		const FVector2D& segmentStart,
		const FVector2D& segmentEnd)
	{
		const FVector2D segment = segmentEnd - segmentStart;
		const double segmentLengthSquared = segment.SizeSquared();
		if (segmentLengthSquared <= KINDA_SMALL_NUMBER)
		{
			return (point - segmentStart).SizeSquared();
		}

		const double projectedRatio = FVector2D::DotProduct(point - segmentStart, segment) / segmentLengthSquared;
		const FVector2D closestPoint = segmentStart + segment * FMath::Clamp(projectedRatio, 0.0, 1.0);
		return (point - closestPoint).SizeSquared();
	}

	// Identifies vertex handles so transform editing can update corridor axis points.
	bool IsCorridorVertexHandlePlaceable(const UScenarioPlaceableComponent* placeableComponent)
	{
		return placeableComponent
			&& placeableComponent->AuthoringRole == EScenarioPlaceableAuthoringRole::CorridorVertexHandle;
	}

	// Identifies segment handles so transform editing can update a corridor polyline edge.
	bool IsCorridorSegmentHandlePlaceable(const UScenarioPlaceableComponent* placeableComponent)
	{
		return placeableComponent
			&& placeableComponent->AuthoringRole == EScenarioPlaceableAuthoringRole::CorridorSegmentHandle;
	}

	// Groups all Corridor axis handles behind one editor-only placeable predicate.
	bool IsCorridorHandlePlaceable(const UScenarioPlaceableComponent* placeableComponent)
	{
		return IsCorridorVertexHandlePlaceable(placeableComponent)
			|| IsCorridorSegmentHandlePlaceable(placeableComponent);
	}

	bool IsTransformModeAllowedForPlaceable(
		const UScenarioPlaceableComponent* placeableComponent,
		EScenarioTransformGizmoMode mode)
	{
		if (!placeableComponent) return false;

		if (IsCorridorVertexHandlePlaceable(placeableComponent))
		{
			return mode == EScenarioTransformGizmoMode::Translate;
		}
		if (IsCorridorSegmentHandlePlaceable(placeableComponent))
		{
			return mode == EScenarioTransformGizmoMode::Translate
				|| mode == EScenarioTransformGizmoMode::Rotate;
		}

		switch (mode)
		{
		case EScenarioTransformGizmoMode::Translate:
			return placeableComponent->bAuthoringAllowLocationEdit;
		case EScenarioTransformGizmoMode::Rotate:
			return placeableComponent->bAuthoringAllowRotationEdit;
		case EScenarioTransformGizmoMode::Scale:
			// Scale has no dedicated gizmo handle, so block entering scale mode.
			return false;
		default:
			return false;
		}
	}

	bool IsTransformHandleAllowedForPlaceable(
		const UScenarioPlaceableComponent* placeableComponent,
		EScenarioTransformGizmoHandle handle)
	{
		if (!placeableComponent) return false;

		if (IsCorridorVertexHandlePlaceable(placeableComponent))
		{
			return handle == EScenarioTransformGizmoHandle::TranslateX
				|| handle == EScenarioTransformGizmoHandle::TranslateY
				|| handle == EScenarioTransformGizmoHandle::TranslateXY;
		}
		if (IsCorridorSegmentHandlePlaceable(placeableComponent))
		{
			return handle == EScenarioTransformGizmoHandle::TranslateX
				|| handle == EScenarioTransformGizmoHandle::TranslateY
				|| handle == EScenarioTransformGizmoHandle::TranslateXY
				|| handle == EScenarioTransformGizmoHandle::RotateZ;
		}

		switch (handle)
		{
		case EScenarioTransformGizmoHandle::TranslateX:
		case EScenarioTransformGizmoHandle::TranslateY:
		case EScenarioTransformGizmoHandle::TranslateZ:
		case EScenarioTransformGizmoHandle::TranslateXY:
		case EScenarioTransformGizmoHandle::TranslateXZ:
		case EScenarioTransformGizmoHandle::TranslateYZ:
			return placeableComponent->bAuthoringAllowLocationEdit;
		case EScenarioTransformGizmoHandle::RotateX:
		case EScenarioTransformGizmoHandle::RotateY:
		case EScenarioTransformGizmoHandle::RotateZ:
			return placeableComponent->bAuthoringAllowRotationEdit;
		case EScenarioTransformGizmoHandle::ScaleX:
		case EScenarioTransformGizmoHandle::ScaleY:
		case EScenarioTransformGizmoHandle::ScaleZ:
		case EScenarioTransformGizmoHandle::ScaleXY:
		case EScenarioTransformGizmoHandle::ScaleXZ:
		case EScenarioTransformGizmoHandle::ScaleYZ:
		case EScenarioTransformGizmoHandle::ScaleUniform:
			return placeableComponent->bAuthoringAllowScaleEdit;
		case EScenarioTransformGizmoHandle::None:
		default:
			return false;
		}
	}

	bool IsTransformEditAllowedForPlaceable(
		const UScenarioPlaceableComponent* placeableComponent,
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

	// Object Palette currently has a scenario.json authoring contract only for fixed static obstacles.
	bool IsObjectPalettePlacementSupported(const EScenarioPaletteItemType itemType)
	{
		return itemType == EScenarioPaletteItemType::StaticObstacle;
	}

	FString MakeUniqueScenarioSavePath(const FString& preferredPath)
	{
		FString directory = FPaths::GetPath(preferredPath);
		if (directory.IsEmpty())
		{
			directory = TEXT("Saved/UserProjects/ScenarioEditor");
		}

		const FString baseName = FPaths::GetBaseFilename(preferredPath).IsEmpty()
			? FString(TEXT("scenario"))
			: FPaths::GetBaseFilename(preferredPath);
		const FString extension = FPaths::GetExtension(preferredPath).IsEmpty()
			? FString(TEXT("json"))
			: FPaths::GetExtension(preferredPath);

		for (int32 index = 0; index < 1000; ++index)
		{
			const FString fileName = index == 0
				? FString::Printf(TEXT("%s.%s"), *baseName, *extension)
				: FString::Printf(TEXT("%s_%d.%s"), *baseName, index, *extension);
			FString candidatePath = FPaths::Combine(directory, fileName);
			candidatePath.ReplaceInline(TEXT("\\"), TEXT("/"));
			const FString resolvedCandidatePath = FPaths::IsRelative(candidatePath)
				? FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), candidatePath)
				: FPaths::ConvertRelativePathToFull(candidatePath);
			if (!FPaths::FileExists(resolvedCandidatePath))
			{
				return candidatePath;
			}
		}

		FString fallbackPath = FPaths::Combine(
			directory,
			FString::Printf(
				TEXT("%s_%s.%s"),
				*baseName,
				*FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8),
				*extension));
		fallbackPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		return fallbackPath;
	}
}

AScenarioEditorController::AScenarioEditorController()
{
	PrimaryActorTick.bCanEverTick = true;
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	PlacementPreviewActorClass = AScenarioPlacementPreviewActor::StaticClass();
	TransformGizmoActorClass = AScenarioTransformGizmoActor::StaticClass();
	EditorInputMappingContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/Input/IMC_Editor.IMC_Editor")));
	EditorMoveAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_EditorMove.IA_EditorMove")));
	EditorLookAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_EditorLook.IA_EditorLook")));
	EditorLookCaptureAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_EditorLookCapture.IA_EditorLookCapture")));
	EditorSelectionAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_Selection.IA_Selection")));
	EditorDeselectionAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_Deselection.IA_Deselection")));
	EditorTranslateAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_EditorTranslate.IA_EditorTranslate")));
	EditorRotateAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_EditorRotate.IA_EditorRotate")));
	EditorScaleAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_EditorScale.IA_EditorScale")));
	EditorViewModeToggleAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_EditorViewModeToggle.IA_EditorViewModeToggle")));
	EditorZoomAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_EditorZoom.IA_EditorZoom")));
}

void AScenarioEditorController::BeginPlay()
{
	Super::BeginPlay();
	AddEditorInputMappingContext();
	EnsureAuthoringOutlineCustomDepthEnabled();
	SetObserverMode();
	if (UPlatformRootWidget* platformRootWidget = ShowPlatformRootWidget())
	{
		const EPlatformRootScreen activeScreen = platformRootWidget->GetActiveScreen();
		if (activeScreen != EPlatformRootScreen::Startup && activeScreen != EPlatformRootScreen::ProjectCreate)
		{
			ShowEditorRootWidget();
		}
	}
}

void AScenarioEditorController::EndPlay(const EEndPlayReason::Type endPlayReason)
{
	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().ClearTimer(ScenarioPreviewCaptureTimerHandle);
	}
	RemoveEditorRootWidget();
	RemovePlatformRootWidget();
	Super::EndPlay(endPlayReason);
}

void AScenarioEditorController::Tick(float deltaSeconds)
{
	Super::Tick(deltaSeconds);

	switch (EditorMode)
	{
	case EScenarioEditorControllerMode::Observer:
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
	case EScenarioEditorControllerMode::EditPlacement:
		// RMB is also bound to camera capture, so keep placement cancel independent of Enhanced Input consumption.
		if (WasInputKeyJustPressed(EKeys::RightMouseButton))
		{
			CancelPlacement();
			break;
		}
		UpdatePlacementPreview();
		break;
	case EScenarioEditorControllerMode::EditRegionDraw:
		UpdateRegionDrawPreview();
		break;
	default:
		break;
	}
}

void AScenarioEditorController::SetupInputComponent()
{
	Super::SetupInputComponent();
	BindEditorInputActions();
}

void AScenarioEditorController::SetObserverMode()
{
	EditorMode = EScenarioEditorControllerMode::Observer;
	SelectedStaticObstaclePropId = NAME_None;
	SelectedPlacementItemType = EScenarioPaletteItemType::StaticObstacle;
	SelectedPlacementAssetId = NAME_None;
	bCurrentPlacementValid = false;
	CurrentPlacementFailureReason.Reset();
	bIsLookInputHeld = false;
	bIsRegionDragging = false;
	PressedPlaceableComponent.Reset();
	ResetTransformGizmoDrag();
	SetHoveredPlaceable(nullptr);
	SetSelectedPlaceable(nullptr);
	DestroyPlacementPreview();
	DestroyRegionDrawPreview();
	ApplyInputMode();
}

void AScenarioEditorController::SetEditorViewMode(EScenarioEditorViewMode viewMode)
{
	if (EditorViewMode == viewMode)
	{
		return;
	}

	AScenarioEditorPawn* editorPawn = GetEditorPawn();
	if (!editorPawn) return;

	if (bIsTransformGizmoDragging)
	{
		EndTransformGizmoDrag();
	}

	EditorViewMode = viewMode;
	if (viewMode == EScenarioEditorViewMode::TopDownOrtho)
	{
		editorPawn->EnterTopDownView();
	}
	else
	{
		editorPawn->EnterPerspectiveView();
	}

	RequestFitEditorViewToScenario();
}

void AScenarioEditorController::ToggleEditorViewMode()
{
	SetEditorViewMode(
		EditorViewMode == EScenarioEditorViewMode::Perspective
			? EScenarioEditorViewMode::TopDownOrtho
			: EScenarioEditorViewMode::Perspective);
}

// Queues a scenario viewport fit after entry, loading, or an explicit view-mode switch.
void AScenarioEditorController::RequestFitEditorViewToScenario()
{
	if (bEditorViewFitQueued)
	{
		return;
	}

	UWorld* world = GetWorld();
	if (!IsValid(world))
	{
		FitEditorViewToScenario();
		return;
	}

	bEditorViewFitQueued = true;
	FTimerDelegate fitDelegate;
	fitDelegate.BindUObject(this, &AScenarioEditorController::FitEditorViewToScenario);
	world->GetTimerManager().SetTimerForNextTick(fitDelegate);
}

// Applies the queued scenario viewport fit to the currently active editor view mode.
void AScenarioEditorController::FitEditorViewToScenario()
{
	bEditorViewFitQueued = false;

	AScenarioEditorPawn* editorPawn = GetEditorPawn();
	if (!IsValid(editorPawn))
	{
		return;
	}

	FScenarioPreviewFrame frame;
	FScenarioMapBounds mapBounds;
	if (!TryResolveEditorViewportFrame(frame, mapBounds))
	{
		return;
	}

	if (EditorViewMode == EScenarioEditorViewMode::TopDownOrtho)
	{
		editorPawn->ApplyTopDownFrame(frame.CenterXY, frame.OrthoWidth);
		return;
	}

	int32 viewportWidth = 0;
	int32 viewportHeight = 0;
	GetViewportSize(viewportWidth, viewportHeight);
	const double aspectRatio =
		viewportWidth > 0 && viewportHeight > 0
			? static_cast<double>(viewportWidth) / static_cast<double>(viewportHeight)
			: (16.0 / 9.0);
	const double visibleHeightCm = frame.OrthoWidth / FMath::Max(aspectRatio, KINDA_SMALL_NUMBER);
	const double fitRadiusCm = FVector2D(frame.OrthoWidth, visibleHeightCm).Size() * 0.5;

	const double horizontalFovDegrees =
		editorPawn->CameraComponent
			? static_cast<double>(editorPawn->CameraComponent->FieldOfView)
			: 90.0;
	const double horizontalHalfFovRadians = FMath::DegreesToRadians(
		FMath::Clamp(horizontalFovDegrees, 5.0, 170.0) * 0.5);
	const double verticalHalfFovRadians = FMath::Atan(
		FMath::Tan(horizontalHalfFovRadians) / FMath::Max(aspectRatio, KINDA_SMALL_NUMBER));
	const double fitHalfFovRadians = FMath::Max(
		FMath::Min(horizontalHalfFovRadians, verticalHalfFovRadians),
		FMath::DegreesToRadians(1.0));
	const double fitDistanceCm = fitRadiusCm / FMath::Sin(fitHalfFovRadians);

	editorPawn->ApplyPerspectiveFrame(
		frame.CenterXY,
		mapBounds.CenterZ + EditorPerspectiveFitTargetHeightCm,
		FMath::Max(fitDistanceCm, EditorPerspectiveFitMinDistanceCm),
		EditorPerspectiveFitYawDegrees,
		EditorPerspectiveFitPitchDegrees);
}

// Resolves a viewport-aspect-aware frame for the current authored scenario.
bool AScenarioEditorController::TryResolveEditorViewportFrame(
	FScenarioPreviewFrame& outFrame,
	FScenarioMapBounds& outBounds) const
{
	outFrame = FScenarioPreviewFrame{};
	outBounds = FScenarioMapBounds{};

	const UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!IsValid(authoringSubsystem)
		|| !authoringSubsystem->TryResolveScenarioEditorViewportMapBounds(outBounds))
	{
		return false;
	}

	int32 viewportWidth = 0;
	int32 viewportHeight = 0;
	GetViewportSize(viewportWidth, viewportHeight);
	if (viewportWidth <= 0 || viewportHeight <= 0)
	{
		viewportWidth = 1280;
		viewportHeight = 720;
	}

	FScenarioPreviewFramingSettings framingSettings;
	framingSettings.OutputWidth = viewportWidth;
	framingSettings.OutputHeight = viewportHeight;
	framingSettings.ScenarioPreviewFitScale = FMath::Max(EditorViewportFitScale, 1.0);

	return FScenarioPreviewFramingResolver::TryResolveScenario(
		outBounds,
		framingSettings,
		outFrame);
}

void AScenarioEditorController::SetPlacementSnapToGridEnabled(const bool bEnabled)
{
	bSnapPlacementToGrid = bEnabled;
}

void AScenarioEditorController::TogglePlacementSnapToGrid()
{
	SetPlacementSnapToGridEnabled(!bSnapPlacementToGrid);
}

void AScenarioEditorController::RequestEditorWidgetInputMode(UWidget* focusWidget)
{
	if (!focusWidget) return;

	PruneEditorWidgetInputModeRequests();
	EditorWidgetInputModeRequesters.AddUnique(TWeakObjectPtr(focusWidget));
	ApplyInputMode();
}

void AScenarioEditorController::ReleaseEditorWidgetInputMode(UWidget* focusWidget)
{
	if (!focusWidget) return;

	EditorWidgetInputModeRequesters.RemoveAll(
		[focusWidget](const TWeakObjectPtr<UWidget>& requester)
		{
			return !requester.IsValid() || requester.Get() == focusWidget;
		});
	ApplyInputMode();
}

bool AScenarioEditorController::BeginStaticObstaclePlacement(FName propId)
{
	return BeginPalettePlacement(EScenarioPaletteItemType::StaticObstacle, propId);
}

bool AScenarioEditorController::BeginPalettePlacement(EScenarioPaletteItemType itemType, FName assetId)
{
	if (!IsObjectPalettePlacementSupported(itemType))
	{
		UE_LOG(
			LogScenarioEditorController,
			Warning,
			TEXT("Object Palette placement is not supported for this authoring type | Type: %d | AssetId: %s"),
			static_cast<int32>(itemType),
			*assetId.ToString());
		return false;
	}

	bIsLookInputHeld = false;
	bIsRegionDragging = false;
	PressedPlaceableComponent.Reset();
	ResetTransformGizmoDrag();
	SetHoveredPlaceable(nullptr);
	SetSelectedPlaceable(nullptr);
	DestroyRegionDrawPreview();

	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem) return false;

	UWorld* world = GetWorld();
	if (!world || !PlacementPreviewActorClass) return false;

	if (!PlacementPreviewActor)
	{
		FActorSpawnParameters spawnParams;
		spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		PlacementPreviewActor = world->SpawnActor<AScenarioPlacementPreviewActor>(
			PlacementPreviewActorClass,
			FTransform::Identity,
			spawnParams);
	}

	SelectedPlacementItemType = itemType;
	SelectedPlacementAssetId = assetId;
	SelectedStaticObstaclePropId =
		itemType == EScenarioPaletteItemType::StaticObstacle ? assetId : NAME_None;

	if (!PlacementPreviewActor || !ConfigurePlacementPreviewForSelectedItem(authoringSubsystem))
	{
		UE_LOG(
			LogScenarioEditorController,
			Warning,
			TEXT("Failed to begin placement preview | Type: %d | AssetId: %s | Reason: %s"),
			static_cast<int32>(SelectedPlacementItemType),
			*SelectedPlacementAssetId.ToString(),
			CurrentPlacementFailureReason.IsEmpty() ? TEXT("<none>") : *CurrentPlacementFailureReason);

		DestroyPlacementPreview();
		SelectedPlacementItemType = EScenarioPaletteItemType::StaticObstacle;
		SelectedPlacementAssetId = NAME_None;
		SelectedStaticObstaclePropId = NAME_None;
		return false;
	}

	EditorMode = EScenarioEditorControllerMode::EditPlacement;
	ApplyInputMode();
	UpdatePlacementPreview();
	return true;
}

void AScenarioEditorController::CancelPlacement()
{
	if (EditorMode == EScenarioEditorControllerMode::EditPlacement
		|| EditorMode == EScenarioEditorControllerMode::EditRegionDraw)
	{
		SetObserverMode();
	}
}

bool AScenarioEditorController::ConfirmPlacement()
{
	if (EditorMode != EScenarioEditorControllerMode::EditPlacement || !bCurrentPlacementValid)
	{
		return false;
	}

	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem) return false;

	bool bPlaced = false;
	FString failureReason;
	switch (SelectedPlacementItemType)
	{
	case EScenarioPaletteItemType::StaticObstacle:
	{
		FScenarioPlaceableInstanceSpec placedSpec;
		AScenarioStaticObstacle* placedActor = nullptr;
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
	case EScenarioPaletteItemType::Pedestrian:
	{
		FScenarioDynamicActorSpec placedSpec;
		AActor* placedActor = nullptr;
		bPlaced = authoringSubsystem->AddPedestrian(
			SelectedPlacementAssetId,
			CurrentPlacementTransform,
			placedSpec,
			placedActor,
			failureReason);
		break;
	}
	case EScenarioPaletteItemType::RobotStart:
	{
		FScenarioPlaceableInstanceSpec placedSpec;
		AActor* placedMarker = nullptr;
		bPlaced = authoringSubsystem->SetRobotStartLocation(
			SelectedPlacementAssetId,
			CurrentPlacementTransform,
			placedSpec,
			placedMarker,
			failureReason);
		break;
	}
	case EScenarioPaletteItemType::RobotGoal:
	{
		FScenarioPlaceableInstanceSpec placedSpec;
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
		if (UScenarioEditorRootWidget* rootWidget = GetEditorRootWidget())
		{
			rootWidget->RefreshScenarioInspectorWithOutlinerRegistryRebuild();
		}
		return true;
	}

	CurrentPlacementFailureReason = failureReason;
	if (PlacementPreviewActor)
	{
		PlacementPreviewActor->SetPlacementValid(false);
	}

	UE_LOG(
		LogScenarioEditorController,
		Warning,
		TEXT("Placement failed | Type: %d | AssetId: %s | Reason: %s"),
		static_cast<int32>(SelectedPlacementItemType),
		*SelectedPlacementAssetId.ToString(),
		*CurrentPlacementFailureReason);
	return false;
}

bool AScenarioEditorController::BeginGroundRegionDraw(EScenarioGroundRegionType regionType)
{
	bIsLookInputHeld = false;
	bIsRegionDragging = false;
	PressedPlaceableComponent.Reset();
	ResetTransformGizmoDrag();
	SetHoveredPlaceable(nullptr);
	SetSelectedPlaceable(nullptr);
	DestroyPlacementPreview();

	if (!GetAuthoringSubsystem())
	{
		return false;
	}

	PendingGroundRegionType = regionType;

	// Prepare the preview actor before the drag interaction starts.
	if (AScenarioGroundRegion* previewActor = EnsureRegionDrawPreviewActor())
	{
		previewActor->SetActorHiddenInGame(true);
	}

	EditorMode = EScenarioEditorControllerMode::EditRegionDraw;
	ApplyInputMode();
	return true;
}

void AScenarioEditorController::BeginRegionDrag()
{
	FVector groundPoint = FVector::ZeroVector;
	if (!TraceMouseToGroundRegionPlane(groundPoint))
	{
		return;
	}

	RegionDragStartWorld = groundPoint;
	bIsRegionDragging = true;
	ConfigureRegionDrawPreview(groundPoint, FVector2D::ZeroVector);
}

void AScenarioEditorController::UpdateRegionDrawPreview()
{
	if (!bIsRegionDragging)
	{
		return;
	}

	FVector groundPoint = FVector::ZeroVector;
	if (!TraceMouseToGroundRegionPlane(groundPoint))
	{
		return;
	}

	FVector center = FVector::ZeroVector;
	FVector2D size = FVector2D::ZeroVector;
	ComputeRegionRectFromCorners(RegionDragStartWorld, groundPoint, center, size);
	ConfigureRegionDrawPreview(center, size);
}

void AScenarioEditorController::FinalizeRegionDrag()
{
	bIsRegionDragging = false;

	FVector groundPoint = FVector::ZeroVector;
	if (!TraceMouseToGroundRegionPlane(groundPoint))
	{
		groundPoint = RegionDragStartWorld;
	}

	FVector center = FVector::ZeroVector;
	FVector2D size = FVector2D::ZeroVector;
	ComputeRegionRectFromCorners(RegionDragStartWorld, groundPoint, center, size);

	if (AScenarioGroundRegion* previewActor = RegionDrawPreviewActor.Get())
	{
		previewActor->SetActorHiddenInGame(true);
	}

	// Treat rectangles below the minimum size as stray clicks and skip committing them.
	if (size.X < RegionDrawMinSizeCm || size.Y < RegionDrawMinSizeCm)
	{
		return;
	}

	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		return;
	}

	FScenarioGroundRegionSpec placedSpec;
	FString failureReason;
	if (!authoringSubsystem->AddGroundRegion(
		PendingGroundRegionType,
		center,
		size,
		0.0,
		placedSpec,
		failureReason))
	{
		UE_LOG(
			LogScenarioEditorController,
			Warning,
			TEXT("Ground region placement failed | Reason: %s"),
			*failureReason);
	}

	// Keep EditRegionDraw mode active for continuous drawing.
}

bool AScenarioEditorController::TraceMouseToGroundRegionPlane(FVector& outPoint) const
{
	const FVector planeOrigin(0.0, 0.0, GroundRegionDrawPlaneZCm);
	FVector hitPoint = FVector::ZeroVector;
	if (!TraceMouseToPlane(planeOrigin, FVector::UpVector, hitPoint))
	{
		return false;
	}

	outPoint = SnapLocationIfNeeded(hitPoint);
	outPoint.Z = GroundRegionDrawPlaneZCm;
	return true;
}

void AScenarioEditorController::ComputeRegionRectFromCorners(
	const FVector& cornerA,
	const FVector& cornerB,
	FVector& outCenter,
	FVector2D& outSize) const
{
	outCenter = FVector(
		(cornerA.X + cornerB.X) * 0.5,
		(cornerA.Y + cornerB.Y) * 0.5,
		GroundRegionDrawPlaneZCm);
	outSize = FVector2D(
		FMath::Abs(cornerA.X - cornerB.X),
		FMath::Abs(cornerA.Y - cornerB.Y));
}

void AScenarioEditorController::ConfigureRegionDrawPreview(const FVector& center, const FVector2D& size)
{
	AScenarioGroundRegion* previewActor = EnsureRegionDrawPreviewActor();
	if (!previewActor)
	{
		return;
	}

	FScenarioGroundRegionSpec spec;
	spec.RegionId = TEXT("__region_draw_preview");
	spec.RegionType = PendingGroundRegionType;
	spec.ShapeType = EScenarioGroundShapeType::Rectangle;
	spec.Center = center;
	spec.Size = FVector2D(FMath::Max(size.X, 1.0), FMath::Max(size.Y, 1.0));
	spec.YawDegrees = 0.0;

	previewActor->ConfigureRegion(spec);
	previewActor->SetActorEnableCollision(false);
	previewActor->SetActorHiddenInGame(false);
}

AScenarioGroundRegion* AScenarioEditorController::EnsureRegionDrawPreviewActor()
{
	if (IsValid(RegionDrawPreviewActor))
	{
		return RegionDrawPreviewActor;
	}

	UWorld* world = GetWorld();
	if (!world)
	{
		return nullptr;
	}

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	RegionDrawPreviewActor = world->SpawnActor<AScenarioGroundRegion>(
		AScenarioGroundRegion::StaticClass(),
		FTransform::Identity,
		spawnParams);
	if (RegionDrawPreviewActor)
	{
		RegionDrawPreviewActor->SetActorEnableCollision(false);
	}
	return RegionDrawPreviewActor;
}

void AScenarioEditorController::DestroyRegionDrawPreview()
{
	if (IsValid(RegionDrawPreviewActor))
	{
		RegionDrawPreviewActor->Destroy();
	}

	RegionDrawPreviewActor = nullptr;
}

void AScenarioEditorController::GetStaticObstaclePaletteEntries(TArray<FScenarioStaticObstaclePropEntry>& outEntries) const
{
	outEntries.Reset();
	if (const UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem())
	{
		authoringSubsystem->GetStaticObstaclePaletteEntries(outEntries);
	}
}

bool AScenarioEditorController::ExportAndValidateProjectScenarioJsonString(
	FString& outJsonString,
	TArray<FString>& outDiagnostics) const
{
	outJsonString.Reset();
	outDiagnostics.Reset();

	const UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		outDiagnostics.Add(TEXT("Scenario authoring subsystem is unavailable."));
		return false;
	}

	return authoringSubsystem->ExportAndValidateProjectScenarioJsonString(outJsonString, outDiagnostics);
}

bool AScenarioEditorController::LoadProjectScenarioJsonFile(
	const FString& jsonFilePath,
	FString& outResolvedJsonFilePath,
	TArray<FString>& outDiagnostics)
{
	outResolvedJsonFilePath.Reset();
	outDiagnostics.Reset();

	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		outDiagnostics.Add(TEXT("Scenario authoring subsystem is unavailable."));
		return false;
	}

	CancelPlacement();
	const bool bLoaded = authoringSubsystem->LoadProjectScenarioJsonFile(
		jsonFilePath,
		outResolvedJsonFilePath,
		outDiagnostics);
	if (bLoaded)
	{
		RequestFitEditorViewToScenario();
		ScheduleScenarioPreviewCaptureIfMissing(outResolvedJsonFilePath);
	}
	return bLoaded;
}

// Defers PlayerStart reset so load-time viewport fit can finish first.
void AScenarioEditorController::RequestMoveEditorViewToPlayerStart()
{
	UWorld* world = GetWorld();
	if (!IsValid(world))
	{
		MoveEditorViewToPlayerStart();
		return;
	}

	FTimerDelegate resetDelegate;
	resetDelegate.BindUObject(this, &AScenarioEditorController::HandleMoveEditorViewToPlayerStartTimer);
	world->GetTimerManager().SetTimerForNextTick(resetDelegate);
}

// Bridges next-tick timer execution to the PlayerStart reset command.
void AScenarioEditorController::HandleMoveEditorViewToPlayerStartTimer()
{
	MoveEditorViewToPlayerStart();
}

// Applies the ScenarioEditorMap PlayerStart transform to the active editor camera pawn.
bool AScenarioEditorController::MoveEditorViewToPlayerStart()
{
	UWorld* world = GetWorld();
	AScenarioEditorPawn* editorPawn = GetEditorPawn();
	if (!IsValid(world) || !IsValid(editorPawn))
	{
		return false;
	}

	APlayerStart* playerStart = nullptr;
	for (TActorIterator<APlayerStart> actorIt(world); actorIt; ++actorIt)
	{
		if (IsValid(*actorIt))
		{
			playerStart = *actorIt;
			break;
		}
	}

	if (!IsValid(playerStart))
	{
		UE_LOG(
			LogScenarioEditorController,
			Warning,
			TEXT("Scenario editor PlayerStart view reset skipped because no PlayerStart exists in the map."));
		return false;
	}

	if (editorPawn->IsTopDownViewActive())
	{
		editorPawn->EnterPerspectiveView();
	}
	EditorViewMode = EScenarioEditorViewMode::Perspective;

	const FTransform playerStartTransform = playerStart->GetActorTransform();
	const FRotator playerStartRotation = playerStartTransform.GetRotation().Rotator();
	editorPawn->SetActorLocationAndRotation(
		playerStartTransform.GetLocation(),
		playerStartRotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	SetControlRotation(playerStartRotation);

	if (UPawnMovementComponent* movementComponent = editorPawn->GetMovementComponent())
	{
		movementComponent->StopMovementImmediately();
	}
	return true;
}

void AScenarioEditorController::NewScenarioDraft()
{
	CancelPlacement();
	if (UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem())
	{
		authoringSubsystem->NewDraft();
		RequestFitEditorViewToScenario();
	}
}

// 현재 Editor Draft를 저장하고 성공한 경우 프로젝트 대표 Preview를 갱신한다.
bool AScenarioEditorController::SaveProjectScenarioJsonFile(
	const FString& jsonFilePath,
	FString& outResolvedJsonFilePath,
	TArray<FString>& outDiagnostics)
{
	// 이전 저장 호출의 출력 상태를 초기화한다.
	outResolvedJsonFilePath.Reset();
	outDiagnostics.Reset();

	// JSON 저장 책임을 가진 AuthoringSubsystem을 조회한다.
	UScenarioAuthoringSubsystem* authoringSubsystem =
		GetAuthoringSubsystem();
	if (!IsValid(authoringSubsystem))
	{
		outDiagnostics.Add(
			TEXT("Scenario authoring subsystem is unavailable."));
		return false;
	}

	// 검증된 scenario.json을 먼저 저장한다.
	const bool bSaved =
		authoringSubsystem->SaveProjectScenarioJsonFile(
			jsonFilePath,
			outResolvedJsonFilePath,
			outDiagnostics);
	if (!bSaved)
	{
		return false;
	}

	// Generate preview.png as a derived artifact after the JSON save succeeds.
	ScheduleScenarioPreviewCaptureAfterSave(outResolvedJsonFilePath);

	return true;
}

bool AScenarioEditorController::SaveCurrentScenarioDraft(
	FString& outResolvedJsonFilePath,
	TArray<FString>& outDiagnostics)
{
	const FString savePath = ResolveCurrentScenarioDraftSavePath();
	return SaveProjectScenarioJsonFile(savePath, outResolvedJsonFilePath, outDiagnostics);
}

FString AScenarioEditorController::GetSourceProjectScenarioJsonPath() const
{
	const UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	return authoringSubsystem ? authoringSubsystem->GetSourceProjectScenarioJsonPath() : FString();
}

UScenarioEditorToolbarWidget* AScenarioEditorController::ShowToolbarWidget()
{
	return nullptr;
}

void AScenarioEditorController::RemoveToolbarWidget()
{
}

UScenarioEditorRootWidget* AScenarioEditorController::ShowEditorRootWidget()
{
	if (IsValid(EditorRootWidget))
	{
		if (IsValid(PlatformRootWidget))
		{
			if (!PlatformRootWidget->IsInViewport())
			{
				PlatformRootWidget->AddToViewport(PlatformRootWidgetViewportZOrder);
			}
			HandlePlatformRootScreenChanged(PlatformRootWidget->GetActiveScreen());
			return EditorRootWidget.Get();
		}

		RemoveRouteMarkerOverlayWidget();
		EditorRootWidget = nullptr;
	}

	if (UPlatformRootWidget* platformRootWidget = ShowPlatformRootWidget())
	{
		if (UScenarioEditorRootWidget* rootWidget = platformRootWidget->GetScenarioEditorRootWidget())
		{
			RegisterEditorRootWidget(rootWidget);
			return rootWidget;
		}
	}

	UE_LOG(
		LogScenarioEditorController,
		Error,
		TEXT("WBP_Root did not provide ScenarioEditorRootWidget."));
	return nullptr;
}

void AScenarioEditorController::RemoveEditorRootWidget()
{
	RemoveRouteMarkerOverlayWidget();
	EditorRootWidget = nullptr;
}

UPlatformRootWidget* AScenarioEditorController::ShowPlatformRootWidget()
{
	if (IsValid(PlatformRootWidget))
	{
		if (!PlatformRootWidget->IsInViewport())
		{
			PlatformRootWidget->AddToViewport(PlatformRootWidgetViewportZOrder);
		}
		return PlatformRootWidget.Get();
	}

	TSubclassOf<UPlatformRootWidget> widgetClass = PlatformRootWidgetClass;
	if (!widgetClass)
	{
		const UPlatformUiDeveloperSettings* platformUiSettings = GetDefault<UPlatformUiDeveloperSettings>();
		widgetClass = platformUiSettings
			? platformUiSettings->PlatformRootWidgetClass.LoadSynchronous()
			: nullptr;
	}

	if (!widgetClass)
	{
		UE_LOG(
			LogScenarioEditorController,
			Error,
			TEXT("PlatformRootWidgetClass is not set on the controller or Platform UI project settings."));
		return nullptr;
	}

	PlatformRootWidget = CreateWidget<UPlatformRootWidget>(this, widgetClass);
	if (!PlatformRootWidget)
	{
		UE_LOG(LogScenarioEditorController, Error, TEXT("Failed to create platform root widget."));
		return nullptr;
	}

	PlatformRootWidget->OnActiveScreenChangedNative.RemoveAll(this);
	PlatformRootWidget->OnActiveScreenChangedNative.AddUObject(
		this,
		&AScenarioEditorController::HandlePlatformRootScreenChanged);
	PlatformRootWidget->AddToViewport(PlatformRootWidgetViewportZOrder);
	return PlatformRootWidget.Get();
}

void AScenarioEditorController::RemovePlatformRootWidget()
{
	RemoveEditorRootWidget();
	bPlatformRootScenarioEditorActive = false;

	if (IsValid(PlatformRootWidget))
	{
		PlatformRootWidget->OnActiveScreenChangedNative.RemoveAll(this);
		PlatformRootWidget->RemoveFromParent();
	}

	PlatformRootWidget = nullptr;
}

void AScenarioEditorController::RegisterEditorRootWidget(UScenarioEditorRootWidget* rootWidget)
{
	if (!IsValid(rootWidget))
	{
		return;
	}

	if (IsValid(PlatformRootWidget) && PlatformRootWidget->GetScenarioEditorRootWidget() != rootWidget)
	{
		UE_LOG(
			LogScenarioEditorController,
			Error,
			TEXT("ScenarioEditorRootWidget registration rejected because it is not owned by WBP_Root."));
		return;
	}

	EditorRootWidget = rootWidget;
	if (IsValid(PlatformRootWidget))
	{
		HandlePlatformRootScreenChanged(PlatformRootWidget->GetActiveScreen());
	}
}

void AScenarioEditorController::ClearRegisteredEditorRootWidget(UScenarioEditorRootWidget* rootWidget)
{
	if (!rootWidget || EditorRootWidget.Get() != rootWidget)
	{
		return;
	}

	RemoveRouteMarkerOverlayWidget();
	EditorRootWidget = nullptr;
}

void AScenarioEditorController::HandlePlatformRootScreenChanged(const EPlatformRootScreen screen)
{
	const bool bIsScenarioEditorScreen = screen == EPlatformRootScreen::ScenarioEditor;
	const bool bEnteredScenarioEditorScreen =
		bIsScenarioEditorScreen && !bPlatformRootScenarioEditorActive;
	bPlatformRootScenarioEditorActive = bIsScenarioEditorScreen;

	if (bIsScenarioEditorScreen)
	{
		if (!IsValid(EditorRootWidget))
		{
			ShowEditorRootWidget();
		}
		if (IsValid(EditorRootWidget))
		{
			ShowRouteMarkerOverlayWidget(EditorRootWidget.Get());
		}
		if (bEnteredScenarioEditorScreen)
		{
			RequestFitEditorViewToScenario();
		}
		return;
	}

	RemoveRouteMarkerOverlayWidget();
}

void AScenarioEditorController::ShowRouteMarkerOverlayWidget(UScenarioEditorRootWidget* rootWidget)
{
	if (!IsValid(rootWidget))
	{
		RemoveRouteMarkerOverlayWidget();
		return;
	}

	if (!IsValid(RouteMarkerOverlayWidget))
	{
		RouteMarkerOverlayWidget = CreateWidget<UScenarioEditorRouteMarkerOverlayWidget>(
			this,
			UScenarioEditorRouteMarkerOverlayWidget::StaticClass());
		if (!RouteMarkerOverlayWidget)
		{
			UE_LOG(LogScenarioEditorController, Error, TEXT("Failed to create route marker overlay widget."));
			return;
		}
	}

	RouteMarkerOverlayWidget->ApplyStyleFromRootWidget(rootWidget);
	RouteMarkerOverlayWidget->SetVisibility(ESlateVisibility::HitTestInvisible);

	if (!RouteMarkerOverlayWidget->IsInViewport())
	{
		RouteMarkerOverlayWidget->AddToViewport(
			PlatformRootWidgetViewportZOrder + RouteMarkerOverlayViewportZOrderOffset);
	}
}

void AScenarioEditorController::RemoveRouteMarkerOverlayWidget()
{
	if (IsValid(RouteMarkerOverlayWidget))
	{
		RouteMarkerOverlayWidget->RemoveFromParent();
	}

	RouteMarkerOverlayWidget = nullptr;
}

EScenarioTransformGizmoOrientationMode AScenarioEditorController::GetEffectiveTransformGizmoOrientationMode() const
{
	return GetEffectiveTransformGizmoOrientationModeForPlaceable(SelectedPlaceableComponent.Get());
}

bool AScenarioEditorController::CanEditTransformGizmoOrientationForSelection() const
{
	const UScenarioPlaceableComponent* selectedPlaceable = SelectedPlaceableComponent.Get();
	return IsEditorSelectablePlaceable(selectedPlaceable)
		&& !IsRobotRouteMarkerPlaceable(selectedPlaceable)
		&& !IsCorridorHandlePlaceable(selectedPlaceable);
}

bool AScenarioEditorController::SelectPlaceableByInstanceId(const FString& instanceId)
{
	if (instanceId.IsEmpty())
	{
		ClearSelectedPlaceable();
		return false;
	}

	if (UScenarioPlaceableComponent* selectedPlaceable = SelectedPlaceableComponent.Get();
		IsEditorSelectablePlaceable(selectedPlaceable) && selectedPlaceable->InstanceId == instanceId)
	{
		UpdatePlaceableDetailsForSelection();
		return true;
	}

	UScenarioPlaceableComponent* placeableComponent = FindSelectablePlaceableByInstanceId(instanceId);
	if (!placeableComponent)
	{
		return false;
	}

	SetSelectedPlaceable(placeableComponent);
	return true;
}

void AScenarioEditorController::ClearSelectedPlaceable(const bool bResetTemplateSidebarPanel)
{
	SetSelectedPlaceable(nullptr, bResetTemplateSidebarPanel);
}

void AScenarioEditorController::SetTransformGizmoOrientationMode(
	EScenarioTransformGizmoOrientationMode orientationMode)
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
	UpdatePlaceableDetailsForSelection();
}

bool AScenarioEditorController::TryUpdateSelectedPlaceableTransform(
	const FTransform& transform,
	FString& outFailureReason)
{
	outFailureReason.Reset();

	UScenarioPlaceableComponent* selectedPlaceable = SelectedPlaceableComponent.Get();
	if (!IsEditorSelectablePlaceable(selectedPlaceable) || selectedPlaceable->InstanceId.IsEmpty())
	{
		outFailureReason = TEXT("No selected placeable is editable.");
		return false;
	}

	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		outFailureReason = TEXT("Scenario authoring subsystem is unavailable.");
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
	if (selectedPlaceable->AuthoringRole == EScenarioPlaceableAuthoringRole::CorridorVertexHandle)
	{
		bUpdated = authoringSubsystem->UpdateCorridorVertexHandleTransform(
			selectedPlaceable->InstanceId,
			requestedTransform,
			outFailureReason);
	}
	else if (selectedPlaceable->AuthoringRole == EScenarioPlaceableAuthoringRole::CorridorSegmentHandle)
	{
		bUpdated = authoringSubsystem->UpdateCorridorSegmentHandleTransform(
			selectedPlaceable->InstanceId,
			requestedTransform,
			outFailureReason);
	}
	else if (selectedPlaceable->Category == EScenarioActorCategory::StaticObstacle)
	{
		bUpdated = authoringSubsystem->UpdateStaticObstacleTransform(
			selectedPlaceable->InstanceId,
			requestedTransform,
			outFailureReason);
	}
	else if (selectedPlaceable->Category == EScenarioActorCategory::GroundRegion)
	{
		bUpdated = authoringSubsystem->UpdateGroundRegionTransform(
			selectedPlaceable->InstanceId,
			requestedTransform,
			outFailureReason);
	}
	else if (selectedPlaceable->AuthoringRole == EScenarioPlaceableAuthoringRole::RobotStartMarker)
	{
		bUpdated = authoringSubsystem->UpdateRobotStartPointTransform(requestedTransform, outFailureReason);
	}
	else if (selectedPlaceable->AuthoringRole == EScenarioPlaceableAuthoringRole::RobotGoalMarker)
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
	UpdatePlaceableDetailsForSelection();
	if (UScenarioEditorRootWidget* rootWidget = GetEditorRootWidget())
	{
		rootWidget->RefreshScenarioInspector();
	}
	return true;
}

bool AScenarioEditorController::TryRenameSelectedPlaceableInstanceId(
	const FString& newInstanceId,
	FString& outFailureReason)
{
	outFailureReason.Reset();

	UScenarioPlaceableComponent* selectedPlaceable = SelectedPlaceableComponent.Get();
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

	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		outFailureReason = TEXT("Scenario authoring subsystem is unavailable.");
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
	UpdatePlaceableDetailsForSelection();
	SelectedPlaceableChanged.Broadcast(selectedPlaceable->InstanceId);
	if (UScenarioEditorRootWidget* rootWidget = GetEditorRootWidget())
	{
		rootWidget->RefreshScenarioInspector();
	}
	return true;
}

bool AScenarioEditorController::DeleteSelectedPlaceable(FString& outFailureReason)
{
	outFailureReason.Reset();

	UScenarioPlaceableComponent* selectedPlaceable = SelectedPlaceableComponent.Get();
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

	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		outFailureReason = TEXT("Scenario authoring subsystem is unavailable.");
		return false;
	}

	const FString instanceId = selectedPlaceable->InstanceId;
	const bool bIsGroundRegion = selectedPlaceable->Category == EScenarioActorCategory::GroundRegion;
	selectedPlaceable->SetAuthoringSelected(false);
	const bool bRemoved = bIsGroundRegion
		? authoringSubsystem->RemoveGroundRegion(instanceId, outFailureReason)
		: authoringSubsystem->RemoveStaticObstacle(instanceId, outFailureReason);
	if (!bRemoved)
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
	HidePlaceableDetails();
	SelectedPlaceableChanged.Broadcast(FString());
	if (UScenarioEditorRootWidget* rootWidget = GetEditorRootWidget())
	{
		rootWidget->SetTemplateSidebarPanel(EScenarioTemplateSidebarPanel::Main);
		rootWidget->RefreshScenarioInspector();
	}
	ApplyInputMode();
	return true;
}

void AScenarioEditorController::HandleSelectionStartedInput()
{
	if (EditorMode == EScenarioEditorControllerMode::EditPlacement)
	{
		ConfirmPlacement();
		return;
	}

	if (EditorMode == EScenarioEditorControllerMode::EditRegionDraw)
	{
		if (!IsCursorOverEditorWidgetInputModeFocus())
		{
			BeginRegionDrag();
		}
		return;
	}

	if (EditorMode == EScenarioEditorControllerMode::Observer)
	{
		if (IsCursorOverEditorWidgetInputModeFocus())
		{
			return;
		}

		EScenarioTransformGizmoHandle gizmoHandle = EScenarioTransformGizmoHandle::None;
		FHitResult gizmoHit;
		if (TraceMouseTransformGizmo(gizmoHandle, gizmoHit)
			&& gizmoHandle != EScenarioTransformGizmoHandle::None)
		{
			BeginTransformGizmoDrag(gizmoHandle, gizmoHit);
			return;
		}

		PressedPlaceableComponent = HoveredPlaceableComponent;
	}
}

void AScenarioEditorController::HandleSelectionCompletedInput()
{
	if (EditorMode == EScenarioEditorControllerMode::EditRegionDraw)
	{
		if (bIsRegionDragging)
		{
			FinalizeRegionDrag();
		}
		return;
	}

	if (bIsTransformGizmoDragging)
	{
		EndTransformGizmoDrag();
		return;
	}

	const bool bShouldSelectPressedPlaceable =
		EditorMode == EScenarioEditorControllerMode::Observer
		&& !bIsLookInputHeld;
	UScenarioPlaceableComponent* placeableToSelect = PressedPlaceableComponent.Get();

	PressedPlaceableComponent.Reset();

	if (bShouldSelectPressedPlaceable)
	{
		SetSelectedPlaceable(placeableToSelect);
	}
}

void AScenarioEditorController::HandleCancelPlacementInput()
{
	if (EditorMode == EScenarioEditorControllerMode::Observer)
	{
		if (bIsTransformGizmoDragging)
		{
			EndTransformGizmoDrag();
			return;
		}

		SetSelectedPlaceable(nullptr);
		return;
	}

	if (EditorMode == EScenarioEditorControllerMode::EditRegionDraw)
	{
		SetObserverMode();
		return;
	}

	CancelPlacement();
}

void AScenarioEditorController::HandleTranslateModeInput()
{
	SetTransformGizmoMode(EScenarioTransformGizmoMode::Translate);
}

void AScenarioEditorController::HandleRotateModeInput()
{
	SetTransformGizmoMode(EScenarioTransformGizmoMode::Rotate);
}

void AScenarioEditorController::HandleScaleModeInput()
{
	SetTransformGizmoMode(EScenarioTransformGizmoMode::Scale);
}

void AScenarioEditorController::HandleEditorMoveAction(const FInputActionValue& inputActionValue)
{
	if (EditorMode != EScenarioEditorControllerMode::Observer)
	{
		return;
	}
	if (bIsTransformGizmoDragging)
	{
		return;
	}

	AScenarioEditorPawn* editorPawn = GetEditorPawn();
	if (!editorPawn) return;

	float forwardValue = 0.0f;
	float rightValue = 0.0f;
	float upValue = 0.0f;

	if (!TryResolveEditorKeyboardMoveInput(forwardValue, rightValue, upValue))
	{
		switch (inputActionValue.GetValueType())
		{
		case EInputActionValueType::Axis3D:
		{
			const FVector moveValue = inputActionValue.Get<FVector>();
			forwardValue = moveValue.X;
			rightValue = -moveValue.Y;
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
	}

	if (EditorViewMode == EScenarioEditorViewMode::TopDownOrtho)
	{
		editorPawn->ApplyTopDownPanInput(forwardValue, rightValue);
		editorPawn->ApplyWorldHeightInput(upValue);
		return;
	}

	editorPawn->ApplyMoveInput(forwardValue, rightValue, 0.0f);
	editorPawn->ApplyWorldHeightInput(upValue);
}

bool AScenarioEditorController::TryResolveEditorKeyboardMoveInput(
	float& outForwardValue,
	float& outRightValue,
	float& outUpValue) const
{
	const bool bForwardHeld = IsInputKeyDown(EKeys::W);
	const bool bBackwardHeld = IsInputKeyDown(EKeys::S);
	const bool bRightHeld = IsInputKeyDown(EKeys::D);
	const bool bLeftHeld = IsInputKeyDown(EKeys::A);
	const bool bUpHeld = IsInputKeyDown(EKeys::E);
	const bool bDownHeld = IsInputKeyDown(EKeys::Q);

	if (!bForwardHeld && !bBackwardHeld && !bRightHeld && !bLeftHeld && !bUpHeld && !bDownHeld)
	{
		return false;
	}

	outForwardValue = (bForwardHeld ? 1.0f : 0.0f) - (bBackwardHeld ? 1.0f : 0.0f);
	outRightValue = (bRightHeld ? 1.0f : 0.0f) - (bLeftHeld ? 1.0f : 0.0f);
	outUpValue = (bUpHeld ? 1.0f : 0.0f) - (bDownHeld ? 1.0f : 0.0f);
	return true;
}

void AScenarioEditorController::HandleEditorLookAction(const FInputActionValue& inputActionValue)
{
	if (EditorMode != EScenarioEditorControllerMode::Observer || !bIsLookInputHeld)
	{
		return;
	}

	AScenarioEditorPawn* editorPawn = GetEditorPawn();
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

	// In top-down view, route look input to drag panning instead of rotation.
	if (EditorViewMode == EScenarioEditorViewMode::TopDownOrtho)
	{
		editorPawn->ApplyTopDownDragPanInput(yawValue, pitchValue);
		return;
	}

	editorPawn->ApplyLookInput(
		yawValue * MouseLookSensitivity,
		pitchValue * MouseLookSensitivity);
}

void AScenarioEditorController::HandleLookCaptureStartedInput()
{
	if (EditorMode != EScenarioEditorControllerMode::Observer
		|| bIsTransformGizmoDragging
		|| IsCursorOverEditorWidgetInputModeFocus())
	{
		return;
	}

	PressedPlaceableComponent.Reset();
	BeginLookInputCapture();
}

void AScenarioEditorController::HandleLookCaptureCompletedInput()
{
	EndLookInputCapture();
}

void AScenarioEditorController::HandleViewModeToggleInput()
{
	ToggleEditorViewMode();
}

void AScenarioEditorController::HandleEditorZoomAction(const FInputActionValue& inputActionValue)
{
	if (EditorViewMode != EScenarioEditorViewMode::TopDownOrtho)
	{
		return;
	}

	AScenarioEditorPawn* editorPawn = GetEditorPawn();
	if (!editorPawn) return;

	editorPawn->ApplyTopDownZoomInput(inputActionValue.Get<float>());
}

void AScenarioEditorController::BeginLookInputCapture()
{
	if (bIsLookInputHeld)
	{
		return;
	}

	bIsLookInputHeld = true;
	ApplyInputMode();
}

void AScenarioEditorController::EndLookInputCapture()
{
	if (!bIsLookInputHeld)
	{
		return;
	}

	bIsLookInputHeld = false;
	ApplyInputMode();
}

void AScenarioEditorController::UpdateHoveredPlaceable()
{
	if (bIsLookInputHeld || IsCursorOverEditorWidgetInputModeFocus())
	{
		SetHoveredPlaceable(nullptr);
		return;
	}

	UScenarioPlaceableComponent* hitPlaceableComponent = nullptr;
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

bool AScenarioEditorController::UpdateHoveredTransformGizmo()
{
	if (!IsValid(TransformGizmoActor))
	{
		return false;
	}

	if (bIsTransformGizmoDragging || bIsLookInputHeld || IsCursorOverEditorWidgetInputModeFocus())
	{
		TransformGizmoActor->SetHoveredHandle(EScenarioTransformGizmoHandle::None);
		return false;
	}

	EScenarioTransformGizmoHandle hoveredHandle = EScenarioTransformGizmoHandle::None;
	FHitResult hit;
	const bool bHitGizmo = TraceMouseTransformGizmo(hoveredHandle, hit);
	TransformGizmoActor->SetHoveredHandle(hoveredHandle);
	return bHitGizmo && hoveredHandle != EScenarioTransformGizmoHandle::None;
}

bool AScenarioEditorController::TraceMouseTransformGizmo(
	EScenarioTransformGizmoHandle& outHandle,
	FHitResult& outHit) const
{
	outHandle = EScenarioTransformGizmoHandle::None;

	if (!IsValid(TransformGizmoActor) || TransformGizmoActor->IsHidden())
	{
		return false;
	}

	FVector worldOrigin = FVector::ZeroVector;
	FVector worldDirection = FVector::ForwardVector;
	if (!DeprojectMousePositionToWorld(worldOrigin, worldDirection)) return false;

	const FVector traceEnd = worldOrigin + worldDirection.GetSafeNormal() * PlacementTraceDistanceCm;
	
	FCollisionQueryParams queryParams(SCENE_QUERY_STAT(ScenarioEditorTransformGizmoTrace), true);
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

	const EScenarioTransformGizmoHandle handle =
		TransformGizmoActor->GetHandleForComponent(hit.GetComponent());
	if (handle == EScenarioTransformGizmoHandle::None
		|| !TransformGizmoActor->IsHandleEnabled(handle))
	{
		return false;
	}

	outHit = hit;
	outHandle = handle;
	return true;
}

bool AScenarioEditorController::BeginTransformGizmoDrag(
	EScenarioTransformGizmoHandle handle,
	const FHitResult& hit)
{
	if (handle == EScenarioTransformGizmoHandle::None)
	{
		return false;
	}
	if (!IsValid(TransformGizmoActor) || !TransformGizmoActor->IsHandleEnabled(handle))
	{
		return false;
	}

	UScenarioPlaceableComponent* selectedPlaceable = SelectedPlaceableComponent.Get();
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
	if (handle == EScenarioTransformGizmoHandle::TranslateX || handle == EScenarioTransformGizmoHandle::ScaleX)
	{
		TransformGizmoDragAxis = startXAxis;
	}
	else if (handle == EScenarioTransformGizmoHandle::TranslateY || handle == EScenarioTransformGizmoHandle::ScaleY)
	{
		TransformGizmoDragAxis = startYAxis;
	}
	else if (handle == EScenarioTransformGizmoHandle::TranslateZ || handle == EScenarioTransformGizmoHandle::ScaleZ)
	{
		TransformGizmoDragAxis = startZAxis;
		TransformGizmoDragPlaneNormal = buildCameraFacingAxisPlaneNormal(TransformGizmoDragAxis);
	}
	else if (handle == EScenarioTransformGizmoHandle::TranslateXY || handle == EScenarioTransformGizmoHandle::ScaleXY)
	{
		TransformGizmoDragPlaneNormal = startZAxis;
	}
	else if (handle == EScenarioTransformGizmoHandle::TranslateXZ || handle == EScenarioTransformGizmoHandle::ScaleXZ)
	{
		TransformGizmoDragPlaneNormal = startYAxis;
	}
	else if (handle == EScenarioTransformGizmoHandle::TranslateYZ || handle == EScenarioTransformGizmoHandle::ScaleYZ)
	{
		TransformGizmoDragPlaneNormal = startXAxis;
	}
	else if (handle == EScenarioTransformGizmoHandle::RotateX)
	{
		TransformGizmoDragAxis = startXAxis;
		TransformGizmoDragPlaneNormal = startXAxis;
	}
	else if (handle == EScenarioTransformGizmoHandle::RotateY)
	{
		TransformGizmoDragAxis = startYAxis;
		TransformGizmoDragPlaneNormal = startYAxis;
	}
	else if (handle == EScenarioTransformGizmoHandle::RotateZ)
	{
		TransformGizmoDragAxis = startZAxis;
		TransformGizmoDragPlaneNormal = startZAxis;
	}

	if ((handle == EScenarioTransformGizmoHandle::TranslateX || handle == EScenarioTransformGizmoHandle::TranslateY)
		&& !TransformGizmoDragAxis.IsNearlyZero())
	{
		TransformGizmoDragAxis.Z = 0.0;
		if (!TransformGizmoDragAxis.Normalize())
		{
			TransformGizmoDragAxis = handle == EScenarioTransformGizmoHandle::TranslateX
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

	if (handle == EScenarioTransformGizmoHandle::RotateX
		|| handle == EScenarioTransformGizmoHandle::RotateY
		|| handle == EScenarioTransformGizmoHandle::RotateZ)
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

void AScenarioEditorController::UpdateTransformGizmoDrag()
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

void AScenarioEditorController::EndTransformGizmoDrag()
{
	if (!bIsTransformGizmoDragging)
	{
		return;
	}

	ResetTransformGizmoDrag();
	UpdateTransformGizmoForSelection();
	if (UScenarioEditorRootWidget* rootWidget = GetEditorRootWidget())
	{
		rootWidget->RefreshScenarioInspector();
	}
	ApplyInputMode();
}

void AScenarioEditorController::ResetTransformGizmoDrag()
{
	bIsTransformGizmoDragging = false;
	ActiveTransformGizmoHandle = EScenarioTransformGizmoHandle::None;
	ActiveTransformGizmoInstanceId.Reset();
	TransformGizmoDragStartTransform = FTransform::Identity;
	TransformGizmoDragStartPoint = FVector::ZeroVector;
	TransformGizmoDragPlaneNormal = FVector::UpVector;
	TransformGizmoDragAxis = FVector::ForwardVector;
	TransformGizmoDragStartDirection = FVector::ForwardVector;
	ActiveTransformGizmoOrientationMode = EScenarioTransformGizmoOrientationMode::Relative;
	LastTransformGizmoDragFailureReason.Reset();
	DraggedPlaceableComponent.Reset();

	if (IsValid(TransformGizmoActor))
	{
		TransformGizmoActor->SetHoveredHandle(EScenarioTransformGizmoHandle::None);
	}
}

bool AScenarioEditorController::BuildTransformGizmoDragTransform(FTransform& outTransform) const
{
	if (!bIsTransformGizmoDragging || ActiveTransformGizmoHandle == EScenarioTransformGizmoHandle::None)
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
	case EScenarioTransformGizmoHandle::TranslateX:
	case EScenarioTransformGizmoHandle::TranslateY:
	case EScenarioTransformGizmoHandle::TranslateZ:
	{
		const double axisDistance = FVector::DotProduct(rawDelta, TransformGizmoDragAxis);
		outTransform.SetLocation(startLocation + TransformGizmoDragAxis * axisDistance);
		return true;
	}
	case EScenarioTransformGizmoHandle::TranslateXY:
	case EScenarioTransformGizmoHandle::TranslateXZ:
	case EScenarioTransformGizmoHandle::TranslateYZ:
	{
		outTransform.SetLocation(startLocation + planeDelta);
		return true;
	}
	case EScenarioTransformGizmoHandle::RotateX:
	case EScenarioTransformGizmoHandle::RotateY:
	case EScenarioTransformGizmoHandle::RotateZ:
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
	case EScenarioTransformGizmoHandle::ScaleX:
	case EScenarioTransformGizmoHandle::ScaleY:
	case EScenarioTransformGizmoHandle::ScaleZ:
	{
		FVector scale = TransformGizmoDragStartTransform.GetScale3D();
		const double scaleFactor = makeScaleFactor(FVector::DotProduct(rawDelta, TransformGizmoDragAxis));
		if (ActiveTransformGizmoHandle == EScenarioTransformGizmoHandle::ScaleX)
		{
			scale.X *= scaleFactor;
		}
		else if (ActiveTransformGizmoHandle == EScenarioTransformGizmoHandle::ScaleY)
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
	case EScenarioTransformGizmoHandle::ScaleXY:
	case EScenarioTransformGizmoHandle::ScaleXZ:
	case EScenarioTransformGizmoHandle::ScaleYZ:
	{
		FVector scale = TransformGizmoDragStartTransform.GetScale3D();
		if (ActiveTransformGizmoHandle == EScenarioTransformGizmoHandle::ScaleXY)
		{
			scale.X *= makeScaleFactor(FVector::DotProduct(planeDelta, startXAxis));
			scale.Y *= makeScaleFactor(FVector::DotProduct(planeDelta, startYAxis));
		}
		else if (ActiveTransformGizmoHandle == EScenarioTransformGizmoHandle::ScaleXZ)
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
	case EScenarioTransformGizmoHandle::ScaleUniform:
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

bool AScenarioEditorController::ApplyTransformGizmoDragTransform(const FTransform& transform)
{
	if (ActiveTransformGizmoInstanceId.IsEmpty())
	{
		return false;
	}
	UScenarioPlaceableComponent* draggedPlaceable = DraggedPlaceableComponent.Get();
	if (!IsEditorSelectablePlaceable(draggedPlaceable))
	{
		return false;
	}

	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
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
	else if (draggedPlaceable->AuthoringRole == EScenarioPlaceableAuthoringRole::CorridorVertexHandle)
	{
		bUpdated = authoringSubsystem->UpdateCorridorVertexHandleTransform(
			ActiveTransformGizmoInstanceId,
			requestedTransform,
			failureReason);
	}
	else if (draggedPlaceable->AuthoringRole == EScenarioPlaceableAuthoringRole::CorridorSegmentHandle)
	{
		bUpdated = authoringSubsystem->UpdateCorridorSegmentHandleTransform(
			ActiveTransformGizmoInstanceId,
			requestedTransform,
			failureReason);
	}
	else if (draggedPlaceable->Category == EScenarioActorCategory::StaticObstacle)
	{
		bUpdated = authoringSubsystem->UpdateStaticObstacleTransform(
			ActiveTransformGizmoInstanceId,
			requestedTransform,
			failureReason);
	}
	else if (draggedPlaceable->Category == EScenarioActorCategory::GroundRegion)
	{
		bUpdated = authoringSubsystem->UpdateGroundRegionTransform(
			ActiveTransformGizmoInstanceId,
			requestedTransform,
			failureReason);
	}
	else if (draggedPlaceable->AuthoringRole == EScenarioPlaceableAuthoringRole::RobotStartMarker)
	{
		bUpdated = authoringSubsystem->UpdateRobotStartPointTransform(requestedTransform, failureReason);
	}
	else if (draggedPlaceable->AuthoringRole == EScenarioPlaceableAuthoringRole::RobotGoalMarker)
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
		UpdatePlaceableDetailsForSelection();
		return true;
	}

	if (LastTransformGizmoDragFailureReason != failureReason)
	{
		LastTransformGizmoDragFailureReason = failureReason;
		UE_LOG(
			LogScenarioEditorController,
			Verbose,
			TEXT("Transform gizmo drag rejected | InstanceId: %s, Reason: %s"),
			*ActiveTransformGizmoInstanceId,
			*failureReason);
	}

	return false;
}

bool AScenarioEditorController::TraceMouseToPlane(
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

bool AScenarioEditorController::TraceMouseRobotRouteMarkerOverlay(
	UScenarioPlaceableComponent*& outPlaceableComponent,
	FHitResult& outHit) const
{
	outPlaceableComponent = nullptr;
	outHit = FHitResult();

	float rawMouseX = 0.0f;
	float rawMouseY = 0.0f;
	if (!GetMousePosition(rawMouseX, rawMouseY))
	{
		return false;
	}

	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		return false;
	}

	TArray<FScenarioEditorRouteMarkerOverlayItem> items;
	authoringSubsystem->GetRobotRouteMarkerOverlayItems(items);
	if (items.IsEmpty())
	{
		return false;
	}

	const FVector2D mousePosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(this);
	double bestDistanceSquared = MAX_dbl;
	const FScenarioEditorRouteMarkerOverlayItem* bestItem = nullptr;
	FVector2D bestMarkerPosition = FVector2D::ZeroVector;
	for (const FScenarioEditorRouteMarkerOverlayItem& item : items)
	{
		FVector2D markerPosition = FVector2D::ZeroVector;
		if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
			const_cast<AScenarioEditorController*>(this),
			item.WorldLocation,
			markerPosition,
			true))
		{
			continue;
		}

		if (!IsScreenPointInsideRobotRouteMarkerOverlay(mousePosition, markerPosition))
		{
			continue;
		}

		const double distanceSquared = (mousePosition - markerPosition).SizeSquared();
		if (distanceSquared < bestDistanceSquared)
		{
			bestDistanceSquared = distanceSquared;
			bestItem = &item;
			bestMarkerPosition = markerPosition;
		}
	}

	if (!bestItem)
	{
		return false;
	}

	UScenarioPlaceableComponent* placeableComponent =
		authoringSubsystem->GetRobotRouteMarkerPlaceableComponent(bestItem->Kind);
	if (!IsEditorSelectablePlaceable(placeableComponent)
		|| !authoringSubsystem->SyncRobotRouteMarkerProxyLocation(bestItem->Kind, bestItem->WorldLocation))
	{
		return false;
	}

	outPlaceableComponent = placeableComponent;
	outHit.Location = bestItem->WorldLocation;
	outHit.ImpactPoint = bestItem->WorldLocation;
	outHit.Distance = FVector2D::Distance(mousePosition, bestMarkerPosition);
	return true;
}

bool AScenarioEditorController::TraceMouseCorridorHandleOverlay(
	UScenarioPlaceableComponent*& outPlaceableComponent,
	FHitResult& outHit) const
{
	outPlaceableComponent = nullptr;
	outHit = FHitResult();

	if (!ShouldShowCorridorHandleOverlay())
	{
		return false;
	}

	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		return false;
	}

	TArray<FScenarioEditorCorridorHandleOverlayItem> items;
	authoringSubsystem->GetCorridorHandleOverlayItems(items);
	if (items.IsEmpty())
	{
		return false;
	}

	const FVector2D mousePosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(this);
	const FScenarioEditorCorridorHandleOverlayItem* bestItem = nullptr;
	double bestDistanceSquared = MAX_dbl;
	for (const FScenarioEditorCorridorHandleOverlayItem& item : items)
	{
		if (item.HandleType != EScenarioCorridorHandleType::Vertex)
		{
			continue;
		}

		FVector2D gripPosition = FVector2D::ZeroVector;
		if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
			const_cast<AScenarioEditorController*>(this),
			item.WorldLocation,
			gripPosition,
			true)
			|| !IsScreenPointInsideCorridorVertexHandleOverlay(mousePosition, gripPosition))
		{
			continue;
		}

		const double distanceSquared = (mousePosition - gripPosition).SizeSquared();
		if (distanceSquared < bestDistanceSquared)
		{
			bestDistanceSquared = distanceSquared;
			bestItem = &item;
		}
	}

	for (const FScenarioEditorCorridorHandleOverlayItem& item : items)
	{
		if (item.HandleType != EScenarioCorridorHandleType::Segment)
		{
			continue;
		}

		FVector2D startPosition = FVector2D::ZeroVector;
		FVector2D endPosition = FVector2D::ZeroVector;
		FVector2D gripPosition = FVector2D::ZeroVector;
		if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
			const_cast<AScenarioEditorController*>(this),
			item.SegmentStartWorldLocation,
			startPosition,
			true)
			|| !UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
				const_cast<AScenarioEditorController*>(this),
				item.SegmentEndWorldLocation,
				endPosition,
				true)
			|| !UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
				const_cast<AScenarioEditorController*>(this),
				item.WorldLocation,
				gripPosition,
				true))
		{
			continue;
		}

		double distanceSquared = MAX_dbl;
		if (!IsScreenPointInsideCorridorSegmentHandleOverlay(
			mousePosition,
			startPosition,
			endPosition,
			gripPosition,
			distanceSquared))
		{
			continue;
		}

		if (bestItem && bestItem->HandleType == EScenarioCorridorHandleType::Vertex)
		{
			continue;
		}
		if (distanceSquared < bestDistanceSquared)
		{
			bestDistanceSquared = distanceSquared;
			bestItem = &item;
		}
	}

	if (!bestItem
		|| !authoringSubsystem->SyncCorridorHandleProxyTransform(bestItem->InstanceId, bestItem->WorldTransform))
	{
		return false;
	}

	UScenarioPlaceableComponent* placeableComponent =
		authoringSubsystem->GetCorridorHandlePlaceableComponent(bestItem->InstanceId);
	if (!IsEditorSelectablePlaceable(placeableComponent))
	{
		return false;
	}

	outPlaceableComponent = placeableComponent;
	outHit.Location = bestItem->WorldLocation;
	outHit.ImpactPoint = bestItem->WorldLocation;
	outHit.Distance = FMath::Sqrt(bestDistanceSquared);
	return true;
}

bool AScenarioEditorController::IsScreenPointInsideRobotRouteMarkerOverlay(
	const FVector2D& screenPoint,
	const FVector2D& markerAnchorPoint) const
{
	const FVector2D baseSize(
		FMath::Max(1.0, RobotRouteMarkerOverlayHitSize.X),
		FMath::Max(1.0, RobotRouteMarkerOverlayHitSize.Y));
	const double padding = FMath::Max(0.0, RobotRouteMarkerOverlayHitPaddingPixels);
	const FVector2D markerSize = baseSize + FVector2D(padding * 2.0, padding * 2.0);
	const FVector2D safeAnchor(
		FMath::Clamp(RobotRouteMarkerOverlayHitAnchor.X, 0.0, 1.0),
		FMath::Clamp(RobotRouteMarkerOverlayHitAnchor.Y, 0.0, 1.0));
	const FVector2D topLeft = markerAnchorPoint - baseSize * safeAnchor - FVector2D(padding, padding);
	const FVector2D localPoint = screenPoint - topLeft;
	if (localPoint.X < 0.0
		|| localPoint.Y < 0.0
		|| localPoint.X > markerSize.X
		|| localPoint.Y > markerSize.Y)
	{
		return false;
	}

	const FVector2D circleCenter(markerSize.X * 0.5, markerSize.Y * 0.31);
	const double circleRadius = markerSize.X * 0.39;
	if ((localPoint - circleCenter).SizeSquared() <= FMath::Square(circleRadius))
	{
		return true;
	}

	const FVector2D leftBase(markerSize.X * 0.18, markerSize.Y * 0.43);
	const FVector2D rightBase(markerSize.X * 0.82, markerSize.Y * 0.43);
	const FVector2D tip(markerSize.X * 0.5, markerSize.Y * 0.98);
	return IsPointInsideTriangle2D(localPoint, leftBase, rightBase, tip);
}

bool AScenarioEditorController::IsScreenPointInsideCorridorVertexHandleOverlay(
	const FVector2D& screenPoint,
	const FVector2D& gripCenterPoint) const
{
	const FVector2D safeHitSize(
		FMath::Max(1.0, CorridorVertexHandleOverlayHitSize.X),
		FMath::Max(1.0, CorridorVertexHandleOverlayHitSize.Y));
	const double hitRadius = FMath::Max(safeHitSize.X, safeHitSize.Y) * 0.5;
	return (screenPoint - gripCenterPoint).SizeSquared() <= FMath::Square(hitRadius);
}

bool AScenarioEditorController::IsScreenPointInsideCorridorSegmentHandleOverlay(
	const FVector2D& screenPoint,
	const FVector2D& segmentStartPoint,
	const FVector2D& segmentEndPoint,
	const FVector2D& gripCenterPoint,
	double& outDistanceSquared) const
{
	const FVector2D safeGripHitSize(
		FMath::Max(1.0, CorridorSegmentHandleGripHitSize.X),
		FMath::Max(1.0, CorridorSegmentHandleGripHitSize.Y));
	const FVector2D gripHalfSize = safeGripHitSize * 0.5;
	const FVector2D gripLocalPoint = screenPoint - gripCenterPoint;
	if (FMath::Abs(gripLocalPoint.X) <= gripHalfSize.X
		&& FMath::Abs(gripLocalPoint.Y) <= gripHalfSize.Y)
	{
		outDistanceSquared = gripLocalPoint.SizeSquared();
		return true;
	}

	outDistanceSquared = GetPointToSegmentDistanceSquared2D(screenPoint, segmentStartPoint, segmentEndPoint);
	return outDistanceSquared <= FMath::Square(FMath::Max(0.0, CorridorSegmentHandleLineHitHalfThicknessPixels));
}

bool AScenarioEditorController::TraceMouseSelectablePlaceable(
	UScenarioPlaceableComponent*& outPlaceableComponent,
	FHitResult& outHit) const
{
	outPlaceableComponent = nullptr;
	if (TraceMouseCorridorHandleOverlay(outPlaceableComponent, outHit))
	{
		return true;
	}
	if (TraceMouseRobotRouteMarkerOverlay(outPlaceableComponent, outHit))
	{
		return true;
	}

	FVector worldOrigin = FVector::ZeroVector;
	FVector worldDirection = FVector::ForwardVector;
	if (!DeprojectMousePositionToWorld(worldOrigin, worldDirection)) return false;

	UWorld* world = GetWorld();
	if (!world) return false;

	FCollisionQueryParams queryParams(SCENE_QUERY_STAT(ScenarioEditorSelectableTrace), true);
	queryParams.bReturnPhysicalMaterial = false;
	if (PlacementPreviewActor)
	{
		queryParams.AddIgnoredActor(PlacementPreviewActor);
	}
	if (const APawn* pawn = GetPawn())
	{
		queryParams.AddIgnoredActor(pawn);
	}
	if (IsValid(TransformGizmoActor))
	{
		queryParams.AddIgnoredActor(TransformGizmoActor);
	}

	const FVector traceEnd = worldOrigin + worldDirection.GetSafeNormal() * PlacementTraceDistanceCm;
	TArray<FHitResult> hits;
	if (!world->LineTraceMultiByChannel(
		hits,
		worldOrigin,
		traceEnd,
		PlacementTraceChannel,
		queryParams))
	{
		return false;
	}

	for (const FHitResult& hit : hits)
	{
		AActor* hitActor = hit.GetActor();
		if (!hitActor)
		{
			continue;
		}

		UScenarioPlaceableComponent* placeableComponent = hitActor->FindComponentByClass<UScenarioPlaceableComponent>();
		if (!IsEditorSelectablePlaceable(placeableComponent))
		{
			continue;
		}

		outHit = hit;
		outPlaceableComponent = placeableComponent;
		return true;
	}

	return false;
}

bool AScenarioEditorController::IsEditorSelectablePlaceable(const UScenarioPlaceableComponent* placeableComponent) const
{
	return placeableComponent
		&& placeableComponent->bAuthoringSelectable
		&& (placeableComponent->Category == EScenarioActorCategory::StaticObstacle
			|| placeableComponent->Category == EScenarioActorCategory::GroundRegion
			|| IsCorridorHandlePlaceable(placeableComponent)
			|| IsRobotRouteMarkerPlaceable(placeableComponent));
}

UScenarioPlaceableComponent* AScenarioEditorController::FindSelectablePlaceableByInstanceId(
	const FString& instanceId) const
{
	if (instanceId.IsEmpty())
	{
		return nullptr;
	}

	UWorld* world = GetWorld();
	if (!world)
	{
		return nullptr;
	}

	if (UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem())
	{
		TArray<FScenarioEditorRouteMarkerOverlayItem> routeMarkerItems;
		authoringSubsystem->GetRobotRouteMarkerOverlayItems(routeMarkerItems);
		for (const FScenarioEditorRouteMarkerOverlayItem& item : routeMarkerItems)
		{
			if (item.InstanceId != instanceId)
			{
				continue;
			}

			UScenarioPlaceableComponent* routeMarkerComponent =
				authoringSubsystem->GetRobotRouteMarkerPlaceableComponent(item.Kind);
			if (IsEditorSelectablePlaceable(routeMarkerComponent)
				&& authoringSubsystem->SyncRobotRouteMarkerProxyLocation(item.Kind, item.WorldLocation))
			{
				return routeMarkerComponent;
			}
		}
	}

	if (UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem())
	{
		TArray<FScenarioEditorCorridorHandleOverlayItem> corridorHandleItems;
		authoringSubsystem->GetCorridorHandleOverlayItems(corridorHandleItems);
		for (const FScenarioEditorCorridorHandleOverlayItem& item : corridorHandleItems)
		{
			if (item.InstanceId != instanceId)
			{
				continue;
			}

			UScenarioPlaceableComponent* corridorHandleComponent =
				authoringSubsystem->GetCorridorHandlePlaceableComponent(item.InstanceId);
			if (IsEditorSelectablePlaceable(corridorHandleComponent)
				&& authoringSubsystem->SyncCorridorHandleProxyTransform(item.InstanceId, item.WorldTransform))
			{
				return corridorHandleComponent;
			}
		}
	}

	for (TActorIterator<AActor> actorIt(world); actorIt; ++actorIt)
	{
		AActor* actor = *actorIt;
		UScenarioPlaceableComponent* placeableComponent =
			actor ? actor->FindComponentByClass<UScenarioPlaceableComponent>() : nullptr;
		if (IsEditorSelectablePlaceable(placeableComponent)
			&& placeableComponent->InstanceId == instanceId)
		{
			return placeableComponent;
		}
	}

	return nullptr;
}

bool AScenarioEditorController::ShouldShowCorridorHandleOverlay() const
{
	if (IsCorridorHandlePlaceable(SelectedPlaceableComponent.Get())
		|| IsCorridorHandlePlaceable(HoveredPlaceableComponent.Get())
		|| IsCorridorHandlePlaceable(PressedPlaceableComponent.Get())
		|| IsCorridorHandlePlaceable(DraggedPlaceableComponent.Get()))
	{
		return true;
	}

	const UScenarioEditorRootWidget* rootWidget = GetEditorRootWidget();
	const UScenarioEditorShellViewModel* shellViewModel =
		rootWidget ? rootWidget->GetShellViewModel() : nullptr;
	return shellViewModel
		&& shellViewModel->GetActiveSidebarPanel() == EScenarioTemplateSidebarPanel::Corridor;
}

FString AScenarioEditorController::ResolveCurrentScenarioDraftSavePath() const
{
	const FString sourcePath = GetSourceProjectScenarioJsonPath();
	if (!sourcePath.IsEmpty())
	{
		return sourcePath;
	}

	return MakeUniqueScenarioSavePath(DefaultScenarioDraftSavePath);
}

bool AScenarioEditorController::IsCursorOverEditorWidgetInputModeFocus() const
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

void AScenarioEditorController::SetHoveredPlaceable(UScenarioPlaceableComponent* placeableComponent)
{
	if (HoveredPlaceableComponent.Get() == placeableComponent)
	{
		return;
	}

	if (UScenarioPlaceableComponent* previousHoveredPlaceable = HoveredPlaceableComponent.Get())
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

void AScenarioEditorController::SetSelectedPlaceable(
	UScenarioPlaceableComponent* placeableComponent,
	const bool bResetTemplateSidebarPanel)
{
	if (SelectedPlaceableComponent.Get() == placeableComponent)
	{
		return;
	}

	if (UScenarioPlaceableComponent* previousSelectedPlaceable = SelectedPlaceableComponent.Get())
	{
		previousSelectedPlaceable->SetAuthoringSelected(false);
	}

	SelectedPlaceableComponent = placeableComponent;
	if (placeableComponent)
	{
		placeableComponent->SetAuthoringSelected(true);
	}

	UpdateTransformGizmoForSelection();
	UpdatePlaceableDetailsForSelection();
	SelectedPlaceableChanged.Broadcast(placeableComponent ? placeableComponent->InstanceId : FString());
	if (!placeableComponent && bResetTemplateSidebarPanel)
	{
		if (UScenarioEditorRootWidget* rootWidget = GetEditorRootWidget())
		{
			rootWidget->SetTemplateSidebarPanel(EScenarioTemplateSidebarPanel::Main);
			rootWidget->RefreshScenarioInspector();
		}
	}
}

void AScenarioEditorController::ApplyAuthoringOutlinePostProcessMaterial(
	const UScenarioPlaceableComponent* placeableComponent)
{
	if (!placeableComponent)
	{
		return;
	}

	UMaterialInterface* outlinePostProcessMaterial =
		FScenarioViewportPresentation::ResolveOrLoadMaterial(placeableComponent->AuthoringHoverOutlineMaterial);
	if (!outlinePostProcessMaterial || ActiveAuthoringOutlinePostProcessMaterial.Get() == outlinePostProcessMaterial)
	{
		return;
	}

	AScenarioEditorPawn* editorPawn = GetEditorPawn();
	if (!editorPawn || !editorPawn->CameraComponent)
	{
		return;
	}

	editorPawn->CameraComponent->PostProcessBlendWeight = 1.0f;
	editorPawn->CameraComponent->PostProcessSettings.AddBlendable(outlinePostProcessMaterial, 1.0f);
	ActiveAuthoringOutlinePostProcessMaterial = outlinePostProcessMaterial;
}

void AScenarioEditorController::EnsureAuthoringOutlineCustomDepthEnabled() const
{
	IConsoleVariable* customDepthCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.CustomDepth"));
	if (customDepthCvar && customDepthCvar->GetInt() < 3)
	{
		customDepthCvar->Set(3, ECVF_SetByCode);
	}
}

void AScenarioEditorController::AddEditorInputMappingContext()
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

void AScenarioEditorController::BindEditorInputActions()
{
	UEnhancedInputComponent* enhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!enhancedInputComponent) return;

	if (UInputAction* moveAction = EditorMoveAction.LoadSynchronous())
	{
		enhancedInputComponent->BindAction(
			moveAction,
			ETriggerEvent::Triggered,
			this,
			&AScenarioEditorController::HandleEditorMoveAction);
	}

	if (UInputAction* lookAction = EditorLookAction.LoadSynchronous())
	{
		enhancedInputComponent->BindAction(
			lookAction,
			ETriggerEvent::Triggered,
			this,
			&AScenarioEditorController::HandleEditorLookAction);
	}

	if (UInputAction* lookCaptureAction = EditorLookCaptureAction.LoadSynchronous())
	{
		enhancedInputComponent->BindAction(
			lookCaptureAction,
			ETriggerEvent::Started,
			this,
			&AScenarioEditorController::HandleLookCaptureStartedInput);

		enhancedInputComponent->BindAction(
			lookCaptureAction,
			ETriggerEvent::Completed,
			this,
			&AScenarioEditorController::HandleLookCaptureCompletedInput);

		enhancedInputComponent->BindAction(
			lookCaptureAction,
			ETriggerEvent::Canceled,
			this,
			&AScenarioEditorController::HandleLookCaptureCompletedInput);
	}

	if (UInputAction* selectionAction = EditorSelectionAction.LoadSynchronous())
	{
		enhancedInputComponent->BindAction(
			selectionAction,
			ETriggerEvent::Started,
			this,
			&AScenarioEditorController::HandleSelectionStartedInput);

		enhancedInputComponent->BindAction(
			selectionAction,
			ETriggerEvent::Completed,
			this,
			&AScenarioEditorController::HandleSelectionCompletedInput);

		enhancedInputComponent->BindAction(
			selectionAction,
			ETriggerEvent::Canceled,
			this,
			&AScenarioEditorController::HandleSelectionCompletedInput);
	}

	if (UInputAction* deselectionAction = EditorDeselectionAction.LoadSynchronous())
	{
		enhancedInputComponent->BindAction(
			deselectionAction,
			ETriggerEvent::Started,
			this,
			&AScenarioEditorController::HandleCancelPlacementInput);
	}

	if (UInputAction* translateAction = EditorTranslateAction.LoadSynchronous())
	{
		enhancedInputComponent->BindAction(
			translateAction,
			ETriggerEvent::Started,
			this,
			&AScenarioEditorController::HandleTranslateModeInput);
	}

	if (UInputAction* rotateAction = EditorRotateAction.LoadSynchronous())
	{
		enhancedInputComponent->BindAction(
			rotateAction,
			ETriggerEvent::Started,
			this,
			&AScenarioEditorController::HandleRotateModeInput);
	}

	if (UInputAction* scaleAction = EditorScaleAction.LoadSynchronous())
	{
		enhancedInputComponent->BindAction(
			scaleAction,
			ETriggerEvent::Started,
			this,
			&AScenarioEditorController::HandleScaleModeInput);
	}

	if (UInputAction* viewModeToggleAction = EditorViewModeToggleAction.LoadSynchronous())
	{
		enhancedInputComponent->BindAction(
			viewModeToggleAction,
			ETriggerEvent::Started,
			this,
			&AScenarioEditorController::HandleViewModeToggleInput);
	}

	if (UInputAction* zoomAction = EditorZoomAction.LoadSynchronous())
	{
		enhancedInputComponent->BindAction(
			zoomAction,
			ETriggerEvent::Triggered,
			this,
			&AScenarioEditorController::HandleEditorZoomAction);
	}
}

void AScenarioEditorController::UpdatePlacementPreview()
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

	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if ((SelectedPlacementItemType == EScenarioPaletteItemType::StaticObstacle
			|| SelectedPlacementItemType == EScenarioPaletteItemType::Pedestrian
			|| SelectedPlacementItemType == EScenarioPaletteItemType::RobotStart
			|| SelectedPlacementItemType == EScenarioPaletteItemType::RobotGoal)
		&& authoringSubsystem)
	{
		CurrentPlacementTransform =
			authoringSubsystem->ResolveEditorGroundActorPlacementTransform(CurrentPlacementTransform);
	}
	if (!authoringSubsystem)
	{
		bCurrentPlacementValid = false;
		CurrentPlacementFailureReason = TEXT("Scenario authoring subsystem is unavailable.");
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

bool AScenarioEditorController::ConfigurePlacementPreviewForSelectedItem(
	UScenarioAuthoringSubsystem* authoringSubsystem)
{
	if (!PlacementPreviewActor || !authoringSubsystem)
	{
		CurrentPlacementFailureReason = TEXT("Scenario authoring subsystem is unavailable.");
		return false;
	}

	PlacementPreviewActor->ClearPreview();

	switch (SelectedPlacementItemType)
	{
	case EScenarioPaletteItemType::StaticObstacle:
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

		FScenarioStaticObstaclePropEntry propEntry;
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
	case EScenarioPaletteItemType::Pedestrian:
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
	case EScenarioPaletteItemType::RobotStart:
		PlacementPreviewActor->SetActorHiddenInGame(true);
		return true;
	case EScenarioPaletteItemType::RobotGoal:
		PlacementPreviewActor->SetActorHiddenInGame(true);
		return true;
	default:
		CurrentPlacementFailureReason = TEXT("Unknown palette placement item type.");
		return false;
	}
}

bool AScenarioEditorController::ValidatePlacementForSelectedItem(
	const UScenarioAuthoringSubsystem* authoringSubsystem,
	FString& outFailureReason) const
{
	outFailureReason.Reset();
	if (!authoringSubsystem)
	{
		outFailureReason = TEXT("Scenario authoring subsystem is unavailable.");
		return false;
	}

	switch (SelectedPlacementItemType)
	{
	case EScenarioPaletteItemType::StaticObstacle:
		return authoringSubsystem->CanPlaceStaticObstacle(
			SelectedStaticObstaclePropId,
			CurrentPlacementTransform,
			outFailureReason);
	case EScenarioPaletteItemType::Pedestrian:
	case EScenarioPaletteItemType::RobotStart:
		return authoringSubsystem->CanPlaceEditorGroundActor(
			CurrentPlacementTransform,
			outFailureReason);
	case EScenarioPaletteItemType::RobotGoal:
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

bool AScenarioEditorController::HasAuthoredRobotStart(
	const UScenarioAuthoringSubsystem* authoringSubsystem) const
{
	if (!authoringSubsystem)
	{
		return false;
	}

	const FScenarioWorldSpec draftWorldSpec = authoringSubsystem->GetDraftWorldSpec();
	for (const FScenarioPlaceableInstanceSpec& spec : draftWorldSpec.Placeables)
	{
		if ((spec.Category == EScenarioActorCategory::DeliveryBot)
			&& spec.DeliveryBot.bHasStartLocation)
		{
			return true;
		}
	}

	return false;
}

bool AScenarioEditorController::ShouldUseSchemaPlacementPlane() const
{
	switch (SelectedPlacementItemType)
	{
	case EScenarioPaletteItemType::StaticObstacle:
	case EScenarioPaletteItemType::RobotStart:
	case EScenarioPaletteItemType::RobotGoal:
		return true;
	default:
		return false;
	}
}

bool AScenarioEditorController::TraceMousePlacement(FHitResult& outHit) const
{
	outHit = FHitResult();
	if (ShouldUseSchemaPlacementPlane())
	{
		FVector planePoint = FVector::ZeroVector;
		if (!TraceMouseToPlane(FVector::ZeroVector, FVector::UpVector, planePoint))
		{
			return false;
		}

		outHit.TraceStart = planePoint;
		outHit.TraceEnd = planePoint;
		outHit.Location = planePoint;
		outHit.ImpactPoint = planePoint;
		outHit.ImpactNormal = FVector::UpVector;
		outHit.Normal = FVector::UpVector;
		return true;
	}

	FVector worldOrigin = FVector::ZeroVector;
	FVector worldDirection = FVector::ForwardVector;
	if (!DeprojectMousePositionToWorld(worldOrigin, worldDirection)) return false;

	UWorld* world = GetWorld();
	if (!world) return false;

	FCollisionQueryParams queryParams(SCENE_QUERY_STAT(ScenarioEditorPlacementTrace), true);
	queryParams.bReturnPhysicalMaterial = false;
	if (PlacementPreviewActor)
	{
		queryParams.AddIgnoredActor(PlacementPreviewActor);
	}
	if (const APawn* pawn = GetPawn())
	{
		queryParams.AddIgnoredActor(pawn);
	}
	if (UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem())
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

FTransform AScenarioEditorController::BuildPlacementTransform(const FVector& location) const
{
	FVector snappedLocation = SnapLocationIfNeeded(location);
	if (FMath::Abs(snappedLocation.Z) <= PlacementGroundSnapToleranceCm)
	{
		snappedLocation.Z = 0.0;
	}

	return FTransform(FRotator::ZeroRotator, snappedLocation, FVector::OneVector);
}

FVector AScenarioEditorController::SnapLocationIfNeeded(const FVector& location) const
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

void AScenarioEditorController::ApplyInputMode()
{
	if (EditorMode == EScenarioEditorControllerMode::Observer && bIsTransformGizmoDragging)
	{
		bShowMouseCursor = true;
		bEnableClickEvents = true;
		bEnableMouseOverEvents = true;

		FInputModeGameAndUI inputMode;
		inputMode.SetHideCursorDuringCapture(false);
		inputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(inputMode);
	}
	else if (EditorMode == EScenarioEditorControllerMode::Observer && bIsLookInputHeld)
	{
		bShowMouseCursor = false;
		bEnableClickEvents = false;
		bEnableMouseOverEvents = false;

		FInputModeGameOnly inputMode;
		SetInputMode(inputMode);
	}
	else if (EditorMode == EScenarioEditorControllerMode::EditPlacement
		|| EditorMode == EScenarioEditorControllerMode::EditRegionDraw)
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

void AScenarioEditorController::PruneEditorWidgetInputModeRequests()
{
	EditorWidgetInputModeRequesters.RemoveAll(
		[](const TWeakObjectPtr<UWidget>& requester)
		{
			return !requester.IsValid();
		});
}

UWidget* AScenarioEditorController::FindEditorWidgetInputModeFocus() const
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

void AScenarioEditorController::DestroyPlacementPreview()
{
	if (IsValid(PlacementPreviewActor))
	{
		PlacementPreviewActor->ClearPreview();
		PlacementPreviewActor->Destroy();
	}

	PlacementPreviewActor = nullptr;
}

AScenarioTransformGizmoActor* AScenarioEditorController::EnsureTransformGizmoActor()
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
	TransformGizmoActor = world->SpawnActor<AScenarioTransformGizmoActor>(
		TransformGizmoActorClass,
		FTransform::Identity,
		spawnParams);
	return TransformGizmoActor;
}

void AScenarioEditorController::UpdateTransformGizmoForSelection()
{
	UScenarioPlaceableComponent* selectedPlaceable = SelectedPlaceableComponent.Get();
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

	AScenarioTransformGizmoActor* gizmoActor = EnsureTransformGizmoActor();
	if (!gizmoActor)
	{
		return;
	}

	if (!IsTransformModeAllowedForPlaceable(selectedPlaceable, TransformGizmoMode))
	{
		TransformGizmoMode = EScenarioTransformGizmoMode::Translate;
	}

	gizmoActor->ShowForTarget(selectedActor);
	gizmoActor->SetGizmoOrientationMode(GetEffectiveTransformGizmoOrientationModeForPlaceable(selectedPlaceable));
	gizmoActor->SetGizmoMode(TransformGizmoMode);
}

void AScenarioEditorController::HideTransformGizmo()
{
	if (IsValid(TransformGizmoActor))
	{
		TransformGizmoActor->HideGizmo();
	}
}

void AScenarioEditorController::SetTransformGizmoMode(EScenarioTransformGizmoMode mode)
{
	if (UScenarioPlaceableComponent* selectedPlaceable = SelectedPlaceableComponent.Get();
		IsEditorSelectablePlaceable(selectedPlaceable)
		&& !IsTransformModeAllowedForPlaceable(selectedPlaceable, mode))
	{
		mode = EScenarioTransformGizmoMode::Translate;
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

EScenarioTransformGizmoOrientationMode AScenarioEditorController::GetEffectiveTransformGizmoOrientationModeForPlaceable(
	const UScenarioPlaceableComponent* placeableComponent) const
{
	return IsRobotRouteMarkerPlaceable(placeableComponent) || IsCorridorHandlePlaceable(placeableComponent)
		? EScenarioTransformGizmoOrientationMode::World
		: TransformGizmoOrientationMode;
}

void AScenarioEditorController::GetTransformGizmoBasis(
	const FTransform& transform,
	EScenarioTransformGizmoOrientationMode orientationMode,
	FVector& outXAxis,
	FVector& outYAxis,
	FVector& outZAxis) const
{
	if (orientationMode == EScenarioTransformGizmoOrientationMode::World)
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

void AScenarioEditorController::UpdatePlaceableDetailsForSelection()
{
	UScenarioPlaceableComponent* selectedPlaceable = SelectedPlaceableComponent.Get();
	if (!IsEditorSelectablePlaceable(selectedPlaceable))
	{
		HidePlaceableDetails();
		return;
	}

	UScenarioEditorRootWidget* rootWidget = ShowEditorRootWidget();
	if (!rootWidget || !rootWidget->FocusSidebarForSelectedPlaceable(selectedPlaceable))
	{
		return;
	}
}

void AScenarioEditorController::HidePlaceableDetails()
{
	if (UScenarioEditorRootWidget* rootWidget = GetEditorRootWidget())
	{
		rootWidget->HidePlaceableDetails();
	}
}

AScenarioEditorPawn* AScenarioEditorController::GetEditorPawn() const
{
	return Cast<AScenarioEditorPawn>(GetPawn());
}

UScenarioAuthoringSubsystem* AScenarioEditorController::GetAuthoringSubsystem() const
{
	UWorld* world = GetWorld();
	return world ? world->GetSubsystem<UScenarioAuthoringSubsystem>() : nullptr;
}
