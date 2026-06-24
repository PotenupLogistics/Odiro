#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "OdiroActivatableScreenWidget.generated.h"

// Platform 화면 WBP가 Common UI activation/back stack에 참여하기 위한 공통 native base.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UOdiroActivatableScreenWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// Common UI back action으로 닫을 수 있는 화면인지 반환한다.
	UFUNCTION(BlueprintPure, Category = "Platform|CommonUI")
	bool CanHandleBackAction() const { return bHandleBackAction; }

protected:
	// 화면이 back action을 소비하고 deactivate할지 여부.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Platform|CommonUI")
	bool bHandleBackAction = true;

	// Common UI back action 처리 지점.
	virtual bool NativeOnHandleBackAction() override;
};
