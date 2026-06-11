
#include "DeliveryBot/Component/DeliveryBot_PolicyJudgmentComponent.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"


UDeliveryBot_PolicyJudgmentComponent::UDeliveryBot_PolicyJudgmentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

}


void UDeliveryBot_PolicyJudgmentComponent::BeginPlay()
{
	Super::BeginPlay();

	
}


void UDeliveryBot_PolicyJudgmentComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                         FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

}

void UDeliveryBot_PolicyJudgmentComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	bIsDestroying = true;

	CancelPendingRemoteRequest();
	OnPolicyFailed.Clear();

	Super::EndPlay(EndPlayReason);
	
}
FDeliveryBotPolicyDecisionInfo UDeliveryBot_PolicyJudgmentComponent::EvaluatePolicy(const FDeliveryBotPolicyContextInfo& contextInfo)
{

	if (bIsDestroying)
		return FDeliveryBotPolicyDecisionInfo{};

	if (bPolicyFailed)
		return BuildPolicyFailedDecision();
	
	if (!PolicyConfigInfo.bUseRemotePolicy)
		return EvaluateLocalPolicy(contextInfo);

	if (!contextInfo.bHasFrontObject)
	{
		CancelPendingRemoteRequest();
		return FDeliveryBotPolicyDecisionInfo{};
	}
	
	if (CheckRemoteRequestTimeout())
		return BuildPolicyFailedDecision();

	RequestRemotePolicyDecision(contextInfo);

	FDeliveryBotPolicyDecisionInfo remoteDecisionInfo;
	if (TryGetLastRemoteDecision(remoteDecisionInfo))
		return remoteDecisionInfo;

	return BuildWaitingRemoteDecision(contextInfo);
}

void UDeliveryBot_PolicyJudgmentComponent::CancelPendingRemoteRequest()
{
	CancelAndClearPendingHttpRequest();
	ClearPendingRemoteRequest();
	bHasLastRemoteDecision = false;
}

FDeliveryBotPolicyDecisionInfo UDeliveryBot_PolicyJudgmentComponent::EvaluateLocalPolicy(const FDeliveryBotPolicyContextInfo& contextInfo) const
{
	FDeliveryBotPolicyDecisionInfo decisionInfo;

	if (!contextInfo.bHasFrontObject)
	{
		decisionInfo.ActionType = EDeliveryBotPolicyActionType::None;
		return decisionInfo;
	}

	if (contextInfo.bInRepathMoveGraceTime)
	{
		decisionInfo.ActionType = EDeliveryBotPolicyActionType::SlowDown;
		decisionInfo.Reason = TEXT("Repath grace time");
		return decisionInfo;
	}

	if (contextInfo.FrontObjectDistanceM <= contextInfo.StopDistanceM)
	{
		if (PolicyConfigInfo.bUseRepathPolicy && contextInfo.bCanRepath)
		{
			decisionInfo.ActionType = EDeliveryBotPolicyActionType::Repath;
			decisionInfo.Reason = TEXT("Front object is inside stop distance");
			return decisionInfo;
		}

		if (PolicyConfigInfo.bUseStopPolicy)
		{
			decisionInfo.ActionType = EDeliveryBotPolicyActionType::Stop;
			decisionInfo.Reason = TEXT("Front object is too close");
			return decisionInfo;
		}
	}

	if (PolicyConfigInfo.bUseSlowDownPolicy &&
		contextInfo.FrontObjectDistanceM <= contextInfo.SlowDownDistanceM)
	{
		decisionInfo.ActionType = EDeliveryBotPolicyActionType::SlowDown;
		decisionInfo.Reason = TEXT("Front object is inside slowdown distance");
		return decisionInfo;
	}

	decisionInfo.ActionType = EDeliveryBotPolicyActionType::None;
	return decisionInfo;
}

