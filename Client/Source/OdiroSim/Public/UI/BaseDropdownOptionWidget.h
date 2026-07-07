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

	// Keeps option typography authored in WBP while base sync still owns runtime text and state.
	virtual bool ShouldApplyLabelTextStyle() const override;

	// Whether the hovered row fill color is owned by this WBP instead of the color catalog.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Option Style", meta = (ExposeOnSpawn = "true"))
	bool bUseOptionHoverFillColorOverride = false;

	// WBP-authored hovered row fill color used when enabled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Option Style", meta = (EditCondition = "bUseOptionHoverFillColorOverride", ExposeOnSpawn = "true"))
	FLinearColor OptionHoverFillColorOverride = FLinearColor::Transparent;

	// Whether the active/selected row fill color is owned by this WBP instead of the color catalog.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Option Style", meta = (ExposeOnSpawn = "true"))
	bool bUseOptionActiveFillColorOverride = false;

	// WBP-authored active/selected row fill color used when enabled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Option Style", meta = (EditCondition = "bUseOptionActiveFillColorOverride", ExposeOnSpawn = "true"))
	FLinearColor OptionActiveFillColorOverride = FLinearColor::Transparent;

	// Keeps selected option text on the normal label color instead of the accent color.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Option Style", meta = (ExposeOnSpawn = "true"))
	bool bPreserveSelectedLabelColor = false;

	// Optional check mark shown on the selected option row.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> CheckImage;
};
