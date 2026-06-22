#include "Scenario/Widget/ScenarioEditorOutlinerRowWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

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
	Item = viewModel;
	RefreshRow();
}

void UScenarioEditorOutlinerRowWidget::SetSelected(const bool bInSelected)
{
	Item.bSelected = bInSelected;
	RefreshRow();
}

void UScenarioEditorOutlinerRowWidget::SetExpanded(const bool bInExpanded)
{
	Item.bExpanded = bInExpanded;
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
