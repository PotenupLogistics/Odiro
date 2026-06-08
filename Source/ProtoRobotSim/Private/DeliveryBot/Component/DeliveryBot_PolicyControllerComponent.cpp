#include "DeliveryBot/Component/DeliveryBot_PolicyControllerComponent.h"

#include "DeliveryBot/Actor/DeliveryBot.h"
#include "DeliveryBot/Component/DeliveryBot_HttpPolicyComponent.h"
#include "DeliveryBot/Subsystem/DeliveryBot_GridSubsystem.h"
#include "Shared/Struct/DeliveryBot/Drive/DeliveryBotDriveConfigInfo.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"


DEFINE_LOG_CATEGORY_STATIC(LogDeliveryBotPolicyController, Log, All);

UDeliveryBot_PolicyControllerComponent::UDeliveryBot_PolicyControllerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDeliveryBot_PolicyControllerComponent::InitializePolicyController(ADeliveryBot* ownerDeliveryBot,	UDeliveryBot_HttpPolicyComponent* httpPolicyComponent)
{
	OwnerDeliveryBot = ownerDeliveryBot;
	HttpPolicyComponent = httpPolicyComponent;

	UE_LOG(
		LogDeliveryBotPolicyController,
		Log,
		TEXT("Policy controller initialized | AutoStart: %s, WaitEpisodeStart: %s, WaitGrid: %s"),
		bAutoStartPolicyLoop ? TEXT("true") : TEXT("false"),
		bWaitForEpisodeStartBeforePolicyLoop ? TEXT("true") : TEXT("false"),
		bWaitForGridUploadBeforePolicyLoop ? TEXT("true") : TEXT("false")
	);

	if (IsValid(HttpPolicyComponent))
	{
		HttpPolicyComponent->OnParsedPolicyResponse.AddUniqueDynamic(this, &UDeliveryBot_PolicyControllerComponent::HandleParsedPolicyResponse);
		HttpPolicyComponent->OnGridResponse.AddUniqueDynamic(this, &UDeliveryBot_PolicyControllerComponent::HandleGridUploadResponse);
		HttpPolicyComponent->OnEpisodeStartResponse.AddUniqueDynamic(this, &UDeliveryBot_PolicyControllerComponent::HandleEpisodeStartResponse);
		HttpPolicyComponent->OnEpisodeConfigUpdateResponse.AddUniqueDynamic(this, &UDeliveryBot_PolicyControllerComponent::HandleEpisodeConfigUpdateResponse);
	}

	if (bAutoStartPolicyLoop)
	{
		if (bWaitForEpisodeStartBeforePolicyLoop)
		{
			StartEpisodeStartRetryLoop();
		}
		else
		{
			SendEpisodeStartToPolicyServerOnce();
			StartPolicyLoop();
		}
	}
}

void UDeliveryBot_PolicyControllerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopPolicyLoop();
	StopGridUploadRetryLoop();
	StopEpisodeStartRetryLoop();

	if (IsValid(HttpPolicyComponent))
	{
		HttpPolicyComponent->OnParsedPolicyResponse.RemoveDynamic(this, &UDeliveryBot_PolicyControllerComponent::HandleParsedPolicyResponse);
		HttpPolicyComponent->OnGridResponse.RemoveDynamic(this, &UDeliveryBot_PolicyControllerComponent::HandleGridUploadResponse);
		HttpPolicyComponent->OnEpisodeStartResponse.RemoveDynamic(this, &UDeliveryBot_PolicyControllerComponent::HandleEpisodeStartResponse);
		HttpPolicyComponent->OnEpisodeConfigUpdateResponse.RemoveDynamic(this, &UDeliveryBot_PolicyControllerComponent::HandleEpisodeConfigUpdateResponse);
		
	}

	Super::EndPlay(EndPlayReason);
}

void UDeliveryBot_PolicyControllerComponent::StartPolicyLoop()
{
	bHoldStopAfterGoalReached = false;
	bHasValidPolicyMoveCommand = false;

	if (!GetWorld())
	{
		UE_LOG(LogDeliveryBotPolicyController, Warning, TEXT("Policy loop start skipped. World is invalid."));
		return;
	}

	const float safeInterval = FMath::Max(PolicyRequestIntervalSecond, 0.05f);

	GetWorld()->GetTimerManager().SetTimer(
		PolicyLoopTimerHandle,
		this,
		&UDeliveryBot_PolicyControllerComponent::RequestPolicyByTimer,
		safeInterval,
		true,
		0.f
	);

	UE_LOG(LogDeliveryBotPolicyController, Log, TEXT("Policy loop started. Interval: %.2fs"), safeInterval);
}

void UDeliveryBot_PolicyControllerComponent::StopPolicyLoop()
{
	if (!GetWorld())
		return;

	GetWorld()->GetTimerManager().ClearTimer(PolicyLoopTimerHandle);
}