FDeliveryBotPolicyDecisionInfo UDeliveryBot_PolicyJudgmentComponent::BuildWaitingRemoteDecision(const FDeliveryBotPolicyContextInfo& contextInfo) const
{
	FDeliveryBotPolicyDecisionInfo decisionInfo;

	if (!contextInfo.bHasFrontObject)
	{
		decisionInfo.ActionType = EDeliveryBotPolicyActionType::None;
		return decisionInfo;
	}

	if (contextInfo.FrontObjectDistanceM <= contextInfo.StopDistanceM)
	{
		decisionInfo.ActionType = EDeliveryBotPolicyActionType::Stop;
		decisionInfo.Reason = TEXT("Waiting remote policy response near obstacle");
		return decisionInfo;
	}

	decisionInfo.ActionType = EDeliveryBotPolicyActionType::SlowDown;
	decisionInfo.Reason = TEXT("Waiting remote policy response");

	return decisionInfo;
}
void UDeliveryBot_PolicyJudgmentComponent::RequestRemotePolicyDecision(	const FDeliveryBotPolicyContextInfo& contextInfo)
{
	if (!ShouldRequestRemotePolicy())
		return;

	UWorld* world = GetWorld();
	if (!IsValid(world))
		return;

	PendingRequestId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	PendingRequestStartTimeSeconds = world->GetTimeSeconds();

	LastRemoteRequestTimeSeconds = PendingRequestStartTimeSeconds;
	bRemoteRequestPending = true;
	bHasLastRemoteDecision = false;

	const FString requestBody = BuildRemotePolicyRequestJson(contextInfo, PendingRequestId);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> request =	FHttpModule::Get().CreateRequest();

	request->SetURL(PolicyConfigInfo.PolicyServerUrl);
	request->SetVerb(TEXT("POST"));
	request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	request->SetContentAsString(requestBody);
	request->SetTimeout(PolicyConfigInfo.RequestTimeoutSecond);

	PendingHttpRequest = request;
	
	TWeakObjectPtr<UDeliveryBot_PolicyJudgmentComponent> weakThis = this;
	const FString capturedRequestId = PendingRequestId;

	request->OnProcessRequestComplete().BindLambda(
		[weakThis, capturedRequestId](FHttpRequestPtr httpRequest, FHttpResponsePtr httpResponse, bool bWasSuccessful)
		{
			if (!weakThis.IsValid())
				return;

			const int32 responseCode = httpResponse.IsValid() ? httpResponse->GetResponseCode() : 0;
			const FString responseBody = httpResponse.IsValid() ? httpResponse->GetContentAsString() : FString();

			weakThis->HandleRemotePolicyResponse(
				capturedRequestId,
				responseCode,
				responseBody,
				bWasSuccessful);
		}
	);

	if (!request->ProcessRequest())
	{
		BroadcastPolicyFailure(
			EDeliveryBotPolicyFailureType::ProcessRequestFailed,
			0,
			TEXT("Failed to start remote policy request"));

		CancelAndClearPendingHttpRequest();
		ClearPendingRemoteRequest();	
		return;
	}
}

bool UDeliveryBot_PolicyJudgmentComponent::ShouldRequestRemotePolicy() const
{
	if (bIsDestroying)
		return false;

	if (bRemoteRequestPending)
		return false;
	
	const UWorld* world = GetWorld();
	if (!IsValid(world))
		return false;
	const double currentTimeSeconds = world->GetTimeSeconds();
	return currentTimeSeconds - LastRemoteRequestTimeSeconds >=	static_cast<double>(PolicyConfigInfo.MinRequestIntervalSecond);
}
FString UDeliveryBot_PolicyJudgmentComponent::BuildRemotePolicyRequestJson(	const FDeliveryBotPolicyContextInfo& contextInfo, const FString& requestId) const
{
	TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();

	rootObject->SetStringField(TEXT("schemaVersion"), TEXT("1.0.0"));
	rootObject->SetStringField(TEXT("requestId"), requestId);

	TSharedRef<FJsonObject> contextObject = MakeShared<FJsonObject>();
	contextObject->SetBoolField(TEXT("hasFrontObject"), contextInfo.bHasFrontObject);
	contextObject->SetNumberField(TEXT("frontObjectDistanceM"), contextInfo.FrontObjectDistanceM);
	contextObject->SetNumberField(TEXT("stopDistanceM"), contextInfo.StopDistanceM);
	contextObject->SetNumberField(TEXT("slowDownDistanceM"), contextInfo.SlowDownDistanceM);
	contextObject->SetNumberField(TEXT("currentSpeedKmh"), contextInfo.CurrentSpeedKmh);
	contextObject->SetNumberField(TEXT("maxSpeedKmh"), contextInfo.MaxSpeedKmh);
	contextObject->SetBoolField(TEXT("canRepath"), contextInfo.bCanRepath);
	contextObject->SetBoolField(TEXT("inRepathMoveGraceTime"), contextInfo.bInRepathMoveGraceTime);

	rootObject->SetObjectField(TEXT("policyContext"), contextObject);

	FString outputString;
	TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&outputString);
	FJsonSerializer::Serialize(rootObject, writer);

	return outputString;
}


