#include "DeliveryBot/Subsystem/DeliveryBotPythonProcessSubsystem.h"
#include "DeliveryBot/DeliveryBotPythonDeveloperSettings.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Shared/SimulationSetupTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeliveryBotPython, Log, All);

namespace
{
	const TCHAR* ToPythonProcessStateString(EDeliveryBotPythonProcessState state)
	{
		switch (state)
		{
		case EDeliveryBotPythonProcessState::NotStarted:
			return TEXT("NotStarted");
		case EDeliveryBotPythonProcessState::CheckingExistingServer:
			return TEXT("CheckingExistingServer");
		case EDeliveryBotPythonProcessState::Launching:
			return TEXT("Launching");
		case EDeliveryBotPythonProcessState::WaitingForHealth:
			return TEXT("WaitingForHealth");
		case EDeliveryBotPythonProcessState::Ready:
			return TEXT("Ready");
		case EDeliveryBotPythonProcessState::Failed:
			return TEXT("Failed");
		case EDeliveryBotPythonProcessState::Stopped:
			return TEXT("Stopped");
		default:
			return TEXT("Unknown");
		}
	}

	// health 응답과 요청 경로를 같은 비교 형식으로 맞춘다.
	FString NormalizePolicyRuntimePath(FString path)
	{
		FPaths::NormalizeFilename(path);
		return FPaths::ConvertRelativePathToFull(path);
	}

	// /health 응답이 같은 policy package를 사용하는 서버인지 확인한다.
	bool IsHealthResponseOk(const FHttpResponsePtr& response, const FString& expectedPolicyPath)
	{
		if (!response.IsValid() || response->GetResponseCode() < 200 || response->GetResponseCode() >= 300)
			return false;

		TSharedPtr<FJsonObject> rootObject;
		const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(response->GetContentAsString());

		FString status;
		FString policyPath;
		const bool bOk = FJsonSerializer::Deserialize(reader, rootObject)
			&& rootObject.IsValid()
			&& rootObject->TryGetStringField(TEXT("status"), status)
			&& status.Equals(TEXT("ok"), ESearchCase::IgnoreCase);
		if (!bOk)
		{
			return false;
		}

		if (!rootObject->TryGetStringField(TEXT("policyPath"), policyPath))
		{
			return false;
		}

		return NormalizePolicyRuntimePath(policyPath).Equals(
			NormalizePolicyRuntimePath(expectedPolicyPath),
			ESearchCase::IgnoreCase);
	}

	// Python process argument를 공백이 있는 path에도 안전하게 전달한다.
	FString QuotePythonArgument(FString value)
	{
		value.ReplaceInline(TEXT("\""), TEXT("\\\""));
		return FString::Printf(TEXT("\"%s\""), *value);
	}

	// Windows Python launcher는 명시적인 Python 3 선택 인자를 함께 전달한다.
	bool IsPythonLauncherExecutable(const FString& executablePath)
	{
		return FPaths::GetBaseFilename(executablePath).Equals(TEXT("py"), ESearchCase::IgnoreCase);
	}

	FString BuildHealthFailureSummary(const FHttpResponsePtr& response, bool bSucceeded, const FString& expectedPolicyPath)
	{
		const int32 responseCode = response.IsValid() ? response->GetResponseCode() : 0;
		FString responseBody = response.IsValid() ? response->GetContentAsString() : FString();
		responseBody = responseBody.Left(512);

		return FString::Printf(
			TEXT("Succeeded=%s Code=%d ExpectedPolicy=%s Body=%s"),
			bSucceeded ? TEXT("true") : TEXT("false"),
			responseCode,
			*expectedPolicyPath,
			*responseBody);
	}
}

