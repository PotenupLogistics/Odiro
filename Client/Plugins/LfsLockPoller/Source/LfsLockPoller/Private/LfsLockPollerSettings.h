#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "LfsLockPollerSettings.generated.h"

/** Configures the editor-side guard that keeps Git LFS lockable assets read-only unless locked by this user. */
UCLASS(Config = Editor, DefaultConfig, DisplayName = "LFS Lock Poller")
class ULfsLockPollerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Enables the background permission synchronization while the editor is running. */
	UPROPERTY(Config, EditAnywhere, Category = "Polling")
	bool bEnabled = true;

	/** Seconds between lock-state refreshes; lower values increase Git LFS server traffic. */
	UPROPERTY(Config, EditAnywhere, Category = "Polling", meta = (ClampMin = "15", UIMin = "15", ForceUnits = "s"))
	int32 PollIntervalSeconds = 60;

	/** Git executable used for lock and tracked-file queries. */
	UPROPERTY(Config, EditAnywhere, Category = "Git")
	FString GitBinaryPath = TEXT("git");

	/** File extensions treated as exclusive-lock binary assets. */
	UPROPERTY(Config, EditAnywhere, Category = "Lockable")
	TArray<FString> LockableExtensions = { TEXT(".uasset"), TEXT(".umap"), TEXT(".ubulk"), TEXT(".uexp") };
};
