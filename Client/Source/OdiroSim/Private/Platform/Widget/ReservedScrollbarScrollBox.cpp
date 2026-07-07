#include "Platform/Widget/ReservedScrollbarScrollBox.h"

#include "Components/ScrollBoxSlot.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SScrollBox.h"

// SScrollBox 기본 동작에서 scroll 불필요 상태만 Hidden으로 바꿔 공간을 유지한다.
class SOdiroReservedScrollbarScrollBox : public SScrollBox
{
public:
	using FArguments = SScrollBox::FArguments;

	// SScrollBox를 구성한 뒤 disabled ScrollBar visibility를 공간 보존 상태로 바꾼다.
	void Construct(const FArguments& inArgs)
	{
		SScrollBox::Construct(inArgs);
		SetScrollbarDisabledVisibility(EVisibility::Hidden);
	}

	// 내부 ScrollBar가 scroll 불필요 상태에서도 layout 공간을 차지하도록 만든다.
	void SetScrollbarDisabledVisibility(const EVisibility inVisibility)
	{
		if (ScrollBar.IsValid())
		{
			ScrollBar->SetScrollbarDisabledVisibility(inVisibility);
			ScrollBar->Invalidate(EInvalidateWidget::Layout);
		}
	}
};

TSharedRef<SWidget> UReservedScrollbarScrollBox::RebuildWidget()
{
	TSharedRef<SOdiroReservedScrollbarScrollBox> reservedScrollBox =
		SNew(SOdiroReservedScrollbarScrollBox)
		.Style(&GetWidgetStyle())
		.ScrollBarStyle(&GetWidgetBarStyle())
		.Orientation(GetOrientation())
		.ConsumeMouseWheel(GetConsumeMouseWheel())
		.NavigationDestination(GetNavigationDestination())
		.NavigationScrollPadding(GetNavigationScrollPadding())
		.ScrollWhenFocusChanges(GetScrollWhenFocusChanges())
		.BackPadScrolling(IsBackPadScrolling())
		.FrontPadScrolling(IsFrontPadScrolling())
		.AnimateWheelScrolling(IsAnimateWheelScrolling())
		.ScrollAnimationInterpSpeed(GetScrollAnimationInterpolationSpeed())
		.WheelScrollMultiplier(GetWheelScrollMultiplier())
		.EnableTouchScrolling(GetIsTouchScrollingEnabled())
		.ConsumePointerInput(GetConsumePointerInput())
		.OnUserScrolled(BIND_UOBJECT_DELEGATE(FOnUserScrolled, SlateHandleUserScrolled))
		.OnScrollBarVisibilityChanged(BIND_UOBJECT_DELEGATE(FOnScrollBarVisibilityChanged, SlateHandleScrollBarVisibilityChanged))
		.OnFocusReceived(BIND_UOBJECT_DELEGATE(FOnScrollBoxFocusReceived, SlateHandleFocusReceived))
		.OnFocusLost(BIND_UOBJECT_DELEGATE(FOnScrollBoxFocusLost, SlateHandleFocusLost));

	MyScrollBox = reservedScrollBox;
	for (UPanelSlot* panelSlot : Slots)
	{
		if (UScrollBoxSlot* typedSlot = Cast<UScrollBoxSlot>(panelSlot))
		{
			typedSlot->Parent = this;
			typedSlot->BuildSlot(MyScrollBox.ToSharedRef());
		}
	}

	ApplyReservedScrollbarVisibility();
	return reservedScrollBox;
}

void UReservedScrollbarScrollBox::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ApplyReservedScrollbarVisibility();
}

void UReservedScrollbarScrollBox::ApplyReservedScrollbarVisibility()
{
	if (!MyScrollBox.IsValid())
	{
		return;
	}

	const TSharedPtr<SOdiroReservedScrollbarScrollBox> reservedScrollBox =
		StaticCastSharedPtr<SOdiroReservedScrollbarScrollBox>(MyScrollBox);
	if (reservedScrollBox.IsValid())
	{
		reservedScrollBox->SetScrollbarDisabledVisibility(EVisibility::Hidden);
	}
}