// GameInstance 생성 시 Python 서버 자동 실행 흐름을 시작한다.
void UDeliveryBotPythonProcessSubsystem::Initialize(FSubsystemCollectionBase& collection)
{
	Super::Initialize(collection);

	if (const UDeliveryBotPythonDeveloperSettings* developerSettings = GetDefault<UDeliveryBotPythonDeveloperSettings>())
	{
		Settings = developerSettings->PythonSettings;
	}

	const FSimulationCommandLineParseResult commandLineResult = FSimulationCommandLine::ParseCurrent();
	if (commandLineResult.bSuccess && commandLineResult.Options.PolicyPort > 0)
	{
		Settings.Port = commandLineResult.Options.PolicyPort;
	}

	if (Settings.bAutoLaunchPythonServer)
	{
		StartPythonServerLifecycle();
	}
}

// GameInstance 종료 시 이 Subsystem이 실행한 Python 서버만 종료한다.
void UDeliveryBotPythonProcessSubsystem::Deinitialize()
{
	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().ClearTimer(HealthPollingTimerHandle);
	}

	StopPythonProcess();
	Super::Deinitialize();
}

// 기존 서버 확인부터 Python 서버 생명주기를 시작한다.
void UDeliveryBotPythonProcessSubsystem::StartPythonServerLifecycle()
{
	LastErrorMessage.Reset();
	LastHealthCheckFailureSummary.Reset();
	PythonState = EDeliveryBotPythonProcessState::CheckingExistingServer;
	HealthCheckStartTimeSeconds = FPlatformTime::Seconds();

	CheckExistingServerHealth();
}

// 이미 켜진 Python 서버가 있으면 재사용하고 없으면 새로 실행한다.
void UDeliveryBotPythonProcessSubsystem::CheckExistingServerHealth()
{
	if (bHealthRequestInFlight)
		return;

	bHealthRequestInFlight = true;
	const FString expectedPolicyPath = ResolvePolicyPackagePath();

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> request = FHttpModule::Get().CreateRequest();
	request->SetURL(BuildHealthUrl());
	request->SetVerb(TEXT("GET"));

	request->OnProcessRequestComplete().BindWeakLambda(this, [this, expectedPolicyPath](FHttpRequestPtr, FHttpResponsePtr response, bool bSucceeded)
		{
			bHealthRequestInFlight = false;
			if (IsHealthResponseOk(response, expectedPolicyPath))
			{
				RecordReady(true);
				return;
			}

			LastHealthCheckFailureSummary = BuildHealthFailureSummary(response, bSucceeded, expectedPolicyPath);
			LaunchPythonProcess();
		});

	request->ProcessRequest();
}

// Python policy runtime 프로세스를 실행한다.
bool UDeliveryBotPythonProcessSubsystem::LaunchPythonProcess()
{
	const FString scriptPath = ResolveServerScriptPath();
	const FString policyPath = ResolvePolicyPackagePath();

	if (!FPaths::FileExists(scriptPath))
	{
		RecordFailed(FString::Printf(TEXT("Python server script not found: %s"), *scriptPath));
		return false;
	}
	if (!FPaths::DirectoryExists(policyPath))
	{
		RecordFailed(FString::Printf(TEXT("Python policy package not found: %s"), *policyPath));
		return false;
	}

	const FString arguments = BuildPythonProcessArguments(scriptPath, policyPath);

	PythonState = EDeliveryBotPythonProcessState::Launching;
	LastHealthCheckFailureSummary.Reset();

	UE_LOG(
		LogDeliveryBotPython,
		Log,
		TEXT("Launching Python policy server | Exe=%s Script=%s Policy=%s HealthUrl=%s"),
		*Settings.PythonExecutablePath,
		*scriptPath,
		*policyPath,
		*BuildHealthUrl());

	PythonProcessHandle = FPlatformProcess::CreateProc(
		*Settings.PythonExecutablePath,
		*arguments,
		false,
		true,
		true,
		nullptr,
		0,
		*FPaths::ProjectDir(),
		nullptr);

	if (!PythonProcessHandle.IsValid())
	{
		RecordFailed(FString::Printf(TEXT("Python server process launch failed. Exe=%s"), *Settings.PythonExecutablePath));
		return false;
	}

	bLaunchedByThisProcess = true;
	PythonState = EDeliveryBotPythonProcessState::WaitingForHealth;
	HealthCheckStartTimeSeconds = FPlatformTime::Seconds();

	StartHealthPolling();
	return true;
}

