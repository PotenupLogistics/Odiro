#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ProjectEpisodeReplayViewerWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UScenarioReplaySubsystem;

// Embedded project-run episode replay viewer used by WBP_Replay.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UProjectEpisodeReplayViewerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Loads one episode replay into the already-created embedded viewer and starts playback.
	UFUNCTION(BlueprintCallable, Category = "Project|Replay")
	bool OpenEpisodeReplay(const FString& EpisodeDirectory);

	// Stops playback, unloads transient replay actors, and clears the viewer state.
	UFUNCTION(BlueprintCallable, Category = "Project|Replay")
	void ResetReplay();

	// Returns the latest viewer diagnostic text for parent UI status mirroring.
	const FString& GetLastDiagnosticsText() const { return LastDiagnosticsText; }

protected:
	// Binds optional WBP controls owned by WBP_Replay.
	virtual void NativeConstruct() override;

	// Unbinds optional WBP controls owned by WBP_Replay.
	virtual void NativeDestruct() override;

private:
	// Toggles the active replay subsystem between playing and paused.
	UFUNCTION()
	void HandlePlayPauseClicked();

	// Pauses playback at the current frame so Play can resume from there.
	UFUNCTION()
	void HandleStopClicked();

	// Seeks the active replay back to the first frame while keeping it loaded.
	UFUNCTION()
	void HandleResetClicked();

	// Hides the embedded viewer and unloads transient replay state.
	UFUNCTION()
	void HandleCloseClicked();

	// Returns the world replay subsystem for this viewer.
	UScenarioReplaySubsystem* GetReplaySubsystem() const;

	// Applies the replay render target to ReplayImage.
	void ApplyReplayRenderTarget();

	// Writes a status message into the optional replay diagnostics text block.
	void SetDiagnosticsText(const FString& Message);

	// Image that displays the replay render target.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> ReplayImage;

	// Button that toggles replay playback between playing and paused.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> PlayPauseButton;

	// Alternate button name for pause/resume controls in newer WBP layouts.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> PauseButton;

	// Button that pauses playback at the current frame.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> StopButton;

	// Button that resets playback to the first frame.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ResetButton;

	// Button that hides the embedded replay viewer.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	// Optional status text owned by WBP_Replay.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReplayDiagnosticsText;

	// Episode directory currently loaded by this viewer.
	UPROPERTY(Transient)
	FString LoadedEpisodeDirectory;

	// Latest diagnostic text emitted by this viewer.
	UPROPERTY(Transient)
	FString LastDiagnosticsText;
};
