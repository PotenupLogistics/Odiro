#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "ProjectEpisodeReplayCardWidget.generated.h"

class UBorder;
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
	// WBP가 작성한 surface 색을 캐시하고 active 상태를 동기화한다.
	virtual void NativeConstruct() override;

	// Native listener를 해제한다.
	virtual void NativeDestruct() override;

	// Dashboard episode item ViewModel에서 replay 요청 상태를 복사한다.
	void InitializeFromEpisodeViewModel(const UExperimentResultEpisodeViewModel* episodeItem);

	// 현재 replay viewer에 열린 episode card 강조 상태를 설정한다.
	void SetActiveReplay(bool bInActiveReplay);

	// 현재 replay viewer에 열린 episode card인지 반환한다.
	bool IsActiveReplay() const { return bActiveReplay; }

	// 이 card가 나타내는 episode id를 반환한다.
	const FString& GetEpisodeId() const { return EpisodeId; }

	// 이 card가 나타내는 replay artifact directory를 반환한다.
	const FString& GetEpisodeDirectory() const { return EpisodeDirectory; }

	// 이 card의 V1 replay artifact 파일이 모두 존재하는지 반환한다.
	bool IsReplayAvailable() const { return bReplayAvailable; }

	// Card가 부모 UI에 replay 열기를 요청할 때 broadcast한다.
	FProjectEpisodeReplayRequestedNative OnReplayRequested;

private:
	// Hover state 처리 없이 card click을 native replay request로 변환한다.
	virtual FReply NativeOnMouseButtonDown(
		const FGeometry& inGeometry,
		const FPointerEvent& inMouseEvent) override;

	// WBP-authored inactive surface 색을 아직 캡처하지 않았다면 가져온다.
	void CacheInactiveReplayCardSurfaceColor();

	// Active replay state를 WBP-owned surface 색에 반영한다.
	void RefreshActiveReplayVisual();

	// Widget Blueprint가 소유한 card surface.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ReplayCardSurface;

	// 이 card가 replay viewer source일 때 사용하는 WBP-authored surface 색.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|Replay Card|Style", meta = (AllowPrivateAccess = "true"))
	FLinearColor ActiveReplayCardSurfaceColor;

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

	// Replay viewer에 현재 열린 episode card 여부.
	UPROPERTY(Transient)
	bool bActiveReplay = false;

	// Construct 이후 캡처한 WBP-authored inactive surface 색.
	UPROPERTY(Transient)
	FLinearColor InactiveReplayCardSurfaceColor;

	// Inactive surface 색을 WBP에서 캡처했는지 여부.
	UPROPERTY(Transient)
	bool bHasInactiveReplayCardSurfaceColor = false;
};
