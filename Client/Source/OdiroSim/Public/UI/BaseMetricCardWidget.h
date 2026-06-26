#pragma once

#include "CoreMinimal.h"
#include "UI/BaseCardWidget.h"
#include "BaseMetricCardWidget.generated.h"

class UTextBlock;

// Dashboard metric card component.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseMetricCardWidget : public UBaseCardWidget
{
	GENERATED_BODY()

public:
	// Creates preview defaults for standalone editor rendering.
	UBaseMetricCardWidget();

	// Applies metric value in addition to base card text.
	virtual void SynchronizeBaseProperties() override;

	// Updates the primary metric value.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Metric Card")
	void SetValueText(FText inValueText);

	// Returns the primary metric value.
	UFUNCTION(BlueprintPure, Category = "UI|Base Metric Card")
	FText GetValueText() const { return ValueText; }

protected:
	// Primary dashboard value text.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetValueText", Setter = "SetValueText", BlueprintGetter = "GetValueText", BlueprintSetter = "SetValueText", Category = "UI|Base Metric Card", meta = (ExposeOnSpawn = "true"))
	FText ValueText;

	// Metric value visual owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ValueTextBlock;
};