// Python 서버가 준비될 때까지 /health를 반복 확인한다.
void UDeliveryBotPythonProcessSubsystem::StartHealthPolling()
{
	UWorld* world = GetWorld();
	if (!IsValid(world))
	{
		RecordFailed(TEXT("World is invalid while starting Python health polling."));
		return;
	}

	world->GetTimerManager().ClearTimer(HealthPollingTimerHandle);
	world->GetTimerManager().SetTimer(
		HealthPollingTimerHandle,
		this,
		&UDeliveryBotPythonProcessSubsystem::SendHealthCheckRequest,
		FMath::Max(Settings.HealthCheckIntervalSeconds, 0.05f),
		true,
		0.f);
}

// /health 요청을 한 번 보내고 성공 시 Ready로 전환한다.
void UDeliveryBotPythonProcessSubsystem::SendHealthCheckRequest()
{
	if (bHealthRequestInFlight)
		return;

	if (bLaunchedByThisProcess
		&& PythonProcessHandle.IsValid()
		&& !FPlatformProcess::IsProcRunning(PythonProcessHandle))
	{
		const FString exitMessage = LastHealthCheckFailureSummary.IsEmpty()
			? TEXT("Python server process exited before health became ready.")
			: FString::Printf(TEXT("Python server process exited before health became ready. LastHealth=%s"), *LastHealthCheckFailureSummary);
		RecordFailed(exitMessage);
		return;
	}

	if (FPlatformTime::Seconds() - HealthCheckStartTimeSeconds > Settings.StartupTimeoutSeconds)
	{
		const FString timeoutMessage = LastHealthCheckFailureSummary.IsEmpty()
			? TEXT("Python health check timed out.")
			: FString::Printf(TEXT("Python health check timed out. LastHealth=%s"), *LastHealthCheckFailureSummary);
		RecordFailed(timeoutMessage);
		return;
	}

	bHealthRequestInFlight = true;

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> request = FHttpModule::Get().CreateRequest();
	request->SetURL(BuildHealthUrl());
	request->SetVerb(TEXT("GET"));

	request->OnProcessRequestComplete().BindWeakLambda(
		this,
		[this](FHttpRequestPtr, FHttpResponsePtr response, bool bSucceeded)
		{
			bHealthRequestInFlight = false;

			if (IsHealthResponseOk(response, ResolvePolicyPackagePath()))
			{
				RecordReady(false);
				return;
			}

			LastHealthCheckFailureSummary = BuildHealthFailureSummary(response, bSucceeded, ResolvePolicyPackagePath());
		});

	request->ProcessRequest();
}

// Python 서버 사용 가능 상태를 기록한다.
void UDeliveryBotPythonProcessSubsystem::RecordReady(bool bAlreadyRunningServer)
{
	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().ClearTimer(HealthPollingTimerHandle);
	}

	if (bAlreadyRunningServer)
	{
		bLaunchedByThisProcess = false;
	}

	PythonState = EDeliveryBotPythonProcessState::Ready;
	LastErrorMessage.Reset();

	UE_LOG(LogDeliveryBotPython, Log, TEXT("Python server ready. BaseUrl=%s Existing=%s"),
		*GetBaseUrl(),
		bAlreadyRunningServer ? TEXT("true") : TEXT("false"));
}

// Python 서버 시작 실패를 기록한다.
void UDeliveryBotPythonProcessSubsystem::RecordFailed(const FString& errorMessage)
{
	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().ClearTimer(HealthPollingTimerHandle);
	}

	bHealthRequestInFlight = false;
	LastErrorMessage = errorMessage;
	PythonState = EDeliveryBotPythonProcessState::Failed;

	StopPythonProcess();

	UE_LOG(LogDeliveryBotPython, Warning, TEXT("%s"), *LastErrorMessage);
}

