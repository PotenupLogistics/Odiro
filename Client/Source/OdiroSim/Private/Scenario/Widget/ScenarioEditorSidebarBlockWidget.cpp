#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "Scenario/Data/WidgetTextStyleCatalog.h"
#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

namespace
{
	// Converts UI hex colors into Slate linear colors with a caller-controlled alpha.
	FLinearColor MakeSidebarBlockColor(const TCHAR* hex, const float alpha = 1.0f)
	{
		FLinearColor color = FLinearColor::FromSRGBColor(FColor::FromHex(hex));
		color.A = alpha;
		return color;
	}

	// Builds the flat brush used by generated block header action buttons.
	FSlateBrush MakeSidebarActionBrush(const TCHAR* hex, const float alpha = 1.0f)
	{
		FSlateBrush brush;
		brush.DrawAs = ESlateBrushDrawType::Box;
		brush.TintColor = FSlateColor(MakeSidebarBlockColor(hex, alpha));
		brush.Margin = FMargin(0.0f);
		brush.ImageSize = FVector2D(32.0f, 32.0f);
		brush.OutlineSettings.Width = 0.0f;
		brush.OutlineSettings.Color = FLinearColor::Transparent;
		brush.OutlineSettings.CornerRadii = FVector4(4.0f, 4.0f, 4.0f, 4.0f);
		brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		return brush;
	}

	// Creates a compact borderless button style matching the sidebar's existing flat controls.
	FButtonStyle MakeSidebarActionButtonStyle()
	{
		FButtonStyle style;
		style.SetNormal(MakeSidebarActionBrush(TEXT("1E1E1E")));
		style.SetHovered(MakeSidebarActionBrush(TEXT("282828")));
		style.SetPressed(MakeSidebarActionBrush(TEXT("151515")));
		style.SetDisabled(MakeSidebarActionBrush(TEXT("1E1E1E"), 0.45f));
		style.SetNormalForeground(FSlateColor(MakeSidebarBlockColor(TEXT("F2F2F2"))));
		style.SetHoveredForeground(FSlateColor(MakeSidebarBlockColor(TEXT("FFFFFF"))));
		style.SetPressedForeground(FSlateColor(MakeSidebarBlockColor(TEXT("DDE8F2"))));
		style.SetDisabledForeground(FSlateColor(MakeSidebarBlockColor(TEXT("878787"))));
		style.SetNormalPadding(FMargin(6.0f, 2.0f));
		style.SetPressedPadding(FMargin(6.0f, 2.0f));
		return style;
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

void UScenarioEditorSidebarBlockWidget::SetAddActionVisible(const bool bInAddActionVisible)
{
	bAddActionVisible = bInAddActionVisible;
	RefreshBlock();
}

void UScenarioEditorSidebarBlockWidget::SetRemoveActionVisible(const bool bInRemoveActionVisible)
{
	bRemoveActionVisible = bInRemoveActionVisible;
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

void UScenarioEditorSidebarBlockWidget::HandleAddActionClicked()
{
	BroadcastBlockSelected();
	OnAddActionRequested.Broadcast();
}

void UScenarioEditorSidebarBlockWidget::HandleRemoveActionClicked()
{
	BroadcastBlockSelected();
	OnRemoveActionRequested.Broadcast();
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
	EnsureActionButtons();
}

void UScenarioEditorSidebarBlockWidget::UnbindControls()
{
	if (ToggleButton)
	{
		ToggleButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarBlockWidget::HandleToggleClicked);
	}
	if (AddActionButton)
	{
		AddActionButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarBlockWidget::HandleAddActionClicked);
	}
	if (RemoveActionButton)
	{
		RemoveActionButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarBlockWidget::HandleRemoveActionClicked);
	}
}

void UScenarioEditorSidebarBlockWidget::EnsureActionButtons()
{
	if (!bAddActionVisible && !bRemoveActionVisible)
	{
		return;
	}

	CreateActionButton(AddActionButton, AddActionTextBlock);
	if (AddActionButton)
	{
		AddActionButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarBlockWidget::HandleAddActionClicked);
		AddActionButton->OnClicked.AddDynamic(
			this,
			&UScenarioEditorSidebarBlockWidget::HandleAddActionClicked);
	}

	CreateActionButton(RemoveActionButton, RemoveActionTextBlock);
	if (RemoveActionButton)
	{
		RemoveActionButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarBlockWidget::HandleRemoveActionClicked);
		RemoveActionButton->OnClicked.AddDynamic(
			this,
			&UScenarioEditorSidebarBlockWidget::HandleRemoveActionClicked);
	}
}

void UScenarioEditorSidebarBlockWidget::CreateActionButton(
	TObjectPtr<UButton>& outButton,
	TObjectPtr<UTextBlock>& outTextBlock)
{
	if (outButton || !BlockHeaderRow)
	{
		return;
	}

	UPanelWidget* headerPanel = Cast<UPanelWidget>(BlockHeaderRow.Get());
	if (!headerPanel)
	{
		return;
	}

	outButton = NewObject<UButton>(this);
	outTextBlock = NewObject<UTextBlock>(outButton.Get());
	if (!outButton || !outTextBlock)
	{
		outButton = nullptr;
		outTextBlock = nullptr;
		return;
	}

	outButton->SetContent(outTextBlock.Get());
	outButton->SetStyle(MakeSidebarActionButtonStyle());
	if (UPanelSlot* actionSlot = headerPanel->AddChild(outButton.Get()))
	{
		if (UHorizontalBoxSlot* horizontalSlot = Cast<UHorizontalBoxSlot>(actionSlot))
		{
			horizontalSlot->SetPadding(FMargin(4.0f, 0.0f, 0.0f, 0.0f));
			horizontalSlot->SetHorizontalAlignment(HAlign_Right);
			horizontalSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
	outButton->SetVisibility(ESlateVisibility::Collapsed);
}

void UScenarioEditorSidebarBlockWidget::SetActionButtonState(
	UButton* button,
	UTextBlock* textBlock,
	const bool bVisible,
	const FString& label) const
{
	if (button)
	{
		button->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		button->SetStyle(MakeSidebarActionButtonStyle());
		button->SetBackgroundColor(FLinearColor::White);
		button->SetColorAndOpacity(FLinearColor::White);
	}
	if (textBlock)
	{
		textBlock->SetText(FText::FromString(label));
		textBlock->SetJustification(ETextJustify::Center);
		textBlock->SetColorAndOpacity(FSlateColor(MakeSidebarBlockColor(TEXT("F2F2F2"))));
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
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(
		AddActionTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Label);
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(
		RemoveActionTextBlock.Get(),
		TextStyleCatalog,
		EWidgetTextStyleRole::Label);

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
	EnsureActionButtons();

	if (ToggleTextBlock)
	{
		FWidgetTransform toggleTransform;
		toggleTransform.Angle = bExpanded ? 90.0f : 0.0f;
		ToggleTextBlock->SetRenderTransform(toggleTransform);
	}
	SetTextBlockText(NameTextBlock.Get(), BlockName);
	SetTextBlockText(PathTextBlock.Get(), BlockPath);
	SetTextBlockText(BadgeTextBlock.Get(), BadgeText);
	SetActionButtonState(AddActionButton.Get(), AddActionTextBlock.Get(), bAddActionVisible, TEXT("+"));
	SetActionButtonState(RemoveActionButton.Get(), RemoveActionTextBlock.Get(), bRemoveActionVisible, TEXT("-"));
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
