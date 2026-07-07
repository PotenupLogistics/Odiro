#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "ProjectAiSuggestionSectionWidget.generated.h"

class UPanelWidget;
class UWidget;

// WBP-editable bullet row used inside one AI suggestion section list.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UProjectAiSuggestionListItemWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// Applies one parsed API list item to the WBP-authored row template.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentResult")
	void InitializeListItem(const FString& itemText);

	// Applies item text from WBP preview properties or runtime parsed data.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentResult")
	void SetItemText(FText itemText);

protected:
	// Applies WBP-authored preview text before runtime data is injected.
	virtual void NativePreConstruct() override;

	// Design-time text shown when this item is used as a WBP placeholder.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ExperimentResult|Preview", meta = (MultiLine = "true"))
	FText PreviewItemText;

private:
	// Applies runtime text to BaseText or native TextBlock widgets.
	static void SetRuntimeText(UWidget* textWidget, const FString& text, bool bAutoWrap = false);

	// Parsed item body display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ItemText;
};

// WBP-editable header/list section used by AI suggestion rows.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UProjectAiSuggestionSectionWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// Applies one parsed API section to the WBP-authored section template.
	UFUNCTION(BlueprintCallable, Category = "Platform|ExperimentResult")
	void InitializeSection(const FString& headerText, const TArray<FString>& listItems);

protected:
	// Applies WBP-authored preview text before runtime data is injected.
	virtual void NativePreConstruct() override;

	// Optional WBP row template for each parsed list item.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ExperimentResult|List")
	TSubclassOf<UProjectAiSuggestionListItemWidget> ListItemWidgetClass;

	// Prefix used only by the text fallback when no item template is configured.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ExperimentResult|List")
	FString FallbackBulletPrefix = TEXT("- ");

	// Design-time section header shown when this section is used as a WBP placeholder.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ExperimentResult|Preview")
	FText PreviewHeaderText;

	// Design-time first list item text forwarded to the WBP-authored item placeholder.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|ExperimentResult|Preview", meta = (MultiLine = "true"))
	FText PreviewListItemText;

private:
	// Applies optional section preview text to the first WBP-authored list item placeholder.
	void ApplyPreviewListItemText();

	// Resolves a WBP-owned list item template from class defaults or design-time children.
	TSubclassOf<UProjectAiSuggestionListItemWidget> ResolveListItemWidgetClass() const;

	// Rebuilds optional WBP-authored item row widgets.
	bool RebuildListItemWidgets(const TArray<FString>& listItems);

	// Builds a compact fallback list when no item row template is configured.
	FString BuildFallbackListText(const TArray<FString>& listItems) const;

	// Applies runtime text to BaseText or native TextBlock widgets.
	static void SetRuntimeText(UWidget* textWidget, const FString& text, bool bAutoWrap = false);

	// Shows or hides one optional WBP-owned widget.
	static void SetOptionalWidgetVisible(UWidget* widget, bool bVisible);

	// Parsed section header display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HeaderText;

	// Compact fallback list display.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ListText;

	// Optional WBP-owned list host for item row templates.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> ListItemBox;
};
