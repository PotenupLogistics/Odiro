#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "ExperimentResultIterationSelectorWidget.generated.h"

class UButton;
class UOdiroListItemViewModel;
class UTextBlock;
class UWidget;
class UExperimentResultIterationSelectorWidget;

// Native click signal emitted by the selector adapter for its parent surface.
DECLARE_MULTICAST_DELEGATE_OneParam(
	FExperimentResultIterationSelectorNative,
	UExperimentResultIterationSelectorWidget*);

// Project result episode selector row/card adapter; layout and style live in WBP.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UExperimentResultIterationSelectorWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// Broadcasts this selector when the WBP button is clicked.
	FExperimentResultIterationSelectorNative OnSelectorClicked;

	// Injects the item ViewModel represented by this selector.
	UFUNCTION(BlueprintCallable, Category = "Platform|ProjectResult")
	void InitializeFromItemViewModel(UOdiroListItemViewModel* itemViewModel);

	// Item ViewModel currently represented by this selector.
	UFUNCTION(BlueprintPure, Category = "Platform|ProjectResult")
	UOdiroListItemViewModel* GetItemViewModel() const { return ItemViewModel; }

	// Result JSON path represented by this selector.
	UFUNCTION(BlueprintPure, Category = "Platform|ProjectResult")
	FString GetResultPath() const;

protected:
	// Binds native click forwarding after WBP children are initialized.
	virtual void NativeOnInitialized() override;

	// Releases native click forwarding and external subscribers before teardown.
	virtual void NativeDestruct() override;

private:
	// Forwards the bound WBP button click to the parent widget.
	UFUNCTION()
	void HandleSelectorButtonClicked();

	// Applies item ViewModel display state to the bound WBP widgets.
	void RefreshFromItemViewModel();

	// Selector click target owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> SelectorButton;

	// Label text owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SelectorLabelText;

	// Optional visual state marker owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SelectedIndicator;

	// Item state injected by the parent ViewModel/widget.
	UPROPERTY(Transient)
	TObjectPtr<UOdiroListItemViewModel> ItemViewModel;
};
