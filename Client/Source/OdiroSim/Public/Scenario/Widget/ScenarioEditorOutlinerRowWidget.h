#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "ScenarioEditorOutlinerRowWidget.generated.h"

class UBorder;
class UButton;
class UHorizontalBox;
class UImage;
class UScenarioEditorListItemViewModel;
class USpacer;
class UTextBlock;
class UTexture2D;
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
	// Creates default icon references used when the row WBP has no custom overrides.
	explicit UScenarioEditorOutlinerRowWidget(const FObjectInitializer& objectInitializer);

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
	// Finds the WBP-owned image inside ExpandButton so layout and styling stay Blueprint-authored.
	void EnsureExpandIconImage();
	// Applies the current expanded/collapsed icon state to the WBP-authored expand control.
	void ApplyExpandButtonState();
	// Resolves the authored or runtime-created semantic icon image for this row.
	UImage* ResolveOutlinerIconImage();
	// Resolves the semantic icon texture from the row group or actor category.
	UTexture2D* ResolveIconTexture() const;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|Editor|Outliner", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UBorder> SelectionBorder;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|Editor|Outliner", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> RowButton;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|Editor|Outliner", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> ExpandButton;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|Editor|Outliner", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ExpandGlyphText;

	// Optional WBP-owned image shown inside ExpandButton.
	UPROPERTY(BlueprintReadOnly, Category = "Scenario|Editor|Outliner", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> ExpandIconImage;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|Editor|Outliner", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ItemLabelText;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|Editor|Outliner", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ItemSubtitleText;

	// Optional icon image authored by WBP; created at runtime when the older WBP lacks the slot.
	UPROPERTY(BlueprintReadOnly, Category = "Scenario|Editor|Outliner", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> OutlinerIconImage;

	// Existing horizontal text row used as an insertion point for runtime icon fallback.
	UPROPERTY(BlueprintReadOnly, Category = "Scenario|Editor|Outliner", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UHorizontalBox> OutlinerRowTextBox;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|Editor|Outliner", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<USpacer> RowIndentSpacer;

	UPROPERTY(BlueprintReadOnly, Category = "Scenario|Editor|Outliner", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> SelectionIndicator;

	// Indentation applied per view-model depth; value is configured by the row WBP.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Outliner", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float IndentPerDepth = 14.0f;

	// Fixed pixel size for the outliner semantic icon.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Outliner", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float OutlinerIconSize = 14.0f;

	// Icon shown by ExpandButton when this row is expanded.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Style", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> ExpandedExpandButtonIconTexture;

	// Icon shown by ExpandButton when this row is collapsed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Style", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> CollapsedExpandButtonIconTexture;

	// Icon texture for corridor rows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Outliner", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> CorridorIcon;

	// Icon texture for robot rows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Outliner", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> RobotIcon;

	// Icon texture for obstacle rows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Outliner", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> ObstacleIcon;

	// Icon texture for pedestrian rows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Outliner", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> PedestrianIcon;

	UPROPERTY(Transient)
	FScenarioOutlinerItemViewModel Item;

	// Row 표시 데이터를 제공하는 outliner item ViewModel 참조.
	UPROPERTY(Transient)
	TObjectPtr<UScenarioEditorListItemViewModel> ItemViewModel;
};
