#pragma once

#include "CoreMinimal.h"
#include "Scenario/Widget/ScenarioPlaceableDetailsWidget.h"
#include "ScenarioPlaceableContextMenuWidget.generated.h"

// Compatibility wrapper for older UMG assets that still use the context-menu class name.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UScenarioPlaceableContextMenuWidget : public UScenarioPlaceableDetailsWidget
{
	GENERATED_BODY()
};
