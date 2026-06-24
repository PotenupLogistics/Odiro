#include "Scenario/Widget/ScenarioEditorSidebarWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/World.h"
#include "Scenario/ScenarioEditorUiSubsystem.h"
#include "Scenario/ViewModel/ScenarioTemplateSidebarViewModel.h"
#include "Scenario/Widget/ScenarioEditorSidebarCorridorPanel.h"
#include "Scenario/Widget/ScenarioEditorSidebarMainPanel.h"
#include "Scenario/Widget/ScenarioEditorSidebarObstaclePanel.h"
#include "Scenario/Widget/ScenarioEditorSidebarPedestrianPanel.h"
#include "Scenario/Data/ScenarioEditorWidgetClassCatalog.h"
#include "Scenario/Data/WidgetTextStyleCatalog.h"

namespace
{
	// Verifies that a resolved panel widget has the native type required by the sidebar shell.
	bool ScenarioEditorSidebarIsExpectedPanelWidget(
		const EScenarioTemplateSidebarPanel panel,
		const UWidget* widget)
	{
		switch (panel)
		{
		case EScenarioTemplateSidebarPanel::Main:
			return Cast<UScenarioEditorSidebarMainPanel>(widget) != nullptr;
		case EScenarioTemplateSidebarPanel::Corridor:
			return Cast<UScenarioEditorSidebarCorridorPanel>(widget) != nullptr;
		case EScenarioTemplateSidebarPanel::Obstacle:
			return Cast<UScenarioEditorSidebarObstaclePanel>(widget) != nullptr;
		case EScenarioTemplateSidebarPanel::Pedestrian:
			return Cast<UScenarioEditorSidebarPedestrianPanel>(widget) != nullptr;
		default:
			return false;
		}
	}
}

void UScenarioEditorSidebarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (WidgetClassCatalog.IsNull())
	{
		WidgetClassCatalog = UScenarioEditorWidgetClassCatalog::MakeDefaultCatalogReference();
	}
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ConfigureChildPanelDependencies();
	CollapseLegacySummaryWidgets();
	RefreshFromDraft();
}

void UScenarioEditorSidebarWidget::SetActivePanel(const EScenarioTemplateSidebarPanel panel)
{
	if (ActivePanel == panel)
	{
		return;
	}

	ActivePanel = panel;
	RefreshFromDraft();
}

void UScenarioEditorSidebarWidget::SetTextStyleCatalog(
	TSoftObjectPtr<UWidgetTextStyleCatalog> catalog)
{
	TextStyleCatalog = catalog;
	ConfigureChildPanelDependencies();
}

void UScenarioEditorSidebarWidget::SetWidgetClassCatalog(
	TSoftObjectPtr<UScenarioEditorWidgetClassCatalog> catalog)
{
	WidgetClassCatalog = catalog.IsNull()
		? UScenarioEditorWidgetClassCatalog::MakeDefaultCatalogReference()
		: catalog;
	ConfigureChildPanelDependencies();
}

void UScenarioEditorSidebarWidget::RefreshFromDraft()
{
	UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this);
	UScenarioTemplateSidebarViewModel* templateSidebarViewModel = uiSubsystem
		? uiSubsystem->GetTemplateSidebarViewModel()
		: nullptr;
	FScenarioDocument scenarioTemplate;
	FString failureReason;
	if (!templateSidebarViewModel || !templateSidebarViewModel->TryGetDraftScenario(scenarioTemplate, failureReason))
	{
		SetSidebarShellText(
			PanelToTitle(ActivePanel),
			failureReason.IsEmpty() ? TEXT("ScenarioTemplateSidebarViewModel unavailable.") : failureReason);
		return;
	}

	RefreshFromTemplate(scenarioTemplate);
}

