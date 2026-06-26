#include "Scenario/Widget/ScenarioEditorSidebarWidget.h"

#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/World.h"
#include "Scenario/Editor/ScenarioEditorController.h"
#include "Scenario/ScenarioEditorUiSubsystem.h"
#include "Scenario/ViewModel/ScenarioEditorShellViewModel.h"
#include "Scenario/ViewModel/ScenarioTemplateSidebarViewModel.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarCorridorPanel.h"
#include "Scenario/Widget/ScenarioEditorSidebarMainPanel.h"
#include "Scenario/Widget/ScenarioEditorSidebarObstaclePanel.h"
#include "Scenario/Widget/ScenarioEditorSidebarPedestrianPanel.h"
#include "Scenario/Data/ScenarioEditorWidgetClassCatalog.h"
#include "Scenario/Data/WidgetTextStyleCatalog.h"
#include "TimerManager.h"

namespace
{
	// Extra padding applied when the shell auto-scrolls a selected block into view.
	constexpr float SelectedBlockScrollPadding = 12.0f;

	// Maximum deferred attempts for selection scroll after panel rebuild or block expansion.
	constexpr int32 SelectedBlockScrollMaxAttempts = 5;

	// Stable authoring proxy instance id used for the robot start marker.
	const FString RobotStartMarkerInstanceId(TEXT("robot_start_point"));

	// Stable authoring proxy instance id used for the robot goal marker.
	const FString RobotGoalMarkerInstanceId(TEXT("robot_goal_point"));

	// Stable prefix used by authoring corridor vertex proxy components.
	const FString CorridorVertexHandleIdPrefix(TEXT("corridor_vertex_"));

	// Stable prefix used by authoring corridor segment proxy components.
	const FString CorridorSegmentHandleIdPrefix(TEXT("corridor_segment_"));

	// Formats a corridor proxy instance id without reaching into private authoring helpers.
	FString MakeScenarioEditorCorridorHandleId(const FString& prefix, const int32 itemIndex)
	{
		return FString::Printf(TEXT("%s%03d"), *prefix, itemIndex);
	}

	// Parses paths shaped like root.list.path[index] without accepting the non-indexed [] parent path.
	bool TryParseScenarioEditorIndexedBlockPath(
		const FString& blockPath,
		const TCHAR* listPath,
		int32& outItemIndex)
	{
		outItemIndex = INDEX_NONE;
		const FString prefix = FString::Printf(TEXT("%s["), listPath);
		if (!blockPath.StartsWith(prefix) || !blockPath.EndsWith(TEXT("]")))
		{
			return false;
		}

		const int32 indexStart = prefix.Len();
		const int32 indexLength = blockPath.Len() - indexStart - 1;
		if (indexLength <= 0)
		{
			return false;
		}

		const FString indexText = blockPath.Mid(indexStart, indexLength);
		if (!indexText.IsNumeric())
		{
			return false;
		}

		outItemIndex = FCString::Atoi(*indexText);
		return outItemIndex >= 0;
	}

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
	RequestEditorWidgetInputMode();
}

void UScenarioEditorSidebarWidget::NativeDestruct()
{
	ReleaseEditorWidgetInputMode();
	UnbindAllPanelBlockSelection();
	Super::NativeDestruct();
}

void UScenarioEditorSidebarWidget::SetActivePanel(const EScenarioTemplateSidebarPanel panel)
{
	if (ActivePanel == panel)
	{
		return;
	}

	UnbindPanelBlockSelection(ResolvePanelWidget(ActivePanel));
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
	const bool bRefreshed = RefreshActivePanelContent(scenarioTemplate);
	if (!bRefreshed)
	{
		SetSidebarShellText(
			PanelToTitle(ActivePanel),
			TEXT("Scenario editor panel widget class is missing."));
	}
	RefreshPanelSwitcher();
	if (bRefreshed)
	{
		BindPanelBlockSelection(ResolvePanelWidget(ActivePanel));
		ApplySelectedBlockFocus(true);
	}
}