void UDeliveryBot_PolicyJudgmentComponent::HandleRemotePolicyResponse(const FString& requestId,int32 responseCode,const FString& responseBody,bool bWasSuccessful)
{
	if (bIsDestroying)
		return;

	if (bPolicyFailed)
	{
		CancelAndClearPendingHttpRequest();
		ClearPendingRemoteRequest();
		return;
	}

	if (!requestId.Equals(PendingRequestId, ESearchCase::CaseSensitive))
		return;
	
	if (!bWasSuccessful)
	{
		bHasLastRemoteDecision = false;

		BroadcastPolicyFailure(
			EDeliveryBotPolicyFailureType::HttpRequestFailed,
			responseCode,
			TEXT("Remote policy HTTP request failed"));

		CancelAndClearPendingHttpRequest();
		ClearPendingRemoteRequest();
		return;
	}

	if (responseCode < 200 || responseCode >= 300)
	{
		bHasLastRemoteDecision = false;

		BroadcastPolicyFailure(	EDeliveryBotPolicyFailureType::HttpError,responseCode,
			FString::Printf(TEXT("Remote policy HTTP error: %d"), responseCode));

		CancelAndClearPendingHttpRequest();
		ClearPendingRemoteRequest();
		return;
	}
	
	
	FDeliveryBotPolicyDecisionInfo decisionInfo;
	if (!ParseRemotePolicyResponse(responseBody, decisionInfo))
	{
		bHasLastRemoteDecision = false;

		BroadcastPolicyFailure(
			EDeliveryBotPolicyFailureType::InvalidResponse,
			responseCode,
			TEXT("Remote policy response is invalid"));

		CancelAndClearPendingHttpRequest();
		ClearPendingRemoteRequest();
		return;
	}

	decisionInfo.bFromRemoteApi = true;
	LastRemoteDecisionInfo = decisionInfo;
	bHasLastRemoteDecision = true;

	CancelAndClearPendingHttpRequest();
	ClearPendingRemoteRequest();

}
bool UDeliveryBot_PolicyJudgmentComponent::ParseRemotePolicyResponse( const FString& responseBody, FDeliveryBotPolicyDecisionInfo& outDecisionInfo) const
{
	TSharedPtr<FJsonObject> rootObject;
	TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(responseBody);

	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
		return false;

	FString selectedAction;
	if (!rootObject->TryGetStringField(TEXT("selectedAction"), selectedAction))
		return false;

	EDeliveryBotPolicyActionType actionType{ EDeliveryBotPolicyActionType::None };
	if (!TryGetActionTypeFromString(selectedAction, actionType))
		return false;

	outDecisionInfo.ActionType = actionType;
	rootObject->TryGetStringField(TEXT("reason"), outDecisionInfo.Reason);

	return true;
}

bool UDeliveryBot_PolicyJudgmentComponent::TryGetLastRemoteDecision(FDeliveryBotPolicyDecisionInfo& outDecisionInfo) const
{
	if (!bHasLastRemoteDecision)
		return false;

	outDecisionInfo = LastRemoteDecisionInfo;
	return true;
}

