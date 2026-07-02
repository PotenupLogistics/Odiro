#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ScenarioEditorSidebarBlockWidget.generated.h"

class UButton;
class UHorizontalBox;
class UImage;
class USpacer;
class UTextBlock;
class UVerticalBox;
class UWidget;
class UWidgetTextStyleCatalog;
class UTexture2D;

// Broadcasts the Scenario Template path represented by a selected sidebar block.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FScenarioEditorSidebarBlockSelected,
	const FString&,
	BlockPath);

// Broadcasts a structural action requested from a sidebar block header.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FScenarioEditorSidebarBlockActionRequested);

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

	// Observes block clicks before child controls so selection can follow body-row interactions.
	virtual FReply NativeOnPreviewMouseButtonDown(
		const FGeometry& inGeometry,
		const FPointerEvent& inMouseEvent) override;

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

	// Whether this block should render as a focused detail root without hierarchy indentation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	bool bFocusedDetailLayout = false;

	// Whether this block should render only its body as an invisible detail host.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	bool bDetailHostLayout = false;

	// Whether this block header uses asset thumbnail and two-line summary text.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	bool bAssetHeaderSummaryVisible = false;

	// Whether the stable Scenario Template path is shown in the header.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	bool bPathTextVisible = false;

	// Whether the header exposes an add action for this block's collection.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	bool bAddActionVisible = false;

	// Whether the header exposes a remove action for this block's item.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	bool bRemoveActionVisible = false;

	// Shared typography catalog used by header text.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	TSoftObjectPtr<UWidgetTextStyleCatalog> TextStyleCatalog;

	// Optional WBP-owned outline region for block visuals.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UWidget> OutlineBorder;

	// Optional WBP-owned content region for block visuals.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UWidget> ContentBorder;

	// Optional WBP-owned header row used for block spacing.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UWidget> BlockHeaderRow;

	// Optional button that toggles body visibility.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UButton> ToggleButton;

	// Optional text block showing the expanded/collapsed glyph.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UTextBlock> ToggleTextBlock;

	// Optional image showing the expanded/collapsed glyph when the WBP owns icon binding.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UImage> ToggleIconImage;

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

	// Emits when the block header or background is selected.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarBlockSelected OnBlockSelected;

	// Emits when the block header add action is clicked.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarBlockActionRequested OnAddActionRequested;

	// Emits when the block header remove action is clicked.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarBlockActionRequested OnRemoveActionRequested;

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

	// Updates focused detail layout state and removes hierarchy indentation while active.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetFocusedDetailLayout(bool bInFocusedDetailLayout);

	// Updates detail host layout state for wrapper blocks that should not be visually exposed.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetDetailHostLayout(bool bInDetailHostLayout);

	// Updates whether the header add action is visible.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetAddActionVisible(bool bInAddActionVisible);

	// Updates whether the header remove action is visible.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetRemoveActionVisible(bool bInRemoveActionVisible);

	// Updates whether the header displays its stable Scenario Template path.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetPathTextVisible(bool bInPathTextVisible);

	// Updates the shared typography catalog used by this block.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetTextStyleCatalog(TSoftObjectPtr<UWidgetTextStyleCatalog> catalog);

	// Updates the optional asset summary header used by object detail blocks.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetAssetHeaderSummary(
		const FText& typeText,
		const FText& nameText,
		TSoftObjectPtr<UTexture2D> thumbnailTexture,
		bool bVisible);

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

	// Handles add action clicks from the generated header button.
	UFUNCTION()
	void HandleAddActionClicked();

	// Handles remove action clicks from the generated header button.
	UFUNCTION()
	void HandleRemoveActionClicked();

	// Generated add button owned by the header row when action visibility requires it.
	UPROPERTY(Transient)
	TObjectPtr<UButton> AddActionButton;

	// Generated add button text owned by AddActionButton.
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AddActionTextBlock;

	// Generated add button icon owned by AddActionButton.
	UPROPERTY(Transient)
	TObjectPtr<UImage> AddActionIconImage;

	// Generated remove button owned by the header row when action visibility requires it.
	UPROPERTY(Transient)
	TObjectPtr<UButton> RemoveActionButton;

	// Generated remove button text owned by RemoveActionButton.
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RemoveActionTextBlock;

	// Generated remove button icon owned by RemoveActionButton.
	UPROPERTY(Transient)
	TObjectPtr<UImage> RemoveActionIconImage;

	// Generated fill spacer that pushes header actions to the right edge.
	UPROPERTY(Transient)
	TObjectPtr<USpacer> HeaderActionSpacer;

	// Generated container that owns add/remove actions at the header right edge.
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> HeaderActionBox;

	// Generated asset summary container owned by the header row.
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> AssetHeaderContainer;

	// Generated thumbnail image shown by the asset summary header.
	UPROPERTY(Transient)
	TObjectPtr<UImage> AssetHeaderThumbnailImage;

	// Generated vertical text stack owned by AssetHeaderContainer.
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> AssetHeaderTextBox;

	// Generated first-line asset kind text.
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AssetHeaderTypeTextBlock;

	// Generated second-line object name text.
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AssetHeaderNameTextBlock;

	// Cached asset kind text for the generated asset summary header.
	UPROPERTY(Transient)
	FText AssetHeaderTypeText;

	// Cached object name text for the generated asset summary header.
	UPROPERTY(Transient)
	FText AssetHeaderNameText;

	// Cached thumbnail texture for the generated asset summary header.
	UPROPERTY(Transient)
	TSoftObjectPtr<UTexture2D> AssetHeaderThumbnailTexture;

	// Broadcasts this block path as the active sidebar selection.
	void BroadcastBlockSelected();
	// Returns true when this block should claim a click before child controls handle it.
	bool ShouldBroadcastSelectionForPointer(const FPointerEvent& mouseEvent) const;
	// Binds optional local controls.
	void BindControls();
	// Releases optional local control bindings.
	void UnbindControls();
	// Creates generated header action buttons when the WBP header can host them.
	void EnsureActionButtons();
	// Creates the right-aligned header action container.
	void EnsureHeaderActionContainer();
	// Replaces the optional text toggle glyph with the configured icon image.
	void EnsureToggleIcon();
	// Creates the generated asset summary header when the WBP header can host it.
	void EnsureAssetHeaderSummary();
	// Creates one generated header button and attaches it to the header row.
	void CreateActionButton(
		TObjectPtr<UButton>& outButton,
		TObjectPtr<UTextBlock>& outTextBlock,
		TObjectPtr<UImage>& outIconImage,
		const TCHAR* iconPath);
	// Applies visibility and label state to one generated header button.
	void SetActionButtonState(
		UButton* button,
		UTextBlock* textBlock,
		UImage* iconImage,
		bool bVisible,
		const FString& label,
		const TCHAR* iconPath) const;
	// Applies visibility state to the generated header action container.
	void SetHeaderActionContainerVisible(bool bVisible) const;
	// Applies expanded/collapsed icon and flat button style to the header toggle.
	void ApplyToggleButtonState() const;
	// Applies cached asset summary data to generated header widgets.
	void ApplyAssetHeaderSummaryState();
	// Applies shared sidebar spacing and block surface styling.
	void ApplyVisualStyle();
	// Applies stored metadata, styles, and state to bound controls.
	void RefreshBlock();
	// Applies text to a bound text block.
	void SetTextBlockText(UTextBlock* textBlock, const FString& text) const;
};
