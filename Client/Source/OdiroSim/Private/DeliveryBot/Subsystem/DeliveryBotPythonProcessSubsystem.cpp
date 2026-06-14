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

DEFINE_LOG_CATEGORY_STATIC(LogDeliveryBotPython, Log, All);

namespace
{
	// /health 응답이 Python 서버 준비 완료인지 확인한다.
	bool IsHealthResponseOk(const FHttpResponsePtr& response)
	{
		if (!response.IsValid() || response->GetResponseCode() < 200 || response->GetResponseCode() >= 300)
			return false;

		TSharedPtr<FJsonObject> rootObject;
		const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(response->GetContentAsString());

		FString status;
		return FJsonSerializer::Deserialize(reader, rootObject)
			&& rootObject.IsValid()
			&& rootObject->TryGetStringField(TEXT("status"), status)
			&& status.Equals(TEXT("ok"), ESearchCase::IgnoreCase);
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

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> request = FHttpModule::Get().CreateRequest();
	request->SetURL(BuildHealthUrl());
	request->SetVerb(TEXT("GET"));

	request->OnProcessRequestComplete().BindWeakLambda(this, [this](FHttpRequestPtr, FHttpResponsePtr response, bool)
		{
			bHealthRequestInFlight = false;
			if (IsHealthResponseOk(response))
			{
				RecordReady(true);
				return;
			}
			LaunchPythonProcess();
		});

	request->ProcessRequest();
}

// Tools/PythonAgent/server.py 프로세스를 실행한다.
bool UDeliveryBotPythonProcessSubsystem::LaunchPythonProcess()
{
	const FString scriptPath = ResolveServerScriptPath();

	if (!FPaths::FileExists(scriptPath))
	{
		RecordFailed(FString::Printf(TEXT("Python server script not found: %s"), *scriptPath));
		return false;
	}

	const FString arguments = FString::Printf(TEXT("-u \"%s\""), *scriptPath);

	PythonState = EDeliveryBotPythonProcessState::Launching;

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
		RecordFailed(TEXT("Python server process launch failed."));
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

	if (FPlatformTime::Seconds() - HealthCheckStartTimeSeconds > Settings.StartupTimeoutSeconds)
	{
		RecordFailed(TEXT("Python health check timed out."));
		return;
	}

	bHealthRequestInFlight = true;

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> request = FHttpModule::Get().CreateRequest();
	request->SetURL(BuildHealthUrl());
	request->SetVerb(TEXT("GET"));

	request->OnProcessRequestComplete().BindWeakLambda(
		this,
		[this](FHttpRequestPtr, FHttpResponsePtr response, bool)
		{
			bHealthRequestInFlight = false;

			if (IsHealthResponseOk(response))
			{
				RecordReady(false);
			}
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

// server.py 절대 경로를 만든다.
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