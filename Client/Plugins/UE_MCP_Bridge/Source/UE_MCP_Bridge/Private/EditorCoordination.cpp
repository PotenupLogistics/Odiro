#include "EditorCoordination.h"

#include "MCPBridgeOperationCoordinator.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#if PLATFORM_WINDOWS
#include "ILiveCodingModule.h"
#endif

namespace
{
	FCriticalSection GActiveRequestsMutex;
	int32 GActiveRequests = 0;

	/** Load a JSON object from disk when the file exists and is valid JSON. */
	TSharedPtr<FJsonObject> LoadJsonObject(const FString& FilePath)
	{
		FString Text;
		if (!FPaths::FileExists(FilePath) || !FFileHelper::LoadFileToString(Text, *FilePath))
		{
			return nullptr;
		}

		TSharedPtr<FJsonObject> Object;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
		{
			return nullptr;
		}
		return Object;
	}

	/** Save a JSON object to disk after ensuring its parent directory exists. */
	bool SaveJsonObject(const FString& FilePath, const TSharedRef<FJsonObject>& Object)
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), true);

		FString Serialized;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
		if (!FJsonSerializer::Serialize(Object, Writer))
		{
			return false;
		}

		const FString TempPath = FilePath + TEXT(".tmp.") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
		if (!FFileHelper::SaveStringToFile(Serialized, *TempPath))
		{
			return false;
		}

	IFileManager& FileManager = IFileManager::Get();
	if (!FileManager.Move(*FilePath, *TempPath, true, true, true, true))
	{
		FileManager.Delete(*TempPath, false, true, true);
			return false;
		}
		return true;
	}

	/** Return whether a persisted maintenance phase represents a terminal reload state. */
	bool IsTerminalMaintenancePhase(const FString& Phase)
	{
		return Phase.Equals(TEXT("completed"), ESearchCase::IgnoreCase)
			|| Phase.Equals(TEXT("failed"), ESearchCase::IgnoreCase)
			|| Phase.Equals(TEXT("restart_failed"), ESearchCase::IgnoreCase)
			|| Phase.Equals(TEXT("port_timeout"), ESearchCase::IgnoreCase)
			|| Phase.Equals(TEXT("editor_crashed"), ESearchCase::IgnoreCase)
			|| Phase.Equals(TEXT("modal_blocked"), ESearchCase::IgnoreCase)
			|| Phase.Equals(TEXT("crash_report_pending"), ESearchCase::IgnoreCase);
	}

	/** Build the Live Coding status payload shared by reload coordination status. */
	TSharedRef<FJsonObject> BuildLiveCodingStatusObject()
	{
		TSharedRef<FJsonObject> LiveCoding = MakeShared<FJsonObject>();
#if PLATFORM_WINDOWS
		ILiveCodingModule* Live = FModuleManager::GetModulePtr<ILiveCodingModule>(TEXT("LiveCoding"));
		LiveCoding->SetBoolField(TEXT("available"), Live != nullptr);
		LiveCoding->SetBoolField(TEXT("enabledByDefault"), false);
		LiveCoding->SetBoolField(TEXT("enabledForSession"), false);
		LiveCoding->SetBoolField(TEXT("canEnableForSession"), false);
		LiveCoding->SetBoolField(TEXT("started"), false);
		LiveCoding->SetBoolField(TEXT("compiling"), false);
		LiveCoding->SetStringField(TEXT("enableError"), TEXT(""));
		if (Live)
		{
			LiveCoding->SetBoolField(TEXT("enabledByDefault"), Live->IsEnabledByDefault());
			LiveCoding->SetBoolField(TEXT("enabledForSession"), Live->IsEnabledForSession());
			LiveCoding->SetBoolField(TEXT("canEnableForSession"), Live->CanEnableForSession());
			LiveCoding->SetBoolField(TEXT("started"), Live->HasStarted());
			LiveCoding->SetBoolField(TEXT("compiling"), Live->IsCompiling());
			LiveCoding->SetStringField(TEXT("enableError"), Live->GetEnableErrorText().ToString());
		}
#else
		LiveCoding->SetBoolField(TEXT("available"), false);
		LiveCoding->SetBoolField(TEXT("enabledByDefault"), false);
		LiveCoding->SetBoolField(TEXT("enabledForSession"), false);
		LiveCoding->SetBoolField(TEXT("canEnableForSession"), false);
		LiveCoding->SetBoolField(TEXT("started"), false);
		LiveCoding->SetBoolField(TEXT("compiling"), false);
		LiveCoding->SetStringField(TEXT("enableError"), TEXT(""));
		LiveCoding->SetStringField(TEXT("note"), TEXT("Live Coding is Windows-only."));
#endif
		return LiveCoding;
	}
}

