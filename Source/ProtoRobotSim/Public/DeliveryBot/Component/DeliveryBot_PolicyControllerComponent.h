#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shared/Struct/DeliveryBot/Drive/DeliveryBotDriveConfigInfo.h"
#include "Shared/Struct/DeliveryBot/Drive/DeliveryBotMovementInfo.h"
#include "Shared/Struct/DeliveryBot/Policy/DeliveryBotHttpPolicyResponseInfo.h"
#include "Shared/EpisodeConfigTypes.h"
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

	//  버튼 없이 자동 정책 주행을 시작할 때 호출
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|PolicyController")
	bool StartPolicyRunAfterSpawn();
	
	// 자동 시작 전에 이전 실행 상태를 초기화한다.
	void ResetPolicyRunStartupState();
	
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|PolicyController")
	void SetStartupPolicySpecFileName(const FString& policySpecFileName);

	// 매 프레임 처리해야 할 로직을 실행
	void TickPolicy(float deltaTime);

public:
	// GridSubsystem에서 생성한 Grid JSON을 Python 서버로 1회 전송한다.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|PolicyController")
	bool SendGridToPolicyServerOnce();
	
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|PolicyController")
	bool SendEpisodeStartToPolicyServerOnce();
	
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|PolicyController")
	bool SendEpisodeConfigUpdateToPolicyServerOnce();
	
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|PolicyController")
	bool ApplyRuntimeDriveConfigAndSendConfigUpdate(const FDeliveryBotDriveConfigInfo& driveConfigInfo);

	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|PolicyController")
	bool SendCurrentRuntimeConfigUpdateToPolicyServerOnce();
	
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|PolicyController")
	bool SendEpisodeStartAndStartPolicyLoopOnce();
	

	
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
	
	// Python 응답의 episode/config/grid version이 현재 Unreal 기준과 같은지 검증한다.
	bool TryValidatePolicyResponseVersions(const FDeliveryBotHttpPolicyResponseInfo& responseInfo,	FString& outErrorMessage) const;

	// /episode/start 응답 body에서 episode/config/grid version을 읽어 현재 기대 version으로 저장한다.
	bool TryUpdateExpectedPolicyVersionsFromEpisodeStartResponse(const FString& responseBody, FString& outErrorMessage);
	
	// /grid/update 응답 body에서 gridVersion을 읽어 현재 기대 GridVersion으로 저장한다.
	bool TryUpdateExpectedGridVersionFromGridUploadResponse(const FString& responseBody, FString& outErrorMessage);
	
	bool TryUpdateExpectedConfigVersionFromEpisodeConfigUpdateResponse(const FString& responseBody,	FString& outErrorMessage);
	
	UFUNCTION()
	void HandleEpisodeConfigUpdateResponse(bool bWasSuccessful, int32 responseCode, const FString& responseBody);
	
	void RecordPolicyFailure(const FDeliveryBotHttpPolicyResponseInfo& responseInfo);
	
	bool IsGoalReachedPolicyResponse(const FDeliveryBotHttpPolicyResponseInfo& responseInfo) const;
	
	// 성공하면 /episode/start 재시도 루프를 시작
	UFUNCTION()
	void HandleStartupPolicySpecUpdateResponse(bool bWasSuccessful, int32 responseCode, const FString& responseBody);
	
	
private:  // Episode종료
	void BindEpisodeEvaluationEndedEvent();
	void UnbindEpisodeEvaluationEndedEvent();
	
private:  // 경로 표시
	void UpdateLastPolicyPathFromResponse(const FDeliveryBotHttpPolicyResponseInfo& responseInfo);
	void DrawLastPolicyPath() const;
	
private: // 그리드를 파이썬으로 전송
	void StartGridUploadRetryLoop();
	void StopGridUploadRetryLoop();
	void RequestGridUploadByTimer();
	UFUNCTION()
	void HandleGridUploadResponse(bool bWasSuccessful, int32 responseCode, const FString& responseBody);
	
	
private: // 시작 설정 값 파이썬으로 전송  -> Grid + 차량 설정 + Lidar 설정 + Start/Goal + Motion 설정
	void StartEpisodeStartRetryLoop();
	void StopEpisodeStartRetryLoop();
	void RequestEpisodeStartByTimer();
	void StopPolicyRunForEpisodeEnd(const FEpisodeEvaluationResult& result);// 정책 루프를 멈추고 차량을 ParkingStop 상태로 고정
	
	UFUNCTION()
	void HandleEpisodeStartResponse(bool bWasSuccessful, int32 responseCode, const FString& responseBody);
	UFUNCTION()
	void HandleEpisodeEnded(FEpisodeEvaluationResult result); // EpisodeEvaluationSubsystem이 에피소드 종료를 알렸을 때 호출
	
	
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|PolicyController", meta = (AllowPrivateAccess = "true"))
	bool bWaitForEpisodeStartBeforePolicyLoop{ true };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|PolicyController", meta = (AllowPrivateAccess = "true"))
	float EpisodeStartRetryIntervalSecond{ 0.5f };
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeliveryBot|PolicyController", meta = (AllowPrivateAccess = "true"))
	TArray<FString> PolicyFailureHistory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|PolicyController", meta = (AllowPrivateAccess = "true"))
	int32 MaxPolicyFailureHistoryCount{ 300 };
	
	// 소환 후 자동 시작할 때 Python으로 보낼 기본 PolicySpec 파일명
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|PolicyController", meta = (AllowPrivateAccess = "true"))
	FString StartupPolicySpecFileName{ TEXT("PolicySpec_DefaultDelivery") };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|PolicyController|Debug", meta = (AllowPrivateAccess = "true"))
	bool bLogPolicyRuntimeMessages{ false };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|PolicyController|Debug", meta = (AllowPrivateAccess = "true"))
	bool bDrawPolicyPathDebug{ true };

private:
	FTimerHandle GridUploadRetryTimerHandle;
	FTimerHandle PolicyLoopTimerHandle;
	FTimerHandle EpisodeStartRetryTimerHandle;
	FDeliveryBotMoveCommandInfo LastValidPolicyMoveCommand{};

	bool bHasValidPolicyMoveCommand{ false };
	bool bHasCompletedGridUpload{ false };
	bool bHasCompletedEpisodeStart{ false };
	bool bHasExpectedPolicyVersions{ false };
	bool bStartPolicyLoopAfterNextEpisodeStart{ false };
	bool bHoldStopAfterGoalReached{ false };
	
	float LastValidPolicyActionWorldTimeSeconds{ 0.f };
	

	int32 ConsecutivePolicyFailureCount{ 0 };
	int32 LastHandledPolicyResponseSequence { 0 };
	
	int32 ExpectedEpisodeVersion{ 0 };
	int32 ExpectedConfigVersion{ 0 };
	int32 ExpectedGridVersion{ 0 };

	TArray<FVector> LastPolicyPathPointsCm;
};
