
#include "Episode/Editor/EpisodeEditorController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Episode/Editor/EpisodeAuthoringSubsystem.h"
#include "Episode/Editor/EpisodeEditorPawn.h"
#include "Episode/Editor/EpisodePlacementPreviewActor.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Components/Widget.h"
#include "Widgets/SWidget.h"

AEpisodeEditorController::AEpisodeEditorController()
{
	PrimaryActorTick.bCanEverTick = true;
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	PlacementPreviewActorClass = AEpisodePlacementPreviewActor::StaticClass();
	EditorInputMappingContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/Input/IMC_Editor.IMC_Editor")));
	EditorMoveAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_EditorMove.IA_EditorMove")));
	EditorLookAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_EditorLook.IA_EditorLook")));
	EditorSelectionAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_Selection.IA_Selection")));
	EditorDeselectionAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/IA_Deselection.IA_Deselection")));
}

void AEpisodeEditorController::BeginPlay()
{
	Super::BeginPlay();
	AddEditorInputMappingContext();
	SetObserverMode();
}

void AEpisodeEditorController::Tick(float deltaSeconds)
{
	Super::Tick(deltaSeconds);

	switch (EditorMode)
	{
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
	bCurrentPlacementValid = false;
	CurrentPlacementFailureReason.Reset();
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
	UEpisodeAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem) return false;
	
	FString failureReason;
	if (!authoringSubsystem->CanPlaceStaticObstacle(propId, FTransform::Identity, failureReason)
		&& failureReason.StartsWith(TEXT("Unknown static obstacle prop")))
	{
		CurrentPlacementFailureReason = failureReason;
		return false;
	}

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

	if (!PlacementPreviewActor || !PlacementPreviewActor->ConfigureStaticObstacleProp(propId))
	{
		DestroyPlacementPreview();
		return false;
	}

	SelectedStaticObstaclePropId = propId;
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

	FEpisodePlaceableInstanceSpec placedSpec;
	AEpisodeStaticObstacle* placedActor = nullptr;
	const bool bPlaced = authoringSubsystem->AddStaticObstacleInternal(
		SelectedStaticObstaclePropId,
		CurrentPlacementTransform,
		placedSpec,
		placedActor);

	if (bPlaced)
	{
		SetObserverMode();
	}

	return bPlaced;
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

void AEpisodeEditorController::HandleConfirmPlacementInput()
{
	ConfirmPlacement();
}

void AEpisodeEditorController::HandleCancelPlacementInput()
{
	CancelPlacement();
}

void AEpisodeEditorController::HandleEditorMoveAction(const FInputActionValue& inputActionValue)
{
	if (EditorMode != EEpisodeEditorControllerMode::Observer)
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
	if (EditorMode != EEpisodeEditorControllerMode::Observer || FindEditorWidgetInputModeFocus())
	{
		return;
	}

	AEpisodeEditorPawn* editorPawn = GetEditorPawn();
	if (!editorPawn) return;

	float yawValue = 0.0f;
	float pitchValue = 0.0f;

	switch (inputActionValue.GetValueType())
	{
	case EInputActionValueType::Axis3D:
	{
		const FVector lookValue = inputActionValue.Get<FVector>();
		yawValue = lookValue.X;
		pitchValue = lookValue.Y;
		break;
	}
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

	editorPawn->ApplyLookInput(
		yawValue * MouseLookSensitivity,
		pitchValue * MouseLookSensitivity);
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
			&AEpisodeEditorController::HandleConfirmPlacementInput);
	}

	if (UInputAction* deselectionAction = EditorDeselectionAction.LoadSynchronous())
	{
		enhancedInputComponent->BindAction(
			deselectionAction,
			ETriggerEvent::Started,
			this,
			&AEpisodeEditorController::HandleCancelPlacementInput);
	}
}

void AEpisodeEditorController::UpdatePlacementPreview()
{
	if (!PlacementPreviewActor)
	{
		return;
	}

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
		bCurrentPlacementValid = authoringSubsystem->CanPlaceStaticObstacle(
			SelectedStaticObstaclePropId,
			CurrentPlacementTransform,
			CurrentPlacementFailureReason);
	}

	PlacementPreviewActor->SetActorTransform(CurrentPlacementTransform);
	PlacementPreviewActor->SetPlacementValid(bCurrentPlacementValid);
}

bool AEpisodeEditorController::TraceMousePlacement(FHitResult& outHit) const
{
	FVector worldOrigin = FVector::ZeroVector;
	FVector worldDirection = FVector::ForwardVector;
	if (!DeprojectMousePositionToWorld(worldOrigin, worldDirection))
	{
		return false;
	}

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
	const FVector snappedLocation = SnapLocationIfNeeded(location);
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
	if (EditorMode == EEpisodeEditorControllerMode::EditPlacement)
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

AEpisodeEditorPawn* AEpisodeEditorController::GetEditorPawn() const
{
	return Cast<AEpisodeEditorPawn>(GetPawn());
}

UEpisodeAuthoringSubsystem* AEpisodeEditorController::GetAuthoringSubsystem() const
{
	UWorld* world = GetWorld();
	return world ? world->GetSubsystem<UEpisodeAuthoringSubsystem>() : nullptr;
}
