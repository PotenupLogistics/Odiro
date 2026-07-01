#include "UI/BaseTreeViewWidget.h"
#include "UI/BaseFormElementPrivate.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "UI/BaseWidgetPrivate.h"

using namespace BaseFormElementPrivate;

namespace
{
	// Finds the WBP-authored indent size box even when it is wrapped by a panel.
	USizeBox* ResolveIndentSizeBox(UWidget* indentWidget)
	{
		if (USizeBox* sizeBox = Cast<USizeBox>(indentWidget))
		{
			return sizeBox;
		}
		if (UPanelWidget* panel = Cast<UPanelWidget>(indentWidget))
		{
			for (int32 childIndex = 0; childIndex < panel->GetChildrenCount(); ++childIndex)
			{
				if (USizeBox* childSizeBox = Cast<USizeBox>(panel->GetChildAt(childIndex)))
				{
					return childSizeBox;
				}
			}
		}
		return nullptr;
	}
}

void UBaseTreeRowWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();
	FBaseWidgetSizeConstraints effectiveSizeConstraints = SizeConstraints;
	if (effectiveSizeConstraints.MinWidth <= 0.0f)
	{
		effectiveSizeConstraints.MinWidth = 360.0f;
	}
	BaseWidgetPrivate::ApplySizeConstraints(RootSize.Get(), effectiveSizeConstraints);
	if (RootSizeBox.Get() != RootSize.Get())
	{
		BaseWidgetPrivate::ApplySizeConstraints(RootSizeBox.Get(), effectiveSizeConstraints);
	}

	const UBaseWidgetColorCatalog* colors = GetResolvedBaseColors();
	const UBaseWidgetSizeCatalog* sizes = GetResolvedBaseSizes();
	if (USizeBox* indentSizeBox = ResolveIndentSizeBox(IndentBox.Get()))
	{
		const float resolvedIndentWidth = IndentWidth > 0.0f
			? IndentWidth
			: (sizes ? sizes->Space8 : 16.0f);
		indentSizeBox->SetWidthOverride(FMath::Max(0, Item.Depth) * resolvedIndentWidth);
	}
	if (ExpanderImage)
	{
		ExpanderImage->SetVisibility(Item.bHasChildren ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
		ExpanderImage->SetRenderTransformAngle(Item.bExpanded ? 90.0f : 0.0f);
	}
	SetImageTexture(IconImage.Get(), Item.Icon);
	if (LabelTextBlock)
	{
		SetTextBlockValue(LabelTextBlock.Get(), Item.Label, false);
		ApplyTextStyle(LabelTextBlock.Get(), EBaseTextRole::Label);
		BaseWidgetPrivate::ApplySlotVerticalAlignment(LabelTextBlock.Get(), EBaseVerticalContentAlign::Bottom);
	}
	if (SubLabelTextBlock)
	{
		SetTextBlockValue(SubLabelTextBlock.Get(), Item.SubLabel);
		ApplyTextStyle(SubLabelTextBlock.Get(), EBaseTextRole::Caption);
		BaseWidgetPrivate::ApplySlotVerticalAlignment(SubLabelTextBlock.Get(), EBaseVerticalContentAlign::Bottom);
	}
	if (RightLabelTextBlock)
	{
		SetTextBlockValue(RightLabelTextBlock.Get(), Item.RightLabel);
		ApplyTextStyle(RightLabelTextBlock.Get(), EBaseTextRole::Caption);
		BaseWidgetPrivate::ApplySlotVerticalAlignment(RightLabelTextBlock.Get(), EBaseVerticalContentAlign::Bottom);
	}
	if (SelectionBar && colors)
	{
		ApplyBorderColor(SelectionBar.Get(), colors->AccentColor);
		SetOptionalWidgetVisible(SelectionBar.Get(), Item.bSelected);
	}
	if (colors && sizes)
	{
		const FLinearColor fillColor = Item.bSelected ? colors->SurfacePanelColor : FLinearColor::Transparent;
		const FLinearColor strokeColor = FLinearColor::Transparent;
		BaseWidgetPrivate::ApplyRoundedSurface(
			BorderFrame.Get(),
			SurfaceBorder.Get(),
			fillColor,
			strokeColor,
			sizes->Radius,
			0.0f);
		if (Item.bDisabled)
		{
			ApplyTextColor(LabelTextBlock.Get(), colors->TextFaintColor);
			ApplyTextColor(SubLabelTextBlock.Get(), colors->TextFaintColor);
			ApplyTextColor(RightLabelTextBlock.Get(), colors->TextFaintColor);
		}
	}
}

int32 UBaseTreeRowWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId, const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	BaseWidgetPrivate::UpdateRoundedSurfaceSize(SurfaceBorder.Get(), AllottedGeometry.GetLocalSize());
	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

void UBaseTreeRowWidget::SetItem(const FBaseTreeRowItem& inItem)
{
	Item = inItem;
	SynchronizeBaseProperties();
}

FReply UBaseTreeRowWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	(void)InGeometry;
	if (Item.bDisabled || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	OnRowClicked.Broadcast(this, Item.Id);
	if (Item.bHasChildren)
	{
		OnExpansionRequested.Broadcast(this, Item.Id);
	}
	return FReply::Handled();
}

