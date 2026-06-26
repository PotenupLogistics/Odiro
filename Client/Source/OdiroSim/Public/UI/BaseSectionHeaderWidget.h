#pragma once

#include "CoreMinimal.h"
#include "UI/BaseWidget.h"
#include "BaseSectionHeaderWidget.generated.h"

class UTextBlock;

// Section header component for grouped dashboard and tool surfaces.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseSectionHeaderWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Creates preview defaults for standalone editor rendering.
	UBaseSectionHeaderWidget();

	// Applies section label and supporting description.
	virtual void SynchronizeBaseProperties() override;

	// Updates the section title.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Section Header")
	void SetLabel(FText inLabel);

	// Returns the section title.
	UFUNCTION(BlueprintPure, Category = "UI|Base Section Header")
	FText GetLabel() const { return Label; }

	// Updates the section description.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Section Header")
	void SetDescription(FText inDescription);

	// Returns the section description.
	UFUNCTION(BlueprintPure, Category = "UI|Base Section Header")
	FText GetDescription() const { return Description; }

protected:
	// Section title.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetLabel", Setter = "SetLabel", BlueprintGetter = "GetLabel", BlueprintSetter = "SetLabel", Category = "UI|Base Section Header", meta = (ExposeOnSpawn = "true"))
	FText Label;

	// Optional section description.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetDescription", Setter = "SetDescription", BlueprintGetter = "GetDescription", BlueprintSetter = "SetDescription", Category = "UI|Base Section Header", meta = (ExposeOnSpawn = "true"))
	FText Description;

	// Title visual owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelTextBlock;

	// Description visual owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionTextBlock;
};
