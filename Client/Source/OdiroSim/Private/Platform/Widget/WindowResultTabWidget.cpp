#include "Platform/Widget/WindowResultTabWidget.h"

#include "UI/BaseButtonWidget.h"

void UWindowResultTabWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CloseButton)
	{
		CloseButton->OnBaseClicked.RemoveDynamic(this, &UWindowResultTabWidget::HandleCloseClicked);
		CloseButton->OnBaseClicked.AddDynamic(this, &UWindowResultTabWidget::HandleCloseClicked);
	}
	RefreshCloseVisibility();
}

void UWindowResultTabWidget::NativeDestruct()
{
	if (CloseButton)
	{
		CloseButton->OnBaseClicked.RemoveDynamic(this, &UWindowResultTabWidget::HandleCloseClicked);
	}
	OnCloseRequestedNative.Clear();

	Super::NativeDestruct();
}

void UWindowResultTabWidget::SetClosable(const bool bInClosable)
{
	bClosable = bInClosable;
	RefreshCloseVisibility();
}

void UWindowResultTabWidget::HandleCloseClicked(UBaseButtonWidget* button)
{
	if (!IsValid(button) || button != CloseButton)
	{
		return;
	}

	OnCloseRequestedNative.Broadcast(this);
}

void UWindowResultTabWidget::RefreshCloseVisibility() const
{
	if (CloseButton)
	{
		CloseButton->SetVisibility(bClosable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}
