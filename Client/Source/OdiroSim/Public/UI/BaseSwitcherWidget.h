#pragma once

#include "CoreMinimal.h"
#include "UI/BaseFormElementTypes.h"
#include "UI/BaseWidget.h"
#include "BaseSwitcherWidget.generated.h"

class UBaseButtonWidget;
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
	UPROPERTY(BlueprintAssignable, Category = "UI|Events")
	FBaseSelectionChangedEvent OnSelectionChanged;

protected:
	// Removes generated segment bindings before destruction.
	virtual void NativeDestruct() override;

	// Returns explicit items or design-time examples for an empty designer preview.
	TArray<FBaseSwitcherItem> BuildRenderedItems() const;

	// Rebuilds generated segment buttons when a segment container is present.
	void RebuildSegments(const TArray<FBaseSwitcherItem>& renderedItems);

	// Resolves the segment class, using the icon-capable base button for icon rows.
	TSubclassOf<UBaseButtonWidget> ResolveSegmentWidgetClass(bool bNeedsIcon) const;

	// Updates generated segment button state without replacing the widget tree.
	void RefreshSegments(const TArray<FBaseSwitcherItem>& renderedItems);

	// Removes click bindings from generated segment buttons before tree replacement or destruction.
	void UnbindGeneratedSegments();

	// Handles generated segment button clicks.
	UFUNCTION()
	void HandleSegmentClicked(UBaseButtonWidget* button);

	// Available segment items.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	TArray<FBaseSwitcherItem> Items;

	// Stable id for the selected item.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetSelectedId", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	FName SelectedId;

	// Disabled switcher state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsDisabled", Setter = "SetDisabled", BlueprintGetter = "IsDisabled", BlueprintSetter = "SetDisabled", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	bool bDisabled = false;

	// Widget class used for generated segment buttons.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Classes")
	TSubclassOf<UBaseButtonWidget> SegmentWidgetClass;

	// Panel that receives generated segment buttons.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> SegmentContainer;

	// Stable ids aligned with generated segment child indices.
	TArray<FName> SegmentIdsByChildIndex;
};