void UDeliveryBot_PolicyControllerComponent::RequestPolicyByTimer()
{
	if (!IsValid(OwnerDeliveryBot))
	{
		UE_LOG(LogDeliveryBotPolicyController, Warning, TEXT("Policy request skipped. OwnerDeliveryBot is invalid."));
		return;
	}

	if (!bHasExpectedPolicyVersions)
	{
		UE_LOG(LogDeliveryBotPolicyController, Warning, TEXT("Policy request skipped. Expected policy versions are not initialized yet."));
		return;
	}

	if (!OwnerDeliveryBot->SendPolicyObservationOnce())
	{
		UE_LOG(LogDeliveryBotPolicyController, Warning, TEXT("Policy request skipped. SendPolicyObservationOnce returned false."));
	}
}

void UDeliveryBot_PolicyControllerComponent::TickPolicy(float deltaTime)
{
	if (!IsValid(OwnerDeliveryBot))
		return;

	if (bHoldStopAfterGoalReached)
	{
		OwnerDeliveryBot->ApplyParkingStop();
		return;
	}

	if (!bHasValidPolicyMoveCommand)
		return;

	const UWorld* world = GetWorld();
	if (!world)
		return;

	const float elapsedSinceLastPolicyAction = world->GetTimeSeconds() - LastValidPolicyActionWorldTimeSeconds;

	if (elapsedSinceLastPolicyAction > PolicyActionTimeoutSecond)
	{
		bHasValidPolicyMoveCommand = false;

		const FDeliveryBotMoveCommandInfo stopCommand = BuildStopMoveCommand();
		OwnerDeliveryBot->ApplyMoveCommand(stopCommand, deltaTime);

		UE_LOG(LogDeliveryBotPolicyController, Warning, TEXT("Policy action expired. Stop command applied. Elapsed: %.2fs"), elapsedSinceLastPolicyAction);
		return;
	}

	OwnerDeliveryBot->ApplyMoveCommand(LastValidPolicyMoveCommand, deltaTime);
}

void UDeliveryBot_PolicyControllerComponent::HandleParsedPolicyResponse(const FDeliveryBotHttpPolicyResponseInfo& responseInfo)
{
	LogPolicyResponseReceived(responseInfo);

	if (responseInfo.Sequence <= 0)
	{
		FDeliveryBotHttpPolicyResponseInfo failureInfo = responseInfo;

		if (failureInfo.ErrorMessage.IsEmpty())
		{
			failureInfo.ErrorMessage = TEXT("Policy response has invalid sequence.");
		}

		HandlePolicyFailure(failureInfo);
		return;
	}

	if (responseInfo.Sequence <= LastHandledPolicyResponseSequence)
	{
		LogStalePolicyResponse(responseInfo);
		return;
	}

	LastHandledPolicyResponseSequence = responseInfo.Sequence;

	if (!responseInfo.ErrorMessage.IsEmpty())
	{
		HandlePolicyFailure(responseInfo);
		return;
	}

	FString versionErrorMessage;
	if (!TryValidatePolicyResponseVersions(responseInfo, versionErrorMessage))
	{
		FDeliveryBotHttpPolicyResponseInfo failureInfo = responseInfo;
		failureInfo.ErrorMessage = versionErrorMessage;
		HandlePolicyFailure(failureInfo);
		return;
	}

	FDeliveryBotMoveCommandInfo moveCommandInfo;
	FString validationErrorMessage;

	if (!TryBuildMoveCommandFromPolicyResponse(responseInfo, moveCommandInfo, validationErrorMessage))
	{
		FDeliveryBotHttpPolicyResponseInfo failureInfo = responseInfo;
		failureInfo.ErrorMessage = validationErrorMessage;
		HandlePolicyFailure(failureInfo);
		return;
	}

	const bool bGoalReached = IsGoalReachedPolicyResponse(responseInfo);

	ConsecutivePolicyFailureCount = 0;
	LastValidPolicyMoveCommand = bGoalReached ? BuildStopMoveCommand() : moveCommandInfo;
	bHasValidPolicyMoveCommand = true;

	if (const UWorld* world = GetWorld())
	{
		LastValidPolicyActionWorldTimeSeconds = world->GetTimeSeconds();
	}

	LogValidPolicyAction(responseInfo, LastValidPolicyMoveCommand);

	if (bGoalReached)
	{
		LastValidPolicyMoveCommand = BuildStopMoveCommand();
		bHasValidPolicyMoveCommand = true;
		bHoldStopAfterGoalReached = true;

		UE_LOG(LogDeliveryBotPolicyController, Log, TEXT("Goal reached. Holding stop command and stopping policy request loop."));
		StopPolicyLoop();
		return;
	}
}


