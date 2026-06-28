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
	if (SaveButton)
	{
		SaveButton->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ReturnToMainMenuButton)
	{
		ReturnToMainMenuButton->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (StatusTextBlock)
	{
		StatusTextBlock->SetVisibility(ESlateVisibility::Collapsed);
		UWidgetTextStyleCatalog::ApplyTextBlockStyle(StatusTextBlock.Get(), EWidgetTextStyleRole::Value);
	}
	RefreshSidebarPanelButtons();
	RefreshTransformCommandButtons();
	RefreshViewModeButtons();
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

void UScenarioEditorToolbarWidget::NativeTick(const FGeometry& myGeometry, const float inDeltaTime)
{
	Super::NativeTick(myGeometry, inDeltaTime);
	SyncSidebarPanelFromViewModel();
	RefreshSidebarPanelButtons();
	RefreshTransformCommandButtons();
	RefreshViewModeButtons();
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

void UScenarioEditorToolbarWidget::SelectMoveTransformTool()
{
	if (ToolbarViewModel && ToolbarViewModel->SetTransformGizmoMode(EScenarioTransformGizmoMode::Translate))
	{
		RefreshTransformCommandButtons();
	}
}

void UScenarioEditorToolbarWidget::SelectRotateTransformTool()
{
	if (ToolbarViewModel && ToolbarViewModel->SetTransformGizmoMode(EScenarioTransformGizmoMode::Rotate))
	{
		RefreshTransformCommandButtons();
	}
}

void UScenarioEditorToolbarWidget::SelectWorldCoordinateMode()
{
	if (ToolbarViewModel
		&& ToolbarViewModel->SetTransformGizmoOrientationMode(EScenarioTransformGizmoOrientationMode::World))
	{
		RefreshTransformCommandButtons();
	}
}

void UScenarioEditorToolbarWidget::SelectLocalCoordinateMode()
{
	if (ToolbarViewModel
		&& ToolbarViewModel->SetTransformGizmoOrientationMode(EScenarioTransformGizmoOrientationMode::Relative))
	{
		RefreshTransformCommandButtons();
	}
}

void UScenarioEditorToolbarWidget::SelectTopDownOrthoViewMode()
{
	if (ToolbarViewModel && ToolbarViewModel->SetTopDownOrthoViewMode())
	{
		RefreshViewModeButtons();
	}
}

void UScenarioEditorToolbarWidget::SelectPerspectiveViewMode()
{
	if (ToolbarViewModel && ToolbarViewModel->SetPerspectiveViewMode())
	{
		RefreshViewModeButtons();
	}
}

void UScenarioEditorToolbarWidget::RefreshSidebarPanelButtons()
{
	ApplySidebarPanelButtonState(MainPanelButton.Get(), EScenarioTemplateSidebarPanel::Main);
	ApplySidebarPanelButtonState(CorridorPanelButton.Get(), EScenarioTemplateSidebarPanel::Corridor);
	ApplySidebarPanelButtonState(ObstaclePanelButton.Get(), EScenarioTemplateSidebarPanel::Obstacle);
	ApplySidebarPanelButtonState(PedestrianPanelButton.Get(), EScenarioTemplateSidebarPanel::Pedestrian);
}

void UScenarioEditorToolbarWidget::RefreshTransformCommandButtons()
{
	ApplyTransformModeButtonState(MoveButton.Get(), EScenarioTransformGizmoMode::Translate);
	ApplyTransformModeButtonState(RotateButton.Get(), EScenarioTransformGizmoMode::Rotate);
	ApplyCoordinateModeButtonState(WorldCoordinateButton.Get(), EScenarioTransformGizmoOrientationMode::World);
	ApplyCoordinateModeButtonState(LocalCoordinateButton.Get(), EScenarioTransformGizmoOrientationMode::Relative);
	ApplyCoordinateModeButtonState(WorldOrientationButton.Get(), EScenarioTransformGizmoOrientationMode::World);
	ApplyCoordinateModeButtonState(RelativeOrientationButton.Get(), EScenarioTransformGizmoOrientationMode::Relative);
}

void UScenarioEditorToolbarWidget::RefreshViewModeButtons()
{
	ApplyViewModeButtonState(TopDownOrthoViewButton.Get(), EScenarioEditorViewMode::TopDownOrtho);
	ApplyViewModeButtonState(PerspectiveViewButton.Get(), EScenarioEditorViewMode::Perspective);
	ApplyViewModeButtonState(TopDownOrthoModeButton.Get(), EScenarioEditorViewMode::TopDownOrtho);
	ApplyViewModeButtonState(PerspectiveModeButton.Get(), EScenarioEditorViewMode::Perspective);
	ApplyViewModeButtonState(View2DButton.Get(), EScenarioEditorViewMode::TopDownOrtho);
	ApplyViewModeButtonState(View3DButton.Get(), EScenarioEditorViewMode::Perspective);
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

void UScenarioEditorToolbarWidget::HandleMoveButtonClicked()
{
	SelectMoveTransformTool();
}

void UScenarioEditorToolbarWidget::HandleRotateButtonClicked()
{
	SelectRotateTransformTool();
}

void UScenarioEditorToolbarWidget::HandleWorldCoordinateButtonClicked()
{
	SelectWorldCoordinateMode();
}

void UScenarioEditorToolbarWidget::HandleLocalCoordinateButtonClicked()
{
	SelectLocalCoordinateMode();
}

void UScenarioEditorToolbarWidget::HandleTopDownOrthoViewButtonClicked()
{
	SelectTopDownOrthoViewMode();
}

void UScenarioEditorToolbarWidget::HandlePerspectiveViewButtonClicked()
{
	SelectPerspectiveViewMode();
}

void UScenarioEditorToolbarWidget::BindControls()
{
	if (SaveButton)
	{
		SaveButton->OnClicked.RemoveDynamic(this, &UScenarioEditorToolbarWidget::HandleSaveButtonClicked);
	}

	if (ReturnToMainMenuButton)
	{
		ReturnToMainMenuButton->OnClicked.RemoveDynamic(this, &UScenarioEditorToolbarWidget::HandleReturnButtonClicked);
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

	if (MoveButton)
	{
		MoveButton->OnClicked.RemoveDynamic(this, &UScenarioEditorToolbarWidget::HandleMoveButtonClicked);
		MoveButton->OnClicked.AddDynamic(this, &UScenarioEditorToolbarWidget::HandleMoveButtonClicked);
	}

	if (RotateButton)
	{
		RotateButton->OnClicked.RemoveDynamic(this, &UScenarioEditorToolbarWidget::HandleRotateButtonClicked);
		RotateButton->OnClicked.AddDynamic(this, &UScenarioEditorToolbarWidget::HandleRotateButtonClicked);
	}

	if (WorldCoordinateButton)
	{
		WorldCoordinateButton->OnClicked.RemoveDynamic(
			this, &UScenarioEditorToolbarWidget::HandleWorldCoordinateButtonClicked);
		WorldCoordinateButton->OnClicked.AddDynamic(
			this, &UScenarioEditorToolbarWidget::HandleWorldCoordinateButtonClicked);
	}

	if (LocalCoordinateButton)
	{
		LocalCoordinateButton->OnClicked.RemoveDynamic(
			this, &UScenarioEditorToolbarWidget::HandleLocalCoordinateButtonClicked);
		LocalCoordinateButton->OnClicked.AddDynamic(
			this, &UScenarioEditorToolbarWidget::HandleLocalCoordinateButtonClicked);
	}

	if (WorldOrientationButton)
	{
		WorldOrientationButton->OnClicked.RemoveDynamic(
			this, &UScenarioEditorToolbarWidget::HandleWorldCoordinateButtonClicked);
		WorldOrientationButton->OnClicked.AddDynamic(
			this, &UScenarioEditorToolbarWidget::HandleWorldCoordinateButtonClicked);
	}

	if (RelativeOrientationButton)
	{
		RelativeOrientationButton->OnClicked.RemoveDynamic(
			this, &UScenarioEditorToolbarWidget::HandleLocalCoordinateButtonClicked);
		RelativeOrientationButton->OnClicked.AddDynamic(
			this, &UScenarioEditorToolbarWidget::HandleLocalCoordinateButtonClicked);
	}

	if (TopDownOrthoViewButton)
	{
		TopDownOrthoViewButton->OnClicked.RemoveDynamic(
			this, &UScenarioEditorToolbarWidget::HandleTopDownOrthoViewButtonClicked);
		TopDownOrthoViewButton->OnClicked.AddDynamic(
			this, &UScenarioEditorToolbarWidget::HandleTopDownOrthoViewButtonClicked);
	}

	if (PerspectiveViewButton)
	{
		PerspectiveViewButton->OnClicked.RemoveDynamic(
			this, &UScenarioEditorToolbarWidget::HandlePerspectiveViewButtonClicked);
		PerspectiveViewButton->OnClicked.AddDynamic(
			this, &UScenarioEditorToolbarWidget::HandlePerspectiveViewButtonClicked);
	}

	if (TopDownOrthoModeButton)
	{
		TopDownOrthoModeButton->OnClicked.RemoveDynamic(
			this, &UScenarioEditorToolbarWidget::HandleTopDownOrthoViewButtonClicked);
		TopDownOrthoModeButton->OnClicked.AddDynamic(
			this, &UScenarioEditorToolbarWidget::HandleTopDownOrthoViewButtonClicked);
	}

	if (PerspectiveModeButton)
	{
		PerspectiveModeButton->OnClicked.RemoveDynamic(
			this, &UScenarioEditorToolbarWidget::HandlePerspectiveViewButtonClicked);
		PerspectiveModeButton->OnClicked.AddDynamic(
			this, &UScenarioEditorToolbarWidget::HandlePerspectiveViewButtonClicked);
	}

	if (View2DButton)
	{
		View2DButton->OnClicked.RemoveDynamic(
			this, &UScenarioEditorToolbarWidget::HandleTopDownOrthoViewButtonClicked);
		View2DButton->OnClicked.AddDynamic(
			this, &UScenarioEditorToolbarWidget::HandleTopDownOrthoViewButtonClicked);
	}

	if (View3DButton)
	{
		View3DButton->OnClicked.RemoveDynamic(
			this, &UScenarioEditorToolbarWidget::HandlePerspectiveViewButtonClicked);
		View3DButton->OnClicked.AddDynamic(
			this, &UScenarioEditorToolbarWidget::HandlePerspectiveViewButtonClicked);
	}
}

void UScenarioEditorToolbarWidget::SyncSidebarPanelFromViewModel()
{
	if (!ToolbarViewModel)
	{
		return;
	}

	const EScenarioTemplateSidebarPanel viewModelPanel = ToolbarViewModel->GetActiveSidebarPanel();
	if (ActiveSidebarPanel != viewModelPanel)
	{
		SetActiveSidebarPanel(viewModelPanel);
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

void UScenarioEditorToolbarWidget::ApplyTransformModeButtonState(
	UButton* button,
	const EScenarioTransformGizmoMode mode) const
{
	if (!button)
	{
		return;
	}

	button->SetIsEnabled(ToolbarViewModel != nullptr);
	const EScenarioTransformGizmoMode activeMode = ToolbarViewModel
		? ToolbarViewModel->GetTransformGizmoMode()
		: EScenarioTransformGizmoMode::Translate;
	button->SetRenderOpacity(activeMode == mode ? 1.0f : 0.55f);
}

void UScenarioEditorToolbarWidget::ApplyCoordinateModeButtonState(
	UButton* button,
	const EScenarioTransformGizmoOrientationMode orientationMode) const
{
	if (!button)
	{
		return;
	}

	button->SetIsEnabled(ToolbarViewModel != nullptr);
	const EScenarioTransformGizmoOrientationMode activeOrientationMode = ToolbarViewModel
		? ToolbarViewModel->GetTransformGizmoOrientationMode()
		: EScenarioTransformGizmoOrientationMode::World;
	button->SetRenderOpacity(activeOrientationMode == orientationMode ? 1.0f : 0.55f);
}

void UScenarioEditorToolbarWidget::ApplyViewModeButtonState(
	UButton* button,
	const EScenarioEditorViewMode viewMode) const
{
	if (!button)
	{
		return;
	}

	button->SetIsEnabled(ToolbarViewModel != nullptr);
	const EScenarioEditorViewMode activeViewMode = ToolbarViewModel
		? ToolbarViewModel->GetEditorViewMode()
		: EScenarioEditorViewMode::Perspective;
	button->SetRenderOpacity(activeViewMode == viewMode ? 1.0f : 0.55f);
}
