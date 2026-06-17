#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "ScenarioEditorSidebarFieldRow.generated.h"

class UEditableTextBox;
class UTextBlock;
class SWidget;

// Broadcasts when a Scenario Template field row commits an editable text value.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FScenarioEditorSidebarFieldRowTextCommitted,
	const FText&,
	Text,
	ETextCommit::Type,
	CommitMethod);

// Leaf property row for Scenario Template sidebar fields such as "template_id : value".
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioEditorSidebarFieldRow : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Label displayed on the left side of the field row.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	FString FieldLabel;

	// Current text value shown by either the editable or read-only value control.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	FString ValueText;

	// Controls whether the row should expose the editable text box when one is bound.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Template")
	bool bEditable = true;

	// Optional label text block bound by the UMG row.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UTextBlock> LabelTextBlock;

	// Optional separator text block, normally ":".
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UTextBlock> SeparatorTextBlock;

	// Optional read-only value text block for non-editable rows.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UTextBlock> ValueTextBlock;

	// Optional editable text box for editable rows.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Scenario|Editor|Template")
	TObjectPtr<UEditableTextBox> ValueEditableTextBox;

	// Emits committed text from the editable value control.
	UPROPERTY(BlueprintAssignable, Category = "Scenario|Editor|Template")
	FScenarioEditorSidebarFieldRowTextCommitted OnValueTextCommitted;

	// Updates the row label and refreshes bound controls.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetFieldLabel(const FString& label);

	// Updates the row value and refreshes bound controls.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetValueText(const FString& text);

	// Toggles editable versus read-only value presentation.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Template")
	void SetEditable(bool bInEditable);

	// Returns the value currently displayed by the row.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Template")
	FString GetValueText() const;

private:
	// Handles text commits from the optional editable value control.
	UFUNCTION()
	void HandleValueTextCommitted(const FText& text, ETextCommit::Type commitMethod);

	// Builds the native fallback row tree when no Blueprint-authored tree is present.
	void BuildDefaultWidgetTree();
	// Binds editable control delegates owned by this row.
	void BindControls();
	// Releases editable control delegates owned by this row.
	void UnbindControls();
	// Applies stored label, value, and editability state to bound controls.
	void RefreshRow();
	// Applies text to a bound text block.
	void SetTextBlockText(UTextBlock* textBlock, const FString& text) const;
};