// Python action이 현재 Unreal episode/config/grid 기준과 같은지 확인
bool UDeliveryBot_PolicyControllerComponent::TryValidatePolicyResponseVersions(const FDeliveryBotHttpPolicyResponseInfo& responseInfo, FString& outErrorMessage) const
{
	outErrorMessage.Empty();

	if (!bHasExpectedPolicyVersions)
	{
		outErrorMessage = TEXT("Expected policy versions are not initialized.");
		return false;
	}

	if (responseInfo.EpisodeVersion != ExpectedEpisodeVersion)
	{
		outErrorMessage = FString::Printf(TEXT("Episode version mismatch. Response: %d, Expected: %d"), responseInfo.EpisodeVersion, ExpectedEpisodeVersion);
		return false;
	}

	if (responseInfo.ConfigVersion != ExpectedConfigVersion)
	{
		outErrorMessage = FString::Printf(TEXT("Config version mismatch. Response: %d, Expected: %d"), responseInfo.ConfigVersion, ExpectedConfigVersion);
		return false;
	}

	if (responseInfo.GridVersion != ExpectedGridVersion)
	{
		outErrorMessage = FString::Printf(TEXT("Grid version mismatch. Response: %d, Expected: %d"), responseInfo.GridVersion, ExpectedGridVersion);
		return false;
	}

	return true;
}

// /episode/start 성공 응답에서 version 값을 읽어서 이후 policy 응답 검증 기준으로 저장
bool UDeliveryBot_PolicyControllerComponent::TryUpdateExpectedPolicyVersionsFromEpisodeStartResponse(const FString& responseBody, FString& outErrorMessage)
{
	outErrorMessage.Empty();

	if (responseBody.IsEmpty())
	{
		outErrorMessage = TEXT("Episode start response body is empty.");
		return false;
	}

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(responseBody);

	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		outErrorMessage = TEXT("Failed to parse episode start response JSON.");
		return false;
	}

	int32 parsedEpisodeVersion = 0;
	int32 parsedConfigVersion = 0;
	int32 parsedGridVersion = 0;

	if (!rootObject->TryGetNumberField(TEXT("episodeVersion"), parsedEpisodeVersion))
	{
		outErrorMessage = TEXT("Episode start response has no episodeVersion.");
		return false;
	}

	if (!rootObject->TryGetNumberField(TEXT("configVersion"), parsedConfigVersion))
	{
		outErrorMessage = TEXT("Episode start response has no configVersion.");
		return false;
	}

	if (!rootObject->TryGetNumberField(TEXT("gridVersion"), parsedGridVersion))
	{
		outErrorMessage = TEXT("Episode start response has no gridVersion.");
		return false;
	}

	ExpectedEpisodeVersion = parsedEpisodeVersion;
	ExpectedConfigVersion = parsedConfigVersion;
	ExpectedGridVersion = parsedGridVersion;
	bHasExpectedPolicyVersions = true;

	UE_LOG(
		LogDeliveryBotPolicyController,
		Log,
		TEXT("Expected policy versions updated | Episode: %d, Config: %d, Grid: %d"),
		ExpectedEpisodeVersion,
		ExpectedConfigVersion,
		ExpectedGridVersion
	);

	return true;
}

bool UDeliveryBot_PolicyControllerComponent::TryUpdateExpectedGridVersionFromGridUploadResponse(const FString& responseBody, FString& outErrorMessage)
{
	outErrorMessage.Empty();

	if (responseBody.IsEmpty())
	{
		outErrorMessage = TEXT("Grid upload response body is empty.");
		return false;
	}

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(responseBody);

	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		outErrorMessage = TEXT("Failed to parse grid upload response JSON.");
		return false;
	}

	int32 parsedGridVersion = 0;
	if (!rootObject->TryGetNumberField(TEXT("gridVersion"), parsedGridVersion))
	{
		outErrorMessage = TEXT("Grid upload response has no gridVersion.");
		return false;
	}

	ExpectedGridVersion = parsedGridVersion;

	UE_LOG(LogDeliveryBotPolicyController, Log, TEXT("Expected grid version updated | Grid: %d"), ExpectedGridVersion);

	return true;
}

