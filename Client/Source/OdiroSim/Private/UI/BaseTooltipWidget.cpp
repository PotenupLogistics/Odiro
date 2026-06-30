#include "UI/BaseTooltipWidget.h"
#include "UI/BaseFormElementPrivate.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "UI/BaseWidgetPrivate.h"

using namespace BaseFormElementPrivate;

void UBaseTooltipWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	const UBaseWidgetColorCatalog* colors = GetResolvedBaseColors();
	const UBaseWidgetSizeCatalog* sizes = GetResolvedBaseSizes();
	if (MessageTextBlock)
	{
		SetTextBlockValue(MessageTextBlock.Get(), Message, false);
		ApplyTextStyle(MessageTextBlock.Get(), EBaseTextRole::Caption);
		if (colors)
		{
			ApplyTextColor(MessageTextBlock.Get(), colors->BackgroundColor);
		}
	}
	if (colors && sizes)
	{
		// Tooltips invert the dark theme using existing base tokens.
		BaseWidgetPrivate::ApplyRoundedSurface(
			BorderFrame.Get(),
			SurfaceBorder.Get(),
			colors->TextStrongColor,
			colors->TextStrongColor,
			sizes->Radius,
			sizes->BorderWidth);
	}
}

int32 UBaseTooltipWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId, const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	BaseWidgetPrivate::UpdateRoundedSurfaceSize(SurfaceBorder.Get(), AllottedGeometry.GetLocalSize());
	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

void UBaseTooltipWidget::SetMessage(const FText inMessage)
{
	Message = inMessage;
	SynchronizeBaseProperties();
}

void UBaseTooltipAnchorWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();
	if (!PlaceholderTextBlock)
	{
		return;
	}

	SetTextBlockValue(PlaceholderTextBlock.Get(), PlaceholderText, false);
	ApplyTextStyle(PlaceholderTextBlock.Get(), EBaseTextRole::Caption);
	if (const UBaseWidgetColorCatalog* colors = GetResolvedBaseColors())
	{
		ApplyTextColor(PlaceholderTextBlock.Get(), colors->TextMutedColor);
	}
}

void UBaseTooltipAnchorWidget::SetTooltipMessage(const FText inMessage)
{
	TooltipMessage = inMessage;
	if (ActiveTooltipWidget)
	{
		ActiveTooltipWidget->SetMessage(TooltipMessage);
	}
}

void UBaseTooltipAnchorWidget::SetTooltipDelay(const float inDelaySeconds)
{
	TooltipDelaySeconds = FMath::Max(0.0f, inDelaySeconds);
}

void UBaseTooltipAnchorWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	(void)InGeometry;
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	LastPointerScreenPosition = InMouseEvent.GetScreenSpacePosition();
	QueueTooltip();
}

FReply UBaseTooltipAnchorWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	(void)InGeometry;
	LastPointerScreenPosition = InMouseEvent.GetScreenSpacePosition();
	QueueTooltip();
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UBaseTooltipAnchorWidget::QueueTooltip()
{
	if (ActiveTooltipWidget || TooltipMessage.IsEmpty())
	{
		return;
	}

	if (UWorld* world = GetWorld())
	{
		if (world->GetTimerManager().IsTimerActive(TooltipTimerHandle))
		{
			return;
		}
		world->GetTimerManager().ClearTimer(TooltipTimerHandle);
		world->GetTimerManager().SetTimer(
			TooltipTimerHandle,
			this,
			&UBaseTooltipAnchorWidget::ShowTooltip,
			TooltipDelaySeconds,
			false);
	}
}

void UBaseTooltipAnchorWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	(void)InMouseEvent;
	HideTooltip();
	Super::NativeOnMouseLeave(InMouseEvent);
}

void UBaseTooltipAnchorWidget::NativeDestruct()
{
	HideTooltip();
	Super::NativeDestruct();
}

void UBaseTooltipAnchorWidget::ShowTooltip()
{
	if (ActiveTooltipWidget || TooltipMessage.IsEmpty())
	{
		return;
	}

	if (!TooltipWidgetClass)
	{
		return;
	}
	ActiveTooltipWidget = CreateWidget<UBaseTooltipWidget>(GetWorld(), TooltipWidgetClass);
	if (!ActiveTooltipWidget)
	{
		return;
	}

	ActiveTooltipWidget->SetColorsOverride(ColorsOverride);
	ActiveTooltipWidget->SetSizesOverride(SizesOverride);
	ActiveTooltipWidget->SetMessage(TooltipMessage);
	ActiveTooltipWidget->AddToViewport(TooltipZOrder);
	ActiveTooltipWidget->ForceLayoutPrepass();

	FVector2D pixelPosition = FVector2D::ZeroVector;
	FVector2D mousePosition = FVector2D::ZeroVector;
	USlateBlueprintLibrary::AbsoluteToViewport(this, LastPointerScreenPosition, pixelPosition, mousePosition);
	const FVector2D viewportSize = UWidgetLayoutLibrary::GetViewportSize(this);
	const FVector2D desiredSize = ActiveTooltipWidget->GetDesiredSize();
	const FVector2D tooltipSize(
		EstimatedTooltipSize.X > 0.0f ? EstimatedTooltipSize.X : desiredSize.X,
		EstimatedTooltipSize.Y > 0.0f ? EstimatedTooltipSize.Y : desiredSize.Y);
	ActiveTooltipWidget->SetAlignmentInViewport(ResolveTooltipAlignment(mousePosition, viewportSize, tooltipSize));
	ActiveTooltipWidget->SetPositionInViewport(mousePosition, false);
}

void UBaseTooltipAnchorWidget::HideTooltip()
{
	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().ClearTimer(TooltipTimerHandle);
	}
	if (ActiveTooltipWidget)
	{
		ActiveTooltipWidget->RemoveFromParent();
		ActiveTooltipWidget = nullptr;
	}
}