void UScenarioEditorSidebarWidget::RefreshFromTemplate(const FScenarioDocument& scenarioTemplate)
{
	SetSidebarShellText(PanelToTitle(ActivePanel), FString());
	if (!RefreshActivePanelContent(scenarioTemplate))
	{
		SetSidebarShellText(
			PanelToTitle(ActivePanel),
			TEXT("Scenario editor panel widget class is missing."));
	}
	RefreshPanelSwitcher();
}

bool UScenarioEditorSidebarWidget::RefreshActivePanelContent(
	const FScenarioDocument& scenarioTemplate)
{
	const EScenarioTemplateSidebarPanel panel = ActivePanel;
	UWidget* panelWidget = EnsurePanelWidget(panel);
	if (!panelWidget)
	{
		return false;
	}

	switch (panel)
	{
	case EScenarioTemplateSidebarPanel::Main:
		if (UScenarioEditorSidebarMainPanel* mainPanel = Cast<UScenarioEditorSidebarMainPanel>(panelWidget))
		{
			mainPanel->SetWidgetClassCatalog(WidgetClassCatalog);
			mainPanel->SetTextStyleCatalog(TextStyleCatalog);
			mainPanel->RefreshFromTemplate(scenarioTemplate);
			return true;
		}
		break;
	case EScenarioTemplateSidebarPanel::Corridor:
		if (UScenarioEditorSidebarCorridorPanel* corridorPanel = Cast<UScenarioEditorSidebarCorridorPanel>(panelWidget))
		{
			corridorPanel->SetWidgetClassCatalog(WidgetClassCatalog);
			corridorPanel->SetTextStyleCatalog(TextStyleCatalog);
			corridorPanel->RefreshFromTemplate(scenarioTemplate);
			return true;
		}
		break;
	case EScenarioTemplateSidebarPanel::Obstacle:
		if (UScenarioEditorSidebarObstaclePanel* obstaclePanel = Cast<UScenarioEditorSidebarObstaclePanel>(panelWidget))
		{
			obstaclePanel->SetWidgetClassCatalog(WidgetClassCatalog);
			obstaclePanel->SetTextStyleCatalog(TextStyleCatalog);
			obstaclePanel->RefreshFromTemplate(scenarioTemplate);
			return true;
		}
		break;
	case EScenarioTemplateSidebarPanel::Pedestrian:
		if (UScenarioEditorSidebarPedestrianPanel* pedestrianPanel = Cast<UScenarioEditorSidebarPedestrianPanel>(panelWidget))
		{
			pedestrianPanel->SetWidgetClassCatalog(WidgetClassCatalog);
			pedestrianPanel->SetTextStyleCatalog(TextStyleCatalog);
			pedestrianPanel->RefreshFromTemplate(scenarioTemplate);
			return true;
		}
		break;
	default:
		break;
	}

	SetSidebarShellText(
		PanelToTitle(panel),
		TEXT("Scenario editor panel widget binding has an unexpected type."));
	return false;
}

UWidget* UScenarioEditorSidebarWidget::EnsurePanelWidget(
	const EScenarioTemplateSidebarPanel panel)
{
	UWidget* resolvedWidget = ResolvePanelWidget(panel);
	if (ScenarioEditorSidebarIsExpectedPanelWidget(panel, resolvedWidget))
	{
		return resolvedWidget;
	}

	return EnsureGeneratedPanelWidget(panel);
}

