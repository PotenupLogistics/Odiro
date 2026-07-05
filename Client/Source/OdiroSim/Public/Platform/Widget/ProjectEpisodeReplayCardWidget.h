#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "ProjectEpisodeReplayCardWidget.generated.h"

class UButton;
class UExperimentResultEpisodeViewModel;
class UProjectEpisodeReplayCardWidget;
class UTextBlock;

// Episode replay card 클릭을 부모 UI로 전달하는 native event channel.
DECLARE_MULTICAST_DELEGATE_OneParam(
	FProjectEpisodeReplayRequestedNative,
	UProjectEpisodeReplayCardWidget*);

// Project result episode card의 replay 요청 데이터와 클릭 전달을 소유하는 native base.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UProjectEpisodeReplayCardWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// Card가 소유한 optional WBP control binding을 설정한다.
	virtual void NativeConstruct() override;

	// Card click binding과 native listener를 해제한다.
	virtual void NativeDestruct() override;

	// Dashboard episode item ViewModel에서 replay 요청 상태를 복사한다.
	void InitializeFromEpisodeViewModel(const UExperimentResultEpisodeViewModel* episodeItem);

	// 이 card가 나타내는 episode id를 반환한다.
	const FString& GetEpisodeId() const { return EpisodeId; }

	// 이 card가 나타내는 replay artifact directory를 반환한다.
	const FString& GetEpisodeDirectory() const { return EpisodeDirectory; }

	// 이 card의 V1 replay artifact 파일이 모두 존재하는지 반환한다.
	bool IsReplayAvailable() const { return bReplayAvailable; }

	// Card가 부모 UI에 replay 열기를 요청할 때 broadcast한다.
	FProjectEpisodeReplayRequestedNative OnReplayRequested;

private:
	// Bound button click을 native replay request로 변환한다.
	UFUNCTION()
	void HandleReplayCardClicked();

	// Card click target을 덮는 optional WBP button.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ReplayCardButton;

	// Episode id label owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EpisodeIdText;

	// Episode success/failure label owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EpisodeStateText;

	// Episode duration label owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EpisodeDurationText;

	// Replay availability label owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReplayAvailabilityText;

	// Command routing을 위해 item ViewModel에서 복사한 episode id.
	UPROPERTY(Transient)
	FString EpisodeId;

	// Replay loading을 위해 item ViewModel에서 복사한 episode directory.
	UPROPERTY(Transient)
	FString EpisodeDirectory;

	// Item ViewModel에서 복사한 replay file availability.
	UPROPERTY(Transient)
	bool bReplayAvailable = false;
};