void UScenarioEditorSidebarWidget::ApplySelectedBlockFocus(const bool bScrollIntoView)
{
	ApplyActivePanelSelectionState();
	if (bScrollIntoView)
	{
		RequestScrollSelectedBlockIntoView();
	}
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

void UScenarioEditorSidebarWidget::RequestEditorWidgetInputMode()
{
	if (UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this))
	{
		uiSubsystem->RequestEditorWidgetInputMode(this);
	}
}

void UScenarioEditorSidebarWidget::ReleaseEditorWidgetInputMode()
{
	if (UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this))
	{
		uiSubsystem->ReleaseEditorWidgetInputMode(this);
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

void UScenarioEditorSidebarWidget::BindPanelBlockSelection(UWidget* panelWidget)
{
	UnbindPanelBlockSelection(panelWidget);

	TArray<UScenarioEditorSidebarBlockWidget*> blockWidgets;
	CollectPanelBlockWidgets(panelWidget, blockWidgets);
	for (UScenarioEditorSidebarBlockWidget* blockWidget : blockWidgets)
	{
		BindBlockSelection(blockWidget);
	}
}

void UScenarioEditorSidebarWidget::UnbindPanelBlockSelection(UWidget* panelWidget)
{
	TArray<UScenarioEditorSidebarBlockWidget*> blockWidgets;
	CollectPanelBlockWidgets(panelWidget, blockWidgets);
	for (UScenarioEditorSidebarBlockWidget* blockWidget : blockWidgets)
	{
		UnbindBlockSelection(blockWidget);
	}
}

void UScenarioEditorSidebarWidget::UnbindAllPanelBlockSelection()
{
	for (EScenarioTemplateSidebarPanel panel : {
		EScenarioTemplateSidebarPanel::Main,
		EScenarioTemplateSidebarPanel::Corridor,
		EScenarioTemplateSidebarPanel::Obstacle,
		EScenarioTemplateSidebarPanel::Pedestrian })
	{
		UnbindPanelBlockSelection(ResolvePanelWidget(panel));
	}
}

void UScenarioEditorSidebarWidget::BindBlockSelection(UScenarioEditorSidebarBlockWidget* blockWidget)
{
	if (!blockWidget)
	{
		return;
	}

	blockWidget->OnBlockSelected.RemoveDynamic(
		this,
		&UScenarioEditorSidebarWidget::HandlePanelBlockSelected);
	blockWidget->OnBlockSelected.AddDynamic(
		this,
		&UScenarioEditorSidebarWidget::HandlePanelBlockSelected);
}

void UScenarioEditorSidebarWidget::UnbindBlockSelection(UScenarioEditorSidebarBlockWidget* blockWidget)
{
	if (blockWidget)
	{
		blockWidget->OnBlockSelected.RemoveDynamic(
			this,
			&UScenarioEditorSidebarWidget::HandlePanelBlockSelected);
	}
}

void UScenarioEditorSidebarWidget::HandlePanelBlockSelected(const FString& blockPath)
{
	if (UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this))
	{
		if (UScenarioEditorShellViewModel* shellViewModel = uiSubsystem->GetShellViewModel())
		{
			FString placeableId;
			if (TryResolvePlaceableIdForBlockPath(blockPath, placeableId)
				&& shellViewModel->SelectPlaceable(placeableId))
			{
				shellViewModel->FocusPlaceableTemplateBlock(ActivePanel, blockPath, placeableId);
				ApplyActivePanelSelectionState();
				RequestScrollSelectedBlockIntoView();
				return;
			}

			shellViewModel->SelectTemplateBlock(ActivePanel, blockPath);
		}
	}

	ApplyActivePanelSelectionState();
	RequestScrollSelectedBlockIntoView();
}

