#include "LfsLockPollerSettings.h"

#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include <atomic>

DEFINE_LOG_CATEGORY_STATIC(LogLfsLockPoller, Log, All);

namespace LfsLockPoller
{
	/** Immutable settings captured on the game thread before one poll cycle starts. */
	struct FPollConfig
	{
		/** Git executable used by this poll cycle. */
		FString GitBinaryPath;

		/** Extensions that must stay guarded by Git LFS locks. */
		TSet<FString> LockableExtensions;
	};

	/** Summary of permission changes applied during one poll cycle. */
	struct FPermissionSummary
	{
		/** Files made writable because the current user owns the active lock. */
		int32 WritableCount = 0;

		/** Files made read-only because the current user does not own the active lock. */
		int32 ReadOnlyCount = 0;

		/** Files whose filesystem attributes could not be updated. */
		int32 FailureCount = 0;
	};

	/** Quotes one native process argument for Git command-line execution. */
	FString QuoteArgument(const FString& value)
	{
		FString escaped = value;
		escaped.ReplaceInline(TEXT("\""), TEXT("\\\""));
		return FString::Printf(TEXT("\"%s\""), *escaped);
	}

	/** Normalizes a repository path so Git output can be compared case-insensitively. */
	FString NormalizeGitPath(const FString& path)
	{
		FString normalized = path;
		normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
		normalized.TrimStartAndEndInline();
		while (normalized.StartsWith(TEXT("./")))
		{
			normalized.RightChopInline(2);
		}
		return normalized;
	}

	/** Normalizes extensions from settings for direct comparison with FPaths::GetExtension. */
	TSet<FString> NormalizeExtensions(const TArray<FString>& extensions)
	{
		TSet<FString> normalizedExtensions;
		for (FString extension : extensions)
		{
			extension.TrimStartAndEndInline();
			if (extension.IsEmpty())
			{
				continue;
			}
			if (!extension.StartsWith(TEXT(".")))
			{
				extension.InsertAt(0, TEXT("."));
			}
			normalizedExtensions.Add(extension.ToLower());
		}
		return normalizedExtensions;
	}

	/** Runs one Git command and returns false when Git exits unsuccessfully. */
	bool RunGitCommand(const FString& gitBinaryPath, const FString& repositoryRoot, const FString& arguments, FString& outStdout, FString& outStderr)
	{
		int32 returnCode = 1;
		const FString parameters = FString::Printf(TEXT("-C %s %s"), *QuoteArgument(repositoryRoot), *arguments);
		FPlatformProcess::ExecProcess(*gitBinaryPath, *parameters, &returnCode, &outStdout, &outStderr);
		return returnCode == 0;
	}

	/** Resolves the Git repository root that contains the current Unreal project. */
	bool ResolveRepositoryRoot(const FString& gitBinaryPath, FString& outRepositoryRoot, FString& outError)
	{
		FString stdoutText;
		FString stderrText;
		const FString projectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		if (!RunGitCommand(gitBinaryPath, projectDir, TEXT("rev-parse --show-toplevel"), stdoutText, stderrText))
		{
			outError = stderrText.IsEmpty() ? stdoutText : stderrText;
			return false;
		}

		outRepositoryRoot = stdoutText.TrimStartAndEnd();
		FPaths::NormalizeDirectoryName(outRepositoryRoot);
		return !outRepositoryRoot.IsEmpty();
	}

	/** Converts an absolute path under the repository root to Git's repository-relative form. */
	bool TryMakeRepositoryRelativePath(const FString& repositoryRoot, const FString& absolutePath, FString& outRepositoryPath)
	{
		FString root = repositoryRoot;
		FPaths::NormalizeDirectoryName(root);
		if (!root.EndsWith(TEXT("/")))
		{
			root += TEXT("/");
		}

		outRepositoryPath = absolutePath;
		FPaths::NormalizeFilename(outRepositoryPath);
		if (!FPaths::MakePathRelativeTo(outRepositoryPath, *root))
		{
			return false;
		}

		outRepositoryPath = NormalizeGitPath(outRepositoryPath);
		return !outRepositoryPath.IsEmpty() && !outRepositoryPath.StartsWith(TEXT(".."));
	}

	/** Splits Git's line-separated path output. Unreal asset names must not contain newlines. */
	TArray<FString> SplitGitPathLines(const FString& text)
	{
		TArray<FString> paths;
		text.ParseIntoArrayLines(paths, true);
		for (FString& path : paths)
		{
			path.TrimStartAndEndInline();
		}
		return paths;
	}

	/** Returns true when a Git path has one of the configured lockable extensions. */
	bool IsLockablePath(const FString& repositoryPath, const TSet<FString>& lockableExtensions)
	{
		const FString extension = FPaths::GetExtension(repositoryPath, true).ToLower();
		return lockableExtensions.Contains(extension);
	}

