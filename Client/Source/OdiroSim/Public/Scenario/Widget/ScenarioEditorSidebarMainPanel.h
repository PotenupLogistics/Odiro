#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "Shared/ScenarioTemplateTypes.h"
#include "ScenarioEditorSidebarMainPanel.generated.h"

class UTextBlock;
class UScenarioAuthoringSubsystem;
class UScenarioEditorSidebarFieldRow;
class SWidget;

// Main Scenario Template sidebar panel for template metadata and robot anchors.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioEditorSidebarMainPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Optional editable row for scenario_template.template_id.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> TemplateIdFieldRow;

	// Optional read-only row for scenario_template.version.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> VersionFieldRow;

	// Optional editable row for scenario_template.intent.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> IntentFieldRow;

	// Optional read-only row summarizing the robot start anchor.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> RobotStartFieldRow;

	// Optional read-only row summarizing the robot goal anchor.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UScenarioEditorSidebarFieldRow> RobotGoalFieldRow;

	// Optional diagnostics text for rejected metadata edits.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UTextBlock> DiagnosticsTextBlock;

	// Pulls the current draft Scenario Template and refreshes this panel.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void RefreshFromDraft();

	// Refreshes this panel from the provided Scenario Template document.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void RefreshFromTemplate(const FScenarioTemplateDocument& scenarioTemplate);

private:
	// Handles template_id edits committed by the field row.
	UFUNCTION()
	void HandleTemplateIdCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Handles intent edits committed by the field row.
	UFUNCTION()
	void HandleIntentCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Builds the native fallback panel tree when no Blueprint-authored tree is present.
	void BuildDefaultWidgetTree();
	// Binds child field row delegates owned by this panel.
	void BindFieldRows();
	// Releases child field row delegates owned by this panel.
	void UnbindFieldRows();
	// Applies static labels and editability to field rows.
	void ConfigureFieldRows();
	// Resolves the authoring subsystem that owns the draft template.
	UScenarioAuthoringSubsystem* GetAuthoringSubsystem() const;
	// Commits a template_id edit to the draft template.
	void CommitTemplateIdText(const FText& text);
	// Commits an intent edit to the draft template.
	void CommitIntentText(const FText& text);
	// Applies diagnostics to the optional diagnostics text block.
	void SetDiagnosticsText(const FString& text) const;
	// Returns a stable label for a robot anchor type.
	static FString RobotAnchorTypeToString(EScenarioTemplateRobotAnchorType type);
	// Returns a stable label for a robot heading hint.
	static FString RobotHeadingToString(EScenarioTemplateRobotHeading heading);
	// Formats one authored numeric value for compact display.
	static FString FormatNumberValue(const FScenarioTemplateNumberValue& value, const FString& suffix = FString());
	// Formats a robot anchor in template-space terms.
	static FString FormatRobotAnchor(const FScenarioTemplateRobotAnchor& anchor);
};
