#pragma once

#include "CoreMinimal.h"
#include "UI/BaseButtonWidget.h"
#include "UI/BaseFormElementTypes.h"
#include "BaseToggleButtonWidget.generated.h"

class UBaseSwitchWidget;
class UTextBlock;
class UWidget;

// Button or switch shaped checked-state control.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseToggleButtonWidget : public UBaseButtonWidget
{
	GENERATED_BODY()

public:
	// Applies button state plus optional switch visuals to bound WBP controls.
	virtual void SynchronizeBaseProperties() override;

	// Updates the toggle presentation style.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Toggle")
	void SetToggleStyle(EBaseToggleButtonStyle inToggleStyle);

	// Returns the toggle presentation style.
	UFUNCTION(BlueprintPure, Category = "UI|Base Toggle")
	EBaseToggleButtonStyle GetToggleStyle() const { return ToggleStyle; }

	// Updates the checked state.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Toggle")
	void SetCheckState(ECheckBoxState inCheckState);

	// Returns the checked state.
	UFUNCTION(BlueprintPure, Category = "UI|Base Toggle")
	ECheckBoxState GetCheckState() const { return CheckState; }

	// Updates whether button-style toggles show the secondary state label.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Toggle")
	void SetShowStateText(bool bInShowStateText);

	// Returns whether button-style toggles show the secondary state label.
	UFUNCTION(BlueprintPure, Category = "UI|Base Toggle")
	bool ShouldShowStateText() const { return bShowStateText; }

	// Broadcasts after checked state changes.
	UPROPERTY(BlueprintAssignable, Category = "UI|Events")
	FBaseCheckStateChangedEvent OnCheckStateChanged;

protected:
	// Toggles checked state before forwarding the CommonUI click event.
	virtual void NativeOnClicked() override;

	// Toggle presentation style.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetToggleStyle", Setter = "SetToggleStyle", BlueprintGetter = "GetToggleStyle", BlueprintSetter = "SetToggleStyle", Category = "UI|Style", meta = (ExposeOnSpawn = "true"))
	EBaseToggleButtonStyle ToggleStyle = EBaseToggleButtonStyle::Button;

	// Current checked state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetCheckState", Setter = "SetCheckState", BlueprintGetter = "GetCheckState", BlueprintSetter = "SetCheckState", Category = "UI|State", meta = (ExposeOnSpawn = "true"))
	ECheckBoxState CheckState = ECheckBoxState::Unchecked;

	// State text shown when the toggle is checked.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	FText CheckedStateText = NSLOCTEXT("BaseToggleButtonWidget", "CheckedStateText", "On");

	// State text shown when the toggle is unchecked.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	FText UncheckedStateText = NSLOCTEXT("BaseToggleButtonWidget", "UncheckedStateText", "Off");

	// State text shown when the toggle is indeterminate.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	FText UndeterminedStateText = NSLOCTEXT("BaseToggleButtonWidget", "UndeterminedStateText", "Mixed");

	// Whether button-style toggles render their checked-state text.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "ShouldShowStateText", Setter = "SetShowStateText", BlueprintGetter = "ShouldShowStateText", BlueprintSetter = "SetShowStateText", Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	bool bShowStateText = false;

	// Optional button-style visual root owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ButtonVisualRoot;

	// Optional switch-style visual root owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SwitchVisualRoot;

	// Optional switch visual child that owns WBP-authored switch layout.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseSwitchWidget> SwitchVisual;

	// Optional state text for button-style toggles.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StateTextBlock;

	// Optional button-style content row owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ToggleContent;
};
