#pragma once

#include "CoreMinimal.h"
#include "Shared/Struct/DeliveryBot/Drive/DeliveryBotMovementInfo.h"
#include "Shared/Struct/DeliveryBot/Result/DeliveryBotPythonCaptureRefInfo.h"
#include "Shared/Types/DeliveryBotPolicyDecisionTypes.h"
#include "DeliveryBotPolicyDecisionResultInfo.generated.h"

// Python policy가 선택한 decision metadata를 저장한다.
USTRUCT(BlueprintType)
struct FDeliveryBotPolicyDecisionInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SelectedPolicy{}; // action을 선택한 policy 이름

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Reason{}; // policy가 action을 선택한 이유 코드
};

// DeliveryBot policy decision 결과와 capture refs를 함께 저장한다.
USTRUCT(BlueprintType)
struct FDeliveryBotPolicyDecisionResultInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Sequence{ INDEX_NONE }; // Python decide 요청 sequence

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RunTimeSeconds{ 0.f }; // decision 시점 episode runtime

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EDeliveryBotPolicyDecisionStatusTypes Status{ EDeliveryBotPolicyDecisionStatusTypes::Unknown }; // decision 처리 상태

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDeliveryBotMoveCommandInfo MoveCommand{}; // Unreal이 적용할 이동 명령

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDeliveryBotPolicyDecisionInfo Decision{}; // Python policy decision metadata

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FDeliveryBotPythonCaptureRefInfo> CaptureRefs{}; // Python response.captures 참조 목록

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ErrorCode{}; // status가 Error일 때의 machine-readable code

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ErrorMessage{}; // status가 Error일 때의 human-readable message
};
