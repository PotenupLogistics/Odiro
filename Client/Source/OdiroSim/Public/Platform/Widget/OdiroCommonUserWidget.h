#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "OdiroCommonUserWidget.generated.h"

// Platform row/card WBP가 Common UI input/action 환경에서 공유하는 native base.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UOdiroCommonUserWidget : public UCommonUserWidget
{
	GENERATED_BODY()
};
