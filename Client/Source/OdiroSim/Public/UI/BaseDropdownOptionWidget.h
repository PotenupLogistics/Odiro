#pragma once

#include "CoreMinimal.h"
#include "UI/BaseButtonWidget.h"
#include "BaseDropdownOptionWidget.generated.h"

class UImage;

// Dropdown option row: a borderless button whose selection is owned by the parent
// dropdown (no self-toggle), showing accent label text plus a check on the
// selected row. The rounded surface belongs to the dropdown list wrapper, not the
// individual rows.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseDropdownOptionWidget : public UBaseButtonWidget
{
	GENERATED_BODY()

public:
	// Creates a chrome-free option row; the dropdown list wrapper owns the panel surface.
	UBaseDropdownOptionWidget(const FObjectInitializer& objectInitializer = FObjectInitializer::Get());

	// Applies the borderless row visuals plus selected accent text and check mark.
	virtual void SynchronizeBaseProperties() override;

protected:
	// Disables CommonUI self-selection so only the dropdown drives the active row.
	virtual void NativeConstruct() override;

	// Optional check mark shown on the selected option row.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> CheckImage;
};
