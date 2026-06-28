#include "UI/BaseFormElementsGalleryWidget.h"

#include "UI/BaseTreeViewWidget.h"

void UBaseFormElementsGalleryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (GalleryTreeView)
	{
		GalleryTreeItems = GalleryTreeView->GetItems();
		GalleryTreeView->OnExpansionRequested.RemoveDynamic(this, &UBaseFormElementsGalleryWidget::HandleTreeExpansionRequested);
		GalleryTreeView->OnExpansionRequested.AddDynamic(this, &UBaseFormElementsGalleryWidget::HandleTreeExpansionRequested);
		ApplyTreeExpansionState();
	}
}

void UBaseFormElementsGalleryWidget::NativeDestruct()
{
	if (GalleryTreeView)
	{
		GalleryTreeView->OnExpansionRequested.RemoveDynamic(this, &UBaseFormElementsGalleryWidget::HandleTreeExpansionRequested);
	}

	Super::NativeDestruct();
}

void UBaseFormElementsGalleryWidget::HandleTreeExpansionRequested(UWidget* widget, const FName rowId)
{
	(void)widget;
	for (FBaseTreeRowItem& item : GalleryTreeItems)
	{
		if (item.Id == rowId && item.bHasChildren)
		{
			item.bExpanded = !item.bExpanded;
			break;
		}
	}
	ApplyTreeExpansionState();
}

void UBaseFormElementsGalleryWidget::ApplyTreeExpansionState()
{
	if (!GalleryTreeView)
	{
		return;
	}

	TArray<int32> collapsedDepths;
	TArray<FBaseTreeRowItem> visibleItems;
	for (const FBaseTreeRowItem& item : GalleryTreeItems)
	{
		while (collapsedDepths.Num() > 0 && item.Depth <= collapsedDepths.Last())
		{
			collapsedDepths.Pop();
		}
		if (collapsedDepths.Num() > 0)
		{
			continue;
		}

		visibleItems.Add(item);
		if (item.bHasChildren && !item.bExpanded)
		{
			collapsedDepths.Add(item.Depth);
		}
	}

	GalleryTreeView->SetItems(visibleItems);
}
