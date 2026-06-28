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

void UBaseTreeRowWidget::SynchronizeBaseProperties()
{
	Super::SynchronizeBaseProperties();

	const UBaseWidgetTokenCatalog* tokens = GetResolvedBaseTokens();
	if (USizeBox* indentSizeBox = Cast<USizeBox>(IndentBox))
	{
		const float resolvedIndentWidth = IndentWidth > 0.0f
			? IndentWidth
			: (tokens ? tokens->Space4 : 0.0f);
		indentSizeBox->SetWidthOverride(FMath::Max(0, Item.Depth) * resolvedIndentWidth);
	}
	SetOptionalWidgetVisible(ExpanderImage.Get(), Item.bHasChildren);
	if (ExpanderImage)
	{
		ExpanderImage->SetRenderTransformAngle(Item.bExpanded ? 90.0f : 0.0f);
	}
	SetImageTexture(IconImage.Get(), Item.Icon);
	if (LabelTextBlock)
	{
		SetTextBlockValue(LabelTextBlock.Get(), Item.Label, false);
		ApplyTextStyle(LabelTextBlock.Get(), EBaseTextRole::Label);
	}
	if (SubLabelTextBlock)
	{
		SetTextBlockValue(SubLabelTextBlock.Get(), Item.SubLabel);
		ApplyTextStyle(SubLabelTextBlock.Get(), EBaseTextRole::Caption);
	}
	if (RightLabelTextBlock)
	{
		SetTextBlockValue(RightLabelTextBlock.Get(), Item.RightLabel);
		ApplyTextStyle(RightLabelTextBlock.Get(), EBaseTextRole::Caption);
	}
	if (SelectionBar && tokens)
	{
		ApplyBorderColor(SelectionBar.Get(), tokens->AccentColor);
		SetOptionalWidgetVisible(SelectionBar.Get(), Item.bSelected);
	}
	if (tokens)
	{
		const FLinearColor fillColor = Item.bSelected ? tokens->SurfacePanelColor : FLinearColor::Transparent;
		const FLinearColor strokeColor = FLinearColor::Transparent;
		BaseWidgetPrivate::ApplyRoundedSurface(
			BorderFrame.Get(),
			SurfaceBorder.Get(),
			fillColor,
			strokeColor,
			tokens->Radius,
			0.0f);
		if (Item.bDisabled)
		{
			ApplyTextColor(LabelTextBlock.Get(), tokens->TextFaintColor);
			ApplyTextColor(SubLabelTextBlock.Get(), tokens->TextFaintColor);
			ApplyTextColor(RightLabelTextBlock.Get(), tokens->TextFaintColor);
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
		renderedItem.bSelected = renderedItem.Id == SelectedId || renderedItem.bSelected;
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
