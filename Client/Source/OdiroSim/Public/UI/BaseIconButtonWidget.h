#pragma once

#include "CoreMinimal.h"
#include "UI/BaseButtonWidget.h"
#include "BaseIconButtonWidget.generated.h"

// Icon-only CommonUI button component used directly as a WBP parent class.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseIconButtonWidget : public UBaseButtonWidget
{
	GENERATED_BODY()

public:
	// Creates preview defaults for standalone editor rendering.
	UBaseIconButtonWidget(const FObjectInitializer& objectInitializer = FObjectInitializer::Get());
};
