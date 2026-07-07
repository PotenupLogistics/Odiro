#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ContentWidget.h"
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
#include "UI/BaseButtonWidget.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	// Default texture path for the expanded block toggle icon.
	const TCHAR* SidebarBlockExpandedIconPath = TEXT("/Game/Textures/Icon/T_Icon_CaretDown.T_Icon_CaretDown");

	// Default texture path for the collapsed block toggle icon.
	const TCHAR* SidebarBlockCollapsedIconPath = TEXT("/Game/Textures/Icon/T_Icon_CaretRight.T_Icon_CaretRight");

	// Loads a sidebar icon from a Blueprint-configurable soft reference.
	UTexture2D* LoadSidebarBlockIconTexture(const TSoftObjectPtr<UTexture2D>& textureReference)
	{
		return textureReference.IsNull() ? nullptr : textureReference.LoadSynchronous();
	}

	// Loads a built-in sidebar icon fallback from a fixed content path.
	UTexture2D* LoadSidebarBlockIconTexture(const TCHAR* texturePath)
	{
		return texturePath ? LoadObject<UTexture2D>(nullptr, texturePath) : nullptr;
	}

	// Resolves a configured icon first, then falls back to the product default caret.
	UTexture2D* ResolveSidebarBlockIconTexture(
		const TSoftObjectPtr<UTexture2D>& textureReference,
		const TCHAR* fallbackTexturePath)
	{
		if (UTexture2D* texture = LoadSidebarBlockIconTexture(textureReference))
		{
			return texture;
		}

		return LoadSidebarBlockIconTexture(fallbackTexturePath);
	}

	// Resolves stale Blueprint-saved defaults where collapsed inherited the expanded caret asset.
	UTexture2D* ResolveSidebarBlockToggleIconTexture(
		const bool bExpanded,
		const TSoftObjectPtr<UTexture2D>& expandedTextureReference,
		const TSoftObjectPtr<UTexture2D>& collapsedTextureReference)
	{
		if (bExpanded)
		{
			return ResolveSidebarBlockIconTexture(
				expandedTextureReference,
				SidebarBlockExpandedIconPath);
		}

		const FSoftObjectPath collapsedPath =
			collapsedTextureReference.ToSoftObjectPath();
		const bool bCollapsedUsesExpandedDefault =
			!collapsedTextureReference.IsNull()
			&& collapsedPath == expandedTextureReference.ToSoftObjectPath()
			&& collapsedPath == FSoftObjectPath(SidebarBlockExpandedIconPath);
		if (bCollapsedUsesExpandedDefault)
		{
			return LoadSidebarBlockIconTexture(SidebarBlockCollapsedIconPath);
		}

		return ResolveSidebarBlockIconTexture(
			collapsedTextureReference,
			SidebarBlockCollapsedIconPath);
	}

	// Updates only the image resource so WBP-authored size, tint, and layout remain authoritative.
	void SetSidebarBlockIconTexture(UImage* image, UTexture2D* texture)
	{
		if (!image || !texture)
		{
			return;
		}

		FSlateBrush brush = image->GetBrush();
		brush.SetResourceObject(texture);
		if (brush.GetDrawType() == ESlateBrushDrawType::NoDrawType)
		{
			brush.DrawAs = ESlateBrushDrawType::Image;
		}
		image->SetBrush(brush);
		image->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	// Finds an icon image inside WBP-authored toggle content without requiring a fixed widget name.
	UImage* FindSidebarBlockIconImageWidget(UWidget* widget)
	{
		if (!widget)
		{
			return nullptr;
		}
		if (UImage* image = Cast<UImage>(widget))
		{
			return image;
		}
		if (UPanelWidget* panelWidget = Cast<UPanelWidget>(widget))
		{
			for (int32 childIndex = 0; childIndex < panelWidget->GetChildrenCount(); ++childIndex)
			{
				if (UImage* childImage = FindSidebarBlockIconImageWidget(
					panelWidget->GetChildAt(childIndex)))
				{
					return childImage;
				}
			}
		}
		if (UContentWidget* contentWidget = Cast<UContentWidget>(widget))
		{
			return FindSidebarBlockIconImageWidget(contentWidget->GetContent());
		}
		return nullptr;
	}

	// Returns whether the current pointer is inside a visible child control.
	bool IsPointerOverVisibleWidget(const UWidget* widget, const FPointerEvent& mouseEvent)
	{
		return widget
			&& widget->IsVisible()
			&& widget->GetCachedGeometry().IsUnderLocation(mouseEvent.GetScreenSpacePosition());
	}
}

