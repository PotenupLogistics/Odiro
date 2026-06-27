#include "Scenario/ViewModel/ScenarioEditorToolbarViewModel.h"

#include "Scenario/ScenarioEditorUiSubsystem.h"
#include "Scenario/ViewModel/ScenarioEditorShellViewModel.h"
#include "Scenario/ViewModel/ScenarioTemplateSidebarViewModel.h"

void UScenarioEditorToolbarViewModel::InitializeForSubsystem(UScenarioEditorUiSubsystem* uiSubsystem)
{
	UiSubsystem = uiSubsystem;
}

void UScenarioEditorToolbarViewModel::SetDefaultSavePath(const FString& path)
{
	UE_MVVM_SET_PROPERTY_VALUE(DefaultSavePath, path);
}

void UScenarioEditorToolbarViewModel::SetStartupMapId(const FString& mapId)
{
	UE_MVVM_SET_PROPERTY_VALUE(StartupMapId, mapId);
}

bool UScenarioEditorToolbarViewModel::SaveScenario()
{
	if (!UiSubsystem)
	{
		SetStatusText(TEXT("Save failed: ScenarioEditorUiSubsystem unavailable."));
		return false;
	}

	FString resolvedPath;
	TArray<FString> diagnostics;
	if (!UiSubsystem->SaveScenario(DefaultSavePath, resolvedPath, diagnostics))
	{
		SetStatusText(diagnostics.IsEmpty()
			? FString::Printf(TEXT("Save failed: %s"), *DefaultSavePath)
			: FString::Join(diagnostics, TEXT("\n")));
		return false;
	}

	SetStatusText(FString::Printf(TEXT("Saved: %s"), *resolvedPath));
	return true;
}

bool UScenarioEditorToolbarViewModel::ReturnToStartup()
{
	if (!UiSubsystem)
	{
		SetStatusText(TEXT("Return failed: ScenarioEditorUiSubsystem unavailable."));
		return false;
	}

	if (!UiSubsystem->ReturnToStartup(StartupMapId))
	{
		SetStatusText(TEXT("Return failed: Startup map id unavailable."));
		return false;
	}

	return true;
}

void UScenarioEditorToolbarViewModel::RequestEditorWidgetInputMode(UWidget* requestingWidget)
{
	if (UiSubsystem)
	{
		UiSubsystem->RequestEditorWidgetInputMode(requestingWidget);
	}
}

void UScenarioEditorToolbarViewModel::ReleaseEditorWidgetInputMode(UWidget* requestingWidget)
{
	if (UiSubsystem)
	{
		UiSubsystem->ReleaseEditorWidgetInputMode(requestingWidget);
	}
}

void UScenarioEditorToolbarViewModel::SelectSidebarPanel(const EScenarioTemplateSidebarPanel panel)
{
	UE_MVVM_SET_PROPERTY_VALUE(ActiveSidebarPanel, panel);

	if (UiSubsystem)
	{
		if (UScenarioEditorShellViewModel* shellViewModel = UiSubsystem->GetShellViewModel())
		{
			shellViewModel->SelectTemplatePanel(panel);
		}
		if (UScenarioTemplateSidebarViewModel* sidebarViewModel = UiSubsystem->GetTemplateSidebarViewModel())
		{
			sidebarViewModel->SelectPanel(panel);
		}
	}
}

bool UScenarioEditorToolbarViewModel::SetEditorViewMode(const EScenarioEditorViewMode viewMode)
{
	return UiSubsystem && UiSubsystem->SetEditorViewMode(viewMode);
}

bool UScenarioEditorToolbarViewModel::SetTopDownOrthoViewMode()
{
	return SetEditorViewMode(EScenarioEditorViewMode::TopDownOrtho);
}

bool UScenarioEditorToolbarViewModel::SetPerspectiveViewMode()
{
	return SetEditorViewMode(EScenarioEditorViewMode::Perspective);
}

bool UScenarioEditorToolbarViewModel::SetTransformGizmoMode(const EScenarioTransformGizmoMode mode)
{
	return UiSubsystem && UiSubsystem->SetTransformGizmoMode(mode);
}

bool UScenarioEditorToolbarViewModel::SetTransformGizmoOrientationMode(
	const EScenarioTransformGizmoOrientationMode orientationMode)
{
	return UiSubsystem && UiSubsystem->SetTransformGizmoOrientationMode(orientationMode);
}

EScenarioTransformGizmoMode UScenarioEditorToolbarViewModel::GetTransformGizmoMode() const
{
	return UiSubsystem
		? UiSubsystem->GetTransformGizmoMode()
		: EScenarioTransformGizmoMode::Translate;
}

EScenarioEditorViewMode UScenarioEditorToolbarViewModel::GetEditorViewMode() const
{
	return UiSubsystem
		? UiSubsystem->GetEditorViewMode()
		: EScenarioEditorViewMode::Perspective;
}

EScenarioTransformGizmoOrientationMode UScenarioEditorToolbarViewModel::GetTransformGizmoOrientationMode() const
{
	return UiSubsystem
		? UiSubsystem->GetTransformGizmoOrientationMode()
		: EScenarioTransformGizmoOrientationMode::World;
}

EScenarioTransformGizmoOrientationMode UScenarioEditorToolbarViewModel::GetEffectiveTransformGizmoOrientationMode() const
{
	return UiSubsystem
		? UiSubsystem->GetEffectiveTransformGizmoOrientationMode()
		: EScenarioTransformGizmoOrientationMode::World;
}

bool UScenarioEditorToolbarViewModel::CanEditTransformGizmoOrientationForSelection() const
{
	return UiSubsystem && UiSubsystem->CanEditTransformGizmoOrientationForSelection();
}

void UScenarioEditorToolbarViewModel::SetStatusText(const FString& message)
{
	UE_MVVM_SET_PROPERTY_VALUE(StatusText, message);
}
