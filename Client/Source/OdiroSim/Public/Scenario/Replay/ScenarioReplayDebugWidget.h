#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ScenarioReplayDebugWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;

// Temporary native widget base for manually loading and playing one episode replay.
UCLASS()
class ODIROSIM_API UScenarioReplayDebugWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Updates the episode directory used by the Replay button.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Replay")
	void SetDebugEpisodeDirectory(const FString& EpisodeDirectory);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// Handles the temporary Replay button click.
	UFUNCTION()
	void HandleReplayClicked();

	// Handles the temporary Play/Pause button click.
	UFUNCTION()
	void HandlePlayPauseClicked();

	// Writes diagnostics into the optional debug text block.
	void SetDiagnosticsText(const FString& Message);

	// Binds to a WBP button named ReplayButton when present.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ReplayButton;

	// Binds to a WBP button named PlayPauseButton when present.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> PlayPauseButton;

	// Binds to a WBP image named ReplayImage when present.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> ReplayImage;

	// Binds to a WBP text block named ReplayDiagnosticsText when present.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReplayDiagnosticsText;

	// Episode directory loaded by the temporary debug button.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Replay", meta = (AllowPrivateAccess = "true"))
	FString DebugEpisodeDirectory;
};
