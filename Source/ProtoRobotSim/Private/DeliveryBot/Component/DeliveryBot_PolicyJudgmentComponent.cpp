// Fill out your copyright notice in the Description page of Project Settings.

#include "DeliveryBot/Component/DeliveryBot_PolicyJudgmentComponent.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
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

FDeliveryBotPolicyDecisionInfo UDeliveryBot_PolicyJudgmentComponent::EvaluatePolicy(const FDeliveryBotPolicyContextInfo& contextInfo)
{
	if (!PolicyConfigInfo.bUseRemotePolicy)
		return EvaluateLocalPolicy(contextInfo);

	if (!contextInfo.bHasFrontObject)
	{
		bHasLastRemoteDecision = false;
		FDeliveryBotPolicyDecisionInfo decisionInfo;
		decisionInfo.ActionType = EDeliveryBotPolicyActionType::None;
		return decisionInfo;
	}

	RequestRemotePolicyDecision(contextInfo);
	FDeliveryBotPolicyDecisionInfo remoteDecisionInfo;
	if (TryGetLastRemoteDecision(remoteDecisionInfo))
		return remoteDecisionInfo;

	return BuildWaitingRemoteDecision(contextInfo);
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

void UDeliveryBot_PolicyJudgmentComponent::RequestRemotePolicyDecision(const FDeliveryBotPolicyContextInfo& contextInfo)
{
	if (!ShouldRequestRemotePolicy())
		return;

	UWorld* world = GetWorld();
	if (!IsValid(world))
		return;

	LastRemoteRequestTimeSeconds = world->GetTimeSeconds();
	bRemoteRequestPending = true;
	bHasLastRemoteDecision = false;

	const FString requestBody = BuildRemotePolicyRequestJson(contextInfo);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> request =	FHttpModule::Get().CreateRequest();
	request->SetURL(PolicyConfigInfo.PolicyServerUrl);
	request->SetVerb(TEXT("POST"));
	request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	request->SetContentAsString(requestBody);
	request->SetTimeout(PolicyConfigInfo.RequestTimeoutSecond);

	TWeakObjectPtr<UDeliveryBot_PolicyJudgmentComponent> weakThis{ this };

	request->OnProcessRequestComplete().BindLambda(
		[weakThis](FHttpRequestPtr httpRequest, FHttpResponsePtr httpResponse, bool bWasSuccessful)
		{
			if (!weakThis.IsValid())
				return;
			const int32 responseCode = httpResponse.IsValid() ? httpResponse->GetResponseCode()	: 0;
			const FString responseBody = httpResponse.IsValid()	? httpResponse->GetContentAsString() : FString{};

			weakThis->HandleRemotePolicyResponse(responseCode, responseBody, bWasSuccessful);
		}
	);

	if (!request->ProcessRequest())
		bRemoteRequestPending = false;
}
bool UDeliveryBot_PolicyJudgmentComponent::ShouldRequestRemotePolicy() const
{
	if (bRemoteRequestPending)
		return false;

	const UWorld* world = GetWorld();
	if (!IsValid(world))
		return false;
	const double currentTimeSeconds = world->GetTimeSeconds();
	return currentTimeSeconds - LastRemoteRequestTimeSeconds >=	static_cast<double>(PolicyConfigInfo.MinRequestIntervalSecond);
}

FString UDeliveryBot_PolicyJudgmentComponent::BuildRemotePolicyRequestJson(const FDeliveryBotPolicyContextInfo& contextInfo) const
{
	TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();

	rootObject->SetStringField(TEXT("schemaVersion"), TEXT("1.0.0"));
	rootObject->SetStringField(TEXT("requestId"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens));

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

void UDeliveryBot_PolicyJudgmentComponent::HandleRemotePolicyResponse(int32 responseCode, const FString& responseBody, bool bWasSuccessful)
{
	bRemoteRequestPending = false;

	if (!bWasSuccessful || responseCode < 200 || responseCode >= 300)
	{
		bHasLastRemoteDecision = false;
		return;
	}

	FDeliveryBotPolicyDecisionInfo decisionInfo;
	if (!ParseRemotePolicyResponse(responseBody, decisionInfo))
	{
		bHasLastRemoteDecision = false;
		return;
	}

	decisionInfo.bFromRemoteApi = true;
	LastRemoteDecisionInfo = decisionInfo;
	bHasLastRemoteDecision = true;
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

	outDecisionInfo.ActionType = GetActionTypeFromString(selectedAction);
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

EDeliveryBotPolicyActionType UDeliveryBot_PolicyJudgmentComponent::GetActionTypeFromString(const FString& actionString) const
{
	if (actionString.Equals(TEXT("SlowDown"), ESearchCase::IgnoreCase))
		return EDeliveryBotPolicyActionType::SlowDown;

	if (actionString.Equals(TEXT("Stop"), ESearchCase::IgnoreCase))
		return EDeliveryBotPolicyActionType::Stop;

	if (actionString.Equals(TEXT("Repath"), ESearchCase::IgnoreCase))
		return EDeliveryBotPolicyActionType::Repath;

	if (actionString.Equals(TEXT("Avoidance"), ESearchCase::IgnoreCase))
		return EDeliveryBotPolicyActionType::Avoidance;

	if (actionString.Equals(TEXT("RequestControl"), ESearchCase::IgnoreCase))
		return EDeliveryBotPolicyActionType::RequestControl;

	return EDeliveryBotPolicyActionType::None;
}