#include "Platform/Widget/ProjectOverviewScreenWidget.h"

#include "Engine/World.h"
#include "Platform/PlatformUiSubsystem.h"
#include "Platform/ViewModel/ProjectWorkspaceViewModel.h"
#include "UI/BaseButtonWidget.h"
#include "UI/BaseTextWidget.h"

void UProjectOverviewScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (OpenScenarioButton)
	{
		OpenScenarioButton->OnBaseClicked.RemoveDynamic(
			this,
			&UProjectOverviewScreenWidget::HandleScenarioButtonClicked);
		OpenScenarioButton->OnBaseClicked.AddDynamic(
			this,
			&UProjectOverviewScreenWidget::HandleScenarioButtonClicked);
	}
	if (OpenRobotButton)
	{
		OpenRobotButton->OnBaseClicked.RemoveDynamic(this, &UProjectOverviewScreenWidget::HandleRobotButtonClicked);
		OpenRobotButton->OnBaseClicked.AddDynamic(this, &UProjectOverviewScreenWidget::HandleRobotButtonClicked);
	}
	if (OpenExperimentButton)
	{
		OpenExperimentButton->OnBaseClicked.RemoveDynamic(
			this,
			&UProjectOverviewScreenWidget::HandleExperimentButtonClicked);
		OpenExperimentButton->OnBaseClicked.AddDynamic(
			this,
			&UProjectOverviewScreenWidget::HandleExperimentButtonClicked);
	}

	RefreshFromViewModel();
}

void UProjectOverviewScreenWidget::NativeDestruct()
{
	if (OpenScenarioButton)
	{
		OpenScenarioButton->OnBaseClicked.RemoveDynamic(
			this,
			&UProjectOverviewScreenWidget::HandleScenarioButtonClicked);
	}
	if (OpenRobotButton)
	{
		OpenRobotButton->OnBaseClicked.RemoveDynamic(this, &UProjectOverviewScreenWidget::HandleRobotButtonClicked);
	}
	if (OpenExperimentButton)
	{
		OpenExperimentButton->OnBaseClicked.RemoveDynamic(
			this,
			&UProjectOverviewScreenWidget::HandleExperimentButtonClicked);
	}

	Super::NativeDestruct();
}

void UProjectOverviewScreenWidget::RefreshFromViewModel()
{
	UProjectWorkspaceViewModel* viewModel = ResolveViewModel();
	if (!viewModel)
	{
		// Editor widget captures keep authored preview status; runtime screens report missing bindings.
		if (UWorld* world = GetWorld(); world && world->WorldType == EWorldType::Editor)
		{
			return;
		}
		if (StatusText)
		{
			StatusText->SetText(NSLOCTEXT("OdiroPlatform", "OverviewViewModelMissing", "Workspace ViewModel 없음"));
		}
		return;
	}

	viewModel->RefreshFromProjectSession();
	if (ProjectPathText)
	{
		ProjectPathText->SetText(FText::FromString(viewModel->GetActiveProjectPath()));
	}
	if (ScenarioPathText)
	{
		ScenarioPathText->SetText(FText::FromString(viewModel->GetActiveScenarioPath()));
	}
	if (RunCountText)
	{
		RunCountText->SetText(FText::AsNumber(viewModel->GetRunItems().Num()));
	}
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(viewModel->GetStatusText()));
	}
}

UProjectWorkspaceViewModel* UProjectOverviewScreenWidget::ResolveViewModel()
{
	if (ProjectWorkspaceViewModel)
	{
		return ProjectWorkspaceViewModel.Get();
	}

	UPlatformUiSubsystem* platformUiSubsystem = UPlatformUiSubsystem::ResolveForWorldContext(this);
	ProjectWorkspaceViewModel = platformUiSubsystem ? platformUiSubsystem->GetProjectWorkspaceViewModel() : nullptr;
	return ProjectWorkspaceViewModel.Get();
}

void UProjectOverviewScreenWidget::HandleScenarioButtonClicked(UBaseButtonWidget* button)
{
	if (IsValid(button) && button == OpenScenarioButton)
	{
		OnScenarioRequested.Broadcast(this);
	}
}

void UProjectOverviewScreenWidget::HandleRobotButtonClicked(UBaseButtonWidget* button)
{
	if (IsValid(button) && button == OpenRobotButton)
	{
		OnRobotRequested.Broadcast(this);
	}
}

void UProjectOverviewScreenWidget::HandleExperimentButtonClicked(UBaseButtonWidget* button)
{
	if (IsValid(button) && button == OpenExperimentButton)
	{
		OnExperimentRequested.Broadcast(this);
	}
}