UWidget* UScenarioEditorSidebarWidget::EnsureGeneratedPanelWidget(
	const EScenarioTemplateSidebarPanel panel)
{
	if (UWidget* generatedWidget = ResolveGeneratedPanelWidget(panel))
	{
		return generatedWidget;
	}
	if (!PanelSwitcher || !GetWorld())
	{
		return nullptr;
	}

	UWidget* panelWidget = nullptr;
	switch (panel)
	{
	case EScenarioTemplateSidebarPanel::Main:
		if (TSubclassOf<UScenarioEditorSidebarMainPanel> panelClass =
			UScenarioEditorWidgetClassCatalog::ResolveSidebarMainPanelWidgetClass(WidgetClassCatalog))
		{
			panelWidget = CreateWidget<UScenarioEditorSidebarMainPanel>(
				GetWorld(),
				panelClass,
				TEXT("GeneratedMainPanelWidget"));
		}
		break;
	case EScenarioTemplateSidebarPanel::Corridor:
		if (TSubclassOf<UScenarioEditorSidebarCorridorPanel> panelClass =
			UScenarioEditorWidgetClassCatalog::ResolveSidebarCorridorPanelWidgetClass(WidgetClassCatalog))
		{
			panelWidget = CreateWidget<UScenarioEditorSidebarCorridorPanel>(
				GetWorld(),
				panelClass,
				TEXT("GeneratedCorridorPanelWidget"));
		}
		break;
	case EScenarioTemplateSidebarPanel::Obstacle:
		if (TSubclassOf<UScenarioEditorSidebarObstaclePanel> panelClass =
			UScenarioEditorWidgetClassCatalog::ResolveSidebarObstaclePanelWidgetClass(WidgetClassCatalog))
		{
			panelWidget = CreateWidget<UScenarioEditorSidebarObstaclePanel>(
				GetWorld(),
				panelClass,
				TEXT("GeneratedObstaclePanelWidget"));
		}
		break;
	case EScenarioTemplateSidebarPanel::Pedestrian:
		if (TSubclassOf<UScenarioEditorSidebarPedestrianPanel> panelClass =
			UScenarioEditorWidgetClassCatalog::ResolveSidebarPedestrianPanelWidgetClass(WidgetClassCatalog))
		{
			panelWidget = CreateWidget<UScenarioEditorSidebarPedestrianPanel>(
				GetWorld(),
				panelClass,
				TEXT("GeneratedPedestrianPanelWidget"));
		}
		break;
	default:
		break;
	}

	if (!panelWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("Scenario editor sidebar panel WBP class is missing for %s."), *PanelToTitle(panel));
		return nullptr;
	}

	PanelSwitcher->AddChild(panelWidget);

	switch (panel)
	{
	case EScenarioTemplateSidebarPanel::Main:
		GeneratedMainPanelWidget = panelWidget;
		break;
	case EScenarioTemplateSidebarPanel::Corridor:
		GeneratedCorridorPanelWidget = panelWidget;
		break;
	case EScenarioTemplateSidebarPanel::Obstacle:
		GeneratedObstaclePanelWidget = panelWidget;
		break;
	case EScenarioTemplateSidebarPanel::Pedestrian:
		GeneratedPedestrianPanelWidget = panelWidget;
		break;
	default:
		break;
	}

	ConfigureChildPanelDependencies();
	return panelWidget;
}

UWidget* UScenarioEditorSidebarWidget::ResolveGeneratedPanelWidget(
	const EScenarioTemplateSidebarPanel panel) const
{
	switch (panel)
	{
	case EScenarioTemplateSidebarPanel::Main:
		return GeneratedMainPanelWidget.Get();
	case EScenarioTemplateSidebarPanel::Corridor:
		return GeneratedCorridorPanelWidget.Get();
	case EScenarioTemplateSidebarPanel::Obstacle:
		return GeneratedObstaclePanelWidget.Get();
	case EScenarioTemplateSidebarPanel::Pedestrian:
		return GeneratedPedestrianPanelWidget.Get();
	default:
		return nullptr;
	}
}

void UScenarioEditorSidebarWidget::RefreshPanelSwitcher()
{
	if (!PanelSwitcher)
	{
		return;
	}

	if (UWidget* panelWidget = EnsurePanelWidget(ActivePanel))
	{
		PanelSwitcher->SetActiveWidget(panelWidget);
	}
}

