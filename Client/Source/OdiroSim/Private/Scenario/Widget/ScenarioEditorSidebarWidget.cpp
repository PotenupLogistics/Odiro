#include "Scenario/Widget/ScenarioEditorSidebarWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/World.h"
#include "Scenario/Editor/ScenarioAuthoringSubsystem.h"
#include "Scenario/Widget/ScenarioEditorSidebarMainPanel.h"
#include "Widget/WidgetTextStyleCatalog.h"

namespace
{
	FString JoinLines(const TArray<FString>& lines)
	{
		return lines.IsEmpty() ? FString(TEXT("None")) : FString::Join(lines, TEXT("\n"));
	}

	FString FormatMeters(const double value)
	{
		return FString::Printf(TEXT("%.2fm"), value);
	}
}

void UScenarioEditorSidebarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ApplyTextStyles();
	RefreshFromDraft();
}

void UScenarioEditorSidebarWidget::SetActivePanel(const EScenarioTemplateSidebarPanel panel)
{
	ActivePanel = panel;
	RefreshFromDraft();
}

void UScenarioEditorSidebarWidget::SetTextStyleCatalog(
	TSoftObjectPtr<UWidgetTextStyleCatalog> catalog)
{
	TextStyleCatalog = catalog;
	ApplyTextStyles();
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

	RefreshFromTemplate(authoringSubsystem->GetDraftScenarioTemplate());
}

void UScenarioEditorSidebarWidget::RefreshFromTemplate(const FScenarioTemplateDocument& scenarioTemplate)
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
	if (MainPanelWidget)
	{
		MainPanelWidget->SetTextStyleCatalog(TextStyleCatalog);
		MainPanelWidget->RefreshFromTemplate(scenarioTemplate);
	}
	RefreshPanelSwitcher();
	RefreshFallbackTextVisibility();
	ApplyTextStyles();
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
	const bool bHasSpecializedMainPanel = ActivePanel == EScenarioTemplateSidebarPanel::Main
		&& PanelSwitcher
		&& MainPanelWidget;
	const ESlateVisibility visibility = bHasSpecializedMainPanel
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
		return Cast<UWidget>(MainPanelWidget.Get());
	case EScenarioTemplateSidebarPanel::Corridor:
		return CorridorPanelWidget.Get();
	case EScenarioTemplateSidebarPanel::Obstacle:
		return ObstaclePanelWidget.Get();
	case EScenarioTemplateSidebarPanel::Pedestrian:
		return PedestrianPanelWidget.Get();
	default:
		return nullptr;
	}
}

void UScenarioEditorSidebarWidget::BuildMainPanelText(
	const FScenarioTemplateDocument& scenarioTemplate,
	FString& outPrimaryText,
	FString& outSecondaryText,
	FString& outListText) const
{
	outPrimaryText = FString::Printf(
		TEXT("schema: %s\nversion: %d\ntemplate_id: %s"),
		*scenarioTemplate.Schema,
		scenarioTemplate.Version,
		scenarioTemplate.TemplateId.IsEmpty() ? TEXT("(unset)") : *scenarioTemplate.TemplateId);

	outSecondaryText = FString::Printf(
		TEXT("intent: %s"),
		scenarioTemplate.Intent.IsEmpty() ? TEXT("(unset)") : *scenarioTemplate.Intent);

	TArray<FString> robotLines;
	robotLines.Add(FString::Printf(TEXT("start: %s"), *FormatRobotAnchor(scenarioTemplate.Robot.Start)));
	robotLines.Add(FString::Printf(TEXT("goal: %s"), *FormatRobotAnchor(scenarioTemplate.Robot.Goal)));
	outListText = JoinLines(robotLines);
}

void UScenarioEditorSidebarWidget::BuildCorridorPanelText(
	const FScenarioTemplateDocument& scenarioTemplate,
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
	const FScenarioTemplateDocument& scenarioTemplate,
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
	const FScenarioTemplateDocument& scenarioTemplate,
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

void UScenarioEditorSidebarWidget::ApplyTextStyles()
{
	ApplyTextBlockStyle(PanelTitleTextBlock.Get(), EWidgetTextStyleRole::Title);
	ApplyTextBlockStyle(PrimaryFieldsTextBlock.Get(), EWidgetTextStyleRole::Value);
	ApplyTextBlockStyle(SecondaryFieldsTextBlock.Get(), EWidgetTextStyleRole::Value);
	ApplyTextBlockStyle(ListSummaryTextBlock.Get(), EWidgetTextStyleRole::Value);
	ApplyTextBlockStyle(DiagnosticsTextBlock.Get(), EWidgetTextStyleRole::Value);
	if (MainPanelWidget)
	{
		MainPanelWidget->SetTextStyleCatalog(TextStyleCatalog);
	}
}

FWidgetTextStyle UScenarioEditorSidebarWidget::ResolveTextStyle(
	const EWidgetTextStyleRole role) const
{
	if (const UWidgetTextStyleCatalog* catalog = TextStyleCatalog.LoadSynchronous())
	{
		return catalog->GetStyle(role);
	}

	TSoftObjectPtr<UWidgetTextStyleCatalog> defaultCatalog =
		UWidgetTextStyleCatalog::MakeDefaultCatalogReference();
	if (const UWidgetTextStyleCatalog* catalog = defaultCatalog.LoadSynchronous())
	{
		return catalog->GetStyle(role);
	}

	return UWidgetTextStyleCatalog::MakeDefaultStyle(role);
}

void UScenarioEditorSidebarWidget::ApplyTextBlockStyle(
	UTextBlock* textBlock,
	const EWidgetTextStyleRole role) const
{
	if (!textBlock)
	{
		return;
	}

	const FWidgetTextStyle style = ResolveTextStyle(role);
	textBlock->SetFont(style.Font);
	textBlock->SetColorAndOpacity(FSlateColor(style.Color));
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
