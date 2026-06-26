#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
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

	// Visual treatment resolved from the sidebar block hierarchy depth.
	struct FSidebarBlockSurfaceStyle
	{
		// Background color used by the block content border.
		FLinearColor ContentColor = MakeSidebarBlockColor(TEXT("0B0B0B"));

		// Outline color used by the outer block border.
		FLinearColor OutlineColor = MakeSidebarBlockColor(TEXT("0E0E0E"));

		// Padding that exposes the outer border as a hierarchy strip.
		FMargin OutlinePadding = FMargin(1.0f);

		// Padding applied inside the content border.
		FMargin ContentPadding = FMargin(6.0f, 4.0f, 6.0f, 6.0f);

		// Padding applied before the block body rows.
		FMargin BodyPadding = FMargin(10.0f, 7.0f, 4.0f, 2.0f);
	};

	// Counts semantic path depth below root so nested template blocks get distinct surfaces.
	int32 ResolveSidebarBlockDepth(const FString& blockPath, const bool bNested)
	{
		FString relativePath = blockPath;
		if (relativePath.StartsWith(TEXT("root.")))
		{
			relativePath.RightChopInline(5);
		}
		else if (relativePath == TEXT("root") || relativePath == TEXT("scenario"))
		{
			relativePath.Reset();
		}

		int32 depth = 0;
		for (const TCHAR character : relativePath)
		{
			if (character == TEXT('.'))
			{
				++depth;
			}
		}
		return bNested ? FMath::Max(depth, 1) : depth;
	}

	// Resolves a stronger block surface palette and indentation for the requested depth.
	FSidebarBlockSurfaceStyle ResolveSidebarSurfaceStyle(
		const int32 blockDepth,
		const bool bSelected,
		const bool bShowNormalOutline)
	{
		const int32 clampedDepth = FMath::Clamp(blockDepth, 0, 3);
		const TCHAR* contentColors[] = {
			TEXT("080808"),
			TEXT("121212"),
			TEXT("1C1C1C"),
			TEXT("262626")
		};
		const TCHAR* outlineColors[] = {
			TEXT("101010"),
			TEXT("2A2A2A"),
			TEXT("404040"),
			TEXT("555555")
		};

		FSidebarBlockSurfaceStyle style;
		style.ContentColor = bSelected
			? MakeSidebarBlockColor(TEXT("0A1824"))
			: MakeSidebarBlockColor(contentColors[clampedDepth]);
		style.OutlineColor = bSelected
			? MakeSidebarBlockColor(TEXT("2498FF"))
			: MakeSidebarBlockColor(bShowNormalOutline || blockDepth > 0
				? outlineColors[clampedDepth]
				: TEXT("070707"));
		style.OutlinePadding = blockDepth > 0 || bSelected
			? FMargin(2.0f + static_cast<float>(clampedDepth), 0.0f, 0.0f, 0.0f)
			: FMargin(1.0f, 0.0f, 0.0f, 0.0f);
		style.ContentPadding = FMargin(
			6.0f + static_cast<float>(clampedDepth * 2),
			4.0f + static_cast<float>(clampedDepth),
			0.0f,
			6.0f + static_cast<float>(clampedDepth));
		style.BodyPadding = FMargin(
			10.0f + static_cast<float>(clampedDepth * 4),
			7.0f,
			0.0f,
			3.0f + static_cast<float>(clampedDepth));
		return style;
	}

	// Creates a guaranteed box brush so C++ color changes do not depend on the WBP brush asset.
	FSlateBrush MakeSidebarSurfaceBrush()
	{
		FSlateBrush brush;
		brush.DrawAs = ESlateBrushDrawType::Box;
		brush.TintColor = FSlateColor(FLinearColor::White);
		return brush;
	}

	// Applies compact hierarchy-heading typography without changing the shared catalog asset.
	void ApplyCompactBlockNameStyle(
		UTextBlock* textBlock,
		const TSoftObjectPtr<UWidgetTextStyleCatalog>& catalogReference,
		const bool bNested,
		const int32 blockDepth)
	{
		if (!IsValid(textBlock)) return;

		FWidgetTextStyle style = UWidgetTextStyleCatalog::ResolveStyle(
			catalogReference,
			bNested ? EWidgetTextStyleRole::Label : EWidgetTextStyleRole::Title);
		style.Font.Size = 14.f;
		textBlock->SetFont(style.Font);
		textBlock->SetColorAndOpacity(FSlateColor(style.Color));
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

void UScenarioEditorSidebarBlockWidget::SetFocusedDetailLayout(const bool bInFocusedDetailLayout)
{
	bFocusedDetailLayout = bInFocusedDetailLayout;
	RefreshBlock();
}

void UScenarioEditorSidebarBlockWidget::SetDetailHostLayout(const bool bInDetailHostLayout)
{
	bDetailHostLayout = bInDetailHostLayout;
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

void UScenarioEditorSidebarBlockWidget::SetAssetHeaderSummary(
	const FText& typeText,
	const FText& nameText,
	TSoftObjectPtr<UTexture2D> thumbnailTexture,
	const bool bVisible)
{
	AssetHeaderTypeText = typeText;
	AssetHeaderNameText = nameText;
	AssetHeaderThumbnailTexture = thumbnailTexture;
	bAssetHeaderSummaryVisible = bVisible;
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
			return childWidget->IsA<UScenarioEditorSidebarFieldRow>();
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

void UScenarioEditorSidebarBlockWidget::EnsureAssetHeaderSummary()
{
	if (AssetHeaderContainer || !BlockHeaderRow)
	{
		return;
	}

	UPanelWidget* headerPanel = Cast<UPanelWidget>(BlockHeaderRow.Get());
	if (!headerPanel)
	{
		return;
	}

	AssetHeaderContainer = NewObject<UHorizontalBox>(this);
	AssetHeaderThumbnailImage = NewObject<UImage>(AssetHeaderContainer.Get());
	AssetHeaderTextBox = NewObject<UVerticalBox>(AssetHeaderContainer.Get());
	AssetHeaderTypeTextBlock = NewObject<UTextBlock>(AssetHeaderTextBox.Get());
	AssetHeaderNameTextBlock = NewObject<UTextBlock>(AssetHeaderTextBox.Get());
	if (!AssetHeaderContainer
		|| !AssetHeaderThumbnailImage
		|| !AssetHeaderTextBox
		|| !AssetHeaderTypeTextBlock
		|| !AssetHeaderNameTextBlock)
	{
		AssetHeaderContainer = nullptr;
		AssetHeaderThumbnailImage = nullptr;
		AssetHeaderTextBox = nullptr;
		AssetHeaderTypeTextBlock = nullptr;
		AssetHeaderNameTextBlock = nullptr;
		return;
	}

	if (UHorizontalBoxSlot* thumbnailSlot =
		AssetHeaderContainer->AddChildToHorizontalBox(AssetHeaderThumbnailImage.Get()))
	{
		thumbnailSlot->SetPadding(FMargin(0.0f, 0.0f, 7.0f, 0.0f));
		thumbnailSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UHorizontalBoxSlot* textSlot =
		AssetHeaderContainer->AddChildToHorizontalBox(AssetHeaderTextBox.Get()))
	{
		textSlot->SetPadding(FMargin(0.0f));
		textSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UVerticalBoxSlot* typeSlot =
		AssetHeaderTextBox->AddChildToVerticalBox(AssetHeaderTypeTextBlock.Get()))
	{
		typeSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 1.0f));
	}
	AssetHeaderTextBox->AddChildToVerticalBox(AssetHeaderNameTextBlock.Get());

	if (UPanelSlot* headerSlot = headerPanel->AddChild(AssetHeaderContainer.Get()))
	{
		if (UHorizontalBoxSlot* horizontalSlot = Cast<UHorizontalBoxSlot>(headerSlot))
		{
			horizontalSlot->SetPadding(FMargin(6.0f, 0.0f, 4.0f, 0.0f));
			horizontalSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
	AssetHeaderContainer->SetVisibility(ESlateVisibility::Collapsed);
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

void UScenarioEditorSidebarBlockWidget::ApplyAssetHeaderSummaryState()
{
	const bool bShowAssetHeader = bAssetHeaderSummaryVisible && AssetHeaderContainer;
	if (NameTextBlock)
	{
		NameTextBlock->SetVisibility(bShowAssetHeader
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}
	if (PathTextBlock)
	{
		PathTextBlock->SetVisibility(bShowAssetHeader
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}
	if (BadgeTextBlock)
	{
		BadgeTextBlock->SetVisibility(bShowAssetHeader
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}
	if (!AssetHeaderContainer)
	{
		return;
	}

	AssetHeaderContainer->SetVisibility(bShowAssetHeader
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed);
	if (!bShowAssetHeader)
	{
		return;
	}

	if (AssetHeaderTypeTextBlock)
	{
		AssetHeaderTypeTextBlock->SetText(AssetHeaderTypeText);
		FWidgetTextStyle typeStyle =
			UWidgetTextStyleCatalog::ResolveStyle(TextStyleCatalog, EWidgetTextStyleRole::Caption);
		typeStyle.Font.Size = 11;
		AssetHeaderTypeTextBlock->SetFont(typeStyle.Font);
		AssetHeaderTypeTextBlock->SetColorAndOpacity(FSlateColor(typeStyle.Color));
	}
	if (AssetHeaderNameTextBlock)
	{
		AssetHeaderNameTextBlock->SetText(AssetHeaderNameText);
		FWidgetTextStyle nameStyle =
			UWidgetTextStyleCatalog::ResolveStyle(TextStyleCatalog, EWidgetTextStyleRole::Label);
		nameStyle.Font.Size = 14;
		AssetHeaderNameTextBlock->SetFont(nameStyle.Font);
		AssetHeaderNameTextBlock->SetColorAndOpacity(FSlateColor(nameStyle.Color));
	}
	if (AssetHeaderThumbnailImage)
	{
		UTexture2D* thumbnailTexture = AssetHeaderThumbnailTexture.IsNull()
			? nullptr
			: AssetHeaderThumbnailTexture.LoadSynchronous();
		if (thumbnailTexture)
		{
			AssetHeaderThumbnailImage->SetBrushFromTexture(thumbnailTexture, true);
			AssetHeaderThumbnailImage->SetDesiredSizeOverride(FVector2D(34.0f, 34.0f));
			AssetHeaderThumbnailImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			AssetHeaderThumbnailImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UScenarioEditorSidebarBlockWidget::ApplyVisualStyle()
{
	const int32 blockDepth = bFocusedDetailLayout || bDetailHostLayout
		? 0
		: ResolveSidebarBlockDepth(BlockPath, bNested);
	const FSidebarBlockSurfaceStyle surfaceStyle =
		ResolveSidebarSurfaceStyle(blockDepth, bSelected, bShowNormalOutline);
	FSidebarBlockSurfaceStyle resolvedSurfaceStyle = surfaceStyle;
	if (bFocusedDetailLayout)
	{
		resolvedSurfaceStyle.OutlinePadding = FMargin(0.0f);
		resolvedSurfaceStyle.ContentPadding.Left = 0.0f;
		resolvedSurfaceStyle.BodyPadding.Left = 0.0f;
	}
	if (bDetailHostLayout)
	{
		resolvedSurfaceStyle.OutlineColor = FLinearColor::Transparent;
		resolvedSurfaceStyle.ContentColor = FLinearColor::Transparent;
		resolvedSurfaceStyle.OutlinePadding = FMargin(0.0f);
		resolvedSurfaceStyle.ContentPadding = FMargin(0.0f);
		resolvedSurfaceStyle.BodyPadding = FMargin(0.0f);
	}

	if (UBorder* outlineBorder = Cast<UBorder>(OutlineBorder.Get()))
	{
		outlineBorder->SetBrush(MakeSidebarSurfaceBrush());
		outlineBorder->SetPadding(resolvedSurfaceStyle.OutlinePadding);
		outlineBorder->SetBrushColor(resolvedSurfaceStyle.OutlineColor);
	}

	if (UBorder* contentBorder = Cast<UBorder>(ContentBorder.Get()))
	{
		contentBorder->SetBrush(MakeSidebarSurfaceBrush());
		contentBorder->SetPadding(resolvedSurfaceStyle.ContentPadding);
		contentBorder->SetBrushColor(resolvedSurfaceStyle.ContentColor);
	}

	if (BlockHeaderRow)
	{
		BlockHeaderRow->SetVisibility(bDetailHostLayout ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		if (UVerticalBoxSlot* headerSlot = Cast<UVerticalBoxSlot>(BlockHeaderRow->Slot))
		{
			headerSlot->SetPadding(FMargin(0.0f, 1.0f, 0.0f, 3.0f));
		}
	}

	if (BodyBox)
	{
		if (UVerticalBoxSlot* bodySlot = Cast<UVerticalBoxSlot>(BodyBox->Slot))
		{
			bodySlot->SetPadding(resolvedSurfaceStyle.BodyPadding);
		}
	}

	ApplyCompactBlockNameStyle(
		NameTextBlock.Get(),
		TextStyleCatalog,
		bNested,
		blockDepth);
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
		selectedBorder->SetPadding(FMargin(3.0f, 0.0f, 0.0f, 0.0f));
		selectedBorder->SetBrushColor(MakeSidebarBlockColor(TEXT("2498FF")));
	}
}

void UScenarioEditorSidebarBlockWidget::RefreshBlock()
{
	EnsureAssetHeaderSummary();
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
	SetActionButtonState(AddActionButton.Get(), AddActionTextBlock.Get(), bAddActionVisible && !bDetailHostLayout, TEXT("+"));
	SetActionButtonState(RemoveActionButton.Get(), RemoveActionTextBlock.Get(), bRemoveActionVisible && !bDetailHostLayout, TEXT("-"));
	ApplyVisualStyle();
	ApplyAssetHeaderSummaryState();

	if (SelectedStateWidget)
	{
		SelectedStateWidget->SetVisibility(bSelected && !bDetailHostLayout
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
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