UWidget* UScenarioEditorSidebarWidget::ResolvePanelWidget(
	const EScenarioTemplateSidebarPanel panel) const
{
	switch (panel)
	{
	case EScenarioTemplateSidebarPanel::Main:
		return MainPanelWidget ? Cast<UWidget>(MainPanelWidget.Get()) : GeneratedMainPanelWidget.Get();
	case EScenarioTemplateSidebarPanel::Corridor:
		return GeneratedCorridorPanelWidget ? GeneratedCorridorPanelWidget.Get() : CorridorPanelWidget.Get();
	case EScenarioTemplateSidebarPanel::Obstacle:
		return GeneratedObstaclePanelWidget ? GeneratedObstaclePanelWidget.Get() : ObstaclePanelWidget.Get();
	case EScenarioTemplateSidebarPanel::Pedestrian:
		return GeneratedPedestrianPanelWidget ? GeneratedPedestrianPanelWidget.Get() : PedestrianPanelWidget.Get();
	default:
		return nullptr;
	}
}

void UScenarioEditorSidebarWidget::SetSidebarShellText(
	const FString& title,
	const FString& diagnosticsText)
{
	SetTextBlockText(PanelTitleTextBlock.Get(), title);
	SetTextBlockText(DiagnosticsTextBlock.Get(), diagnosticsText);
	if (DiagnosticsTextBlock)
	{
		DiagnosticsTextBlock->SetVisibility(
			diagnosticsText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	CollapseLegacySummaryWidgets();
}

void UScenarioEditorSidebarWidget::CollapseLegacySummaryWidgets() const
{
	if (FallbackSummaryContainer)
	{
		FallbackSummaryContainer->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	if (PrimaryFieldsTextBlock)
	{
		PrimaryFieldsTextBlock->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (SecondaryFieldsTextBlock)
	{
		SecondaryFieldsTextBlock->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ListSummaryTextBlock)
	{
		ListSummaryTextBlock->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UScenarioEditorSidebarWidget::SetTextBlockText(UTextBlock* textBlock, const FString& text) const
{
	if (textBlock)
	{
		textBlock->SetText(FText::FromString(text));
	}
}

void UScenarioEditorSidebarWidget::ConfigureChildPanelDependencies() const
{
	if (MainPanelWidget)
	{
		MainPanelWidget->SetWidgetClassCatalog(WidgetClassCatalog);
		MainPanelWidget->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (UScenarioEditorSidebarMainPanel* mainPanel = Cast<UScenarioEditorSidebarMainPanel>(GeneratedMainPanelWidget.Get()))
	{
		mainPanel->SetWidgetClassCatalog(WidgetClassCatalog);
		mainPanel->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (UScenarioEditorSidebarCorridorPanel* corridorPanel = Cast<UScenarioEditorSidebarCorridorPanel>(GeneratedCorridorPanelWidget.Get()))
	{
		corridorPanel->SetWidgetClassCatalog(WidgetClassCatalog);
		corridorPanel->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (UScenarioEditorSidebarObstaclePanel* obstaclePanel = Cast<UScenarioEditorSidebarObstaclePanel>(GeneratedObstaclePanelWidget.Get()))
	{
		obstaclePanel->SetWidgetClassCatalog(WidgetClassCatalog);
		obstaclePanel->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (UScenarioEditorSidebarPedestrianPanel* pedestrianPanel = Cast<UScenarioEditorSidebarPedestrianPanel>(GeneratedPedestrianPanelWidget.Get()))
	{
		pedestrianPanel->SetWidgetClassCatalog(WidgetClassCatalog);
		pedestrianPanel->SetTextStyleCatalog(TextStyleCatalog);
	}
}

FString UScenarioEditorSidebarWidget::PanelToTitle(const EScenarioTemplateSidebarPanel panel)
{
	switch (panel)
	{
	case EScenarioTemplateSidebarPanel::Main:
		return TEXT("기본 정보");
	case EScenarioTemplateSidebarPanel::Corridor:
		return TEXT("통로");
	case EScenarioTemplateSidebarPanel::Obstacle:
		return TEXT("장애물");
	case EScenarioTemplateSidebarPanel::Pedestrian:
		return TEXT("보행자");
	default:
		return TEXT("시나리오 템플릿");
	}
}
