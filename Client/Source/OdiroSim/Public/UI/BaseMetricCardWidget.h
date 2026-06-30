#pragma once

#include "CoreMinimal.h"
#include "UI/BaseCardWidget.h"
#include "UI/BaseFormElementTypes.h"
#include "BaseMetricCardWidget.generated.h"

class UTextBlock;

// Dashboard metric card component.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseMetricCardWidget : public UBaseCardWidget
{
	GENERATED_BODY()

public:
	// Applies metric value in addition to base card text.
	virtual void SynchronizeBaseProperties() override;

	// Updates the primary metric value.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Metric Card")
	void SetValueText(FText inValueText);

	// Returns the primary metric value.
	UFUNCTION(BlueprintPure, Category = "UI|Base Metric Card")
	FText GetValueText() const { return ValueText; }

	// Updates vertical placement of card content inside spare height.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Metric Card")
	void SetContentVAlign(EBaseVerticalContentAlign inContentVAlign);

	// Returns vertical placement of card content inside spare height.
	UFUNCTION(BlueprintPure, Category = "UI|Base Metric Card")
	EBaseVerticalContentAlign GetContentVAlign() const { return ContentVAlign; }

protected:
	// Primary dashboard value text.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetValueText", Setter = "SetValueText", BlueprintGetter = "GetValueText", BlueprintSetter = "SetValueText", Category = "UI|Contents", meta = (ExposeOnSpawn = "true"))
	FText ValueText;

	// Vertical placement for metric card content when the widget has spare height.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetContentVAlign", Setter = "SetContentVAlign", BlueprintGetter = "GetContentVAlign", BlueprintSetter = "SetContentVAlign", Category = "UI|Layout", meta = (ExposeOnSpawn = "true"))
	EBaseVerticalContentAlign ContentVAlign = EBaseVerticalContentAlign::Middle;

	// Metric value visual owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ValueTextBlock;
};
