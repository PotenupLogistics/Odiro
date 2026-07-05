#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Scenario/Replay/ScenarioReplaySubsystem.h"
#include "ProjectEpisodeReplayInterestEventCardWidget.generated.h"

class UBorder;
class UButton;
class UProjectEpisodeReplayInterestEventCardWidget;
class UTextBlock;

// Native event channel used when an interest event card is selected.
DECLARE_MULTICAST_DELEGATE_TwoParams(
	FProjectEpisodeReplayInterestEventSelectedNative,
	UProjectEpisodeReplayInterestEventCardWidget*,
	double);

// Reusable replay interest-event card driven by WBP_ReplayInterestEventCard.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UProjectEpisodeReplayInterestEventCardWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// Binds the card button owned by the Widget Blueprint.
	virtual void NativeConstruct() override;

	// Unbinds the card button and clears native listeners.
	virtual void NativeDestruct() override;

	// Copies one replay event marker into the card's authored text and accent widgets.
	void InitializeFromEventMarker(
		const FScenarioReplayEventMarker& InMarker,
		bool bInitiallySelected);

	// Updates the selected visual overlay without changing card data.
	void SetSelected(bool bNewSelected);

	// Returns the replay time represented by this card.
	double GetTimeSeconds() const { return EventMarker.TimeSeconds; }

	// Returns the stable event index represented by this card.
	int32 GetEventIndex() const { return EventMarker.EventIndex; }

	// Broadcast when the user selects this card.
	FProjectEpisodeReplayInterestEventSelectedNative OnInterestEventSelected;

private:
	// Converts the WBP button click into a replay event selection.
	UFUNCTION()
	void HandleCardClicked();

	// Click target owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CardButton;

	// Event category pill background owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> EventTypePill;

	// Event type label owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EventTypeText;

	// Event replay time label owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EventTimeText;

	// Event title label owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EventTitleText;

	// Event summary label owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EventSummaryText;

	// Event accent strip owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> EventAccentBar;

	// Selection overlay owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SelectedOverlay;

	// Replay event marker currently represented by this card.
	UPROPERTY(Transient)
	FScenarioReplayEventMarker EventMarker;
};
