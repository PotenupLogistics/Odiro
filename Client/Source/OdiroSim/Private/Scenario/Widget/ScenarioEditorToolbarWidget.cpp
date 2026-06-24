#include "Scenario/Widget/ScenarioEditorToolbarWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Scenario/ScenarioEditorUiSubsystem.h"
#include "Scenario/ViewModel/ScenarioEditorToolbarViewModel.h"
#include "Scenario/Widget/ScenarioEditorRootWidget.h"
#include "Scenario/Data/WidgetTextStyleCatalog.h"

void UScenarioEditorToolbarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	InitializeViewModel();
	BindControls();
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(StatusTextBlock.Get(), EWidgetTextStyleRole::Value);
	RefreshSidebarPanelButtons();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	RequestEditorWidgetInputMode();
	if (ToolbarViewModel)
	{
		ToolbarViewModel->SetStatusText(TEXT("준비됨"));
		SetStatusText(ToolbarViewModel->GetStatusText());
	}
	else
	{
		SetStatusText(TEXT("준비됨"));
	}
}

void UScenarioEditorToolbarWidget::NativeDestruct()
{
	ReleaseEditorWidgetInputMode();
	Super::NativeDestruct();
}

bool UScenarioEditorToolbarWidget::SaveScenario()
{
	if (!ToolbarViewModel)
	{
		SetStatusText(TEXT("저장 실패: ScenarioEditorToolbarViewModel unavailable."));
		return false;
	}

	ToolbarViewModel->SetDefaultSavePath(DefaultSavePath);
	const bool bSaved = ToolbarViewModel->SaveScenario();
	SetStatusText(ToolbarViewModel->GetStatusText());
	return bSaved;
}

void UScenarioEditorToolbarWidget::ReturnToMainMenu()
{
	if (!ToolbarViewModel)
	{
		SetStatusText(TEXT("복귀 실패: ScenarioEditorToolbarViewModel unavailable."));
		return;
	}

	ToolbarViewModel->SetStartupMapId(StartupMapId);
	if (!ToolbarViewModel->ReturnToStartup())
	{
		SetStatusText(ToolbarViewModel->GetStatusText());
	}
}

void UScenarioEditorToolbarWidget::SetActiveSidebarPanel(const EScenarioTemplateSidebarPanel panel)
{
	if (ActiveSidebarPanel == panel)
	{
		return;
	}

	ActiveSidebarPanel = panel;
	if (ToolbarViewModel)
	{
		ToolbarViewModel->SelectSidebarPanel(ActiveSidebarPanel);
	}
	RefreshSidebarPanelButtons();
	OnSidebarPanelChanged.Broadcast(ActiveSidebarPanel);
}

void UScenarioEditorToolbarWidget::SelectMainSidebarPanel()
{
	SetActiveSidebarPanel(EScenarioTemplateSidebarPanel::Main);
}

void UScenarioEditorToolbarWidget::SelectCorridorSidebarPanel()
{
	SetActiveSidebarPanel(EScenarioTemplateSidebarPanel::Corridor);
}

void UScenarioEditorToolbarWidget::SelectObstacleSidebarPanel()
{
	SetActiveSidebarPanel(EScenarioTemplateSidebarPanel::Obstacle);
}

void UScenarioEditorToolbarWidget::SelectPedestrianSidebarPanel()
{
	SetActiveSidebarPanel(EScenarioTemplateSidebarPanel::Pedestrian);
}

void UScenarioEditorToolbarWidget::RefreshSidebarPanelButtons()
{
	ApplySidebarPanelButtonState(MainPanelButton.Get(), EScenarioTemplateSidebarPanel::Main);
	ApplySidebarPanelButtonState(CorridorPanelButton.Get(), EScenarioTemplateSidebarPanel::Corridor);
	ApplySidebarPanelButtonState(ObstaclePanelButton.Get(), EScenarioTemplateSidebarPanel::Obstacle);
	ApplySidebarPanelButtonState(PedestrianPanelButton.Get(), EScenarioTemplateSidebarPanel::Pedestrian);
}

void UScenarioEditorToolbarWidget::HandleSaveButtonClicked()
{
	SaveScenario();
}

void UScenarioEditorToolbarWidget::HandleReturnButtonClicked()
{
	ReturnToMainMenu();
}

void UScenarioEditorToolbarWidget::HandleMainPanelButtonClicked()
{
	SelectMainSidebarPanel();
}

void UScenarioEditorToolbarWidget::HandleCorridorPanelButtonClicked()
{
	SelectCorridorSidebarPanel();
}

