#pragma once

#include "CoreMinimal.h"
#include "UI/BaseCardWidget.h"
#include "BaseEmptyStateWidget.generated.h"

// Empty state component used directly as a WBP parent class.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseEmptyStateWidget : public UBaseCardWidget
{
	GENERATED_BODY()

public:
	// Creates preview defaults for standalone editor rendering.
	UBaseEmptyStateWidget();
};
