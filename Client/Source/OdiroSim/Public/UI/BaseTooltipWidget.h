#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "UI/BaseWidget.h"
#include "BaseTooltipWidget.generated.h"

class UBorder;
class UTextBlock;
class UWidget;

// White floating tooltip body with base-token text styling.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseTooltipWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Applies tooltip text and white surface styling.
	virtual void SynchronizeBaseProperties() override;

	// Updates the tooltip message.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Tooltip")
	void SetMessage(FText inMessage);

	// Returns the tooltip message.
	UFUNCTION(BlueprintPure, Category = "UI|Base Tooltip")
	FText GetMessage() const { return Message; }

protected:
	// Feeds rounded tooltip material size on every paint.
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// Tooltip body text.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetMessage", Setter = "SetMessage", BlueprintGetter = "GetMessage", BlueprintSetter = "SetMessage", Category = "UI|Base Tooltip", meta = (ExposeOnSpawn = "true"))
	FText Message;

	// Rounded tooltip surface owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SurfaceBorder;

	// Tooltip message text owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MessageTextBlock;
};

// Hover anchor that shows a base tooltip near the cursor after a delay.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseTooltipAnchorWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// Updates the tooltip message passed to spawned tooltip widgets.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Tooltip")
	void SetTooltipMessage(FText inMessage);

	// Returns the tooltip message passed to spawned tooltip widgets.
	UFUNCTION(BlueprintPure, Category = "UI|Base Tooltip")
	FText GetTooltipMessage() const { return TooltipMessage; }

	// Updates the hover delay before the tooltip appears.
	UFUNCTION(BlueprintCallable, Category = "UI|Base Tooltip")
	void SetTooltipDelay(float inDelaySeconds);

	// Returns the hover delay before the tooltip appears.
	UFUNCTION(BlueprintPure, Category = "UI|Base Tooltip")
	float GetTooltipDelay() const { return TooltipDelaySeconds; }

protected:
	// Starts the delayed tooltip timer.
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// Starts the same delayed timer when synthetic or routed hover arrives as movement.
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// Cancels and hides the active tooltip.
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	// Cleans up the active tooltip and timer.
	virtual void NativeDestruct() override;

	// Spawns the tooltip widget at the current cursor position.
	void ShowTooltip();

	// Queues the delayed tooltip once while the pointer remains over the anchor.
	void QueueTooltip();

	// Removes the active tooltip widget.
	void HideTooltip();

	// Tooltip widget class to spawn on hover.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Tooltip")
	TSubclassOf<UBaseTooltipWidget> TooltipWidgetClass;

	// Tooltip message passed to spawned tooltip widgets.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetTooltipMessage", Setter = "SetTooltipMessage", BlueprintGetter = "GetTooltipMessage", BlueprintSetter = "SetTooltipMessage", Category = "UI|Base Tooltip", meta = (ExposeOnSpawn = "true"))
	FText TooltipMessage;

	// Hover delay before the tooltip appears.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetTooltipDelay", Setter = "SetTooltipDelay", BlueprintGetter = "GetTooltipDelay", BlueprintSetter = "SetTooltipDelay", Category = "UI|Base Tooltip", meta = (ClampMin = "0.0", ExposeOnSpawn = "true"))
	float TooltipDelaySeconds = 1.0f;

	// Optional WBP-provided tooltip size estimate used for viewport-edge alignment before layout is measured.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Tooltip", meta = (ClampMin = "0.0", ExposeOnSpawn = "true"))
	FVector2D EstimatedTooltipSize = FVector2D::ZeroVector;

	// Viewport layer used for spawned tooltip widgets.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Base Tooltip", meta = (ExposeOnSpawn = "true"))
	int32 TooltipZOrder = 0;

	// Active tooltip widget owned by this anchor while visible.
	UPROPERTY(Transient)
	TObjectPtr<UBaseTooltipWidget> ActiveTooltipWidget;

	// Last pointer location in Slate absolute coordinates, used for viewport placement.
	UPROPERTY(Transient)
	FVector2D LastPointerScreenPosition = FVector2D::ZeroVector;

	// Timer handle for delayed tooltip display.
	FTimerHandle TooltipTimerHandle;
};
