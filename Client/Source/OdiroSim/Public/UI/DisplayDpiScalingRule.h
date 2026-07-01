#pragma once

#include "CoreMinimal.h"
#include "Engine/DPICustomScalingRule.h"
#include "DisplayDpiScalingRule.generated.h"

// 현재 게임 창이 위치한 display의 native DPI scale을 UMG DPI rule로 전달한다.
UCLASS()
class ODIROSIM_API UDisplayDpiScalingRule : public UDPICustomScalingRule
{
	GENERATED_BODY()

public:
	// UMG viewport scale 계산 시 현재 display DPI scale을 반환한다.
	virtual float GetDPIScaleBasedOnSize(FIntPoint size) const override;

private:
	// Game viewport window 또는 활성 Slate window에서 native DPI scale을 읽는다.
	static float ResolveGameWindowDpiScale();
};
