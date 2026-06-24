#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "ScenarioEditorOutlinerRowWidget.generated.h"

class UBorder;
class UButton;
class UScenarioEditorListItemViewModel;
class USpacer;
class UTextBlock;
class UWidget;
class UWidgetTextStyleCatalog;

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
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Shared typography catalog retained for older assets; WBP owns visible text style.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Outliner")
	TSoftObjectPtr<UWidgetTextStyleCatalog> TextStyleCatalog;

	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Outliner")
	FScenarioOutlinerRowItemEvent OnRowSelected;

	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Outliner")
	FScenarioOutlinerRowItemEvent OnRowExpansionToggled;

	// Initializes row text, depth, selection state, and semantic payload.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Outliner")
	void InitializeRow(const FScenarioOutlinerItemViewModel& viewModel);

	// Initializes row display from the subsystem-owned item ViewModel.
	void InitializeFromItemViewModel(UScenarioEditorListItemViewModel* itemViewModel);

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
	void RefreshRow();

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

	// Indentation applied per view-model depth; value is configured by the row WBP.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Outliner", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float IndentPerDepth = 14.0f;

	UPROPERTY(Transient)
	FScenarioOutlinerItemViewModel Item;

	// Row 표시 데이터를 제공하는 outliner item ViewModel 참조.
	UPROPERTY(Transient)
	TObjectPtr<UScenarioEditorListItemViewModel> ItemViewModel;
};