void UScenarioEditorSidebarWidget::ApplyActivePanelSelectionState()
{
	UWidget* panelWidget = ResolvePanelWidget(ActivePanel);
	if (UScenarioEditorSidebarMainPanel* mainPanel = Cast<UScenarioEditorSidebarMainPanel>(panelWidget))
	{
		mainPanel->ApplySelectedBlockPath();
		return;
	}
	if (UScenarioEditorSidebarCorridorPanel* corridorPanel = Cast<UScenarioEditorSidebarCorridorPanel>(panelWidget))
	{
		corridorPanel->ApplySelectedBlockPath();
		return;
	}
	if (UScenarioEditorSidebarObstaclePanel* obstaclePanel = Cast<UScenarioEditorSidebarObstaclePanel>(panelWidget))
	{
		obstaclePanel->ApplySelectedBlockPath();
		return;
	}
	if (UScenarioEditorSidebarPedestrianPanel* pedestrianPanel = Cast<UScenarioEditorSidebarPedestrianPanel>(panelWidget))
	{
		pedestrianPanel->ApplySelectedBlockPath();
	}
}

void UScenarioEditorSidebarWidget::RequestScrollSelectedBlockIntoView()
{
	RequestScrollSelectedBlockIntoView(SelectedBlockScrollMaxAttempts);
}

void UScenarioEditorSidebarWidget::RequestScrollSelectedBlockIntoView(const int32 attemptsRemaining)
{
	SelectedBlockScrollAttemptsRemaining = FMath::Max(
		SelectedBlockScrollAttemptsRemaining,
		FMath::Max(0, attemptsRemaining));

	if (bSelectedBlockScrollPending)
	{
		return;
	}

	UWorld* world = GetWorld();
	if (!world)
	{
		const int32 attemptsForThisScroll = SelectedBlockScrollAttemptsRemaining;
		SelectedBlockScrollAttemptsRemaining = 0;
		ScrollSelectedBlockIntoView(attemptsForThisScroll);
		return;
	}

	bSelectedBlockScrollPending = true;
	world->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(
			this,
			[this]
			{
				bSelectedBlockScrollPending = false;
				const int32 attemptsForThisScroll = SelectedBlockScrollAttemptsRemaining;
				SelectedBlockScrollAttemptsRemaining = 0;
				ScrollSelectedBlockIntoView(attemptsForThisScroll);
			}));
}

void UScenarioEditorSidebarWidget::ScrollSelectedBlockIntoView(const int32 attemptsRemaining)
{
	if (!SidebarScrollBox)
	{
		return;
	}

	const UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this);
	const UScenarioEditorShellViewModel* shellViewModel = uiSubsystem ? uiSubsystem->GetShellViewModel() : nullptr;
	const FString selectedBlockPath = shellViewModel ? shellViewModel->GetSelectedTemplateBlockPath() : FString();
	if (selectedBlockPath.IsEmpty())
	{
		return;
	}

	UScenarioEditorSidebarBlockWidget* blockWidget = FindActivePanelBlockWidgetByPath(selectedBlockPath);
	if (!blockWidget)
	{
		if (attemptsRemaining > 0)
		{
			RequestScrollSelectedBlockIntoView(attemptsRemaining - 1);
		}
		return;
	}

	SidebarScrollBox->InvalidateLayoutAndVolatility();
	blockWidget->InvalidateLayoutAndVolatility();
	SidebarScrollBox->ScrollWidgetIntoView(
		blockWidget,
		true,
		EDescendantScrollDestination::TopOrLeft,
		SelectedBlockScrollPadding);
}

