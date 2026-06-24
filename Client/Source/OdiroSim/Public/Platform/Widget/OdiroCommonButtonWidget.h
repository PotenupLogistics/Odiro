#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "OdiroCommonButtonWidget.generated.h"

// Platform WBP 버튼이 Common UI input/action routing을 공유하기 위한 native base.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UOdiroCommonButtonWidget : public UCommonButtonBase
{
	GENERATED_BODY()
};
