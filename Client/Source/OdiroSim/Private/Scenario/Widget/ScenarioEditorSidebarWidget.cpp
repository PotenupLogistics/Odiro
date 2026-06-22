#include "Scenario/Widget/ScenarioEditorSidebarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ContentWidget.h"
#include "Components/PanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WidgetSwitcherSlot.h"
#include "Engine/World.h"
#include "Scenario/Editor/ScenarioAuthoringSubsystem.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarCorridorPanel.h"
#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"
#include "Scenario/Widget/ScenarioEditorSidebarMainPanel.h"
#include "Scenario/Widget/ScenarioEditorSidebarObstaclePanel.h"
#include "Scenario/Widget/ScenarioEditorSidebarPedestrianPanel.h"
#include "Scenario/Data/ScenarioEditorWidgetClassCatalog.h"
#include "Scenario/Data/WidgetTextStyleCatalog.h"

namespace
{
	constexpr float SidebarPanelContentTopPadding = 4.0f;

	FString JoinLines(const TArray<FString>& lines)
	{
		return lines.IsEmpty() ? FString(TEXT("None")) : FString::Join(lines, TEXT("\n"));
	}

	FString FormatMeters(const double value)
	{
		return FString::Printf(TEXT("%.2fm"), value);
	}

	// Infers a conservative editor control type for generated read-only rows.
	EScenarioEditorSidebarFieldInputType InferGeneratedFieldInputType(const FString& label)
	{
		const FString normalizedLabel = label.ToLower();
		if (normalizedLabel.Contains(TEXT("range"))
			|| normalizedLabel.Contains(TEXT("along_m"))
			|| normalizedLabel.Contains(TEXT("offset_m")))
		{
			return EScenarioEditorSidebarFieldInputType::Range;
		}
		if (normalizedLabel.Contains(TEXT("count"))
			|| normalizedLabel.Contains(TEXT("version")))
		{
			return EScenarioEditorSidebarFieldInputType::Integer;
		}
		if (normalizedLabel.Contains(TEXT("_m"))
			|| normalizedLabel.Contains(TEXT("density"))
			|| normalizedLabel.Contains(TEXT("cooperation"))
			|| normalizedLabel.Contains(TEXT("speed")))
		{
			return EScenarioEditorSidebarFieldInputType::Number;
		}
		if (normalizedLabel.Contains(TEXT("type"))
			|| normalizedLabel.Contains(TEXT("kind"))
			|| normalizedLabel.Contains(TEXT("allow_")))
		{
			return EScenarioEditorSidebarFieldInputType::EnumText;
		}
		return EScenarioEditorSidebarFieldInputType::Text;
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
	UWorld* world = GetWorld();
	const UScenarioAuthoringSubsystem* authoringSubsystem = world
		? world->GetSubsystem<UScenarioAuthoringSubsystem>()
		: nullptr;
	if (!authoringSubsystem)
	{
		SetSidebarText(
			PanelToTitle(ActivePanel),
			TEXT(""),
			TEXT(""),
			TEXT(""),
			TEXT("ScenarioAuthoringSubsystem unavailable."));
		SetFallbackTextVisibility(ESlateVisibility::SelfHitTestInvisible);
		return;
	}

	RefreshFromTemplate(authoringSubsystem->GetDraftScenario());
}

void UScenarioEditorSidebarWidget::RefreshFromTemplate(const FScenarioDocument& scenarioTemplate)
{
	FString primaryText;
	FString secondaryText;
	FString listText;

	switch (ActivePanel)
	{
	case EScenarioTemplateSidebarPanel::Main:
		BuildMainPanelText(scenarioTemplate, primaryText, secondaryText, listText);
		break;
	case EScenarioTemplateSidebarPanel::Corridor:
		BuildCorridorPanelText(scenarioTemplate, primaryText, secondaryText, listText);
		break;
	case EScenarioTemplateSidebarPanel::Obstacle:
		BuildObstaclePanelText(scenarioTemplate, primaryText, secondaryText, listText);
		break;
	case EScenarioTemplateSidebarPanel::Pedestrian:
		BuildPedestrianPanelText(scenarioTemplate, primaryText, secondaryText, listText);
		break;
	default:
		break;
	}

	SetSidebarText(PanelToTitle(ActivePanel), primaryText, secondaryText, listText, TEXT(""));
	if (ActivePanel == EScenarioTemplateSidebarPanel::Main && MainPanelWidget)
	{
		MainPanelWidget->SetWidgetClassCatalog(WidgetClassCatalog);
		MainPanelWidget->SetTextStyleCatalog(TextStyleCatalog);
		MainPanelWidget->RefreshFromTemplate(scenarioTemplate);
	}
	RefreshGeneratedPanelContent(scenarioTemplate);
	RefreshPanelSwitcher();
	RefreshFallbackTextVisibility();
}

void UScenarioEditorSidebarWidget::RefreshGeneratedPanelContent(
	const FScenarioDocument& scenarioTemplate)
{
	if (!PanelSwitcher || !WidgetTree)
	{
		return;
	}

	const EScenarioTemplateSidebarPanel panel = ActivePanel;
	UWidget* panelWidget = ResolvePanelWidget(panel);
	if (!panelWidget)
	{
		panelWidget = EnsureGeneratedPanelWidget(panel);
	}
	if (!panelWidget)
	{
		SetSidebarText(
			PanelToTitle(panel),
			TEXT(""),
			TEXT(""),
			TEXT(""),
			TEXT("Scenario editor panel widget class is missing."));
		return;
	}

	switch (panel)
	{
	case EScenarioTemplateSidebarPanel::Main:
		if (UScenarioEditorSidebarMainPanel* mainPanel = Cast<UScenarioEditorSidebarMainPanel>(panelWidget))
		{
			mainPanel->SetWidgetClassCatalog(WidgetClassCatalog);
			mainPanel->SetTextStyleCatalog(TextStyleCatalog);
			mainPanel->RefreshFromTemplate(scenarioTemplate);
			return;
		}
		break;
	case EScenarioTemplateSidebarPanel::Corridor:
		if (UScenarioEditorSidebarCorridorPanel* corridorPanel = Cast<UScenarioEditorSidebarCorridorPanel>(panelWidget))
		{
			corridorPanel->SetWidgetClassCatalog(WidgetClassCatalog);
			corridorPanel->SetTextStyleCatalog(TextStyleCatalog);
			corridorPanel->RefreshFromTemplate(scenarioTemplate);
			return;
		}
		break;
	case EScenarioTemplateSidebarPanel::Obstacle:
		if (UScenarioEditorSidebarObstaclePanel* obstaclePanel = Cast<UScenarioEditorSidebarObstaclePanel>(panelWidget))
		{
			obstaclePanel->SetWidgetClassCatalog(WidgetClassCatalog);
			obstaclePanel->SetTextStyleCatalog(TextStyleCatalog);
			obstaclePanel->RefreshFromTemplate(scenarioTemplate);
			return;
		}
		break;
	case EScenarioTemplateSidebarPanel::Pedestrian:
		if (UScenarioEditorSidebarPedestrianPanel* pedestrianPanel = Cast<UScenarioEditorSidebarPedestrianPanel>(panelWidget))
		{
			pedestrianPanel->SetWidgetClassCatalog(WidgetClassCatalog);
			pedestrianPanel->SetTextStyleCatalog(TextStyleCatalog);
			pedestrianPanel->RefreshFromTemplate(scenarioTemplate);
			return;
		}
		break;
	default:
		break;
	}

	UWidget* generatedWidget = EnsureGeneratedPanelWidget(panel);
	if (generatedWidget && generatedWidget != panelWidget)
	{
		PanelSwitcher->SetActiveWidget(generatedWidget);
		RefreshGeneratedPanelContent(scenarioTemplate);
		return;
	}

	SetSidebarText(
		PanelToTitle(panel),
		TEXT(""),
		TEXT(""),
		TEXT(""),
		TEXT("Scenario editor panel widget binding has an unexpected type."));
}

UWidget* UScenarioEditorSidebarWidget::EnsureGeneratedPanelWidget(
	const EScenarioTemplateSidebarPanel panel)
{
	if (UWidget* generatedWidget = ResolveGeneratedPanelWidget(panel))
	{
		return generatedWidget;
	}
	if (!PanelSwitcher || !WidgetTree)
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
			panelWidget = WidgetTree->ConstructWidget<UScenarioEditorSidebarMainPanel>(
				panelClass,
				TEXT("GeneratedMainPanelWidget"));
		}
		break;
	case EScenarioTemplateSidebarPanel::Corridor:
		if (TSubclassOf<UScenarioEditorSidebarCorridorPanel> panelClass =
			UScenarioEditorWidgetClassCatalog::ResolveSidebarCorridorPanelWidgetClass(WidgetClassCatalog))
		{
			panelWidget = WidgetTree->ConstructWidget<UScenarioEditorSidebarCorridorPanel>(
				panelClass,
				TEXT("GeneratedCorridorPanelWidget"));
		}
		break;
	case EScenarioTemplateSidebarPanel::Obstacle:
		if (TSubclassOf<UScenarioEditorSidebarObstaclePanel> panelClass =
			UScenarioEditorWidgetClassCatalog::ResolveSidebarObstaclePanelWidgetClass(WidgetClassCatalog))
		{
			panelWidget = WidgetTree->ConstructWidget<UScenarioEditorSidebarObstaclePanel>(
				panelClass,
				TEXT("GeneratedObstaclePanelWidget"));
		}
		break;
	case EScenarioTemplateSidebarPanel::Pedestrian:
		if (TSubclassOf<UScenarioEditorSidebarPedestrianPanel> panelClass =
			UScenarioEditorWidgetClassCatalog::ResolveSidebarPedestrianPanelWidgetClass(WidgetClassCatalog))
		{
			panelWidget = WidgetTree->ConstructWidget<UScenarioEditorSidebarPedestrianPanel>(
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

	if (UWidget* panelWidget = ResolvePanelWidget(ActivePanel))
	{
		PanelSwitcher->SetActiveWidget(panelWidget);
	}
}

void UScenarioEditorSidebarWidget::RefreshFallbackTextVisibility() const
{
	const bool bHasActivePanelWidget = PanelSwitcher && ResolvePanelWidget(ActivePanel);
	const ESlateVisibility visibility = bHasActivePanelWidget
		? ESlateVisibility::Collapsed
		: ESlateVisibility::SelfHitTestInvisible;

	SetFallbackTextVisibility(visibility);
}

void UScenarioEditorSidebarWidget::SetFallbackTextVisibility(const ESlateVisibility visibility) const
{
	if (FallbackSummaryContainer)
	{
		FallbackSummaryContainer->SetVisibility(visibility);
		return;
	}

	if (PrimaryFieldsTextBlock)
	{
		PrimaryFieldsTextBlock->SetVisibility(visibility);
	}
	if (SecondaryFieldsTextBlock)
	{
		SecondaryFieldsTextBlock->SetVisibility(visibility);
	}
	if (ListSummaryTextBlock)
	{
		ListSummaryTextBlock->SetVisibility(visibility);
	}
	if (DiagnosticsTextBlock)
	{
		DiagnosticsTextBlock->SetVisibility(visibility);
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

void UScenarioEditorSidebarWidget::BuildMainPanelText(
	const FScenarioDocument& scenarioTemplate,
	FString& outPrimaryText,
	FString& outSecondaryText,
	FString& outListText) const
{
	outPrimaryText = FString::Printf(
		TEXT("schema: %s\nversion: %d\nscenario_id: %s"),
		*scenarioTemplate.Schema,
		scenarioTemplate.Version,
		scenarioTemplate.ScenarioId.IsEmpty() ? TEXT("(unset)") : *scenarioTemplate.ScenarioId);

	outSecondaryText = FString::Printf(
		TEXT("intent: %s"),
		scenarioTemplate.Intent.IsEmpty() ? TEXT("(unset)") : *scenarioTemplate.Intent);

	TArray<FString> robotLines;
	robotLines.Add(FString::Printf(TEXT("start: %s"), *FormatRobotAnchor(scenarioTemplate.Robot.Start)));
	robotLines.Add(FString::Printf(TEXT("goal: %s"), *FormatRobotAnchor(scenarioTemplate.Robot.Goal)));
	outListText = JoinLines(robotLines);
}

void UScenarioEditorSidebarWidget::BuildCorridorPanelText(
	const FScenarioDocument& scenarioTemplate,
	FString& outPrimaryText,
	FString& outSecondaryText,
	FString& outListText) const
{
	const FScenarioTemplateCorridor& corridor = scenarioTemplate.Corridor;
	outPrimaryText = FString::Printf(
		TEXT("axis: %d point(s), %s\nwalkway_width: %s"),
		corridor.Axis.PointsMeters.Num(),
		*FormatMeters(MeasureAxisLengthMeters(corridor.Axis.PointsMeters)),
		*FormatNumberValue(corridor.WalkwayWidthMeters, TEXT("m")));

	TArray<FString> laneLines;
	laneLines.Add(FString::Printf(TEXT("building_side: %d lane(s)"), corridor.BuildingSide.Num()));
	for (const FScenarioTemplateLaneRule& lane : corridor.BuildingSide)
	{
		laneLines.Add(FString::Printf(TEXT("  - %s"), *FormatLaneRule(lane)));
	}
	laneLines.Add(FString::Printf(TEXT("curb_side: %d lane(s)"), corridor.CurbSide.Num()));
	for (const FScenarioTemplateLaneRule& lane : corridor.CurbSide)
	{
		laneLines.Add(FString::Printf(TEXT("  - %s"), *FormatLaneRule(lane)));
	}
	outSecondaryText = JoinLines(laneLines);

	TArray<FString> segmentLines;
	for (const FScenarioTemplateSegment& segment : corridor.Segments)
	{
		segmentLines.Add(FString::Printf(
			TEXT("%s | %s | %.2f..%.2fm | replace: %s"),
			segment.SegmentId.IsEmpty() ? TEXT("(unnamed)") : *segment.SegmentId,
			*SegmentTypeToString(segment.Type),
			segment.AlongRangeMeters.StartMeters,
			segment.AlongRangeMeters.EndMeters,
			*FormatStringValue(segment.ReplacedBySurfaceId)));
	}
	outListText = JoinLines(segmentLines);
}

void UScenarioEditorSidebarWidget::BuildObstaclePanelText(
	const FScenarioDocument& scenarioTemplate,
	FString& outPrimaryText,
	FString& outSecondaryText,
	FString& outListText) const
{
	const FScenarioTemplateObstacleRules& obstacles = scenarioTemplate.Obstacles;
	outPrimaryText = FString::Printf(
		TEXT("min_clear_width: %s\nplacements: %d"),
		*FormatNumberValue(obstacles.MinClearWidthMeters, TEXT("m")),
		obstacles.Placements.Num());

	outSecondaryText = TEXT("fixed/pattern/scatter placement rules");

	TArray<FString> placementLines;
	for (const FScenarioTemplateObstaclePlacement& placement : obstacles.Placements)
	{
		placementLines.Add(FString::Printf(
			TEXT("%s | %s | prop: %s | segment: %s | along: %s | offset: %s"),
			placement.PlacementId.IsEmpty() ? TEXT("(unnamed)") : *placement.PlacementId,
			*ObstaclePlacementKindToString(placement.Kind),
			placement.PropId.IsEmpty() ? TEXT("(unset)") : *placement.PropId,
			placement.At.SegmentId.IsEmpty() ? TEXT("(unset)") : *placement.At.SegmentId,
			*FormatNumberValue(placement.At.AlongMeters, TEXT("m")),
			*FormatNumberValue(placement.At.OffsetMeters, TEXT("m"))));
	}
	outListText = JoinLines(placementLines);
}

void UScenarioEditorSidebarWidget::BuildPedestrianPanelText(
	const FScenarioDocument& scenarioTemplate,
	FString& outPrimaryText,
	FString& outSecondaryText,
	FString& outListText) const
{
	const FScenarioTemplatePedestrianRules& pedestrians = scenarioTemplate.Pedestrians;
	outPrimaryText = FString::Printf(
		TEXT("background_count: %s\nbackground_speed: %s\nencounters: %d"),
		*FormatIntegerValue(pedestrians.Background.Count),
		*FormatNumberValue(pedestrians.Background.SpeedMetersPerSecond, TEXT("m/s")),
		pedestrians.Encounters.Num());

	outSecondaryText = FString::Printf(
		TEXT("spawn_segments: %s"),
		*FormatStringList(pedestrians.Background.SpawnSegmentIds));

	TArray<FString> encounterLines;
	for (const FScenarioTemplatePedestrianEncounter& encounter : pedestrians.Encounters)
	{
		encounterLines.Add(FString::Printf(
			TEXT("%s | %s | segment: %s | persona: %s | meet_offset: %s"),
			encounter.EncounterId.IsEmpty() ? TEXT("(unnamed)") : *encounter.EncounterId,
			*EncounterTypeToString(encounter.Type),
			encounter.AtSegmentId.IsEmpty() ? TEXT("(unset)") : *encounter.AtSegmentId,
			encounter.PersonaId.IsEmpty() ? TEXT("(unset)") : *encounter.PersonaId,
			*FormatNumberValue(encounter.MeetOffsetMeters, TEXT("m"))));
	}
	outListText = JoinLines(encounterLines);
}

void UScenarioEditorSidebarWidget::SetSidebarText(
	const FString& title,
	const FString& primaryText,
	const FString& secondaryText,
	const FString& listText,
	const FString& diagnosticsText)
{
	SetTextBlockText(PanelTitleTextBlock.Get(), title);
	SetTextBlockText(PrimaryFieldsTextBlock.Get(), primaryText);
	SetTextBlockText(SecondaryFieldsTextBlock.Get(), secondaryText);
	SetTextBlockText(ListSummaryTextBlock.Get(), listText);
	SetTextBlockText(DiagnosticsTextBlock.Get(), diagnosticsText);
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
		return TEXT("Main");
	case EScenarioTemplateSidebarPanel::Corridor:
		return TEXT("Corridor");
	case EScenarioTemplateSidebarPanel::Obstacle:
		return TEXT("Obstacle");
	case EScenarioTemplateSidebarPanel::Pedestrian:
		return TEXT("Pedestrian");
	default:
		return TEXT("Scenario Template");
	}
}

FString UScenarioEditorSidebarWidget::RobotAnchorTypeToString(const EScenarioTemplateRobotAnchorType type)
{
	switch (type)
	{
	case EScenarioTemplateRobotAnchorType::Entry:
		return TEXT("entry");
	case EScenarioTemplateRobotAnchorType::Exit:
		return TEXT("exit");
	case EScenarioTemplateRobotAnchorType::CorridorPose:
		return TEXT("corridor_pose");
	default:
		return TEXT("unknown");
	}
}

FString UScenarioEditorSidebarWidget::RobotHeadingToString(const EScenarioTemplateRobotHeading heading)
{
	switch (heading)
	{
	case EScenarioTemplateRobotHeading::Forward:
		return TEXT("forward");
	case EScenarioTemplateRobotHeading::Backward:
		return TEXT("backward");
	case EScenarioTemplateRobotHeading::Auto:
		return TEXT("auto");
	default:
		return TEXT("unknown");
	}
}

FString UScenarioEditorSidebarWidget::SegmentTypeToString(const EScenarioTemplateSegmentType type)
{
	switch (type)
	{
	case EScenarioTemplateSegmentType::Straight:
		return TEXT("straight");
	case EScenarioTemplateSegmentType::Narrowing:
		return TEXT("narrowing");
	case EScenarioTemplateSegmentType::Crosswalk:
		return TEXT("crosswalk");
	case EScenarioTemplateSegmentType::Entrance:
		return TEXT("entrance");
	default:
		return TEXT("unknown");
	}
}

FString UScenarioEditorSidebarWidget::ObstaclePlacementKindToString(
	const EScenarioTemplateObstaclePlacementKind kind)
{
	switch (kind)
	{
	case EScenarioTemplateObstaclePlacementKind::Fixed:
		return TEXT("fixed");
	case EScenarioTemplateObstaclePlacementKind::Pattern:
		return TEXT("pattern");
	case EScenarioTemplateObstaclePlacementKind::Scatter:
		return TEXT("scatter");
	default:
		return TEXT("unknown");
	}
}

FString UScenarioEditorSidebarWidget::EncounterTypeToString(const EScenarioTemplateEncounterType type)
{
	switch (type)
	{
	case EScenarioTemplateEncounterType::OncomingPass:
		return TEXT("oncoming_pass");
	case EScenarioTemplateEncounterType::Overtake:
		return TEXT("overtake");
	case EScenarioTemplateEncounterType::CrossPath:
		return TEXT("cross_path");
	case EScenarioTemplateEncounterType::StandingGroup:
		return TEXT("standing_group");
	default:
		return TEXT("unknown");
	}
}

FString UScenarioEditorSidebarWidget::FormatNumberValue(
	const FScenarioTemplateNumberValue& value,
	const FString& suffix)
{
	if (!value.bIsSet)
	{
		return TEXT("(unset)");
	}

	if (value.Mode == EScenarioTemplateNumberValueMode::Range)
	{
		return FString::Printf(TEXT("%.2f..%.2f%s"), value.MinValue, value.MaxValue, *suffix);
	}

	return FString::Printf(TEXT("%.2f%s"), value.FixedValue, *suffix);
}

FString UScenarioEditorSidebarWidget::FormatIntegerValue(const FScenarioTemplateIntegerValue& value)
{
	if (!value.bIsSet)
	{
		return TEXT("(unset)");
	}

	if (value.Mode == EScenarioTemplateNumberValueMode::Range)
	{
		return FString::Printf(TEXT("%d..%d"), value.MinValue, value.MaxValue);
	}

	return FString::FromInt(value.FixedValue);
}

FString UScenarioEditorSidebarWidget::FormatStringValue(const FScenarioTemplateStringValue& value)
{
	if (!value.bIsSet)
	{
		return TEXT("(unset)");
	}

	if (value.Mode == EScenarioTemplateStringValueMode::Choices)
	{
		return FString::Printf(TEXT("[%s]"), *FormatStringList(value.Choices));
	}

	return value.FixedValue.IsEmpty() ? FString(TEXT("(empty)")) : value.FixedValue;
}

FString UScenarioEditorSidebarWidget::FormatRobotAnchor(const FScenarioTemplateRobotAnchor& anchor)
{
	if (anchor.Type != EScenarioTemplateRobotAnchorType::CorridorPose)
	{
		return FString::Printf(
			TEXT("%s | heading: %s"),
			*RobotAnchorTypeToString(anchor.Type),
			*RobotHeadingToString(anchor.Heading));
	}

	return FString::Printf(
		TEXT("corridor_pose | segment: %s | along: %s | offset: %s | lane: %s | heading: %s"),
		anchor.SegmentId.IsEmpty() ? TEXT("(unset)") : *anchor.SegmentId,
		*FormatNumberValue(anchor.AlongMeters, TEXT("m")),
		*FormatNumberValue(anchor.OffsetMeters, TEXT("m")),
		anchor.LaneId.IsEmpty() ? TEXT("(unset)") : *anchor.LaneId,
		*RobotHeadingToString(anchor.Heading));
}

FString UScenarioEditorSidebarWidget::FormatLaneRule(const FScenarioTemplateLaneRule& lane)
{
	return FString::Printf(
		TEXT("%s | width: %s"),
		lane.SurfaceId.IsEmpty() ? TEXT("(unset)") : *lane.SurfaceId,
		*FormatNumberValue(lane.WidthMeters, TEXT("m")));
}

FString UScenarioEditorSidebarWidget::FormatStringList(const TArray<FString>& values)
{
	return values.IsEmpty() ? FString(TEXT("(none)")) : FString::Join(values, TEXT(", "));
}

double UScenarioEditorSidebarWidget::MeasureAxisLengthMeters(const TArray<FVector2D>& pointsMeters)
{
	double lengthMeters = 0.0;
	for (int32 index = 1; index < pointsMeters.Num(); ++index)
	{
		lengthMeters += FVector2D::Distance(pointsMeters[index - 1], pointsMeters[index]);
	}
	return lengthMeters;
}