void UScenarioEditorToolbarWidget::HandleObstaclePanelButtonClicked()
{
	SelectObstacleSidebarPanel();
}

void UScenarioEditorToolbarWidget::HandlePedestrianPanelButtonClicked()
{
	SelectPedestrianSidebarPanel();
}

void UScenarioEditorToolbarWidget::BindControls()
{
	if (SaveButton)
	{
		SaveButton->OnClicked.RemoveDynamic(this, &UScenarioEditorToolbarWidget::HandleSaveButtonClicked);
		SaveButton->OnClicked.AddDynamic(this, &UScenarioEditorToolbarWidget::HandleSaveButtonClicked);
	}

	if (ReturnToMainMenuButton)
	{
		ReturnToMainMenuButton->OnClicked.RemoveDynamic(this, &UScenarioEditorToolbarWidget::HandleReturnButtonClicked);
		ReturnToMainMenuButton->OnClicked.AddDynamic(this, &UScenarioEditorToolbarWidget::HandleReturnButtonClicked);
	}

	if (MainPanelButton)
	{
		MainPanelButton->OnClicked.RemoveDynamic(this, &UScenarioEditorToolbarWidget::HandleMainPanelButtonClicked);
		MainPanelButton->OnClicked.AddDynamic(this, &UScenarioEditorToolbarWidget::HandleMainPanelButtonClicked);
	}

	if (CorridorPanelButton)
	{
		CorridorPanelButton->OnClicked.RemoveDynamic(this, &UScenarioEditorToolbarWidget::HandleCorridorPanelButtonClicked);
		CorridorPanelButton->OnClicked.AddDynamic(this, &UScenarioEditorToolbarWidget::HandleCorridorPanelButtonClicked);
	}

	if (ObstaclePanelButton)
	{
		ObstaclePanelButton->OnClicked.RemoveDynamic(this, &UScenarioEditorToolbarWidget::HandleObstaclePanelButtonClicked);
		ObstaclePanelButton->OnClicked.AddDynamic(this, &UScenarioEditorToolbarWidget::HandleObstaclePanelButtonClicked);
	}

	if (PedestrianPanelButton)
	{
		PedestrianPanelButton->OnClicked.RemoveDynamic(this, &UScenarioEditorToolbarWidget::HandlePedestrianPanelButtonClicked);
		PedestrianPanelButton->OnClicked.AddDynamic(this, &UScenarioEditorToolbarWidget::HandlePedestrianPanelButtonClicked);
	}
}

void UScenarioEditorToolbarWidget::RequestEditorWidgetInputMode()
{
	if (ToolbarViewModel)
	{
		UWidget* focusWidget = ResolveInputModeFocusWidget();
		RequestedInputModeFocusWidget = focusWidget;
		ToolbarViewModel->RequestEditorWidgetInputMode(focusWidget);
	}
}

void UScenarioEditorToolbarWidget::ReleaseEditorWidgetInputMode()
{
	if (ToolbarViewModel)
	{
		UWidget* focusWidget = RequestedInputModeFocusWidget.Get();
		if (!focusWidget)
		{
			focusWidget = ResolveInputModeFocusWidget();
		}

		ToolbarViewModel->ReleaseEditorWidgetInputMode(focusWidget);
		RequestedInputModeFocusWidget.Reset();
	}
}

void UScenarioEditorToolbarWidget::SetStatusText(const FString& message)
{
	if (StatusTextBlock)
	{
		StatusTextBlock->SetText(FText::FromString(message));
	}
}

void UScenarioEditorToolbarWidget::InitializeViewModel()
{
	UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this);
	ToolbarViewModel = uiSubsystem ? uiSubsystem->GetToolbarViewModel() : nullptr;
	if (ToolbarViewModel)
	{
		ToolbarViewModel->SetDefaultSavePath(DefaultSavePath);
		ToolbarViewModel->SetStartupMapId(StartupMapId);
		ToolbarViewModel->SelectSidebarPanel(ActiveSidebarPanel);
	}
}

UWidget* UScenarioEditorToolbarWidget::ResolveInputModeFocusWidget() const
{
	return ToolbarInputModeFocus.Get();
}

void UScenarioEditorToolbarWidget::ApplySidebarPanelButtonState(
	UButton* button,
	const EScenarioTemplateSidebarPanel panel) const
{
	if (button)
	{
		button->SetRenderOpacity(ActiveSidebarPanel == panel ? 1.0f : 0.55f);
	}
}
