#pragma once

#include "CoreMinimal.h"
#include "UI/BaseFormElementTypes.h"
#include "UI/BaseWidget.h"
#include "BaseFormElementsGalleryWidget.generated.h"

class UBaseTreeViewWidget;
class UWidget;

// Gallery-only adapter that demonstrates caller-owned tree expansion behavior.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseFormElementsGalleryWidget : public UBaseWidget
{
	GENERATED_BODY()

protected:
	// Captures the WBP-authored full tree sample and binds row expansion events.
	virtual void NativeConstruct() override;

	// Releases row expansion bindings.
	virtual void NativeDestruct() override;

	// Toggles one WBP-authored flattened branch and reapplies the visible row list.
	UFUNCTION()
	void HandleTreeExpansionRequested(UWidget* widget, FName rowId);

	// Applies the current expanded/collapsed flags to the bound gallery tree.
	void ApplyTreeExpansionState();

	// Tree sample authored in the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseTreeViewWidget> GalleryTreeView;

	// Full flattened sample captured from the WBP before local expansion filtering.
	UPROPERTY(Transient)
	TArray<FBaseTreeRowItem> GalleryTreeItems;
};
