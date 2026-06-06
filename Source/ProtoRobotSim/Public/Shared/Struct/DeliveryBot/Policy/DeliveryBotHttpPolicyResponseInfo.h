#pragma once

#include "CoreMinimal.h"
#include "DeliveryBotHttpPolicyResponseInfo.generated.h"

USTRUCT(BlueprintType)
struct FDeliveryBotHttpPolicyActionInfo
{
	GENERATED_BODY()

	// Python이 판단한 조향값. Unreal 적용 전 -1.0 ~ 1.0 범위 검증이 필요하다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Steering{ 0.f };

	// Python이 보낸 스로틀 값. 현재 DriveComponent는 targetSpeed 기반이라 기록/확장용으로 먼저 둔다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Throttle{ 0.f }; // 지금은 사용 안함

	// Python이 보낸 브레이크 값. Unreal 적용 전 0.0 ~ 1.0 범위 검증이 필요하다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Brake{ 0.f };

	// Python이 요청한 목표 속도(km/h). Unreal 적용 전 차량 최대 속도와 비교해야 한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TargetSpeedKmh{ 0.f };

	// Python이 보낸 이동 방향 문자열. 예: "Forward", "Reverse"
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Direction{ TEXT("Forward") };
};

USTRUCT(BlueprintType)
struct FDeliveryBotHttpPolicyDebugInfo
{
	GENERATED_BODY()

	// Python 서버에서 사용한 정책 이름. 디버깅/로그용이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString PolicyName{};

	// Python 서버가 이 action을 선택한 이유. 디버깅/로그용이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Reason{};
};

USTRUCT(BlueprintType)
struct FDeliveryBotHttpPolicyResponseInfo
{
	GENERATED_BODY()

	// 응답이 어떤 observation에 대한 것인지 확인하는 번호다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Sequence{ 0 };

	// Python 응답 상태. 예: "ok", "error"
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Status{};

	// action 필드가 정상적으로 파싱되었는지 표시한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasAction{ false };

	// Python이 반환한 실제 이동 판단 값이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDeliveryBotHttpPolicyActionInfo Action{};

	// Python 서버의 디버깅 정보다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDeliveryBotHttpPolicyDebugInfo Debug{};

	// 파싱 실패나 서버 오류가 있을 때 기록할 메시지다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ErrorMessage{};

	// 원본 응답 문자열. 문제 발생 시 로그/분석용으로 남긴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString RawResponseBody{};
};