// 이 Subsystem이 직접 실행한 Python 서버만 종료한다.
void UDeliveryBotPythonProcessSubsystem::StopPythonProcess()
{
	if (!Settings.bStopServerOnShutdown || !bLaunchedByThisProcess)
		return;

	if (PythonProcessHandle.IsValid())
	{
		FPlatformProcess::TerminateProc(PythonProcessHandle, true);
		FPlatformProcess::CloseProc(PythonProcessHandle);
		PythonProcessHandle.Reset();
	}

	bLaunchedByThisProcess = false;
	PythonState = EDeliveryBotPythonProcessState::Stopped;
}

// Python 서버 base URL을 반환한다.
FString UDeliveryBotPythonProcessSubsystem::GetBaseUrl() const
{
	return FString::Printf(TEXT("http://%s:%d"), *Settings.Host, Settings.Port);
}

FString UDeliveryBotPythonProcessSubsystem::GetDebugStatus() const
{
	return FString::Printf(
		TEXT("State=%s BaseUrl=%s LastError=%s LastHealth=%s"),
		ToPythonProcessStateString(PythonState),
		*GetBaseUrl(),
		LastErrorMessage.IsEmpty() ? TEXT("<none>") : *LastErrorMessage,
		LastHealthCheckFailureSummary.IsEmpty() ? TEXT("<none>") : *LastHealthCheckFailureSummary);
}

// /health URL을 만든다.
FString UDeliveryBotPythonProcessSubsystem::BuildHealthUrl() const
{
	FString endpoint = Settings.HealthEndpointPath;
	if (!endpoint.StartsWith(TEXT("/")))
	{
		endpoint = TEXT("/") + endpoint;
	}

	return GetBaseUrl() + endpoint;
}

// policy-runtime.py 절대 경로를 만든다.
FString UDeliveryBotPythonProcessSubsystem::ResolveServerScriptPath() const
{
	FString scriptPath = Settings.ServerScriptRelativePath;

	if (FPaths::IsRelative(scriptPath))
	{
		scriptPath = FPaths::Combine(FPaths::ProjectDir(), scriptPath);
	}

	FPaths::NormalizeFilename(scriptPath);
	return FPaths::ConvertRelativePathToFull(scriptPath);
}

// ProjectRun이면 snapshot policy, 아니면 legacy 개발 policy를 사용한다.
FString UDeliveryBotPythonProcessSubsystem::ResolvePolicyPackagePath() const
{
	const FSimulationCommandLineParseResult commandLineResult = FSimulationCommandLine::ParseCurrent();
	if (commandLineResult.bSuccess && commandLineResult.Options.bProjectRun)
	{
		return FUserProjectRunSnapshot::BuildPaths(
			commandLineResult.Options.ProjectPath,
			commandLineResult.Options.RunId).PolicyPath;
	}

	FString policyPath = TEXT("Tools/PythonAgent/agent");
	if (FPaths::IsRelative(policyPath))
	{
		policyPath = FPaths::Combine(FPaths::ProjectDir(), policyPath);
	}

	FPaths::NormalizeFilename(policyPath);
	return FPaths::ConvertRelativePathToFull(policyPath);
}

FString UDeliveryBotPythonProcessSubsystem::BuildPythonProcessArguments(
	const FString& scriptPath,
	const FString& policyPath) const
{
	const FString launcherVersionArgument = IsPythonLauncherExecutable(Settings.PythonExecutablePath)
		? TEXT("-3 ")
		: FString();

	return FString::Printf(
		TEXT("%s-u %s --host %s --port %d --policy-mode runtime --policy-path %s"),
		*launcherVersionArgument,
		*QuotePythonArgument(scriptPath),
		*QuotePythonArgument(Settings.Host),
		Settings.Port,
		*QuotePythonArgument(policyPath));
}
