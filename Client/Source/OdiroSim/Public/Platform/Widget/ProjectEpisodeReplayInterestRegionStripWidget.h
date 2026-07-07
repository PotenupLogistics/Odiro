#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Scenario/Replay/ScenarioReplaySubsystem.h"
#include "ProjectEpisodeReplayInterestRegionStripWidget.generated.h"

class UHorizontalBox;
class UProjectEpisodeReplayInterestEventCardWidget;
class UProjectEpisodeReplayInterestRegionStripWidget;
class UScrollBox;
class UTextBlock;

// Native event channel used when an interest-region strip requests a replay seek.
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FProjectEpisodeReplayInterestRegionSelectedNative,
	UProjectEpisodeReplayInterestRegionStripWidget*,
	double,
	int32);

// Horizontal replay interest-region strip driven by WBP_ReplayInterestRegionStrip.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UProjectEpisodeReplayInterestRegionStripWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// Sets the default event-card Widget Blueprint class used by the strip.
	UProjectEpisodeReplayInterestRegionStripWidget(const FObjectInitializer& ObjectInitializer);

	// Clears runtime card listeners before the Widget Blueprint is destroyed.
	virtual void NativeDestruct() override;

	// Rebuilds the strip from replay event markers.
	void SetEventMarkers(
		const TArray<FScenarioReplayEventMarker>& InMarkers,
		double CurrentTimeSeconds);

	// Clears all runtime cards and count text.
	void ClearEventMarkers();

	// Updates the selected card from current replay time.
	void SetCurrentTime(
		double CurrentTimeSeconds,
		bool bScrollSelectedIntoView = false);

	// Selects and scrolls to a card by stable event index.
	bool FocusEventByIndex(int32 EventIndex);

	// Broadcast when one card is selected.
	FProjectEpisodeReplayInterestRegionSelectedNative OnInterestEventSelected;

private:
	// Handles one runtime card selection.
	void HandleCardSelected(
		UProjectEpisodeReplayInterestEventCardWidget* CardWidget,
		double TimeSeconds,
		int32 EventIndex);

	// Returns the card index that is close enough to the current replay time.
	int32 ResolveSelectedCardIndex(double CurrentTimeSeconds) const;

	// Applies selection state to all cards and optionally scrolls the selected card into view.
	void SetSelectedCardIndex(
		int32 NewSelectedCardIndex,
		bool bScrollSelectedIntoView);

	// Finds the runtime card index for one card pointer.
	int32 FindCardIndex(
		const UProjectEpisodeReplayInterestEventCardWidget* CardWidget) const;

	// Finds the runtime card index for one stable replay event id.
	int32 FindCardIndexByEventIndex(int32 EventIndex) const;

	// Toggles the WBP-authored card row and empty-state message for current card availability.
	void UpdateEmptyStateVisibility(bool bHasCards);

	// Count label owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> InterestCountText;

	// Horizontal scroll view owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> InterestScrollBox;

	// Runtime card row owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> InterestCardRow;

	// Empty-state message owned by the Widget Blueprint when no replay events exist.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyStateText;

	// Widget Blueprint class used for each replay event card.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|Replay", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UProjectEpisodeReplayInterestEventCardWidget> EventCardWidgetClass;

	// Event markers currently represented by the strip.
	UPROPERTY(Transient)
	TArray<FScenarioReplayEventMarker> EventMarkers;

	// Runtime card widgets currently owned by the strip.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProjectEpisodeReplayInterestEventCardWidget>> EventCards;

	// Current selected card index, or INDEX_NONE when no card is active.
	int32 SelectedCardIndex = INDEX_NONE;

	// User-focused event id kept selected while replay time remains near it.
	int32 FocusedEventIndex = INDEX_NONE;

	// Maximum time distance for a card to be considered active.
	double SelectionWindowSeconds = 0.35;
};
