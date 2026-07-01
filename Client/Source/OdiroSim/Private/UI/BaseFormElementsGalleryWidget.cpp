#include "UI/BaseFormElementsGalleryWidget.h"

#include "UI/BaseTreeViewWidget.h"

namespace
{
	TArray<FBaseTreeRowItem> BuildDefaultGalleryTreeItems()
	{
		FBaseTreeRowItem robotRow;
		robotRow.Id = TEXT("robot");
		robotRow.Depth = 0;
		robotRow.Label = NSLOCTEXT("BaseFormElementsGalleryWidget", "DefaultTreeRobotLabel", "Robot");
		robotRow.SubLabel = NSLOCTEXT("BaseFormElementsGalleryWidget", "DefaultTreeRobotSubLabel", "depth 0");
		robotRow.RightLabel = NSLOCTEXT("BaseFormElementsGalleryWidget", "DefaultTreeRobotRightLabel", "Open");
		robotRow.bHasChildren = true;
		robotRow.bExpanded = true;
		robotRow.bSelected = true;

		FBaseTreeRowItem cameraRow;
		cameraRow.Id = TEXT("camera");
		cameraRow.Depth = 1;
		cameraRow.Label = NSLOCTEXT("BaseFormElementsGalleryWidget", "DefaultTreeCameraLabel", "Camera");
		cameraRow.SubLabel = NSLOCTEXT("BaseFormElementsGalleryWidget", "DefaultTreeCameraSubLabel", "depth 1");
		cameraRow.RightLabel = NSLOCTEXT("BaseFormElementsGalleryWidget", "DefaultTreeCameraRightLabel", "Live");

		FBaseTreeRowItem lidarRow;
		lidarRow.Id = TEXT("lidar");
		lidarRow.Depth = 1;
		lidarRow.Label = NSLOCTEXT("BaseFormElementsGalleryWidget", "DefaultTreeLidarLabel", "LiDAR");
		lidarRow.SubLabel = NSLOCTEXT("BaseFormElementsGalleryWidget", "DefaultTreeLidarSubLabel", "depth 1");
		lidarRow.RightLabel = NSLOCTEXT("BaseFormElementsGalleryWidget", "DefaultTreeLidarRightLabel", "Ready");

		return { robotRow, cameraRow, lidarRow };
	}
}

void UBaseFormElementsGalleryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (GalleryTreeView)
	{
		GalleryTreeItems = GalleryTreeView->GetItems();
		if (GalleryTreeItems.IsEmpty())
		{
			GalleryTreeItems = BuildDefaultGalleryTreeItems();
		}
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
