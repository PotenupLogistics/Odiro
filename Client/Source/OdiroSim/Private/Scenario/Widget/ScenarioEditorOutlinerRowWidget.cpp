#include "Scenario/Widget/ScenarioEditorOutlinerRowWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Scenario/ViewModel/ScenarioEditorListItemViewModel.h"

namespace
{
	constexpr float MinimumIndentSpacerHeight = 1.0f;
}

void UScenarioEditorOutlinerRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindControls();
	RefreshRow();
}

void UScenarioEditorOutlinerRowWidget::NativeDestruct()
{
	UnbindControls();
	Super::NativeDestruct();
}

void UScenarioEditorOutlinerRowWidget::InitializeRow(const FScenarioOutlinerItemViewModel& viewModel)
{
	ItemViewModel = nullptr;
	Item = viewModel;
	RefreshRow();
}

void UScenarioEditorOutlinerRowWidget::InitializeFromItemViewModel(
	UScenarioEditorListItemViewModel* itemViewModel)
{
	ItemViewModel = itemViewModel;
	FScenarioOutlinerItemViewModel viewModel;
	if (ItemViewModel)
	{
		viewModel.ItemKey = ItemViewModel->GetItemId();
		viewModel.ParentKey = ItemViewModel->GetParentKey();
		viewModel.Label = FText::FromString(ItemViewModel->GetTitle());
		viewModel.Subtitle = FText::FromString(ItemViewModel->GetSubtitle());
		viewModel.Depth = ItemViewModel->GetDepth();
		viewModel.bExpandable = ItemViewModel->IsExpandable();
		viewModel.bExpanded = ItemViewModel->IsExpanded();
		viewModel.bSelected = ItemViewModel->IsSelected();
		viewModel.ItemType = ItemViewModel->GetOutlinerItemType();
		viewModel.TemplatePanel = ItemViewModel->GetTemplatePanel();
		viewModel.InstanceId = ItemViewModel->GetInstanceId();
		viewModel.ActorCategory = ItemViewModel->GetActorCategory();
	}

	Item = viewModel;
	RefreshRow();
}

void UScenarioEditorOutlinerRowWidget::SetSelected(const bool bInSelected)
{
	Item.bSelected = bInSelected;
	if (ItemViewModel)
	{
		ItemViewModel->SetSelected(bInSelected);
	}
	RefreshRow();
}

void UScenarioEditorOutlinerRowWidget::SetExpanded(const bool bInExpanded)
{
	Item.bExpanded = bInExpanded;
	if (ItemViewModel)
	{
		ItemViewModel->SetExpanded(bInExpanded);
	}
	RefreshRow();
}

void UScenarioEditorOutlinerRowWidget::HandleRowClicked()
{
	OnRowSelected.Broadcast(Item);
}

void UScenarioEditorOutlinerRowWidget::HandleExpandClicked()
{
	if (!Item.bExpandable)
	{
		OnRowSelected.Broadcast(Item);
		return;
	}

	Item.bExpanded = !Item.bExpanded;
	RefreshRow();
	OnRowExpansionToggled.Broadcast(Item);
}

void UScenarioEditorOutlinerRowWidget::BindControls()
{
	if (RowButton)
	{
		RowButton->OnClicked.RemoveDynamic(this, &UScenarioEditorOutlinerRowWidget::HandleRowClicked);
		RowButton->OnClicked.AddDynamic(this, &UScenarioEditorOutlinerRowWidget::HandleRowClicked);
	}
	if (ExpandButton)
	{
		ExpandButton->OnClicked.RemoveDynamic(this, &UScenarioEditorOutlinerRowWidget::HandleExpandClicked);
		ExpandButton->OnClicked.AddDynamic(this, &UScenarioEditorOutlinerRowWidget::HandleExpandClicked);
	}
}

void UScenarioEditorOutlinerRowWidget::UnbindControls()
{
	if (RowButton)
	{
		RowButton->OnClicked.RemoveDynamic(this, &UScenarioEditorOutlinerRowWidget::HandleRowClicked);
	}
	if (ExpandButton)
	{
		ExpandButton->OnClicked.RemoveDynamic(this, &UScenarioEditorOutlinerRowWidget::HandleExpandClicked);
	}
}

void UScenarioEditorOutlinerRowWidget::RefreshRow()
{
	if (SelectionIndicator)
	{
		SelectionIndicator->SetVisibility(Item.bSelected ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
	}

	if (RowIndentSpacer)
	{
		RowIndentSpacer->SetSize(
			FVector2D(FMath::Max(0, Item.Depth) * IndentPerDepth, MinimumIndentSpacerHeight));
	}

	if (ExpandGlyphText)
	{
		FWidgetTransform glyphTransform;
		glyphTransform.Angle = (Item.bExpandable && Item.bExpanded) ? 90.0f : 0.0f;
		ExpandGlyphText->SetRenderTransform(glyphTransform);
	}
	if (ExpandButton)
	{
		ExpandButton->SetVisibility(Item.bExpandable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (ItemLabelText)
	{
		ItemLabelText->SetText(Item.Label);
	}
	if (ItemSubtitleText)
	{
		ItemSubtitleText->SetText(Item.Subtitle);
		ItemSubtitleText->SetVisibility(Item.Subtitle.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}