bool UDeliveryBot_PolicyControllerComponent::TryBuildMoveCommandFromPolicyResponse(const FDeliveryBotHttpPolicyResponseInfo& responseInfo,	FDeliveryBotMoveCommandInfo& outMoveCommandInfo, FString& outErrorMessage) const
{
	outMoveCommandInfo = FDeliveryBotMoveCommandInfo{};
	outErrorMessage.Empty();

	if (!IsValid(OwnerDeliveryBot))
	{
		outErrorMessage = TEXT("OwnerDeliveryBot is invalid.");
		return false;
	}

	if (!responseInfo.bHasAction)
	{
		outErrorMessage = TEXT("Policy response has no action.");
		return false;
	}

	const FDeliveryBotHttpPolicyActionInfo& action = responseInfo.Action;

	if (!FMath::IsFinite(action.Steering) || action.Steering < -1.f || action.Steering > 1.f)
	{
		outErrorMessage = FString::Printf(TEXT("Invalid steering: %.3f"), action.Steering);
		return false;
	}


	if (!FMath::IsFinite(action.Brake) || action.Brake < 0.f || action.Brake > 1.f)
	{
		outErrorMessage = FString::Printf(TEXT("Invalid brake: %.3f"), action.Brake);
		return false;
	}

	if (!FMath::IsFinite(action.TargetSpeedKmh) || action.TargetSpeedKmh < 0.f)
	{
		outErrorMessage = FString::Printf(TEXT("Invalid targetSpeedKmh: %.3f"), action.TargetSpeedKmh);
		return false;
	}

	EDeliveryBotMoveDirectionType moveDirectionType;
	if (!TryGetMoveDirectionTypeFromPolicyDirection(action.Direction, moveDirectionType))
	{
		outErrorMessage = FString::Printf(TEXT("Invalid direction: %s"), *action.Direction);
		return false;
	}

	const float maxAllowedSpeedKmh = OwnerDeliveryBot->GetMaxPolicySpeedKmh(moveDirectionType);
	
	if (action.TargetSpeedKmh > maxAllowedSpeedKmh)
	{
		outErrorMessage = FString::Printf(
			TEXT("Target speed exceeds limit. Target: %.3f, Max: %.3f"),
			action.TargetSpeedKmh,
			maxAllowedSpeedKmh
		);
		return false;
	}

	outMoveCommandInfo.TargetSpeedKmh = action.TargetSpeedKmh;
	outMoveCommandInfo.Steering = action.Steering;
	outMoveCommandInfo.Brake = action.Brake;
	outMoveCommandInfo.bBrake = action.Brake > KINDA_SMALL_NUMBER;
	outMoveCommandInfo.MoveDirectionType = moveDirectionType;

	return true;
}

bool UDeliveryBot_PolicyControllerComponent::TryGetMoveDirectionTypeFromPolicyDirection(const FString& direction, EDeliveryBotMoveDirectionType& outMoveDirectionType) const
{
	const FString normalizedDirection = direction.TrimStartAndEnd();

	if (normalizedDirection.Equals(TEXT("Forward"), ESearchCase::IgnoreCase))
	{
		outMoveDirectionType = EDeliveryBotMoveDirectionType::Forward;
		return true;
	}

	if (normalizedDirection.Equals(TEXT("Reverse"), ESearchCase::IgnoreCase))
	{
		outMoveDirectionType = EDeliveryBotMoveDirectionType::Reverse;
		return true;
	}

	return false;
}

void UDeliveryBot_PolicyControllerComponent::HandlePolicyFailure(const FDeliveryBotHttpPolicyResponseInfo& responseInfo)
{
	RecordPolicyFailure(responseInfo);
	
	++ConsecutivePolicyFailureCount;

	UE_LOG(
		LogDeliveryBotPolicyController,
		Warning,
		TEXT("Policy failure | Seq: %d, Count: %d/%d, Error: %s"),
		responseInfo.Sequence,
		ConsecutivePolicyFailureCount,
		MaxConsecutivePolicyFailureCount,
		*responseInfo.ErrorMessage
	);

	if (ConsecutivePolicyFailureCount >= MaxConsecutivePolicyFailureCount)
	{
		bHasValidPolicyMoveCommand = false;
		StopPolicyLoop();

		if (IsValid(OwnerDeliveryBot))
		{
			OwnerDeliveryBot->ApplyMoveCommand(BuildStopMoveCommand(), 0.f);
		}

		UE_LOG(LogDeliveryBotPolicyController,	Error, TEXT("Policy failure limit exceeded. Policy loop stopped."));
	}
}

FDeliveryBotMoveCommandInfo UDeliveryBot_PolicyControllerComponent::BuildStopMoveCommand() const
{
	FDeliveryBotMoveCommandInfo stopCommand;
	stopCommand.TargetSpeedKmh = 0.f;
	stopCommand.Steering = 0.f;
	stopCommand.Brake = 1.f;
	stopCommand.bBrake = true;
	stopCommand.MoveDirectionType = EDeliveryBotMoveDirectionType::Forward;
	return stopCommand;
}

void UDeliveryBot_PolicyControllerComponent::LogPolicyResponseReceived(const FDeliveryBotHttpPolicyResponseInfo& responseInfo) const
{
	UE_LOG(
		LogDeliveryBotPolicyController,
		Log,
		TEXT("Policy response received | Seq: %d, Status: %s, HasAction: %s, EpisodeVersion: %d, ConfigVersion: %d, GridVersion: %d, Error: %s"),
		responseInfo.Sequence,
		*responseInfo.Status,
		responseInfo.bHasAction ? TEXT("true") : TEXT("false"),
		responseInfo.EpisodeVersion,
		responseInfo.ConfigVersion,
		responseInfo.GridVersion,
		*responseInfo.ErrorMessage
	);
}