UScenarioEditorSidebarBlockWidget::UScenarioEditorSidebarBlockWidget(
	const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{
	ExpandedToggleIconTexture = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(SidebarBlockExpandedIconPath));
	CollapsedToggleIconTexture = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(SidebarBlockCollapsedIconPath));
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

void UScenarioEditorSidebarBlockWidget::SetPathTextVisible(const bool bInPathTextVisible)
{
	bPathTextVisible = bInPathTextVisible;
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
			bodySlot->SetPadding(bFieldRowChild
				? GeneratedFieldRowSlotPadding
				: GeneratedBodyWidgetSlotPadding);
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
}

void UScenarioEditorSidebarBlockWidget::HandleAddActionClicked(UBaseButtonWidget* button)
{
	(void)button;
	OnAddActionRequested.Broadcast();
}

void UScenarioEditorSidebarBlockWidget::HandleRemoveActionClicked(UBaseButtonWidget* button)
{
	(void)button;
	OnRemoveActionRequested.Broadcast();
}

void UScenarioEditorSidebarBlockWidget::BroadcastBlockSelected()
{
	OnBlockSelected.Broadcast(BlockPath);
}

bool UScenarioEditorSidebarBlockWidget::ShouldBroadcastSelectionForPointer(
	const FPointerEvent& mouseEvent) const
{
	if (IsPointerOverVisibleWidget(AddActionButton.Get(), mouseEvent)
		|| IsPointerOverVisibleWidget(RemoveActionButton.Get(), mouseEvent)
		|| IsPointerOverVisibleWidget(ToggleButton.Get(), mouseEvent))
	{
		return false;
	}

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
	EnsureToggleIcon();
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
		AddActionButton->OnBaseClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarBlockWidget::HandleAddActionClicked);
	}
	if (RemoveActionButton)
	{
		RemoveActionButton->OnBaseClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarBlockWidget::HandleRemoveActionClicked);
	}
}

void UScenarioEditorSidebarBlockWidget::EnsureActionButtons()
{
	if (AddActionButton)
	{
		AddActionButton->OnBaseClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarBlockWidget::HandleAddActionClicked);
		AddActionButton->OnBaseClicked.AddDynamic(
			this,
			&UScenarioEditorSidebarBlockWidget::HandleAddActionClicked);
	}

	if (RemoveActionButton)
	{
		RemoveActionButton->OnBaseClicked.RemoveDynamic(
			this,
			&UScenarioEditorSidebarBlockWidget::HandleRemoveActionClicked);
		RemoveActionButton->OnBaseClicked.AddDynamic(
			this,
			&UScenarioEditorSidebarBlockWidget::HandleRemoveActionClicked);
	}
}

