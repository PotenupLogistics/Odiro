#pragma once

#include "CoreMinimal.h"
#include "UI/BaseFormElementTypes.h"
#include "UI/BaseWidget.h"
#include "BaseSwitcherWidget.generated.h"

class UBaseButtonWidget;
class UBaseToggleButtonWidget;
class UPanelWidget;

// Segmented single-selection control backed by stable item ids.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseSwitcherWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Applies item and selection state to bound segment widgets.
	virtual void SynchronizeBaseProperties() override;

	// Replaces the available segment items.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Switcher")
	void SetItems(const TArray<FBaseSwitcherItem>& inItems);

	// Returns the available segment items.
	UFUNCTION(BlueprintPure, Category = "UI|Base Switcher")
	const TArray<FBaseSwitcherItem>& GetItems() const { return Items; }

	// Selects an enabled item by stable id.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Switcher")
	bool SelectItemById(FName itemId);

	// Returns the selected item id.
	UFUNCTION(BlueprintPure, Category = "UI|Base Switcher")
	FName GetSelectedId() const { return SelectedId; }

	// Updates whether the switcher is disabled.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Switcher")
	void SetDisabled(bool bInDisabled);

	// Returns whether the switcher is disabled.
	UFUNCTION(BlueprintPure, Category = "UI|Base Switcher")
	bool IsDisabled() const { return bDisabled; }

	// Broadcasts after selection changes.
	UPROPERTY(BlueprintAssignable, Category = "UI|Base Switcher|Events")
	FBaseSelectionChangedEvent OnSelectionChanged;

protected:
	// Rebuilds generated segment buttons when a segment container is present.
	void RebuildSegments();

	// Updates generated segment button state without replacing the widget tree.
	void RefreshSegments();

	// Handles generated segment button clicks.
	UFUNCTION()
	void HandleSegmentClicked(UBaseButtonWidget* button);

	// Available segment items.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Switcher", meta = (ExposeOnSpawn = "true"))
	TArray<FBaseSwitcherItem> Items;

	// Stable id for the selected item.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetSelectedId", Category = "UI|Base Switcher", meta = (ExposeOnSpawn = "true"))
	FName SelectedId;

	// Disabled switcher state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsDisabled", Setter = "SetDisabled", BlueprintGetter = "IsDisabled", BlueprintSetter = "SetDisabled", Category = "UI|Base Switcher", meta = (ExposeOnSpawn = "true"))
	bool bDisabled = false;

	// Widget class used for generated segment buttons.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Switcher")
	TSubclassOf<UBaseToggleButtonWidget> SegmentWidgetClass;

	// Panel that receives generated segment buttons.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> SegmentContainer;

	// Stable ids for generated segment widgets.
	TMap<TWeakObjectPtr<UBaseToggleButtonWidget>, FName> SegmentIdByWidget;
};
