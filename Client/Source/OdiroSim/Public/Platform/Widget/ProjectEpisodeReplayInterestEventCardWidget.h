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
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FProjectEpisodeReplayInterestEventSelectedNative,
	UProjectEpisodeReplayInterestEventCardWidget*,
	double,
	int32);

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

	// Updates the selected card surface without changing card data.
	void SetSelected(bool bNewSelected);

	// Reapplies WBP-authored rounded material properties to bound card surfaces.
	UFUNCTION(BlueprintCallable, Category = "Platform|Replay Interest Event Card|Material")
	void SynchronizeCardSurfaceProperties();

	// Updates the rounded card material fill color.
	UFUNCTION(BlueprintCallable, Category = "Platform|Replay Interest Event Card|Material")
	void SetCardSurfaceFillColor(FLinearColor inColor);

	// Returns the rounded card material fill color.
	UFUNCTION(BlueprintPure, Category = "Platform|Replay Interest Event Card|Material")
	FLinearColor GetCardSurfaceFillColor() const { return CardSurfaceFillColor; }

	// Updates the rounded card material stroke color.
	UFUNCTION(BlueprintCallable, Category = "Platform|Replay Interest Event Card|Material")
	void SetCardSurfaceStrokeColor(FLinearColor inColor);

	// Returns the rounded card material stroke color.
	UFUNCTION(BlueprintPure, Category = "Platform|Replay Interest Event Card|Material")
	FLinearColor GetCardSurfaceStrokeColor() const { return CardSurfaceStrokeColor; }

	// Updates the rounded card material corner radius in pixels.
	UFUNCTION(BlueprintCallable, Category = "Platform|Replay Interest Event Card|Material")
	void SetCardSurfaceRadius(float inRadius);

	// Returns the rounded card material corner radius in pixels.
	UFUNCTION(BlueprintPure, Category = "Platform|Replay Interest Event Card|Material")
	float GetCardSurfaceRadius() const { return CardSurfaceRadius; }

	// Updates the rounded card material stroke width in pixels.
	UFUNCTION(BlueprintCallable, Category = "Platform|Replay Interest Event Card|Material")
	void SetCardSurfaceBorderWidth(float inBorderWidth);

	// Returns the rounded card material stroke width in pixels.
	UFUNCTION(BlueprintPure, Category = "Platform|Replay Interest Event Card|Material")
	float GetCardSurfaceBorderWidth() const { return CardSurfaceBorderWidth; }

	// Updates the selected rounded card material fill color.
	UFUNCTION(BlueprintCallable, Category = "Platform|Replay Interest Event Card|Material")
	void SetSelectedCardSurfaceFillColor(FLinearColor inColor);

	// Returns the selected rounded card material fill color.
	UFUNCTION(BlueprintPure, Category = "Platform|Replay Interest Event Card|Material")
	FLinearColor GetSelectedCardSurfaceFillColor() const { return SelectedOverlayFillColor; }

	// Updates the selected rounded card material stroke color.
	UFUNCTION(BlueprintCallable, Category = "Platform|Replay Interest Event Card|Material")
	void SetSelectedCardSurfaceStrokeColor(FLinearColor inColor);

	// Returns the selected rounded card material stroke color.
	UFUNCTION(BlueprintPure, Category = "Platform|Replay Interest Event Card|Material")
	FLinearColor GetSelectedCardSurfaceStrokeColor() const { return SelectedOverlayStrokeColor; }

	// Returns the replay time represented by this card.
	double GetTimeSeconds() const { return EventMarker.TimeSeconds; }

	// Returns the stable event index represented by this card.
	int32 GetEventIndex() const { return EventMarker.EventIndex; }

	// Broadcast when the user selects this card.
	FProjectEpisodeReplayInterestEventSelectedNative OnInterestEventSelected;

protected:
	// Keeps the rounded card material synced in designer and runtime previews.
	virtual void NativePreConstruct() override;

	// Keeps Details-panel edits aligned with rounded material parameters.
	virtual void SynchronizeProperties() override;