void UDeliveryBot_PolicyControllerComponent::LogStalePolicyResponse(const FDeliveryBotHttpPolicyResponseInfo& responseInfo) const
{
	UE_LOG(
		LogDeliveryBotPolicyController,
		Warning,
		TEXT("Stale policy response ignored | Seq: %d, LastHandledSeq: %d, Status: %s, Error: %s"),
		responseInfo.Sequence,
		LastHandledPolicyResponseSequence,
		*responseInfo.Status,
		*responseInfo.ErrorMessage
	);
}

void UDeliveryBot_PolicyControllerComponent::LogValidPolicyAction(const FDeliveryBotHttpPolicyResponseInfo& responseInfo, const FDeliveryBotMoveCommandInfo& moveCommandInfo) const
{
	UE_LOG(
		LogDeliveryBotPolicyController,
		Log,
		TEXT("Valid policy action | Seq: %d, Steering: %.2f, Brake: %.2f, TargetSpeed: %.2fkm/h, Direction: %s, Policy: %s"),
		responseInfo.Sequence,
		moveCommandInfo.Steering,
		moveCommandInfo.Brake,
		moveCommandInfo.TargetSpeedKmh,
		*responseInfo.Action.Direction,
		*responseInfo.Debug.PolicyName
	);
}

// Grid가 아직 안 만들어졌거나 서버가 늦게 켜졌을 때 재시도
void UDeliveryBot_PolicyControllerComponent::StartGridUploadRetryLoop()
{
	if (!GetWorld())
		return;

	const float safeInterval = FMath::Max(GridUploadRetryIntervalSecond, 0.1f);

	GetWorld()->GetTimerManager().SetTimer(
		GridUploadRetryTimerHandle,
		this,
		&UDeliveryBot_PolicyControllerComponent::RequestGridUploadByTimer,
		safeInterval,
		true,
		0.f
	);
}

// Grid 전송 성공 또는 PIE 종료 시 재시도 타이머를 정리
void UDeliveryBot_PolicyControllerComponent::StopGridUploadRetryLoop()
{
	if (!GetWorld())
		return;

	GetWorld()->GetTimerManager().ClearTimer(GridUploadRetryTimerHandle);
}

// 타이머에서 반복 호출, Grid 전송이 완료되면 자동으로 멈춤
void UDeliveryBot_PolicyControllerComponent::RequestGridUploadByTimer()
{
	if (bHasCompletedGridUpload)
	{
		StopGridUploadRetryLoop();
		return;
	}
	SendGridToPolicyServerOnce();
}

// GridSubsystem에서 Grid JSON을 만들고 Python 서버로 보냄
bool UDeliveryBot_PolicyControllerComponent::SendGridToPolicyServerOnce()
{
	if (!IsValid(HttpPolicyComponent))
		return false;

	if (HttpPolicyComponent->IsGridRequestInFlight())
		return false;

	UWorld* world = GetWorld();
	if (!IsValid(world))
		return false;

	UDeliveryBot_GridSubsystem* gridSubsystem = world->GetSubsystem<UDeliveryBot_GridSubsystem>();
	if (!IsValid(gridSubsystem))
		return false;

	if (!gridSubsystem->HasBuiltGrid())
	{
		UE_LOG(LogDeliveryBotPolicyController, Warning, TEXT("Grid upload skipped. Grid is not built yet."));
		return false;
	}

	FString gridJson;
	if (!gridSubsystem->BuildGridJson(gridJson))
	{
		UE_LOG(LogDeliveryBotPolicyController, Warning, TEXT("Grid upload skipped. Failed to build grid JSON."));
		return false;
	}

	const bool bRequestStarted = HttpPolicyComponent->SendGridJson(gridJson);

	if (bRequestStarted)
	{
		UE_LOG(
			LogDeliveryBotPolicyController,
			Log,
			TEXT("Grid upload request sent. Cells: %d, JsonLength: %d"),
			gridSubsystem->GetGridCellCount(),
			gridJson.Len()
		);
	}

	return bRequestStarted;
}

