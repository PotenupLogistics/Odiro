#include "Platform/Widget/RobotConfigScreenWidget.h"

#include "Platform/PlatformUiSubsystem.h"
#include "Platform/ViewModel/RobotProfileViewModel.h"
#include "Platform/Widget/RobotConfigEditorWidget.h"
#include "UI/BaseButtonWidget.h"

namespace
{
// Finds an existing command button inside the wrapped robot config editor.
UBaseButtonWidget* FindRobotConfigEditorButton(URobotConfigEditorWidget* editor, const FName buttonName)
{
	return editor ? Cast<UBaseButtonWidget>(editor->GetWidgetFromName(buttonName)) : nullptr;
}

// Dispatches an existing editor button command without exposing new editor public API.
bool ClickRobotConfigEditorButton(URobotConfigEditorWidget* editor, const FName buttonName)
{
	UBaseButtonWidget* button = FindRobotConfigEditorButton(editor, buttonName);
	if (!button)
	{
		return false;
	}

	button->OnBaseClicked.Broadcast(button);
	return true;
}
}

bool URobotConfigScreenWidget::ReloadProfile()
{
	if (!RobotConfigEditor)
	{
		return false;
	}

	UPlatformUiSubsystem* platformUiSubsystem = UPlatformUiSubsystem::ResolveForWorldContext(this);
	URobotProfileViewModel* viewModel = platformUiSubsystem ? platformUiSubsystem->GetRobotProfileViewModel() : nullptr;
	if (!viewModel || !viewModel->LoadFromActiveProject())
	{
		return false;
	}

	return ClickRobotConfigEditorButton(RobotConfigEditor.Get(), FName(TEXT("ResetProfileButton")));
}

bool URobotConfigScreenWidget::ResetProfileInputs()
{
	return ClickRobotConfigEditorButton(RobotConfigEditor.Get(), FName(TEXT("ResetProfileButton")));
}

bool URobotConfigScreenWidget::SaveProfile()
{
	return ClickRobotConfigEditorButton(RobotConfigEditor.Get(), FName(TEXT("SaveProfileButton")));
}

void URobotConfigScreenWidget::SetRobotPreviewActive(const bool bActive)
{
	if (!RobotConfigEditor)
	{
		return;
	}

	if (bActive)
	{
		RobotConfigEditor->ActivateRobotPreview();
	}
	else
	{
		RobotConfigEditor->DeactivateRobotPreview();
	}
}
