#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scenario/Widget/ScenarioEditorToolbarWidget.h"
#include "Shared/ScenarioTemplateTypes.h"
#include "ScenarioEditorSidebarWidget.generated.h"

class UScrollBox;
class UTextBlock;
class UScenarioEditorSidebarMainPanel;
class UWidget;
class UWidgetSwitcher;
class UWidgetTextStyleCatalog;
class SWidget;

// Scenario Template block viewer and panel switch host used by the editor side sidebar.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioEditorSidebarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Builds a native sidebar tree when no Blueprint-authored root widget exists.
	virtual TSharedRef<SWidget> RebuildWidget() override;

	// Refreshes the read-only sidebar after UMG construction.
	virtual void NativeConstruct() override;

	// Scenario Template block currently displayed by the sidebar.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	EScenarioTemplateSidebarPanel ActivePanel = EScenarioTemplateSidebarPanel::Main;

	// Shared typography catalog used by the sidebar and child panel widgets.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	TSoftObjectPtr<UWidgetTextStyleCatalog> TextStyleCatalog;

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

	// Optional scroll area that owns the active Scenario Template block panel.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScrollBox> SidebarScrollBox;

	// Optional container that groups legacy read-only summary text widgets.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UWidget> FallbackSummaryContainer;

	// Optional switcher that hosts Main/Corridor/Obstacle/Pedestrian panel widgets.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UWidgetSwitcher> PanelSwitcher;

	// Optional specialized Main panel widget for template metadata and robot anchors.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarMainPanel> MainPanelWidget;

	// Optional specialized Corridor panel placeholder used by the panel switcher.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UWidget> CorridorPanelWidget;

	// Optional specialized Obstacle panel placeholder used by the panel switcher.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UWidget> ObstaclePanelWidget;

	// Optional specialized Pedestrian panel placeholder used by the panel switcher.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UWidget> PedestrianPanelWidget;

	// Selects the Scenario Template block displayed by the sidebar.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetActivePanel(EScenarioTemplateSidebarPanel panel);

	// Updates the shared typography catalog used by this sidebar and its children.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetTextStyleCatalog(TSoftObjectPtr<UWidgetTextStyleCatalog> catalog);

	// Pulls the current draft Scenario Template from the authoring subsystem and renders it.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void RefreshFromDraft();

	// Renders a read-only view of the provided Scenario Template document.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void RefreshFromTemplate(const FScenarioTemplateDocument& scenarioTemplate);

private:
	// Generated Main panel used only when no specialized Main panel is bound.
	UPROPERTY(Transient)
	TObjectPtr<UWidget> GeneratedMainPanelWidget;

	// Generated Corridor panel used only when no Blueprint placeholder is bound.
	UPROPERTY(Transient)
	TObjectPtr<UWidget> GeneratedCorridorPanelWidget;

	// Generated Obstacle panel used only when no Blueprint placeholder is bound.
	UPROPERTY(Transient)
	TObjectPtr<UWidget> GeneratedObstaclePanelWidget;

	// Generated Pedestrian panel used only when no Blueprint placeholder is bound.
	UPROPERTY(Transient)
	TObjectPtr<UWidget> GeneratedPedestrianPanelWidget;

	// Builds the native sidebar tree that mirrors the HTML discussion prototype.
	void BuildDefaultWidgetTree();
	// Applies scroll box defaults shared by native and Blueprint-authored trees.
	void ConfigureScrollBox() const;
	// Wraps a Blueprint-authored PanelSwitcher in a generated scroll area when no scroll box is bound.
	bool EnsurePanelSwitcherIsScrollable();
	// Rebuilds generated block content for placeholder panel widgets.
	void RefreshGeneratedPanelContent(const FScenarioTemplateDocument& scenarioTemplate);
	// Creates or returns the generated fallback widget for one panel.
	UWidget* EnsureGeneratedPanelWidget(EScenarioTemplateSidebarPanel panel);
	// Returns the generated fallback widget for one panel without creating it.
	UWidget* ResolveGeneratedPanelWidget(EScenarioTemplateSidebarPanel panel) const;
	// Replaces a panel placeholder's content with generated block content.
	bool ApplyGeneratedContentToPanelWidget(UWidget* panelWidget, UWidget* contentWidget) const;
	// Builds one generated block panel for the active template data.
	UWidget* BuildGeneratedPanelContent(
		EScenarioTemplateSidebarPanel panel,
		const FScenarioTemplateDocument& scenarioTemplate);
	// Applies the active panel to the optional UMG widget switcher.
	void RefreshPanelSwitcher();
	// Applies a small inset so panel outlines are not clipped by the switcher viewport.
	void ApplyPanelContentPadding(UWidget* panelWidget) const;
	// Shows read-only summary text when the active panel has no specialized widget yet.
	void RefreshFallbackTextVisibility() const;
	// Applies one visibility state to the legacy read-only summary area.
	void SetFallbackTextVisibility(ESlateVisibility visibility) const;
	// Resolves the switcher child widget for one sidebar panel.
	UWidget* ResolvePanelWidget(EScenarioTemplateSidebarPanel panel) const;
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
	// Applies shared typography to title, summary text, and child panel widgets.
	void ApplyTextStyles();
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
