#pragma once

#include "CoreMinimal.h"

// Python /scenario/end 응답 결과를 평가 종료 흐름에 전달한다.
struct ODIROSIM_API FDeliveryBotPythonScenarioEndResult
{
	bool bRequested{ false };
	bool bSucceeded{ false };
	FString Status{};
	FString ResponseJson{};
	FString ErrorMessage{};
};

// Python /scenario/end 완료 callback 타입이다.
using FDeliveryBotPythonScenarioEndCallback =
	TFunction<void(const FDeliveryBotPythonScenarioEndResult& Result)>;