UScenarioEditorSidebarBlockWidget* UScenarioEditorSidebarWidget::FindActivePanelBlockWidgetByPath(
	const FString& blockPath) const
{
	UWidget* panelWidget = ResolvePanelWidget(ActivePanel);
	if (const UScenarioEditorSidebarMainPanel* mainPanel = Cast<UScenarioEditorSidebarMainPanel>(panelWidget))
	{
		return mainPanel->FindBlockWidgetByPath(blockPath);
	}
	if (const UScenarioEditorSidebarCorridorPanel* corridorPanel = Cast<UScenarioEditorSidebarCorridorPanel>(panelWidget))
	{
		return corridorPanel->FindBlockWidgetByPath(blockPath);
	}
	if (const UScenarioEditorSidebarObstaclePanel* obstaclePanel = Cast<UScenarioEditorSidebarObstaclePanel>(panelWidget))
	{
		return obstaclePanel->FindBlockWidgetByPath(blockPath);
	}
	if (const UScenarioEditorSidebarPedestrianPanel* pedestrianPanel = Cast<UScenarioEditorSidebarPedestrianPanel>(panelWidget))
	{
		return pedestrianPanel->FindBlockWidgetByPath(blockPath);
	}
	return nullptr;
}

void UScenarioEditorSidebarWidget::CollectPanelBlockWidgets(
	UWidget* panelWidget,
	TArray<UScenarioEditorSidebarBlockWidget*>& outBlockWidgets) const
{
	if (const UScenarioEditorSidebarMainPanel* mainPanel = Cast<UScenarioEditorSidebarMainPanel>(panelWidget))
	{
		mainPanel->CollectBlockWidgets(outBlockWidgets);
		return;
	}
	if (const UScenarioEditorSidebarCorridorPanel* corridorPanel = Cast<UScenarioEditorSidebarCorridorPanel>(panelWidget))
	{
		corridorPanel->CollectBlockWidgets(outBlockWidgets);
		return;
	}
	if (const UScenarioEditorSidebarObstaclePanel* obstaclePanel = Cast<UScenarioEditorSidebarObstaclePanel>(panelWidget))
	{
		obstaclePanel->CollectBlockWidgets(outBlockWidgets);
		return;
	}
	if (const UScenarioEditorSidebarPedestrianPanel* pedestrianPanel = Cast<UScenarioEditorSidebarPedestrianPanel>(panelWidget))
	{
		pedestrianPanel->CollectBlockWidgets(outBlockWidgets);
	}
}

bool UScenarioEditorSidebarWidget::TryResolvePlaceableIdForBlockPath(
	const FString& blockPath,
	FString& outInstanceId) const
{
	outInstanceId.Reset();
	if (blockPath == TEXT("root.robot.start"))
	{
		outInstanceId = RobotStartMarkerInstanceId;
		return true;
	}
	if (blockPath == TEXT("root.robot.goal"))
	{
		outInstanceId = RobotGoalMarkerInstanceId;
		return true;
	}

	int32 itemIndex = INDEX_NONE;
	if (TryParseScenarioEditorIndexedBlockPath(
		blockPath,
		TEXT("root.corridor.axis.points_m"),
		itemIndex))
	{
		outInstanceId = MakeScenarioEditorCorridorHandleId(CorridorVertexHandleIdPrefix, itemIndex);
		return true;
	}
	if (TryParseScenarioEditorIndexedBlockPath(
		blockPath,
		TEXT("root.corridor.segments"),
		itemIndex))
	{
		outInstanceId = MakeScenarioEditorCorridorHandleId(CorridorSegmentHandleIdPrefix, itemIndex);
		return true;
	}
	if (TryParseScenarioEditorIndexedBlockPath(
		blockPath,
		TEXT("root.obstacles.placements"),
		itemIndex))
	{
		const UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this);
		const UScenarioTemplateSidebarViewModel* templateSidebarViewModel = uiSubsystem
			? uiSubsystem->GetTemplateSidebarViewModel()
			: nullptr;
		FScenarioDocument scenarioTemplate;
		FString failureReason;
		if (templateSidebarViewModel
			&& templateSidebarViewModel->TryGetDraftScenario(scenarioTemplate, failureReason)
			&& scenarioTemplate.Obstacles.Placements.IsValidIndex(itemIndex)
			&& !scenarioTemplate.Obstacles.Placements[itemIndex].PlacementId.IsEmpty())
		{
			outInstanceId = scenarioTemplate.Obstacles.Placements[itemIndex].PlacementId;
			return true;
		}
	}

	return false;
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
