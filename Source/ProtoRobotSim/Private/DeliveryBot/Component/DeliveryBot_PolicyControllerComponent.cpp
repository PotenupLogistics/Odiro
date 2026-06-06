#include "DeliveryBot/Component/DeliveryBot_PolicyControllerComponent.h"

#include "DeliveryBot/Actor/DeliveryBot.h"
#include "DeliveryBot/Component/DeliveryBot_HttpPolicyComponent.h"
#include "DeliveryBot/Subsystem/DeliveryBot_GridSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeliveryBotPolicyController, Log, All);

UDeliveryBot_PolicyControllerComponent::UDeliveryBot_PolicyControllerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}
void UDeliveryBot_PolicyControllerComponent::InitializePolicyController(ADeliveryBot* ownerDeliveryBot,	UDeliveryBot_HttpPolicyComponent* httpPolicyComponent)
{
	OwnerDeliveryBot = ownerDeliveryBot;
	HttpPolicyComponent = httpPolicyComponent;

	if (IsValid(HttpPolicyComponent))
	{
		HttpPolicyComponent->OnParsedPolicyResponse.AddUniqueDynamic(this, &UDeliveryBot_PolicyControllerComponent::HandleParsedPolicyResponse);
		HttpPolicyComponent->OnGridResponse.AddUniqueDynamic(this, &UDeliveryBot_PolicyControllerComponent::HandleGridUploadResponse);
	}

	if (bAutoStartPolicyLoop)
	{
		if (bWaitForGridUploadBeforePolicyLoop)
		{
			StartGridUploadRetryLoop();
		}
		else
		{
			SendGridToPolicyServerOnce();
			StartPolicyLoop();
		}
	}
}

void UDeliveryBot_PolicyControllerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopPolicyLoop();
	StopGridUploadRetryLoop();

	if (IsValid(HttpPolicyComponent))
	{
		HttpPolicyComponent->OnParsedPolicyResponse.RemoveDynamic(this,	&UDeliveryBot_PolicyControllerComponent::HandleParsedPolicyResponse);
		HttpPolicyComponent->OnGridResponse.RemoveDynamic(this,	&UDeliveryBot_PolicyControllerComponent::HandleGridUploadResponse);
	}

	Super::EndPlay(EndPlayReason);
}

void UDeliveryBot_PolicyControllerComponent::StartPolicyLoop()
{
	if (!GetWorld())
		return;

	const float safeInterval = FMath::Max(PolicyRequestIntervalSecond, 0.05f);

	GetWorld()->GetTimerManager().SetTimer(
		PolicyLoopTimerHandle,
		this,
		&UDeliveryBot_PolicyControllerComponent::RequestPolicyByTimer,
		safeInterval,
		true,
		0.f
	);
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
		return;

	OwnerDeliveryBot->SendPolicyObservationOnce();
}
void UDeliveryBot_PolicyControllerComponent::TickPolicy(float deltaTime)
{
	if (!bHasValidPolicyMoveCommand)
		return;

	if (!IsValid(OwnerDeliveryBot))
		return;

	const UWorld* world = GetWorld();
	if (!world)
		return;

	const float elapsedSinceLastPolicyAction =
		world->GetTimeSeconds() - LastValidPolicyActionWorldTimeSeconds;

	if (elapsedSinceLastPolicyAction > PolicyActionTimeoutSecond)
	{
		bHasValidPolicyMoveCommand = false;

		const FDeliveryBotMoveCommandInfo stopCommand = BuildStopMoveCommand();
		OwnerDeliveryBot->ApplyMoveCommand(stopCommand, deltaTime);

		UE_LOG(
			LogDeliveryBotPolicyController,
			Warning,
			TEXT("Policy action expired. Stop command applied. Elapsed: %.2fs"),
			elapsedSinceLastPolicyAction
		);

		return;
	}

	OwnerDeliveryBot->ApplyMoveCommand(LastValidPolicyMoveCommand, deltaTime);
}

void UDeliveryBot_PolicyControllerComponent::HandleParsedPolicyResponse(
	const FDeliveryBotHttpPolicyResponseInfo& responseInfo)
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

	FDeliveryBotMoveCommandInfo moveCommandInfo;
	FString validationErrorMessage;

	if (!TryBuildMoveCommandFromPolicyResponse(responseInfo, moveCommandInfo, validationErrorMessage))
	{
		FDeliveryBotHttpPolicyResponseInfo failureInfo = responseInfo;
		failureInfo.ErrorMessage = validationErrorMessage;
		HandlePolicyFailure(failureInfo);
		return;
	}

	ConsecutivePolicyFailureCount = 0;
	LastValidPolicyMoveCommand = moveCommandInfo;
	bHasValidPolicyMoveCommand = true;

	if (const UWorld* world = GetWorld())
	{
		LastValidPolicyActionWorldTimeSeconds = world->GetTimeSeconds();
	}

	LogValidPolicyAction(responseInfo, moveCommandInfo);
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

	const FDeliveryBotObservationInfo observation = OwnerDeliveryBot->BuildObservation();
	const float maxAllowedSpeedKmh =
		moveDirectionType == EDeliveryBotMoveDirectionType::Reverse
			? observation.VehicleSpec.MaxReverseSpeedKmh
			: observation.VehicleSpec.MaxSpeedKmh;

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
		TEXT("Policy response received | Seq: %d, Status: %s, HasAction: %s, Error: %s"),
		responseInfo.Sequence,
		*responseInfo.Status,
		responseInfo.bHasAction ? TEXT("true") : TEXT("false"),
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
	
	if (bHttpOk)
	{
		UE_LOG(
			LogDeliveryBotPolicyController,
			Log,
			TEXT("Grid upload response | Success: %s, Code: %d, Body: %s"),
			bWasSuccessful ? TEXT("true") : TEXT("false"),
			responseCode,
			*responseBody
		);
	}
	else
	{
		UE_LOG(
			LogDeliveryBotPolicyController,
			Warning,
			TEXT("Grid upload response | Success: %s, Code: %d, Body: %s"),
			bWasSuccessful ? TEXT("true") : TEXT("false"),
			responseCode,
			*responseBody
		);

		return;
	}

	bHasCompletedGridUpload = true;
	StopGridUploadRetryLoop();

	if (bAutoStartPolicyLoop && bWaitForGridUploadBeforePolicyLoop)
	{
		StartPolicyLoop();
	}
}