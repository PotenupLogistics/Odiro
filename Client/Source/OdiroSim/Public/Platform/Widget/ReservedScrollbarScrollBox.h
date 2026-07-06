#pragma once

#include "CoreMinimal.h"
#include "Components/ScrollBox.h"
#include "ReservedScrollbarScrollBox.generated.h"

// Scroll이 필요 없을 때도 ScrollBar layout 폭을 유지하는 ScrollBox.
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Reserved Scrollbar ScrollBox"))
class ODIROSIM_API UReservedScrollbarScrollBox : public UScrollBox
{
	GENERATED_BODY()

protected:
	// 내부 Slate ScrollBar의 disabled visibility를 Hidden으로 고정해 layout 공간을 보존한다.
	virtual TSharedRef<SWidget> RebuildWidget() override;

	// UMG 속성 동기화 후에도 ScrollBar disabled visibility 보존 규칙을 다시 적용한다.
	virtual void SynchronizeProperties() override;

private:
	// 현재 Slate ScrollBox에 reserved-scrollbar 규칙을 적용한다.
	void ApplyReservedScrollbarVisibility();
};