	/** Reads tracked project Content files that match the configured lockable extensions. */
	bool ReadTrackedLockableFiles(const FPollConfig& config, const FString& repositoryRoot, TArray<FString>& outRepositoryPaths, FString& outError)
	{
		FString contentRoot;
		if (!TryMakeRepositoryRelativePath(repositoryRoot, FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()), contentRoot))
		{
			outError = TEXT("Project Content directory is not inside the Git repository.");
			return false;
		}

		FString stdoutText;
		FString stderrText;
		const FString arguments = FString::Printf(TEXT("-c core.quotePath=false ls-files -- %s"), *QuoteArgument(contentRoot));
		if (!RunGitCommand(config.GitBinaryPath, repositoryRoot, arguments, stdoutText, stderrText))
		{
			outError = stderrText.IsEmpty() ? stdoutText : stderrText;
			return false;
		}

		for (const FString& path : SplitGitPathLines(stdoutText))
		{
			const FString normalizedPath = NormalizeGitPath(path);
			if (IsLockablePath(normalizedPath, config.LockableExtensions))
			{
				outRepositoryPaths.Add(normalizedPath);
			}
		}
		return true;
	}

	/** Adds lock paths from one Git LFS JSON array to the owned-lock set. */
	void AppendLockPathsFromArray(const TArray<TSharedPtr<FJsonValue>>& locks, TSet<FString>& outOwnedLockPaths)
	{
		for (const TSharedPtr<FJsonValue>& lockValue : locks)
		{
			const TSharedPtr<FJsonObject>* lockObject = nullptr;
			if (!lockValue.IsValid() || !lockValue->TryGetObject(lockObject) || lockObject == nullptr || !lockObject->IsValid())
			{
				continue;
			}

			FString path;
			if ((*lockObject)->TryGetStringField(TEXT("path"), path))
			{
				outOwnedLockPaths.Add(NormalizeGitPath(path));
			}
		}
	}

	/** Reads server-verified locks owned by the current Git LFS credentials. */
	bool ReadOwnedLockPaths(const FPollConfig& config, const FString& repositoryRoot, TSet<FString>& outOwnedLockPaths, FString& outError)
	{
		FString stdoutText;
		FString stderrText;
		if (!RunGitCommand(config.GitBinaryPath, repositoryRoot, TEXT("lfs locks --verify --json"), stdoutText, stderrText))
		{
			outError = stderrText.IsEmpty() ? stdoutText : stderrText;
			return false;
		}

		TSharedPtr<FJsonValue> rootValue;
		const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(stdoutText);
		if (!FJsonSerializer::Deserialize(reader, rootValue) || !rootValue.IsValid())
		{
			outError = TEXT("git lfs locks returned invalid JSON.");
			return false;
		}

		const TSharedPtr<FJsonObject>* rootObject = nullptr;
		if (!rootValue->TryGetObject(rootObject) || rootObject == nullptr || !rootObject->IsValid())
		{
			outError = TEXT("git lfs locks --verify returned an unexpected JSON shape.");
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* ours = nullptr;
		if (!(*rootObject)->TryGetArrayField(TEXT("ours"), ours) || ours == nullptr)
		{
			outError = TEXT("git lfs locks --verify JSON did not include an 'ours' lock array.");
			return false;
		}
		AppendLockPathsFromArray(*ours, outOwnedLockPaths);
		return true;
	}

	/** Applies read-only state so only paths in ownedLockPaths remain writable. */
	FPermissionSummary ApplyPermissions(const FString& repositoryRoot, const TArray<FString>& lockablePaths, const TSet<FString>& ownedLockPaths)
	{
		FPermissionSummary summary;
		IPlatformFile& platformFile = FPlatformFileManager::Get().GetPlatformFile();
		for (const FString& repositoryPath : lockablePaths)
		{
			const FString absolutePath = FPaths::ConvertRelativePathToFull(repositoryRoot, repositoryPath);
			if (!platformFile.FileExists(*absolutePath))
			{
				continue;
			}

			const bool bShouldBeWritable = ownedLockPaths.Contains(repositoryPath);
			const bool bIsReadOnly = platformFile.IsReadOnly(*absolutePath);
			if (bIsReadOnly == !bShouldBeWritable)
			{
				continue;
			}

			if (!platformFile.SetReadOnly(*absolutePath, !bShouldBeWritable))
			{
				++summary.FailureCount;
				continue;
			}

			if (bShouldBeWritable)
			{
				++summary.WritableCount;
			}
			else
			{
				++summary.ReadOnlyCount;
			}
		}
		return summary;
	}
}