#if WITH_EDITOR
	// Refreshes the designer preview immediately after card material property edits.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& propertyChangedEvent) override;
#endif

	// Feeds rounded card material size for crisp corners.
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	// Converts the WBP button click into a replay event selection.
	UFUNCTION()
	void HandleCardClicked();

	// Applies WBP-authored rounded card surface styling.
	void ApplyCardSurfaceStyle();

	// Keeps the legacy WBP overlay visually inert while CardBackground owns selection.
	void ClearSelectedOverlayStyle();

	// Click target owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CardButton;

	// Rounded card surface owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> CardBackground;

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

	// Legacy selection overlay owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SelectedOverlay;

	// Card rounded surface fill color owned by WBP_ReplayInterestEventCard.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetCardSurfaceFillColor", Setter = "SetCardSurfaceFillColor", BlueprintGetter = "GetCardSurfaceFillColor", BlueprintSetter = "SetCardSurfaceFillColor", Category = "Platform|Replay Interest Event Card|Material", meta = (AllowPrivateAccess = "true", DisplayName = "Card Surface Fill Color"))
	FLinearColor CardSurfaceFillColor;

	// Card rounded surface stroke color owned by WBP_ReplayInterestEventCard.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetCardSurfaceStrokeColor", Setter = "SetCardSurfaceStrokeColor", BlueprintGetter = "GetCardSurfaceStrokeColor", BlueprintSetter = "SetCardSurfaceStrokeColor", Category = "Platform|Replay Interest Event Card|Material", meta = (AllowPrivateAccess = "true", DisplayName = "Card Surface Stroke Color"))
	FLinearColor CardSurfaceStrokeColor;

	// Card rounded surface corner radius in pixels owned by WBP_ReplayInterestEventCard.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetCardSurfaceRadius", Setter = "SetCardSurfaceRadius", BlueprintGetter = "GetCardSurfaceRadius", BlueprintSetter = "SetCardSurfaceRadius", Category = "Platform|Replay Interest Event Card|Material", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", DisplayName = "Card Surface Radius"))
	float CardSurfaceRadius = 0.0f;

	// Card rounded surface stroke width in pixels owned by WBP_ReplayInterestEventCard.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetCardSurfaceBorderWidth", Setter = "SetCardSurfaceBorderWidth", BlueprintGetter = "GetCardSurfaceBorderWidth", BlueprintSetter = "SetCardSurfaceBorderWidth", Category = "Platform|Replay Interest Event Card|Material", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", DisplayName = "Card Surface Border Width"))
	float CardSurfaceBorderWidth = 0.0f;

	// Selected card surface fill color owned by WBP_ReplayInterestEventCard.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetSelectedCardSurfaceFillColor", Setter = "SetSelectedCardSurfaceFillColor", BlueprintGetter = "GetSelectedCardSurfaceFillColor", BlueprintSetter = "SetSelectedCardSurfaceFillColor", Category = "Platform|Replay Interest Event Card|Material", meta = (AllowPrivateAccess = "true", DisplayName = "Selected Card Surface Fill Color"))
	FLinearColor SelectedOverlayFillColor;

	// Selected card surface stroke color owned by WBP_ReplayInterestEventCard.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetSelectedCardSurfaceStrokeColor", Setter = "SetSelectedCardSurfaceStrokeColor", BlueprintGetter = "GetSelectedCardSurfaceStrokeColor", BlueprintSetter = "SetSelectedCardSurfaceStrokeColor", Category = "Platform|Replay Interest Event Card|Material", meta = (AllowPrivateAccess = "true", DisplayName = "Selected Card Surface Stroke Color"))
	FLinearColor SelectedOverlayStrokeColor;

	// Replay event marker currently represented by this card.
	UPROPERTY(Transient)
	FScenarioReplayEventMarker EventMarker;

	// Current selected state represented by the rounded card surface.
	UPROPERTY(Transient)
	bool bSelected = false;
};
