#pragma once

#include "CoreMinimal.h"
#include "UI/BaseListItemWidget.h"
#include "BaseNotificationRowWidget.generated.h"

// Notification row component used directly as a WBP parent class.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseNotificationRowWidget : public UBaseListItemWidget
{
	GENERATED_BODY()

public:
	// Creates preview defaults for standalone editor rendering.
	UBaseNotificationRowWidget();
};