// Python 서버가 Grid를 정상 수신했는지 확인
void UDeliveryBot_PolicyControllerComponent::HandleGridUploadResponse(bool bWasSuccessful, int32 responseCode, const FString& responseBody)
{
	const bool bHttpOk = bWasSuccessful && responseCode >= 200 && responseCode < 300;
	
	if (!bHttpOk)
	{
		UE_LOG(LogDeliveryBotPolicyController, Warning, TEXT("Grid upload response | Success: %s, Code: %d, Body: %s"), 
				bWasSuccessful ? TEXT("true") : TEXT("false"), responseCode, *responseBody);
		return;
	}

	UE_LOG(LogDeliveryBotPolicyController, Log,
		TEXT("Grid upload response | Success: %s, Code: %d, Body: %s"), bWasSuccessful ? TEXT("true") : TEXT("false"), responseCode, *responseBody);

	FString versionErrorMessage;
	if (!TryUpdateExpectedGridVersionFromGridUploadResponse(responseBody, versionErrorMessage))
	{
		UE_LOG(LogDeliveryBotPolicyController, Error, TEXT("Grid upload response rejected. %s"), *versionErrorMessage);
		return;
	}

	bHasCompletedGridUpload = true;
	StopGridUploadRetryLoop();

	if (!bWaitForEpisodeStartBeforePolicyLoop && bAutoStartPolicyLoop && bWaitForGridUploadBeforePolicyLoop)
	{
		StartPolicyLoop();
	}
}


void UDeliveryBot_PolicyControllerComponent::StartEpisodeStartRetryLoop()
{
	if (!GetWorld())
		return;

	const float safeInterval = FMath::Max(EpisodeStartRetryIntervalSecond, 0.1f);

	GetWorld()->GetTimerManager().SetTimer(
		EpisodeStartRetryTimerHandle,
		this,
		&UDeliveryBot_PolicyControllerComponent::RequestEpisodeStartByTimer,
		safeInterval,
		true,
		0.f
	);
}

void UDeliveryBot_PolicyControllerComponent::StopEpisodeStartRetryLoop()
{
	if (!GetWorld())
		return;

	GetWorld()->GetTimerManager().ClearTimer(EpisodeStartRetryTimerHandle);
}

void UDeliveryBot_PolicyControllerComponent::RequestEpisodeStartByTimer()
{
	if (bHasCompletedEpisodeStart)
	{
		StopEpisodeStartRetryLoop();
		return;
	}

	SendEpisodeStartToPolicyServerOnce();
}
bool UDeliveryBot_PolicyControllerComponent::SendEpisodeStartToPolicyServerOnce()
{
	if (!IsValid(OwnerDeliveryBot) || !IsValid(HttpPolicyComponent))
		return false;

	if (HttpPolicyComponent->IsEpisodeStartRequestInFlight())
		return false;

	FString episodeStartJson;
	if (!OwnerDeliveryBot->BuildEpisodeStartJson(episodeStartJson))
	{
		UE_LOG(LogDeliveryBotPolicyController, Warning, TEXT("Episode start upload skipped. Failed to build episode start JSON."));
		return false;
	}

	const bool bRequestStarted = HttpPolicyComponent->SendEpisodeStartJson(episodeStartJson);

	if (bRequestStarted)
	{
		UE_LOG(
			LogDeliveryBotPolicyController,
			Log,
			TEXT("Episode start request sent. JsonLength: %d"),
			episodeStartJson.Len()
		);
	}

	return bRequestStarted;
}

void UDeliveryBot_PolicyControllerComponent::HandleEpisodeStartResponse(bool bWasSuccessful, int32 responseCode, const FString& responseBody)
{
	const bool bHttpOk = bWasSuccessful && responseCode >= 200 && responseCode < 300;

	if (!bHttpOk)
	{
		UE_LOG(LogDeliveryBotPolicyController, Warning, TEXT("Episode start response | Success: %s, Code: %d, Body: %s"), 
				bWasSuccessful ? TEXT("true") : TEXT("false"), responseCode, *responseBody);
		return;
	}

	UE_LOG(	LogDeliveryBotPolicyController,	Log, TEXT("Episode start response | Success: %s, Code: %d, Body: %s"), bWasSuccessful ? TEXT("true") : TEXT("false"), responseCode, *responseBody);

	FString versionErrorMessage;
	if (!TryUpdateExpectedPolicyVersionsFromEpisodeStartResponse(responseBody, versionErrorMessage))
	{
		UE_LOG(	LogDeliveryBotPolicyController,	Error,	TEXT("Episode start response rejected. %s"), *versionErrorMessage);

		return;
	}

	bHasCompletedEpisodeStart = true;
	StopEpisodeStartRetryLoop();

	const bool bShouldStartPolicyLoop = bAutoStartPolicyLoop || bStartPolicyLoopAfterNextEpisodeStart;
	bStartPolicyLoopAfterNextEpisodeStart = false;

	if (bShouldStartPolicyLoop)
	{
		UE_LOG(LogDeliveryBotPolicyController, Log, TEXT("Episode start completed. Starting policy loop."));
		StartPolicyLoop();
	}
	else
	{
		UE_LOG(LogDeliveryBotPolicyController, Warning, TEXT("Episode start completed, but policy loop start was not requested."));
	}
}


