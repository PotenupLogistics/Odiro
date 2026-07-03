#pragma once

#include "CoreMinimal.h"
#include "UI/BaseWidget.h"
#include "BaseTextWidget.generated.h"

class UTextBlock;

// Standalone text component base for WBP-owned text layouts.
UCLASS(Abstract, BlueprintType, Blueprintable)
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

	// Applies a runtime color override after the semantic text role style.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Text")
	void SetColorAndOpacityOverride(FLinearColor inColorAndOpacity);

	// Clears the runtime color override and returns to the semantic text role color.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Text")
	void ClearColorAndOpacityOverride();

	// Updates whether the bound text block should wrap within its allotted width.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Text")
	void SetAutoWrapText(bool bInAutoWrapText);

	// Returns whether the bound text block wraps within its allotted width.
	UFUNCTION(BlueprintPure, Category = "UI|Base Text")
	bool IsAutoWrapText() const { return bAutoWrapText; }

protected:
	// Display text shown by this component.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintGetter = "GetText", BlueprintSetter = "SetText", Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	FText Text;

	// Semantic typography role used for this text component.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetTextRole", Setter = "SetTextRole", BlueprintGetter = "GetTextRole", BlueprintSetter = "SetTextRole", Category = "UI|Style", meta = (ExposeOnSpawn = "true"))
	EBaseTextRole TextRole = EBaseTextRole::Body;

	// Disabled text state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsDisabled", Setter = "SetDisabled", BlueprintGetter = "IsDisabled", BlueprintSetter = "SetDisabled", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	bool bDisabled = false;

	// Runtime color override applied after the semantic text role when enabled.
	UPROPERTY(Transient)
	FLinearColor ColorAndOpacityOverride = FLinearColor::White;

	// Whether ColorAndOpacityOverride is currently active.
	UPROPERTY(Transient)
	bool bHasColorAndOpacityOverride = false;

	// Whether the bound text block wraps within its allotted width.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsAutoWrapText", Setter = "SetAutoWrapText", BlueprintGetter = "IsAutoWrapText", BlueprintSetter = "SetAutoWrapText", Category = "UI|Layout", meta = (ExposeOnSpawn = "true"))
	bool bAutoWrapText = false;

	// Text visual owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TextBlock;
};