UBaseTreeViewWidget::UBaseTreeViewWidget(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{
	FBaseTreeRowItem rootRow;
	rootRow.Id = TEXT("root");
	rootRow.Depth = 0;
	rootRow.Label = NSLOCTEXT("BaseTreeViewWidget", "DefaultRootLabel", "Parent row");
	rootRow.SubLabel = NSLOCTEXT("BaseTreeViewWidget", "DefaultRootSubLabel", "depth 0");
	rootRow.RightLabel = NSLOCTEXT("BaseTreeViewWidget", "DefaultRootRightLabel", "Open");
	rootRow.bHasChildren = true;
	rootRow.bExpanded = true;
	rootRow.bSelected = true;

	FBaseTreeRowItem childRow;
	childRow.Id = TEXT("child");
	childRow.Depth = 1;
	childRow.Label = NSLOCTEXT("BaseTreeViewWidget", "DefaultChildLabel", "Child row");
	childRow.SubLabel = NSLOCTEXT("BaseTreeViewWidget", "DefaultChildSubLabel", "depth 1");
	childRow.RightLabel = NSLOCTEXT("BaseTreeViewWidget", "DefaultChildRightLabel", "Ready");

	Items = { rootRow, childRow };
	SelectedId = rootRow.Id;
}

void UBaseTreeViewWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();
	if (RowContainer && RowContainer->GetChildrenCount() != Items.Num())
	{
		RebuildRows();
		return;
	}
	RefreshRows();
}

void UBaseTreeViewWidget::SetItems(const TArray<FBaseTreeRowItem>& inItems)
{
	Items = inItems;
	if (!FindTreeRowById(Items, SelectedId))
	{
		SelectedId = NAME_None;
	}
	RebuildRows();
	SynchronizeBaseProperties();
}

bool UBaseTreeViewWidget::SelectItemById(const FName itemId)
{
	const FBaseTreeRowItem* item = FindTreeRowById(Items, itemId);
	if (!item || item->bDisabled)
	{
		return false;
	}

	const bool bChanged = SelectedId != itemId;
	SelectedId = itemId;
	for (FBaseTreeRowItem& rowItem : Items)
	{
		rowItem.bSelected = rowItem.Id == SelectedId;
	}
	SynchronizeBaseProperties();
	if (bChanged)
	{
		OnSelectionChanged.Broadcast(this, SelectedId);
	}
	return true;
}

void UBaseTreeViewWidget::RebuildRows()
{
	if (!RowContainer || !RowWidgetClass || !GetWorld())
	{
		return;
	}

	RowContainer->ClearChildren();
	for (const FBaseTreeRowItem& item : Items)
	{
		UBaseTreeRowWidget* rowWidget = CreateWidget<UBaseTreeRowWidget>(GetWorld(), RowWidgetClass);
		if (!rowWidget)
		{
			continue;
		}

		rowWidget->OnRowClicked.RemoveDynamic(this, &UBaseTreeViewWidget::HandleGeneratedRowClicked);
		rowWidget->OnRowClicked.AddDynamic(this, &UBaseTreeViewWidget::HandleGeneratedRowClicked);
		rowWidget->OnExpansionRequested.RemoveDynamic(this, &UBaseTreeViewWidget::HandleGeneratedExpansionRequested);
		rowWidget->OnExpansionRequested.AddDynamic(this, &UBaseTreeViewWidget::HandleGeneratedExpansionRequested);
		rowWidget->SetColorsOverride(ColorsOverride);
		rowWidget->SetSizesOverride(SizesOverride);
		RowContainer->AddChild(rowWidget);
	}
	RefreshRows();
}

void UBaseTreeViewWidget::RefreshRows()
{
	if (!RowContainer)
	{
		return;
	}

	const int32 childCount = FMath::Min(RowContainer->GetChildrenCount(), Items.Num());
	for (int32 itemIndex = 0; itemIndex < childCount; ++itemIndex)
	{
		UBaseTreeRowWidget* rowWidget = Cast<UBaseTreeRowWidget>(RowContainer->GetChildAt(itemIndex));
		if (!rowWidget)
		{
			continue;
		}

		FBaseTreeRowItem renderedItem = Items[itemIndex];
		renderedItem.bSelected = SelectedId.IsNone() ? renderedItem.bSelected : renderedItem.Id == SelectedId;
		rowWidget->SetColorsOverride(ColorsOverride);
		rowWidget->SetSizesOverride(SizesOverride);
		rowWidget->SetItem(renderedItem);
	}
}

void UBaseTreeViewWidget::HandleGeneratedRowClicked(UWidget* widget, const FName rowId)
{
	if (SelectItemById(rowId))
	{
		OnRowClicked.Broadcast(widget ? widget : this, rowId);
	}
}

void UBaseTreeViewWidget::HandleGeneratedExpansionRequested(UWidget* widget, const FName rowId)
{
	OnExpansionRequested.Broadcast(widget ? widget : this, rowId);
}
