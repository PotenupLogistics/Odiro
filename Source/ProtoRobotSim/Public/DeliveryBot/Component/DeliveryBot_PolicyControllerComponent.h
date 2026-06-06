#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shared/Struct/DeliveryBot/Drive/DeliveryBotMovementInfo.h"
#include "Shared/Struct/DeliveryBot/Policy/DeliveryBotHttpPolicyResponseInfo.h"
#include "DeliveryBot_PolicyControllerComponent.generated.h"

class ADeliveryBot;
class UDeliveryBot_HttpPolicyComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROTOROBOTSIM_API UDeliveryBot_PolicyControllerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeliveryBot_PolicyControllerComponent();

	void InitializePolicyController(ADeliveryBot* ownerDeliveryBot,	UDeliveryBot_HttpPolicyComponent* httpPolicyComponent);

	// Python policy 서버로 observation을 주기적으로 보내는 타이머를 시작
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|PolicyController")
	void StartPolicyLoop();

	// Python policy 요청 타이머를 중지
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|PolicyController")
	void StopPolicyLoop();

	// GridSubsystem에서 생성한 Grid JSON을 Python 서버로 1회 전송한다.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|PolicyController")
	bool SendGridToPolicyServerOnce();
	
	// 매 프레임 처리해야 할 로직을 실행
	void TickPolicy(float deltaTime);

	
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void RequestPolicyByTimer();

	// 파싱한 Python 응답을 받는 함수
	UFUNCTION()
	void HandleParsedPolicyResponse(const FDeliveryBotHttpPolicyResponseInfo& responseInfo);

	
	// Python action을 Unreal 이동 명령으로 변환하고 값 범위를 검증한다.
	bool TryBuildMoveCommandFromPolicyResponse(const FDeliveryBotHttpPolicyResponseInfo& responseInfo,	FDeliveryBotMoveCommandInfo& outMoveCommandInfo, FString& outErrorMessage) const;

	// Python의 direction 문자열을 Unreal 이동 방향 enum으로 변환한다.
	bool TryGetMoveDirectionTypeFromPolicyDirection(const FString& direction, EDeliveryBotMoveDirectionType& outMoveDirectionType) const;

	// HTTP 실패, JSON 파싱 실패, action 검증 실패를 누적 처리한다.
	void HandlePolicyFailure(const FDeliveryBotHttpPolicyResponseInfo& responseInfo);

	// 정책 action이 만료되거나 실패가 누적됐을 때 사용할 정지 명령을 만든다.
	FDeliveryBotMoveCommandInfo BuildStopMoveCommand() const;
	
	// Python 응답을 수신했다는 사실을 기록한다. 성공/실패/stale 여부와 관계없이 호출한다.
	void LogPolicyResponseReceived(const FDeliveryBotHttpPolicyResponseInfo& responseInfo) const;

	// 이미 더 최신 sequence를 처리한 뒤 늦게 도착한 응답을 기록한다.
	void LogStalePolicyResponse(const FDeliveryBotHttpPolicyResponseInfo& responseInfo) const;

	// 검증을 통과해 실제 적용 가능한 action을 기록한다.
	void LogValidPolicyAction(const FDeliveryBotHttpPolicyResponseInfo& responseInfo,	const FDeliveryBotMoveCommandInfo& moveCommandInfo) const;
	
	void StartGridUploadRetryLoop();
	void StopGridUploadRetryLoop();
	void RequestGridUploadByTimer();

	UFUNCTION()
	void HandleGridUploadResponse(bool bWasSuccessful, int32 responseCode, const FString& responseBody);
	
private:
	UPROPERTY()
	TObjectPtr<ADeliveryBot> OwnerDeliveryBot;

	UPROPERTY()
	TObjectPtr<UDeliveryBot_HttpPolicyComponent> HttpPolicyComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|PolicyController", meta = (AllowPrivateAccess = "true"))
	bool bAutoStartPolicyLoop{ true };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|PolicyController", meta = (AllowPrivateAccess = "true"))
	float PolicyRequestIntervalSecond{ 0.2f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|PolicyController", meta = (AllowPrivateAccess = "true"))
	float PolicyActionTimeoutSecond{ 1.5f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|PolicyController", meta = (AllowPrivateAccess = "true"))
	int32 MaxConsecutivePolicyFailureCount{ 5 };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|PolicyController", meta = (AllowPrivateAccess = "true"))
	bool bWaitForGridUploadBeforePolicyLoop{ true };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|PolicyController", meta = (AllowPrivateAccess = "true"))
	float GridUploadRetryIntervalSecond{ 0.5f };


	
private:
	FTimerHandle GridUploadRetryTimerHandle;
	FTimerHandle PolicyLoopTimerHandle;
	
	FDeliveryBotMoveCommandInfo LastValidPolicyMoveCommand{};
	
	bool bHasValidPolicyMoveCommand{ false };
	bool bHasCompletedGridUpload{ false };
	
	float LastValidPolicyActionWorldTimeSeconds{ 0.f };
	
	int32 ConsecutivePolicyFailureCount{ 0 };
	int32 LastHandledPolicyResponseSequence { 0 };
};