bool UDeliveryBot_PolicyControllerComponent::SendEpisodeConfigUpdateToPolicyServerOnce()
{
	if (!IsValid(OwnerDeliveryBot) || !IsValid(HttpPolicyComponent))
		return false;

	if (HttpPolicyComponent->IsEpisodeConfigUpdateRequestInFlight())
		return false;

	if (HttpPolicyComponent->IsRequestInFlight())
	{
		UE_LOG(LogDeliveryBotPolicyController, Warning, TEXT("Config update skipped. Policy request is still in flight."));
		return false;
	}
	if (!bHasExpectedPolicyVersions)
	{
		UE_LOG(LogDeliveryBotPolicyController, Warning, TEXT("Config update skipped. Expected policy versions are not initialized yet."));
		return false;
	}
	
	FString configUpdateJson;
	if (!OwnerDeliveryBot->BuildEpisodeConfigUpdateJson(configUpdateJson))
	{
		UE_LOG(LogDeliveryBotPolicyController, Warning, TEXT("Config update skipped. Failed to build config update JSON."));
		return false;
	}

	StopPolicyLoop();

	const bool bRequestStarted = HttpPolicyComponent->SendEpisodeConfigUpdateJson(configUpdateJson);

	if (bRequestStarted)
	{
		UE_LOG(
			LogDeliveryBotPolicyController,
			Log,
			TEXT("Episode config update request sent. JsonLength: %d"),
			configUpdateJson.Len()
		);
	}
	else if (bAutoStartPolicyLoop && bHasExpectedPolicyVersions)
	{
		StartPolicyLoop();
	}

	return bRequestStarted;
}

bool UDeliveryBot_PolicyControllerComponent::ApplyRuntimeDriveConfigAndSendConfigUpdate(const FDeliveryBotDriveConfigInfo& driveConfigInfo)
{
	UE_LOG(
		LogDeliveryBotPolicyController,
		Log,
		TEXT("Runtime drive config update requested | MaxSpeed: %.2f, MaxReverseSpeed: %.2f"),
		driveConfigInfo.MaxSpeedKmh,
		driveConfigInfo.MaxReverseSpeedKmh
	);

	if (!IsValid(OwnerDeliveryBot) || !IsValid(HttpPolicyComponent))
	{
		UE_LOG(LogDeliveryBotPolicyController, Warning, TEXT("Drive config update skipped. Owner or HTTP component is invalid."));
		return false;
	}

	if (!bHasExpectedPolicyVersions)
	{
		UE_LOG(LogDeliveryBotPolicyController, Warning, TEXT("Drive config update skipped. Expected policy versions are not initialized yet."));
		return false;
	}

	if (HttpPolicyComponent->IsEpisodeConfigUpdateRequestInFlight())
	{
		UE_LOG(LogDeliveryBotPolicyController, Warning, TEXT("Drive config update skipped. Config update request is already in flight."));
		return false;
	}

	if (HttpPolicyComponent->IsRequestInFlight())
	{
		UE_LOG(LogDeliveryBotPolicyController, Warning, TEXT("Drive config update skipped. Policy request is still in flight."));
		return false;
	}

	StopPolicyLoop();

	OwnerDeliveryBot->ApplyRuntimeDriveConfigInfo(driveConfigInfo);

	const bool bRequestStarted = SendEpisodeConfigUpdateToPolicyServerOnce();

	UE_LOG(
		LogDeliveryBotPolicyController,
		Log,
		TEXT("Runtime drive config update request result | Started: %s"),
		bRequestStarted ? TEXT("true") : TEXT("false")
	);

	return bRequestStarted;
}

bool UDeliveryBot_PolicyControllerComponent::SendCurrentRuntimeConfigUpdateToPolicyServerOnce()
{
	UE_LOG(LogDeliveryBotPolicyController, Log, TEXT("Current runtime config update requested."));

	if (!IsValid(OwnerDeliveryBot) || !IsValid(HttpPolicyComponent))
	{
		UE_LOG(LogDeliveryBotPolicyController, Warning, TEXT("Current config update skipped. Owner or HTTP component is invalid."));
		return false;
	}

	if (!bHasExpectedPolicyVersions)
	{
		UE_LOG(LogDeliveryBotPolicyController, Warning, TEXT("Current config update skipped. Expected policy versions are not initialized yet."));
		return false;
	}

	if (HttpPolicyComponent->IsEpisodeConfigUpdateRequestInFlight())
	{
		UE_LOG(LogDeliveryBotPolicyController, Warning, TEXT("Current config update skipped. Config update request is already in flight."));
		return false;
	}

	if (HttpPolicyComponent->IsRequestInFlight())
	{
		UE_LOG(LogDeliveryBotPolicyController, Warning, TEXT("Current config update skipped. Policy request is still in flight. Try again shortly."));
		return false;
	}

	OwnerDeliveryBot->ApplyCurrentSetupInfoToRuntimeComponents();

	return SendEpisodeConfigUpdateToPolicyServerOnce();
}

