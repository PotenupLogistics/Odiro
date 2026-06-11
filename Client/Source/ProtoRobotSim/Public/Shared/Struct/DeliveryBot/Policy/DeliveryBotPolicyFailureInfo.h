#pragma once

#include "CoreMinimal.h"
#include "DeliveryBotPolicyFailureInfo.generated.h"

UENUM(BlueprintType)
enum class EDeliveryBotPolicyFailureType : uint8
{
	None,
	ProcessRequestFailed, // UE에서 HTTP 요청 시작 자체가 실패함.
	HttpRequestFailed, // 서버 연결 실패, 네트워크 실패, 응답 자체 실패.
	Timeout, // 설정된 시간 안에 Python 응답이 오지 않음.
	HttpError, // 서버가 응답했지만 200번대가 아님. 예: 500, 404.
	InvalidResponse, // JSON 파싱 실패, selectedAction 누락, 응답 형식 오류.
	UnknownAction // Python이 UE가 모르는 행동값을 반환
};

USTRUCT(BlueprintType)
struct FDeliveryBotPolicyFailureInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EDeliveryBotPolicyFailureType FailureType{ EDeliveryBotPolicyFailureType::None };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString RequestId{}; // Python 요청 ID

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString PolicyServerUrl{}; // 요청한 API 주소

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ResponseCode{ 0 }; // HTTP 응답 코드

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RequestElapsedSecond{ 0.f }; // 요청 후 실패까지 걸린 시간

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Message{}; // 정책 요청 실패 메시지
};
