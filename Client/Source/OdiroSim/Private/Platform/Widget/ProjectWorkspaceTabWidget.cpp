#include "Platform/Widget/ProjectWorkspaceTabWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

void UProjectWorkspaceTabWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (TabButton)
	{
		InactiveTabButtonStyle = TabButton->GetStyle();
		bHasInactiveTabButtonStyle = true;
		TabButton->OnClicked.RemoveDynamic(this, &UProjectWorkspaceTabWidget::HandleTabButtonClicked);
		TabButton->OnClicked.AddDynamic(this, &UProjectWorkspaceTabWidget::HandleTabButtonClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &UProjectWorkspaceTabWidget::HandleCloseButtonClicked);
		CloseButton->OnClicked.AddDynamic(this, &UProjectWorkspaceTabWidget::HandleCloseButtonClicked);
	}

	ApplyTabVisualState();
	BP_OnTabStateChanged(bActive, bClosable);
}

void UProjectWorkspaceTabWidget::NativeDestruct()
{
	if (TabButton)
	{
		TabButton->OnClicked.RemoveDynamic(this, &UProjectWorkspaceTabWidget::HandleTabButtonClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &UProjectWorkspaceTabWidget::HandleCloseButtonClicked);
	}

	Super::NativeDestruct();
}

void UProjectWorkspaceTabWidget::SetTabId(const FName& tabId)
{
	TabId = tabId;
}

void UProjectWorkspaceTabWidget::SetTabLabel(const FText& label)
{
	if (TabLabelText)
	{
		TabLabelText->SetText(label);
	}
}

void UProjectWorkspaceTabWidget::SetTabActive(const bool bInActive)
{
	if (bActive == bInActive)
	{
		return;
	}

	bActive = bInActive;
	ApplyTabVisualState();
	BP_OnTabStateChanged(bActive, bClosable);
}

void UProjectWorkspaceTabWidget::SetTabClosable(const bool bInClosable)
{
	if (bClosable == bInClosable)
	{
		return;
	}

	bClosable = bInClosable;
	ApplyTabVisualState();
	BP_OnTabStateChanged(bActive, bClosable);
}

void UProjectWorkspaceTabWidget::SetTabVisible(const bool bInVisible)
{
	SetVisibility(bInVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UProjectWorkspaceTabWidget::HandleTabButtonClicked()
{
	OnSelectedRequested.Broadcast(this);
}

void UProjectWorkspaceTabWidget::HandleCloseButtonClicked()
{
	OnCloseRequested.Broadcast(this);
}

void UProjectWorkspaceTabWidget::ApplyTabVisualState()
{
	if (CloseButton)
	{
		CloseButton->SetVisibility(bClosable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (ActiveIndicator)
	{
		ActiveIndicator->SetVisibility(bActive ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (TabButton)
	{
		if (bActive && ActiveTabButtonStyleSource)
		{
			FButtonStyle activeStyle = ActiveTabButtonStyleSource->GetStyle();
			if (bHasInactiveTabButtonStyle)
			{
				activeStyle.SetNormalPadding(InactiveTabButtonStyle.NormalPadding);
				activeStyle.SetPressedPadding(InactiveTabButtonStyle.PressedPadding);
			}
			TabButton->SetStyle(activeStyle);
		}
		else if (bHasInactiveTabButtonStyle)
		{
			TabButton->SetStyle(InactiveTabButtonStyle);
		}
	}
}