FString FMCPBridgeCoordination::GetStateDirectory()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UE_MCP_Bridge"));
}

FString FMCPBridgeCoordination::GetMaintenanceFilePath()
{
	return FPaths::Combine(GetStateDirectory(), TEXT("maintenance.json"));
}

bool FMCPBridgeCoordination::IsMaintenanceActive()
{
	TSharedPtr<FJsonObject> Maintenance = LoadJsonObject(GetMaintenanceFilePath());
	if (!Maintenance.IsValid())
	{
		return FPaths::FileExists(GetMaintenanceFilePath());
	}

	FString Phase;
	if (Maintenance->TryGetStringField(TEXT("phase"), Phase) && IsTerminalMaintenancePhase(Phase))
	{
		return false;
	}
	return true;
}

bool FMCPBridgeCoordination::IsCoordinationMethod(const FString& MethodName)
{
	return MethodName.StartsWith(TEXT("coordination_"), ESearchCase::IgnoreCase);
}

void FMCPBridgeCoordination::BeginActiveRequest()
{
	FScopeLock Lock(&GActiveRequestsMutex);
	++GActiveRequests;
}

void FMCPBridgeCoordination::EndActiveRequest()
{
	FScopeLock Lock(&GActiveRequestsMutex);
	GActiveRequests = FMath::Max(0, GActiveRequests - 1);
}

int32 FMCPBridgeCoordination::GetActiveRequestCount()
{
	FScopeLock Lock(&GActiveRequestsMutex);
	return GActiveRequests;
}

TSharedPtr<FJsonValue> FMCPBridgeCoordination::MakeMaintenancePendingResult(const FString& MethodName)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), false);
	Result->SetStringField(TEXT("error"), TEXT("maintenance_pending"));
	Result->SetStringField(TEXT("code"), TEXT("maintenance_pending"));
	Result->SetStringField(TEXT("method"), MethodName);
	Result->SetStringField(TEXT("maintenancePath"), GetMaintenanceFilePath());
	Result->SetNumberField(TEXT("retryAfterMs"), 1000.0);
	return MakeShared<FJsonValueObject>(Result);
}

TSharedPtr<FJsonValue> FMCPBridgeCoordination::BuildStatusResult()
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("stateDirectory"), GetStateDirectory());
	Result->SetStringField(TEXT("maintenancePath"), GetMaintenanceFilePath());
	Result->SetBoolField(TEXT("maintenance"), IsMaintenanceActive());
	Result->SetNumberField(TEXT("activeRequests"), GetActiveRequestCount());
	Result->SetObjectField(TEXT("operations"), FMCPBridgeOperationCoordinator::BuildSnapshotObject());
	Result->SetNumberField(TEXT("editorPid"), static_cast<double>(FPlatformProcess::GetCurrentProcessId()));
	Result->SetStringField(TEXT("timestamp"), FDateTime::UtcNow().ToIso8601());
	Result->SetObjectField(TEXT("liveCoding"), BuildLiveCodingStatusObject());

	if (TSharedPtr<FJsonObject> Maintenance = LoadJsonObject(GetMaintenanceFilePath()))
	{
		Result->SetObjectField(TEXT("maintenanceState"), Maintenance);
		FString Phase;
		if (Maintenance->TryGetStringField(TEXT("phase"), Phase))
		{
			Result->SetBoolField(TEXT("maintenanceTerminal"), IsTerminalMaintenancePhase(Phase));
		}
	}

	const FString PortFilePath = FPaths::Combine(GetStateDirectory(), TEXT("port.json"));
	Result->SetStringField(TEXT("portPath"), PortFilePath);
	if (TSharedPtr<FJsonObject> Port = LoadJsonObject(PortFilePath))
	{
		Result->SetObjectField(TEXT("portState"), Port);
	}

	return MakeShared<FJsonValueObject>(Result);
}

bool FMCPBridgeCoordination::WriteMaintenanceFile(const FString& JobId, const FString& Reason, const FString& Operation, const FString& Phase, FString& OutError)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("jobId"), JobId);
	Object->SetStringField(TEXT("reason"), Reason);
	Object->SetStringField(TEXT("operation"), Operation);
	Object->SetStringField(TEXT("phase"), Phase);
	Object->SetStringField(TEXT("source"), TEXT("UE_MCP_Bridge"));
	Object->SetStringField(TEXT("requestedAt"), FDateTime::UtcNow().ToIso8601());
	Object->SetNumberField(TEXT("editorPid"), static_cast<double>(FPlatformProcess::GetCurrentProcessId()));

	if (!SaveJsonObject(GetMaintenanceFilePath(), Object))
	{
		OutError = FString::Printf(TEXT("Failed to write maintenance sentinel: %s"), *GetMaintenanceFilePath());
		return false;
	}
	return true;
}
