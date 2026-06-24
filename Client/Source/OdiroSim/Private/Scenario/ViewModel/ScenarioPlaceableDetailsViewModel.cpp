#include "Scenario/ViewModel/ScenarioPlaceableDetailsViewModel.h"

#include "Scenario/ScenarioEditorUiSubsystem.h"

void UScenarioPlaceableDetailsViewModel::InitializeForSubsystem(UScenarioEditorUiSubsystem* uiSubsystem)
{
	UiSubsystem = uiSubsystem;
}

void UScenarioPlaceableDetailsViewModel::SetSelectedPlaceable(
	const FString& instanceId,
	const FString& label)
{
	UE_MVVM_SET_PROPERTY_VALUE(SelectedInstanceId, instanceId);
	UE_MVVM_SET_PROPERTY_VALUE(TitleText, label.IsEmpty() ? instanceId : label);
	UE_MVVM_SET_PROPERTY_VALUE(bHasSelection, !instanceId.IsEmpty());
}

void UScenarioPlaceableDetailsViewModel::ClearSelectedPlaceable()
{
	UE_MVVM_SET_PROPERTY_VALUE(SelectedInstanceId, FString());
	UE_MVVM_SET_PROPERTY_VALUE(TitleText, FString());
	UE_MVVM_SET_PROPERTY_VALUE(bHasSelection, false);
}

void UScenarioPlaceableDetailsViewModel::RequestEditorWidgetInputMode(UWidget* requestingWidget)
{
	if (UiSubsystem)
	{
		UiSubsystem->RequestEditorWidgetInputMode(requestingWidget);
	}
}

void UScenarioPlaceableDetailsViewModel::ReleaseEditorWidgetInputMode(UWidget* requestingWidget)
{
	if (UiSubsystem)
	{
		UiSubsystem->ReleaseEditorWidgetInputMode(requestingWidget);
	}
}

bool UScenarioPlaceableDetailsViewModel::SetTransformGizmoOrientationMode(
	const EScenarioTransformGizmoOrientationMode orientationMode)
{
	return UiSubsystem && UiSubsystem->SetTransformGizmoOrientationMode(orientationMode);
}

bool UScenarioPlaceableDetailsViewModel::CanEditTransformGizmoOrientationForSelection() const
{
	return UiSubsystem && UiSubsystem->CanEditTransformGizmoOrientationForSelection();
}

EScenarioTransformGizmoOrientationMode UScenarioPlaceableDetailsViewModel::GetEffectiveTransformGizmoOrientationMode() const
{
	return UiSubsystem
		? UiSubsystem->GetEffectiveTransformGizmoOrientationMode()
		: EScenarioTransformGizmoOrientationMode::World;
}

bool UScenarioPlaceableDetailsViewModel::DeleteSelectedPlaceable(FString& outFailureReason)
{
	outFailureReason.Reset();
	if (!UiSubsystem)
	{
		outFailureReason = TEXT("ScenarioEditorUiSubsystem unavailable.");
		return false;
	}

	return UiSubsystem->DeleteSelectedPlaceable(outFailureReason);
}

bool UScenarioPlaceableDetailsViewModel::RenameSelectedPlaceableInstanceId(
	const FString& instanceId,
	FString& outFailureReason)
{
	outFailureReason.Reset();
	if (!UiSubsystem)
	{
		outFailureReason = TEXT("ScenarioEditorUiSubsystem unavailable.");
		return false;
	}

	return UiSubsystem->RenameSelectedPlaceableInstanceId(instanceId, outFailureReason);
}

bool UScenarioPlaceableDetailsViewModel::UpdateSelectedPlaceableTransform(
	const FTransform& transform,
	FString& outFailureReason)
{
	outFailureReason.Reset();
	if (!UiSubsystem)
	{
		outFailureReason = TEXT("ScenarioEditorUiSubsystem unavailable.");
		return false;
	}

	return UiSubsystem->UpdateSelectedPlaceableTransform(transform, outFailureReason);
}
