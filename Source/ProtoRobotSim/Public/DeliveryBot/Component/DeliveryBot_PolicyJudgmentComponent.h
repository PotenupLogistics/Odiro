// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shared/Struct/DeliveryBotPolicyConfigInfo.h"
#include "Shared/Struct/DeliveryBotPolicyInfo.h"
#include "Shared/Struct/DeliveryBotPolicyFailureInfo.h"
#include "DeliveryBot_PolicyJudgmentComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDeliveryBotPolicyFailedSignature, const FDeliveryBotPolicyFailureInfo&, FailureInfo);
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROTOROBOTSIM_API UDeliveryBot_PolicyJudgmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeliveryBot_PolicyJudgmentComponent();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	
public:
	FDeliveryBotPolicyDecisionInfo EvaluatePolicy(const FDeliveryBotPolicyContextInfo& contextInfo);
	UPROPERTY(BlueprintAssignable, Category = "DeliveryBot|Policy")
	FDeliveryBotPolicyFailedSignature OnPolicyFailed;

	UFUNCTION(BlueprintPure, Category = "DeliveryBot|Policy")
	bool HasPolicyFailed() const { return bPolicyFailed; }
	
private:
	FDeliveryBotPolicyDecisionInfo EvaluateLocalPolicy(const FDeliveryBotPolicyContextInfo& contextInfo) const;

	FDeliveryBotPolicyDecisionInfo BuildWaitingRemoteDecision(const FDeliveryBotPolicyContextInfo& contextInfo) const;

	// Python API 요청
	void RequestRemotePolicyDecision(const FDeliveryBotPolicyContextInfo& contextInfo); 

	// 요청 간격/중복 요청 방지
	bool ShouldRequestRemotePolicy() const;

	//  Context를 JSON으로 변환
	FString BuildRemotePolicyRequestJson(const FDeliveryBotPolicyContextInfo& contextInfo, const FString& requestId) const;

	// HTTP 응답 처리
	void HandleRemotePolicyResponse(const FString& requestId, int32 responseCode, const FString& responseBody, bool bWasSuccessful);

	bool TryGetActionTypeFromString(const FString& actionString, EDeliveryBotPolicyActionType& outActionType) const;
	
	// JSON 응답을 Decision으로 변환
	bool ParseRemotePolicyResponse(const FString& responseBody, FDeliveryBotPolicyDecisionInfo& outDecisionInfo) const;

	// 최근 API 응답 가져오기
	bool TryGetLastRemoteDecision(FDeliveryBotPolicyDecisionInfo& outDecisionInfo) const;

	FDeliveryBotPolicyDecisionInfo BuildPolicyFailedDecision() const;

	bool CheckRemoteRequestTimeout();

	void BroadcastPolicyFailure(EDeliveryBotPolicyFailureType failureType,	int32 responseCode,	const FString& message);
	
	void ClearPendingRemoteRequest();
	
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeliveryBot|Policy")
	FDeliveryBotPolicyConfigInfo PolicyConfigInfo{};
	
private:
	bool bRemoteRequestPending{ false };
	bool bHasLastRemoteDecision{ false };
	bool bPolicyFailed{ false };

	double LastRemoteRequestTimeSeconds{ -1000.0 };
	double PendingRequestStartTimeSeconds{ -1000.0 };

	FString PendingRequestId{};
	FDeliveryBotPolicyDecisionInfo LastRemoteDecisionInfo{};
};