FDeliveryBotPolicyDecisionInfo UDeliveryBot_PolicyJudgmentComponent::BuildPolicyFailedDecision() const
{
	FDeliveryBotPolicyDecisionInfo decisionInfo;
	decisionInfo.ActionType = EDeliveryBotPolicyActionType::Stop;
	decisionInfo.Reason = TEXT("Policy request failed");
	return decisionInfo;
}

bool UDeliveryBot_PolicyJudgmentComponent::CheckRemoteRequestTimeout()
{
	if (!bRemoteRequestPending)
		return false;

	const UWorld* world = GetWorld();
	if (!IsValid(world))
		return false;

	const float elapsedSecond = static_cast<float>(
		world->GetTimeSeconds() - PendingRequestStartTimeSeconds);

	if (elapsedSecond < PolicyConfigInfo.RequestTimeoutSecond)
		return false;

	BroadcastPolicyFailure(
		EDeliveryBotPolicyFailureType::Timeout,
		0,
		TEXT("Remote policy request timeout"));

	CancelAndClearPendingHttpRequest();
	ClearPendingRemoteRequest();
	return true;
}

void UDeliveryBot_PolicyJudgmentComponent::BroadcastPolicyFailure(EDeliveryBotPolicyFailureType failureType, int32 responseCode, const FString& message)
{
	if (bIsDestroying)
		return;

	if (bPolicyFailed)
		return;

	bPolicyFailed = true;

	const UWorld* world = GetWorld();
	const float elapsedSecond = IsValid(world)
		? static_cast<float>(world->GetTimeSeconds() - PendingRequestStartTimeSeconds)
		: 0.f;

	FDeliveryBotPolicyFailureInfo failureInfo;
	failureInfo.FailureType = failureType;
	failureInfo.RequestId = PendingRequestId;
	failureInfo.PolicyServerUrl = PolicyConfigInfo.PolicyServerUrl;
	failureInfo.ResponseCode = responseCode;
	failureInfo.RequestElapsedSecond = FMath::Max(elapsedSecond, 0.f);
	failureInfo.Message = message;

	OnPolicyFailed.Broadcast(failureInfo);
}

void UDeliveryBot_PolicyJudgmentComponent::ClearPendingRemoteRequest()
{
	bRemoteRequestPending = false;
	PendingRequestId.Reset();
	PendingRequestStartTimeSeconds = -1000.0;
}

void UDeliveryBot_PolicyJudgmentComponent::CancelAndClearPendingHttpRequest()
{
	if (!PendingHttpRequest.IsValid())
		return;

	PendingHttpRequest->OnProcessRequestComplete().Unbind();
	PendingHttpRequest->CancelRequest();
	PendingHttpRequest.Reset();
}

bool UDeliveryBot_PolicyJudgmentComponent::TryGetActionTypeFromString(const FString& actionString, EDeliveryBotPolicyActionType& outActionType) const
{
	if (actionString.Equals(TEXT("None"), ESearchCase::IgnoreCase))
	{
		outActionType = EDeliveryBotPolicyActionType::None;
		return true;
	}

	if (actionString.Equals(TEXT("SlowDown"), ESearchCase::IgnoreCase))
	{
		outActionType = EDeliveryBotPolicyActionType::SlowDown;
		return true;
	}

	if (actionString.Equals(TEXT("Stop"), ESearchCase::IgnoreCase))
	{
		outActionType = EDeliveryBotPolicyActionType::Stop;
		return true;
	}

	if (actionString.Equals(TEXT("Repath"), ESearchCase::IgnoreCase))
	{
		outActionType = EDeliveryBotPolicyActionType::Repath;
		return true;
	}

	if (actionString.Equals(TEXT("Avoidance"), ESearchCase::IgnoreCase))
	{
		outActionType = EDeliveryBotPolicyActionType::Avoidance;
		return true;
	}

	if (actionString.Equals(TEXT("RequestControl"), ESearchCase::IgnoreCase))
	{
		outActionType = EDeliveryBotPolicyActionType::RequestControl;
		return true;
	}

	outActionType = EDeliveryBotPolicyActionType::None;
	return false;
}
