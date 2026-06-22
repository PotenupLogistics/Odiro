#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ScenarioEditorSidebarBlockWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class UWidget;
class UWidgetTextStyleCatalog;

// Broadcasts the Scenario Template path represented by a selected sidebar block.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FScenarioEditorSidebarBlockSelected,
	const FString&,
	BlockPath);

// Collapsible Scenario Template tree block used by sidebar panels.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioEditorSidebarBlockWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Binds local header controls after UMG construction.
	virtual void NativeConstruct() override;

	// Releases local header control bindings before teardown.
	virtual void NativeDestruct() override;

	// User-facing label shown in the block header.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	FString BlockName;

	// Stable Scenario Template path represented by this block.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	FString BlockPath;

	// Hierarchy badge text such as Main, Template, Property, or Detail.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	FString BadgeText;

	// Whether the child body is currently visible.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	bool bExpanded = true;

	// Whether the block uses the selected outline treatment.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	bool bSelected = false;

	// Whether an unselected block draws its normal outline.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	bool bShowNormalOutline = true;

	// Whether the block is nested inside another Scenario Template block.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	bool bNested = false;

	// Shared typography catalog used by header text.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	TSoftObjectPtr<UWidgetTextStyleCatalog> TextStyleCatalog;

	// Optional WBP-owned outline region for block visuals.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UWidget> OutlineBorder;

	// Optional WBP-owned content region for block visuals.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UWidget> ContentBorder;

	// Optional button that toggles body visibility.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UButton> ToggleButton;

	// Optional text block showing the expanded/collapsed glyph.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UTextBlock> ToggleTextBlock;

	// Optional header text for the block name.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UTextBlock> NameTextBlock;

	// Optional header text for the stable Scenario Template path.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UTextBlock> PathTextBlock;

	// Optional header text for the hierarchy badge.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UTextBlock> BadgeTextBlock;

	// Optional WBP-owned selected-state visual layer.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UWidget> SelectedStateWidget;

	// Optional container that owns child field rows and nested blocks.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UVerticalBox> BodyBox;

	// Emits when the block header toggle is clicked.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarBlockSelected OnBlockSelected;

	// Updates block header metadata and refreshes bound controls.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetBlockMetadata(const FString& name, const FString& path, const FString& badge);

	// Updates expanded state and body visibility.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetExpanded(bool bInExpanded);

	// Updates selected state and outline treatment.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetSelected(bool bInSelected);

	// Updates whether unselected blocks draw their normal outline.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetShowNormalOutline(bool bInShowNormalOutline);

	// Updates nested state and background treatment.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetNested(bool bInNested);

	// Updates the shared typography catalog used by this block.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetTextStyleCatalog(TSoftObjectPtr<UWidgetTextStyleCatalog> catalog);

	// Adds a child widget to the block body.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void AddBodyChild(UWidget* widget);

	// Clears all widgets owned by the block body.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void ClearBodyChildren();

	// Returns the WBP-owned body container.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	UVerticalBox* GetBodyBox();

private:
	// Handles expand/collapse clicks from the optional toggle button.
	UFUNCTION()
	void HandleToggleClicked();

	// Binds optional local controls.
	void BindControls();
	// Releases optional local control bindings.
	void UnbindControls();
	// Applies stored metadata, styles, and state to bound controls.
	void RefreshBlock();
	// Applies text to a bound text block.
	void SetTextBlockText(UTextBlock* textBlock, const FString& text) const;
};
