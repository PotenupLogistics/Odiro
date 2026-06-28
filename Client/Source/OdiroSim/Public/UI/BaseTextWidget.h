#pragma once

#include "CoreMinimal.h"
#include "UI/BaseWidget.h"
#include "BaseTextWidget.generated.h"

class UTextBlock;

// Standalone text component for base widget layouts.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseTextWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Applies the exposed text, role, and disabled state to the WBP-owned text block.
	virtual void SynchronizeBaseProperties() override;

	// Updates the display text and refreshes the bound WBP text block.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Text")
	void SetText(FText inText);

	// Returns the current display text.
	UFUNCTION(BlueprintPure, Category = "UI|Base Text")
	FText GetText() const { return Text; }

	// Updates the semantic typography role.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Text")
	void SetTextRole(EBaseTextRole inTextRole);

	// Returns the semantic typography role.
	UFUNCTION(BlueprintPure, Category = "UI|Base Text")
	EBaseTextRole GetTextRole() const { return TextRole; }

	// Updates the disabled visual state.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Text")
	void SetDisabled(bool bInDisabled);

	// Returns whether this text should render as disabled.
	UFUNCTION(BlueprintPure, Category = "UI|Base Text")
	bool IsDisabled() const { return bDisabled; }

protected:
	// Display text shown by this component.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintGetter = "GetText", BlueprintSetter = "SetText", Category = "UI|Base Text", meta = (ExposeOnSpawn = "true"))
	FText Text;

	// Semantic typography role used for this text component.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetTextRole", Setter = "SetTextRole", BlueprintGetter = "GetTextRole", BlueprintSetter = "SetTextRole", Category = "UI|Base Text", meta = (ExposeOnSpawn = "true"))
	EBaseTextRole TextRole = EBaseTextRole::Body;

	// Disabled text state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsDisabled", Setter = "SetDisabled", BlueprintGetter = "IsDisabled", BlueprintSetter = "SetDisabled", Category = "UI|Base Text", meta = (ExposeOnSpawn = "true"))
	bool bDisabled = false;

	// Text visual owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TextBlock;
};
