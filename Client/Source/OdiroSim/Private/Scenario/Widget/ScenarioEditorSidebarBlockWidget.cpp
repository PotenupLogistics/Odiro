#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "Scenario/Data/WidgetTextStyleCatalog.h"
#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"

namespace
{
	// Converts UI hex colors into Slate linear colors with a caller-controlled alpha.
	FLinearColor MakeSidebarBlockColor(const TCHAR* hex, const float alpha = 1.0f)
	{
		FLinearColor color = FLinearColor::FromSRGBColor(FColor::FromHex(hex));
		color.A = alpha;
		return color;
	}
}

void UScenarioEditorSidebarBlockWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindControls();
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
	const FLinearColor selectedAccent = MakeSidebarBlockColor(TEXT("2498FF"));
	const FLinearColor normalOutline = MakeSidebarBlockColor(TEXT("0E0E0E"));
	const FLinearColor mutedOutline = MakeSidebarBlockColor(TEXT("070707"));

	if (UBorder* outlineBorder = Cast<UBorder>(OutlineBorder.Get()))
	{
		outlineBorder->SetPadding(FMargin(1.0f));
		outlineBorder->SetBrushColor(bSelected
			? selectedAccent
			: (bShowNormalOutline ? normalOutline : mutedOutline));
	}

	if (UBorder* contentBorder = Cast<UBorder>(ContentBorder.Get()))
	{
		contentBorder->SetPadding(FMargin(6.0f, 4.0f, 6.0f, 6.0f));
		if (bSelected)
		{
			contentBorder->SetBrushColor(MakeSidebarBlockColor(TEXT("07111A")));
		}
		else
		{
			contentBorder->SetBrushColor(bNested
				? MakeSidebarBlockColor(TEXT("070707"))
				: MakeSidebarBlockColor(TEXT("0B0B0B")));
		}
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

	UWidgetTextStyleCatalog::ApplyTextBlockStyle(
		NameTextBlock.Get(),
		TextStyleCatalog,
		bNested ? EWidgetTextStyleRole::Label : EWidgetTextStyleRole::Title);
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(
		PathTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Caption);
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(
		BadgeTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Caption);
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(
		ToggleTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Caption);

	if (NameTextBlock && bSelected)
	{
		NameTextBlock->SetColorAndOpacity(FSlateColor(MakeSidebarBlockColor(TEXT("F4FAFF"))));
	}
	if (BadgeTextBlock)
	{
		BadgeTextBlock->SetColorAndOpacity(FSlateColor(bSelected
			? MakeSidebarBlockColor(TEXT("9FD3FF"))
			: MakeSidebarBlockColor(TEXT("AFC8DF"))));
	}
	if (ToggleTextBlock)
	{
		ToggleTextBlock->SetColorAndOpacity(FSlateColor(bSelected
			? MakeSidebarBlockColor(TEXT("D6ECFF"))
			: MakeSidebarBlockColor(TEXT("9A9A9A"))));
	}
	if (UBorder* selectedBorder = Cast<UBorder>(SelectedStateWidget.Get()))
	{
		selectedBorder->SetPadding(FMargin(2.0f));
		selectedBorder->SetBrushColor(MakeSidebarBlockColor(TEXT("2498FF")));
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
	ApplyVisualStyle();

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
