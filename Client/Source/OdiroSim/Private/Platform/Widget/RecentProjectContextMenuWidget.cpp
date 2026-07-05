#include "Platform/Widget/RecentProjectContextMenuWidget.h"

#include "Components/CanvasPanelSlot.h"
#include "Input/Events.h"
#include "Platform/Widget/PlatformWidgetRuntime.h"
#include "UI/BaseButtonWidget.h"

void URecentProjectContextMenuWidget::OpenAtViewportPosition(const FVector2D& viewportPosition)
{
	UWidget* menuPositionTarget = MenuAnchor ? MenuAnchor.Get() : MenuSurface.Get();
	if (menuPositionTarget)
	{
		if (UCanvasPanelSlot* canvasSlot = Cast<UCanvasPanelSlot>(menuPositionTarget->Slot))
		{
			canvasSlot->SetPosition(viewportPosition);
		}
		else
		{
			menuPositionTarget->SetRenderTranslation(viewportPosition);
		}
	}
	else
	{
		SetPositionInViewport(viewportPosition, false);
	}
}

void URecentProjectContextMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	PlatformWidgetRuntime::ApplyFullscreenViewportSlot(this);

	if (DismissButton)
	{
		DismissButton->OnBaseClicked.RemoveDynamic(
			this,
			&URecentProjectContextMenuWidget::HandleDismissSelected);
		DismissButton->OnBaseClicked.AddDynamic(
			this,
			&URecentProjectContextMenuWidget::HandleDismissSelected);
	}
	if (RemoveFromListButton)
	{
		RemoveFromListButton->OnBaseClicked.RemoveDynamic(
			this,
			&URecentProjectContextMenuWidget::HandleRemoveFromListSelected);
		RemoveFromListButton->OnBaseClicked.AddDynamic(
			this,
			&URecentProjectContextMenuWidget::HandleRemoveFromListSelected);
	}
	if (DeleteProjectButton)
	{
		DeleteProjectButton->OnBaseClicked.RemoveDynamic(
			this,
			&URecentProjectContextMenuWidget::HandleDeleteProjectSelected);
		DeleteProjectButton->OnBaseClicked.AddDynamic(
			this,
			&URecentProjectContextMenuWidget::HandleDeleteProjectSelected);
	}
}

void URecentProjectContextMenuWidget::NativeDestruct()
{
	if (DismissButton)
	{
		DismissButton->OnBaseClicked.RemoveDynamic(
			this,
			&URecentProjectContextMenuWidget::HandleDismissSelected);
	}
	if (RemoveFromListButton)
	{
		RemoveFromListButton->OnBaseClicked.RemoveDynamic(
			this,
			&URecentProjectContextMenuWidget::HandleRemoveFromListSelected);
	}
	if (DeleteProjectButton)
	{
		DeleteProjectButton->OnBaseClicked.RemoveDynamic(
			this,
			&URecentProjectContextMenuWidget::HandleDeleteProjectSelected);
	}

	Super::NativeDestruct();
}

FReply URecentProjectContextMenuWidget::NativeOnPreviewMouseButtonDown(
	const FGeometry& inGeometry,
	const FPointerEvent& inMouseEvent)
{
	(void)inGeometry;
	if (MenuSurface && !MenuSurface->GetCachedGeometry().IsUnderLocation(inMouseEvent.GetScreenSpacePosition()))
	{
		OnDismissRequested.Broadcast(this);
		return FReply::Handled();
	}

	return Super::NativeOnPreviewMouseButtonDown(inGeometry, inMouseEvent);
}

FReply URecentProjectContextMenuWidget::NativeOnMouseButtonDown(
	const FGeometry& inGeometry,
	const FPointerEvent& inMouseEvent)
{
	if (MenuSurface && !MenuSurface->GetCachedGeometry().IsUnderLocation(inMouseEvent.GetScreenSpacePosition()))
	{
		OnDismissRequested.Broadcast(this);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(inGeometry, inMouseEvent);
}

void URecentProjectContextMenuWidget::HandleRemoveFromListSelected(UBaseButtonWidget*)
{
	OnRemoveFromListSelected.Broadcast(this);
}

void URecentProjectContextMenuWidget::HandleDeleteProjectSelected(UBaseButtonWidget*)
{
	OnDeleteProjectSelected.Broadcast(this);
}

void URecentProjectContextMenuWidget::HandleDismissSelected(UBaseButtonWidget*)
{
	OnDismissRequested.Broadcast(this);
}
