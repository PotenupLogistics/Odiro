#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scenario/Widget/ScenarioEditorToolbarWidget.h"
#include "Shared/ScenarioTemplateTypes.h"
#include "ScenarioTemplateSidebarWidget.generated.h"

class UTextBlock;

// Read-only Scenario Template block viewer used by the editor side sidebar.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioTemplateSidebarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Refreshes the read-only sidebar after UMG construction.
	virtual void NativeConstruct() override;

	// Scenario Template block currently displayed by the sidebar.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	EScenarioTemplateSidebarPanel ActivePanel = EScenarioTemplateSidebarPanel::Main;

	// Optional title text for the active Scenario Template block.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UTextBlock> PanelTitleTextBlock;

	// Optional field group for the most important active-block fields.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UTextBlock> PrimaryFieldsTextBlock;

	// Optional field group for secondary active-block fields.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UTextBlock> SecondaryFieldsTextBlock;

	// Optional list-style summary for repeated active-block items.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UTextBlock> ListSummaryTextBlock;

	// Optional status text for missing draft or sidebar binding diagnostics.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UTextBlock> DiagnosticsTextBlock;

	// Selects the Scenario Template block displayed by the read-only sidebar.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetActivePanel(EScenarioTemplateSidebarPanel panel);

	// Pulls the current draft Scenario Template from the authoring subsystem and renders it.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void RefreshFromDraft();

	// Renders a read-only view of the provided Scenario Template document.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void RefreshFromTemplate(const FScenarioTemplateDocument& scenarioTemplate);

private:
	// Builds read-only text for root template metadata and robot anchors.
	void BuildMainPanelText(
		const FScenarioTemplateDocument& scenarioTemplate,
		FString& outPrimaryText,
		FString& outSecondaryText,
		FString& outListText) const;
	// Builds read-only text for Corridor axis, side lanes, and semantic segments.
	void BuildCorridorPanelText(
		const FScenarioTemplateDocument& scenarioTemplate,
		FString& outPrimaryText,
		FString& outSecondaryText,
		FString& outListText) const;
	// Builds read-only text for obstacle rules and placement blocks.
	void BuildObstaclePanelText(
		const FScenarioTemplateDocument& scenarioTemplate,
		FString& outPrimaryText,
		FString& outSecondaryText,
		FString& outListText) const;
	// Builds read-only text for pedestrian background and encounter rules.
	void BuildPedestrianPanelText(
		const FScenarioTemplateDocument& scenarioTemplate,
		FString& outPrimaryText,
		FString& outSecondaryText,
		FString& outListText) const;
	// Applies the resolved sidebar text to optional UMG text blocks.
	void SetSidebarText(
		const FString& title,
		const FString& primaryText,
		const FString& secondaryText,
		const FString& listText,
		const FString& diagnosticsText);
	// Applies text to a bound text block when it exists.
	void SetTextBlockText(UTextBlock* textBlock, const FString& text) const;
	// Returns the display title for one template sidebar panel.
	static FString PanelToTitle(EScenarioTemplateSidebarPanel panel);
	// Returns a stable label for a robot anchor type.
	static FString RobotAnchorTypeToString(EScenarioTemplateRobotAnchorType type);
	// Returns a stable label for a robot heading hint.
	static FString RobotHeadingToString(EScenarioTemplateRobotHeading heading);
	// Returns a stable label for a corridor segment type.
	static FString SegmentTypeToString(EScenarioTemplateSegmentType type);
	// Returns a stable label for an obstacle placement kind.
	static FString ObstaclePlacementKindToString(EScenarioTemplateObstaclePlacementKind kind);
	// Returns a stable label for a pedestrian encounter type.
	static FString EncounterTypeToString(EScenarioTemplateEncounterType type);
	// Formats one authored numeric value for read-only display.
	static FString FormatNumberValue(const FScenarioTemplateNumberValue& value, const FString& suffix = FString());
	// Formats one authored integer value for read-only display.
	static FString FormatIntegerValue(const FScenarioTemplateIntegerValue& value);
	// Formats one authored string value for read-only display.
	static FString FormatStringValue(const FScenarioTemplateStringValue& value);
	// Formats a robot anchor in template-space terms.
	static FString FormatRobotAnchor(const FScenarioTemplateRobotAnchor& anchor);
	// Formats a corridor lane rule.
	static FString FormatLaneRule(const FScenarioTemplateLaneRule& lane);
	// Formats a comma-separated string list.
	static FString FormatStringList(const TArray<FString>& values);
	// Measures the authored corridor polyline in meters.
	static double MeasureAxisLengthMeters(const TArray<FVector2D>& pointsMeters);
};
