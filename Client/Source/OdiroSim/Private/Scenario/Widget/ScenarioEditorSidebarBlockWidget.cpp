#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"

void UScenarioEditorSidebarBlockWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindControls();
	ApplyVisualStyle();
	RefreshBlock();
}

void UScenarioEditorSidebarBlockWidget::NativeDestruct()
{
	UnbindControls();
	Super::NativeDestruct();
}

void UScenarioEditorSidebarBlockWidget::SetBlockMetadata(
	const FString& name,
	const FString& path,
	const FString& badge)
{
	BlockName = name;
	BlockPath = path;
	BadgeText = badge;
	RefreshBlock();
}

void UScenarioEditorSidebarBlockWidget::SetExpanded(const bool bInExpanded)
{
	bExpanded = bInExpanded;
	RefreshBlock();
}

void UScenarioEditorSidebarBlockWidget::SetSelected(const bool bInSelected)
{
	bSelected = bInSelected;
	RefreshBlock();
}

void UScenarioEditorSidebarBlockWidget::SetShowNormalOutline(const bool bInShowNormalOutline)
{
	bShowNormalOutline = bInShowNormalOutline;
	RefreshBlock();
}

void UScenarioEditorSidebarBlockWidget::SetNested(const bool bInNested)
{
	bNested = bInNested;
	RefreshBlock();
}

void UScenarioEditorSidebarBlockWidget::SetTextStyleCatalog(
	TSoftObjectPtr<UWidgetTextStyleCatalog> catalog)
{
	TextStyleCatalog = catalog;
	RefreshBlock();
}

void UScenarioEditorSidebarBlockWidget::AddBodyChild(UWidget* widget)
{
	if (!widget)
	{
		return;
	}

	if (UVerticalBox* bodyBox = GetBodyBox())
	{
		if (UVerticalBoxSlot* bodySlot = bodyBox->AddChildToVerticalBox(widget))
		{
			const bool bFieldRowChild = widget->IsA<UScenarioEditorSidebarFieldRow>();
			bodySlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, bFieldRowChild ? 2.0f : 6.0f));
			bodySlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}
}

void UScenarioEditorSidebarBlockWidget::ClearBodyChildren()
{
	if (UVerticalBox* bodyBox = GetBodyBox())
	{
		bodyBox->ClearChildren();
	}
}

UVerticalBox* UScenarioEditorSidebarBlockWidget::GetBodyBox()
{
	return BodyBox.Get();
}

FReply UScenarioEditorSidebarBlockWidget::NativeOnPreviewMouseButtonDown(
	const FGeometry& inGeometry,
	const FPointerEvent& inMouseEvent)
{
	if (inMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (ShouldBroadcastSelectionForPointer(inMouseEvent))
		{
			BroadcastBlockSelected();
		}
	}

	return Super::NativeOnPreviewMouseButtonDown(inGeometry, inMouseEvent);
}

void UScenarioEditorSidebarBlockWidget::HandleToggleClicked()
{
	SetExpanded(!bExpanded);
	BroadcastBlockSelected();
}

void UScenarioEditorSidebarBlockWidget::BroadcastBlockSelected()
{
	OnBlockSelected.Broadcast(BlockPath);
}

bool UScenarioEditorSidebarBlockWidget::ShouldBroadcastSelectionForPointer(
	const FPointerEvent& mouseEvent) const
{
	if (!BodyBox
		|| !BodyBox->IsVisible()
		|| !BodyBox->GetCachedGeometry().IsUnderLocation(mouseEvent.GetScreenSpacePosition()))
	{
		return true;
	}

	for (int32 childIndex = 0; childIndex < BodyBox->GetChildrenCount(); ++childIndex)
	{
		UWidget* childWidget = BodyBox->GetChildAt(childIndex);
		if (childWidget
			&& childWidget->IsVisible()
			&& childWidget->GetCachedGeometry().IsUnderLocation(mouseEvent.GetScreenSpacePosition()))
		{
			return false;
		}
	}

	return true;
}

void UScenarioEditorSidebarBlockWidget::BindControls()
{
	if (ToggleButton)
	{
		ToggleButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarBlockWidget::HandleToggleClicked);
		ToggleButton->OnClicked.AddDynamic(
			this,
			&UScenarioEditorSidebarBlockWidget::HandleToggleClicked);
	}
}

void UScenarioEditorSidebarBlockWidget::UnbindControls()
{
	if (ToggleButton)
	{
		ToggleButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarBlockWidget::HandleToggleClicked);
	}
}

void UScenarioEditorSidebarBlockWidget::ApplyVisualStyle()
{
	if (UBorder* outlineBorder = Cast<UBorder>(OutlineBorder.Get()))
	{
		outlineBorder->SetPadding(FMargin(1.0f));
		outlineBorder->SetBrushColor(FLinearColor(0.055f, 0.055f, 0.055f, 1.0f));
	}

	if (UBorder* contentBorder = Cast<UBorder>(ContentBorder.Get()))
	{
		contentBorder->SetPadding(FMargin(6.0f, 4.0f, 6.0f, 6.0f));
		contentBorder->SetBrushColor(FLinearColor(0.014f, 0.014f, 0.014f, 1.0f));
	}

	if (BlockHeaderRow)
	{
		if (UVerticalBoxSlot* headerSlot = Cast<UVerticalBoxSlot>(BlockHeaderRow->Slot))
		{
			headerSlot->SetPadding(FMargin(0.0f, 1.0f, 0.0f, 3.0f));
		}
	}

	if (BodyBox)
	{
		if (UVerticalBoxSlot* bodySlot = Cast<UVerticalBoxSlot>(BodyBox->Slot))
		{
			bodySlot->SetPadding(FMargin(10.0f, 7.0f, 4.0f, 2.0f));
		}
	}
}

void UScenarioEditorSidebarBlockWidget::RefreshBlock()
{
	if (ToggleTextBlock)
	{
		FWidgetTransform toggleTransform;
		toggleTransform.Angle = bExpanded ? 90.0f : 0.0f;
		ToggleTextBlock->SetRenderTransform(toggleTransform);
	}
	SetTextBlockText(NameTextBlock.Get(), BlockName);
	SetTextBlockText(PathTextBlock.Get(), BlockPath);
	SetTextBlockText(BadgeTextBlock.Get(), BadgeText);

	if (SelectedStateWidget)
	{
		SelectedStateWidget->SetVisibility(bSelected ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (BodyBox)
	{
		BodyBox->SetVisibility(bExpanded ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UScenarioEditorSidebarBlockWidget::SetTextBlockText(UTextBlock* textBlock, const FString& text) const
{
	if (textBlock)
	{
		textBlock->SetText(FText::FromString(text));
	}
}
