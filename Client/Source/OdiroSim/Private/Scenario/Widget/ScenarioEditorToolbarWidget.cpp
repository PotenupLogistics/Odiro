#include "Scenario/Widget/ScenarioEditorToolbarWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Scenario/Editor/ScenarioEditorController.h"
#include "Scenario/Widget/ScenarioEditorRootWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Shared/ExperimentSettingTypes.h"

namespace
{
	FString MakeUniqueSavePath(const FString& preferredPath)
	{
		FString directory = FPaths::GetPath(preferredPath);
		if (directory.IsEmpty())
		{
			directory = TEXT("Json/Input");
		}

		const FString baseName = FPaths::GetBaseFilename(preferredPath).IsEmpty()
			? FString(TEXT("ScenarioTemplateNew.template"))
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
			if (!FPaths::FileExists(FExperimentSettingJson::ResolveProjectPath(candidatePath)))
			{
				return candidatePath;
			}
		}

		FString fallbackPath = FPaths::Combine(
			directory,
			FString::Printf(TEXT("%s_%s.%s"), *baseName, *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8), *extension));
		fallbackPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		return fallbackPath;
	}
}

void UScenarioEditorToolbarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindControls();
	RefreshSidebarPanelButtons();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	RequestEditorWidgetInputMode();
	SetStatusText(TEXT("준비됨"));
}

void UScenarioEditorToolbarWidget::NativeDestruct()
{
	ReleaseEditorWidgetInputMode();
	Super::NativeDestruct();
}

bool UScenarioEditorToolbarWidget::SaveScenario()
{
	AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		SetStatusText(TEXT("저장 실패: ScenarioEditorController unavailable."));
		return false;
	}

	FString resolvedPath;
	TArray<FString> diagnostics;
	const FString savePath = ResolveSavePath();
	if (!editorController->SaveScenarioSetupJsonFile(savePath, resolvedPath, diagnostics))
	{
		SetStatusText(diagnostics.IsEmpty()
			? FString::Printf(TEXT("저장 실패: %s"), *savePath)
			: FString::Join(diagnostics, TEXT("\n")));
		return false;
	}

	SetStatusText(FString::Printf(TEXT("저장됨: %s"), *resolvedPath));
	return true;
}

void UScenarioEditorToolbarWidget::ReturnToMainMenu()
{
	if (UWorld* world = GetWorld())
	{
		UGameplayStatics::OpenLevel(world, FName(*MainMenuMapId));
	}
}

void UScenarioEditorToolbarWidget::SetActiveSidebarPanel(const EScenarioTemplateSidebarPanel panel)
{
	ActiveSidebarPanel = panel;
	RefreshSidebarPanelButtons();
	OnSidebarPanelChanged.Broadcast(ActiveSidebarPanel);

	if (AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer()))
	{
		if (UScenarioEditorRootWidget* rootWidget = editorController->GetEditorRootWidget())
		{
			rootWidget->SetTemplateSidebarPanel(ActiveSidebarPanel);
		}
	}
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
	if (AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer()))
	{
		UWidget* focusWidget = ResolveInputModeFocusWidget();
		RequestedInputModeFocusWidget = focusWidget;
		editorController->RequestEditorWidgetInputMode(focusWidget);
	}
}

void UScenarioEditorToolbarWidget::ReleaseEditorWidgetInputMode()
{
	if (AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer()))
	{
		UWidget* focusWidget = RequestedInputModeFocusWidget.Get();
		if (!focusWidget)
		{
			focusWidget = ResolveInputModeFocusWidget();
		}

		editorController->ReleaseEditorWidgetInputMode(focusWidget);
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

UWidget* UScenarioEditorToolbarWidget::ResolveInputModeFocusWidget() const
{
	return ToolbarInputModeFocus.Get();
}

FString UScenarioEditorToolbarWidget::ResolveSavePath() const
{
	if (const AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer()))
	{
		const FString sourcePath = editorController->GetSourceScenarioSetupJsonPath();
		if (!sourcePath.IsEmpty())
		{
			return sourcePath;
		}
	}

	return MakeUniqueSavePath(DefaultSavePath);
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
