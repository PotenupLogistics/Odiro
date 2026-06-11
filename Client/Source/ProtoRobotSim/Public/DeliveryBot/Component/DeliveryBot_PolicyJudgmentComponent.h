// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shared/Struct/DeliveryBot/Policy/DeliveryBotPolicyConfigInfo.h"
#include "Shared/Struct/DeliveryBot/Policy/DeliveryBotPolicyInfo.h"
#include "Shared/Struct/DeliveryBot/Policy/DeliveryBotPolicyFailureInfo.h"
#include "DeliveryBot_PolicyJudgmentComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDeliveryBotPolicyFailedSignature, const FDeliveryBotPolicyFailureInfo&, FailureInfo);

class IHttpRequest;
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROTOROBOTSIM_API UDeliveryBot_PolicyJudgmentComponent : public UActorComponent
{
	GENERATED_BODY()

public: // 생성자와 UE Component 생명주기
	UDeliveryBot_PolicyJudgmentComponent();

	virtual void TickComponent(	float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected: // BeginPlay 초기화
	virtual void BeginPlay() override;

public: // 정책 평가와 외부 상태 조회 API
	FDeliveryBotPolicyDecisionInfo EvaluatePolicy(const FDeliveryBotPolicyContextInfo& contextInfo);

	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Policy")
	bool HasPolicyFailed() const { return bPolicyFailed; }

	void CancelPendingRemoteRequest();

public: // 정책 실패를 외부에 알리는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "DeliveryBot|Policy")
	FDeliveryBotPolicyFailedSignature OnPolicyFailed;

private: // 로컬 정책 판단
	FDeliveryBotPolicyDecisionInfo EvaluateLocalPolicy(const FDeliveryBotPolicyContextInfo& contextInfo) const;

	FDeliveryBotPolicyDecisionInfo BuildWaitingRemoteDecision(const FDeliveryBotPolicyContextInfo& contextInfo) const;

private: // 원격 정책 요청 생성과 전송
	void RequestRemotePolicyDecision(const FDeliveryBotPolicyContextInfo& contextInfo);

	bool ShouldRequestRemotePolicy() const;

	FString BuildRemotePolicyRequestJson(const FDeliveryBotPolicyContextInfo& contextInfo, const FString& requestId) const;

private: // 원격 정책 응답 파싱과 최근 decision 관리
	void HandleRemotePolicyResponse(const FString& requestId, int32 responseCode, const FString& responseBody, bool bWasSuccessful);

	bool ParseRemotePolicyResponse(const FString& responseBody, FDeliveryBotPolicyDecisionInfo& outDecisionInfo) const;

	bool TryGetLastRemoteDecision(FDeliveryBotPolicyDecisionInfo& outDecisionInfo) const;

	bool TryGetActionTypeFromString(const FString& actionString, EDeliveryBotPolicyActionType& outActionType) const;

private: // 정책 실패 상태 생성과 broadcast
	FDeliveryBotPolicyDecisionInfo BuildPolicyFailedDecision() const;
	bool CheckRemoteRequestTimeout();

	void BroadcastPolicyFailure(EDeliveryBotPolicyFailureType failureType,	int32 responseCode,	const FString& message);

private: // pending 원격 요청 상태 초기화
	void ClearPendingRemoteRequest();
	void CancelAndClearPendingHttpRequest();
	
protected: // 정책 선택과 원격 API 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Policy")
	FDeliveryBotPolicyConfigInfo PolicyConfigInfo{};

private: // 원격 정책 요청 진행 상태
	bool bRemoteRequestPending{ false };
	bool bHasLastRemoteDecision{ false };
	bool bPolicyFailed{ false };
	bool bIsDestroying{ false };

	double LastRemoteRequestTimeSeconds{ -1000.0 };
	double PendingRequestStartTimeSeconds{ -1000.0 };

	FString PendingRequestId{};
	FDeliveryBotPolicyDecisionInfo LastRemoteDecisionInfo{};
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> PendingHttpRequest{};
};
