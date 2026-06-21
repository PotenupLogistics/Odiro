#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "ScenarioEditorOutlinerRowWidget.generated.h"

class UBorder;
class UButton;
class USpacer;
class UTextBlock;
class UWidget;
class UWidgetTextStyleCatalog;
class SWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FScenarioOutlinerRowItemEvent,
	FScenarioOutlinerItemViewModel,
	Item);

// Single selectable row used by the Scenario Editor outliner.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioEditorOutlinerRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Shared typography catalog used by native fallback rows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Outliner")
	TSoftObjectPtr<UWidgetTextStyleCatalog> TextStyleCatalog;

	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Outliner")
	FScenarioOutlinerRowItemEvent OnRowSelected;

	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Outliner")
	FScenarioOutlinerRowItemEvent OnRowExpansionToggled;

	// Initializes row text, depth, selection state, and semantic payload.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Outliner")
	void InitializeRow(const FScenarioOutlinerItemViewModel& viewModel);

	// Applies the selected visual state owned by the row widget.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Outliner")
	void SetSelected(bool bInSelected);

	// Applies the expanded glyph state owned by the row widget.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Outliner")
	void SetExpanded(bool bInExpanded);

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Outliner")
	FScenarioOutlinerItemViewModel GetItem() const { return Item; }

private:
	UFUNCTION()
	void HandleRowClicked();

	UFUNCTION()
	void HandleExpandClicked();

	void BindControls();
	void UnbindControls();
	void BuildDefaultWidgetTree();
	void RefreshRow();
	void ApplyTextStyles() const;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|Editor|Outliner", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UBorder> SelectionBorder;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|Editor|Outliner", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> RowButton;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|Editor|Outliner", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> ExpandButton;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|Editor|Outliner", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ExpandGlyphText;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|Editor|Outliner", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ItemLabelText;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|Editor|Outliner", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ItemSubtitleText;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|Editor|Outliner", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<USpacer> RowIndentSpacer;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|Editor|Outliner", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> SelectionIndicator;

	UPROPERTY(Transient)
	FScenarioOutlinerItemViewModel Item;
};
