#pragma once

#include "CoreMinimal.h"
#include "UI/BaseButtonWidget.h"
#include "BaseTabWidget.generated.h"

class UBorder;

// Tab component for local navigation.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseTabWidget : public UBaseButtonWidget
{
	GENERATED_BODY()

public:
	// Applies button state plus selected tab indicator.
	virtual void SynchronizeBaseProperties() override;

protected:
	// Selected-state marker owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SelectedIndicator;
};