bool UDeliveryBot_PolicyControllerComponent::TryUpdateExpectedConfigVersionFromEpisodeConfigUpdateResponse(
	const FString& responseBody,
	FString& outErrorMessage)
{
	outErrorMessage.Empty();

	if (!bHasExpectedPolicyVersions)
	{
		outErrorMessage = TEXT("Expected policy versions are not initialized.");
		return false;
	}

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(responseBody);

	if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
	{
		outErrorMessage = TEXT("Failed to parse episode config update response JSON.");
		return false;
	}

	int32 parsedEpisodeVersion = 0;
	int32 parsedConfigVersion = 0;
	int32 parsedGridVersion = 0;

	if (!rootObject->TryGetNumberField(TEXT("episodeVersion"), parsedEpisodeVersion) ||
		!rootObject->TryGetNumberField(TEXT("configVersion"), parsedConfigVersion) ||
		!rootObject->TryGetNumberField(TEXT("gridVersion"), parsedGridVersion))
	{
		outErrorMessage = TEXT("Config update response has missing version fields.");
		return false;
	}

	if (parsedEpisodeVersion != ExpectedEpisodeVersion)
	{
		outErrorMessage = FString::Printf(TEXT("Episode version mismatch during config update. Response: %d, Expected: %d"), parsedEpisodeVersion, ExpectedEpisodeVersion);
		return false;
	}

	if (parsedGridVersion != ExpectedGridVersion)
	{
		outErrorMessage = FString::Printf(TEXT("Grid version mismatch during config update. Response: %d, Expected: %d"), parsedGridVersion, ExpectedGridVersion);
		return false;
	}

	ExpectedConfigVersion = parsedConfigVersion;

	UE_LOG(
		LogDeliveryBotPolicyController,
		Log,
		TEXT("Expected config version updated | Config: %d"),
		ExpectedConfigVersion
	);

	return true;
}


void UDeliveryBot_PolicyControllerComponent::HandleEpisodeConfigUpdateResponse(bool bWasSuccessful, int32 responseCode, const FString& responseBody)
{
	const bool bHttpOk = bWasSuccessful && responseCode >= 200 && responseCode < 300;

	if (!bHttpOk)
	{
		UE_LOG(LogDeliveryBotPolicyController, Warning, TEXT("Episode config update response | Success: %s, Code: %d, Body: %s"),
			bWasSuccessful ? TEXT("true") : TEXT("false"), responseCode, *responseBody);

		if (bAutoStartPolicyLoop && bHasExpectedPolicyVersions)
		{
			StartPolicyLoop();
		}

		return;
	}

	FString versionErrorMessage;
	if (!TryUpdateExpectedConfigVersionFromEpisodeConfigUpdateResponse(responseBody, versionErrorMessage))
	{
		UE_LOG(LogDeliveryBotPolicyController, Error, TEXT("Episode config update response rejected. %s"), *versionErrorMessage);

		if (bAutoStartPolicyLoop && bHasExpectedPolicyVersions)
		{
			StartPolicyLoop();
		}

		return;
	}

	if (bAutoStartPolicyLoop)
	{
		UE_LOG(LogDeliveryBotPolicyController, Log, TEXT("Episode config update completed. Restarting policy loop."));
		StartPolicyLoop();
	}
}

void UDeliveryBot_PolicyControllerComponent::RecordPolicyFailure(const FDeliveryBotHttpPolicyResponseInfo& responseInfo)
{
	const FString failureLine = FString::Printf(
		TEXT("Seq=%d Episode=%d Config=%d Grid=%d Status=%s Error=%s Policy=%s Reason=%s"),
		responseInfo.Sequence,
		responseInfo.EpisodeVersion,
		responseInfo.ConfigVersion,
		responseInfo.GridVersion,
		*responseInfo.Status,
		*responseInfo.ErrorMessage,
		*responseInfo.Debug.PolicyName,
		*responseInfo.Debug.Reason
	);

	PolicyFailureHistory.Add(failureLine);

	const int32 safeMaxCount = FMath::Max(MaxPolicyFailureHistoryCount, 1);
	while (PolicyFailureHistory.Num() > safeMaxCount)
	{
		PolicyFailureHistory.RemoveAt(0);
	}
}

bool UDeliveryBot_PolicyControllerComponent::IsGoalReachedPolicyResponse(const FDeliveryBotHttpPolicyResponseInfo& responseInfo) const
{
	return responseInfo.Debug.Reason.Equals(TEXT("goal_reached"), ESearchCase::IgnoreCase);

}

bool UDeliveryBot_PolicyControllerComponent::SendEpisodeStartAndStartPolicyLoopOnce()
{
	bStartPolicyLoopAfterNextEpisodeStart = true;

	const bool bRequestStarted = SendEpisodeStartToPolicyServerOnce();
	if (!bRequestStarted)
	{
		bStartPolicyLoopAfterNextEpisodeStart = false;
	}

	return bRequestStarted;
}