void UScenarioEditorSidebarBlockWidget::EnsureToggleIcon()
{
	if (ToggleIconImage)
	{
		if (ToggleTextBlock)
		{
			ToggleTextBlock->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (!ToggleButton)
	{
		return;
	}
	if (UWidget* buttonContent = ToggleButton->GetContent())
	{
		if (UImage* contentIconImage = FindSidebarBlockIconImageWidget(buttonContent))
		{
			ToggleIconImage = contentIconImage;
			if (ToggleTextBlock)
			{
				ToggleTextBlock->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		return;
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
		thumbnailSlot->SetPadding(GeneratedAssetThumbnailSlotPadding);
		thumbnailSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UHorizontalBoxSlot* textSlot =
		AssetHeaderContainer->AddChildToHorizontalBox(AssetHeaderTextBox.Get()))
	{
		textSlot->SetPadding(GeneratedAssetTextSlotPadding);
		textSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UVerticalBoxSlot* typeSlot =
		AssetHeaderTextBox->AddChildToVerticalBox(AssetHeaderTypeTextBlock.Get()))
	{
		typeSlot->SetPadding(GeneratedAssetTypeSlotPadding);
	}
	AssetHeaderTextBox->AddChildToVerticalBox(AssetHeaderNameTextBlock.Get());

	if (UPanelSlot* headerSlot = headerPanel->AddChild(AssetHeaderContainer.Get()))
	{
		if (UHorizontalBoxSlot* horizontalSlot = Cast<UHorizontalBoxSlot>(headerSlot))
		{
			horizontalSlot->SetPadding(GeneratedAssetContainerSlotPadding);
			horizontalSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
	AssetHeaderContainer->SetVisibility(ESlateVisibility::Collapsed);
	bGeneratedAssetHeaderSummaryCreated = true;
}

void UScenarioEditorSidebarBlockWidget::SetActionButtonState(
	UBaseButtonWidget* button,
	const bool bVisible) const
{
	if (!button)
	{
		return;
	}

	button->SetDisabled(!bVisible);
	button->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UScenarioEditorSidebarBlockWidget::SetHeaderActionContainerVisible(const bool bVisible) const
{
	const ESlateVisibility visibility = bVisible
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed;
	if (HeaderActionBox)
	{
		HeaderActionBox->SetVisibility(visibility);
	}
}

void UScenarioEditorSidebarBlockWidget::ApplyToggleButtonState() const
{
	UTexture2D* texture = ResolveSidebarBlockToggleIconTexture(
		bExpanded,
		ExpandedToggleIconTexture,
		CollapsedToggleIconTexture);
	if (ToggleIconImage)
	{
		SetSidebarBlockIconTexture(
			ToggleIconImage.Get(),
			texture);
	}
	if (ToggleTextBlock)
	{
		ToggleTextBlock->SetVisibility(ToggleIconImage
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
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
		PathTextBlock->SetVisibility(!bShowAssetHeader && bPathTextVisible
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	if (BadgeTextBlock)
	{
		BadgeTextBlock->SetVisibility(bShowAssetHeader
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}
	if (HeaderActionSpacer)
	{
		HeaderActionSpacer->SetVisibility(bShowAssetHeader
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
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
	}
	if (AssetHeaderNameTextBlock)
	{
		AssetHeaderNameTextBlock->SetText(AssetHeaderNameText);
	}
	if (AssetHeaderThumbnailImage)
	{
		UTexture2D* thumbnailTexture = AssetHeaderThumbnailTexture.IsNull()
			? nullptr
			: AssetHeaderThumbnailTexture.LoadSynchronous();
		if (thumbnailTexture)
		{
			SetSidebarBlockIconTexture(AssetHeaderThumbnailImage.Get(), thumbnailTexture);
			if (bGeneratedAssetHeaderSummaryCreated && GeneratedAssetThumbnailSize > 0.0f)
			{
				AssetHeaderThumbnailImage->SetDesiredSizeOverride(
					FVector2D(GeneratedAssetThumbnailSize, GeneratedAssetThumbnailSize));
			}
		}
		else
		{
			AssetHeaderThumbnailImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UScenarioEditorSidebarBlockWidget::ApplyVisualStyle()
{
	if (BlockHeaderRow)
	{
		BlockHeaderRow->SetVisibility(bDetailHostLayout ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}

void UScenarioEditorSidebarBlockWidget::RefreshBlock()
{
	EnsureToggleIcon();
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
	SetActionButtonState(
		AddActionButton.Get(),
		bAddActionVisible && !bDetailHostLayout);
	SetActionButtonState(
		RemoveActionButton.Get(),
		bRemoveActionVisible && !bDetailHostLayout);
	SetHeaderActionContainerVisible((bAddActionVisible || bRemoveActionVisible) && !bDetailHostLayout);
	ApplyVisualStyle();
	ApplyToggleButtonState();
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
