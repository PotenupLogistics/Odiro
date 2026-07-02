#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "OdiroCommonUserWidget.generated.h"

class UUserWidget;

// Platform row/card WBP가 Common UI input/action 환경에서 공유하는 native base.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UOdiroCommonUserWidget : public UCommonUserWidget
{
	GENERATED_BODY()

protected:
	// Runtime widget tree가 editor undo transaction에 잡히지 않도록 초기화한다.
	virtual void NativeOnInitialized() override;

	// Late-created WBP child까지 포함해 runtime transaction flag를 정리한다.
	virtual void NativeConstruct() override;

	// 지정 runtime widget과 그 WidgetTree 전체를 editor transaction 대상에서 제외한다.
	static void ClearRuntimeTransactionFlagsForWidget(UUserWidget* widget);
};
