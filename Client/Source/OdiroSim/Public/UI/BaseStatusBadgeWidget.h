#pragma once

#include "CoreMinimal.h"
#include "UI/BaseWidget.h"
#include "BaseStatusBadgeWidget.generated.h"

class UBorder;
class UTextBlock;

// Compact semantic status badge component.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseStatusBadgeWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Creates preview defaults for standalone editor rendering.
	UBaseStatusBadgeWidget();

	// Applies badge label and semantic color.
	virtual void SynchronizeBaseProperties() override;

	// Updates the badge label.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Status Badge")
	void SetLabel(FText inLabel);

	// Returns the badge label.
	UFUNCTION(BlueprintPure, Category = "UI|Base Status Badge")
	FText GetLabel() const { return Label; }

	// Updates the badge semantic state.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Status Badge")
	void SetBaseState(EBaseWidgetState inState);

	// Returns the badge semantic state.
	UFUNCTION(BlueprintPure, Category = "UI|Base Status Badge")
	EBaseWidgetState GetBaseState() const { return State; }

	// Updates the disabled visual state.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Status Badge")
	void SetDisabled(bool bInDisabled);

	// Returns whether this badge should render as disabled.
	UFUNCTION(BlueprintPure, Category = "UI|Base Status Badge")
	bool IsDisabled() const { return bDisabled; }

protected:
	// Badge text.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetLabel", Setter = "SetLabel", BlueprintGetter = "GetLabel", BlueprintSetter = "SetLabel", Category = "UI|Base Status Badge", meta = (ExposeOnSpawn = "true"))
	FText Label;

	// Badge semantic state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetBaseState", Setter = "SetBaseState", BlueprintGetter = "GetBaseState", BlueprintSetter = "SetBaseState", Category = "UI|Base Status Badge", meta = (ExposeOnSpawn = "true"))
	EBaseWidgetState State = EBaseWidgetState::Default;

	// Disabled badge state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsDisabled", Setter = "SetDisabled", BlueprintGetter = "IsDisabled", BlueprintSetter = "SetDisabled", Category = "UI|Base Status Badge", meta = (ExposeOnSpawn = "true"))
	bool bDisabled = false;

	// Badge surface owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SurfaceBorder;

	// Square semantic status dot owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> StatusDot;

	// Badge label owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelTextBlock;
};