/** Editor module that periodically reconciles Git LFS lock ownership with local file writability. */
class FLfsLockPollerModule final : public IModuleInterface
{
public:
	/** Starts the periodic poller when plugin settings allow it. */
	virtual void StartupModule() override
	{
		const ULfsLockPollerSettings* settings = GetDefault<ULfsLockPollerSettings>();
		if (settings == nullptr || !settings->bEnabled)
		{
			return;
		}

		const float pollIntervalSeconds = static_cast<float>(FMath::Max(settings->PollIntervalSeconds, 15));
		PollTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(this, &FLfsLockPollerModule::HandlePollTicker),
			pollIntervalSeconds);
		StartPollCycle();
	}

	/** Stops new polling and waits for the current background cycle to finish. */
	virtual void ShutdownModule() override
	{
		bShutdownRequested.store(true);
		if (PollTickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(PollTickerHandle);
			PollTickerHandle.Reset();
		}

		if (ActivePollTask.IsValid())
		{
			ActivePollTask.Wait();
		}
	}

private:
	/** Ticker callback that starts a background poll cycle without overlapping a previous cycle. */
	bool HandlePollTicker(float deltaTime)
	{
		StartPollCycle();
		return !bShutdownRequested.load();
	}

	/** Captures settings on the game thread and dispatches one worker task. */
	void StartPollCycle()
	{
		if (bShutdownRequested.load())
		{
			return;
		}

		bool bExpectedIdle = false;
		if (!bPollInProgress.compare_exchange_strong(bExpectedIdle, true))
		{
			return;
		}

		const ULfsLockPollerSettings* settings = GetDefault<ULfsLockPollerSettings>();
		if (settings == nullptr || !settings->bEnabled)
		{
			bPollInProgress.store(false);
			return;
		}

		LfsLockPoller::FPollConfig config;
		config.GitBinaryPath = settings->GitBinaryPath.IsEmpty() ? TEXT("git") : settings->GitBinaryPath;
		config.LockableExtensions = LfsLockPoller::NormalizeExtensions(settings->LockableExtensions);
		if (config.LockableExtensions.IsEmpty())
		{
			bPollInProgress.store(false);
			return;
		}

		ActivePollTask = Async(EAsyncExecution::ThreadPool, [this, config]()
		{
			RunPollCycle(config);
			bPollInProgress.store(false);
		});
	}

	/** Performs Git queries and filesystem permission updates for one cycle. */
	void RunPollCycle(const LfsLockPoller::FPollConfig& config)
	{
		FString repositoryRoot;
		FString error;
		if (!LfsLockPoller::ResolveRepositoryRoot(config.GitBinaryPath, repositoryRoot, error))
		{
			UE_LOG(LogLfsLockPoller, Warning, TEXT("Unable to resolve Git repository root: %s"), *error.TrimStartAndEnd());
			return;
		}

		TSet<FString> ownedLockPaths;
		if (!LfsLockPoller::ReadOwnedLockPaths(config, repositoryRoot, ownedLockPaths, error))
		{
			UE_LOG(LogLfsLockPoller, Warning, TEXT("Skipping permission sync because Git LFS lock query failed: %s"), *error.TrimStartAndEnd());
			return;
		}

		TArray<FString> lockablePaths;
		if (!LfsLockPoller::ReadTrackedLockableFiles(config, repositoryRoot, lockablePaths, error))
		{
			UE_LOG(LogLfsLockPoller, Warning, TEXT("Skipping permission sync because tracked asset query failed: %s"), *error.TrimStartAndEnd());
			return;
		}

		const LfsLockPoller::FPermissionSummary summary = LfsLockPoller::ApplyPermissions(repositoryRoot, lockablePaths, ownedLockPaths);
		if (summary.WritableCount > 0 || summary.ReadOnlyCount > 0 || summary.FailureCount > 0)
		{
			UE_LOG(LogLfsLockPoller, Display, TEXT("Synchronized LFS lock permissions: writable=%d read_only=%d failed=%d"),
				summary.WritableCount,
				summary.ReadOnlyCount,
				summary.FailureCount);
		}
	}

private:
	/** Registered ticker handle for periodic polling. */
	FTSTicker::FDelegateHandle PollTickerHandle;

	/** Background task running the current Git and filesystem reconciliation. */
	TFuture<void> ActivePollTask;

	/** Prevents concurrent Git LFS lock queries when a previous cycle is still running. */
	std::atomic_bool bPollInProgress { false };

	/** Signals shutdown to the ticker and pending worker task. */
	std::atomic_bool bShutdownRequested { false };
};

IMPLEMENT_MODULE(FLfsLockPollerModule, LfsLockPoller)
