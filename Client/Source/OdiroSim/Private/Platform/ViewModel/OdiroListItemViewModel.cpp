#include "Platform/ViewModel/OdiroListItemViewModel.h"

void UOdiroListItemViewModel::InitializeItem(
	const FString& itemId,
	const FString& title,
	const FString& subtitle,
	const FString& payloadPath)
{
	SetItemId(itemId);
	SetTitle(title);
	SetSubtitle(subtitle);
	SetPayloadPath(payloadPath);
}

void UOdiroListItemViewModel::SetItemId(const FString& itemId)
{
	UE_MVVM_SET_PROPERTY_VALUE(ItemId, itemId);
}

void UOdiroListItemViewModel::SetTitle(const FString& title)
{
	UE_MVVM_SET_PROPERTY_VALUE(Title, title);
}

void UOdiroListItemViewModel::SetSubtitle(const FString& subtitle)
{
	UE_MVVM_SET_PROPERTY_VALUE(Subtitle, subtitle);
}

void UOdiroListItemViewModel::SetPayloadPath(const FString& payloadPath)
{
	UE_MVVM_SET_PROPERTY_VALUE(PayloadPath, payloadPath);
}

void UOdiroListItemViewModel::SetSelected(const bool bInSelected)
{
	UE_MVVM_SET_PROPERTY_VALUE(bSelected, bInSelected);
}

void UOdiroListItemViewModel::SetEnabled(const bool bInEnabled)
{
	UE_MVVM_SET_PROPERTY_VALUE(bEnabled, bInEnabled);
}
