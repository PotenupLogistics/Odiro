#include "Scenario/Widget/ScenarioEditorOutlinerRowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ContentWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "Scenario/ViewModel/ScenarioEditorListItemViewModel.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	constexpr float MinimumIndentSpacerHeight = 1.0f;
	constexpr float OutlinerIconRightPadding = 6.0f;
	const FString CorridorOutlinerKey = TEXT("Scenario/Corridor");
	const FString RobotOutlinerKey = TEXT("Scenario/Robot");
	const FString ObstaclesOutlinerKey = TEXT("Scenario/Obstacles");
	const FString PedestriansOutlinerKey = TEXT("Scenario/Pedestrians");

	// Default texture path for the expanded outliner row toggle icon.
	const TCHAR* OutlinerExpandedIconPath = TEXT("/Game/Textures/Icon/T_Icon_CaretDown.T_Icon_CaretDown");

	// Default texture path for the collapsed outliner row toggle icon.
	const TCHAR* OutlinerCollapsedIconPath = TEXT("/Game/Textures/Icon/T_Icon_CaretRight.T_Icon_CaretRight");

	// Loads an outliner icon from a Blueprint-configurable soft reference.
	UTexture2D* LoadOutlinerIconTexture(const TSoftObjectPtr<UTexture2D>& textureReference)
	{
		return textureReference.IsNull() ? nullptr : textureReference.LoadSynchronous();
	}

	// Loads a built-in outliner icon fallback from a fixed content path.
	UTexture2D* LoadOutlinerIconTexture(const TCHAR* texturePath)
	{
		return texturePath ? LoadObject<UTexture2D>(nullptr, texturePath) : nullptr;
	}

	// Resolves a configured icon first, then falls back to the product default caret.
	UTexture2D* ResolveOutlinerToggleIconTexture(
		const TSoftObjectPtr<UTexture2D>& textureReference,
		const TCHAR* fallbackTexturePath)
	{
		if (UTexture2D* texture = LoadOutlinerIconTexture(textureReference))
		{
			return texture;
		}

		return LoadOutlinerIconTexture(fallbackTexturePath);
	}

	// Resolves stale Blueprint-saved defaults where collapsed inherited the expanded caret asset.
	UTexture2D* ResolveOutlinerExpandButtonIconTexture(
		const bool bExpanded,
		const TSoftObjectPtr<UTexture2D>& expandedTextureReference,
		const TSoftObjectPtr<UTexture2D>& collapsedTextureReference)
	{
		if (bExpanded)
		{
			return ResolveOutlinerToggleIconTexture(
				expandedTextureReference,
				OutlinerExpandedIconPath);
		}

		const FSoftObjectPath collapsedPath =
			collapsedTextureReference.ToSoftObjectPath();
		const bool bCollapsedUsesExpandedDefault =
			!collapsedTextureReference.IsNull()
			&& collapsedPath == expandedTextureReference.ToSoftObjectPath()
			&& collapsedPath == FSoftObjectPath(OutlinerExpandedIconPath);
		if (bCollapsedUsesExpandedDefault)
		{
			return LoadOutlinerIconTexture(OutlinerCollapsedIconPath);
		}

		return ResolveOutlinerToggleIconTexture(
			collapsedTextureReference,
			OutlinerCollapsedIconPath);
	}

	// Updates only the image resource so WBP-authored size, tint, and layout remain authoritative.
	void SetOutlinerIconTexture(UImage* image, UTexture2D* texture)
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

	// Finds an icon image inside WBP-authored button content without requiring a fixed widget name.
	UImage* FindOutlinerIconImageWidget(UWidget* widget)
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
				if (UImage* childImage = FindOutlinerIconImageWidget(
					panelWidget->GetChildAt(childIndex)))
				{
					return childImage;
				}
			}
		}
		if (UContentWidget* contentWidget = Cast<UContentWidget>(widget))
		{
			return FindOutlinerIconImageWidget(contentWidget->GetContent());
		}
		return nullptr;
	}
}

UScenarioEditorOutlinerRowWidget::UScenarioEditorOutlinerRowWidget(
	const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{
	CorridorIcon = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(TEXT("/Game/Widgets/Icon/Icon_Outliner_Corridor.Icon_Outliner_Corridor")));
	RobotIcon = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(TEXT("/Game/Widgets/Icon/Icon_Outliner_Robot.Icon_Outliner_Robot")));
	ObstacleIcon = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(TEXT("/Game/Widgets/Icon/Icon_Outliner_Obstacle.Icon_Outliner_Obstacle")));
	PedestrianIcon = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(TEXT("/Game/Widgets/Icon/Icon_Outliner_Pedestrian.Icon_Outliner_Pedestrian")));
	ExpandedExpandButtonIconTexture = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(OutlinerExpandedIconPath));
	CollapsedExpandButtonIconTexture = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(OutlinerCollapsedIconPath));
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
		ExpandButton->SetVisibility(Item.bExpandable ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
	ApplyExpandButtonState();

	if (ItemLabelText)
	{
		ItemLabelText->SetText(Item.Label);
	}
	if (ItemSubtitleText)
	{
		ItemSubtitleText->SetVisibility(ESlateVisibility::Collapsed);
	}

	UImage* iconImage = ResolveOutlinerIconImage();
	if (iconImage)
	{
		if (UTexture2D* iconTexture = ResolveIconTexture())
		{
			iconImage->SetBrushFromTexture(iconTexture, false);
			FSlateBrush iconBrush = iconImage->GetBrush();
			iconBrush.ImageSize = FVector2D(OutlinerIconSize, OutlinerIconSize);
			iconImage->SetBrush(iconBrush);
			iconImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			iconImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UScenarioEditorOutlinerRowWidget::EnsureExpandIconImage()
{
	if (ExpandIconImage)
	{
		if (ExpandGlyphText)
		{
			ExpandGlyphText->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (!ExpandButton)
	{
		return;
	}
	if (UWidget* buttonContent = ExpandButton->GetContent())
	{
		if (UImage* contentIconImage = FindOutlinerIconImageWidget(buttonContent))
		{
			ExpandIconImage = contentIconImage;
			if (ExpandGlyphText)
			{
				ExpandGlyphText->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
}

void UScenarioEditorOutlinerRowWidget::ApplyExpandButtonState()
{
	EnsureExpandIconImage();
	UTexture2D* texture = ResolveOutlinerExpandButtonIconTexture(
		Item.bExpandable && Item.bExpanded,
		ExpandedExpandButtonIconTexture,
		CollapsedExpandButtonIconTexture);
	if (ExpandIconImage)
	{
		SetOutlinerIconTexture(
			ExpandIconImage.Get(),
			texture);
	}
	if (ExpandGlyphText)
	{
		ExpandGlyphText->SetVisibility(ExpandIconImage
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}
}

UImage* UScenarioEditorOutlinerRowWidget::ResolveOutlinerIconImage()
{
	if (OutlinerIconImage)
	{
		return OutlinerIconImage;
	}
	if (!OutlinerRowTextBox || !WidgetTree)
	{
		return nullptr;
	}

	OutlinerIconImage = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(),
		TEXT("RuntimeOutlinerIconImage"));
	if (!OutlinerIconImage)
	{
		return nullptr;
	}

	if (UPanelSlot* panelSlot = OutlinerRowTextBox->InsertChildAt(0, OutlinerIconImage.Get()))
	{
		if (UHorizontalBoxSlot* horizontalSlot = Cast<UHorizontalBoxSlot>(panelSlot))
		{
			horizontalSlot->SetPadding(FMargin(0.0f, 0.0f, OutlinerIconRightPadding, 0.0f));
			horizontalSlot->SetHorizontalAlignment(HAlign_Center);
			horizontalSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
	return OutlinerIconImage;
}

UTexture2D* UScenarioEditorOutlinerRowWidget::ResolveIconTexture() const
{
	const auto loadIcon = [](const TSoftObjectPtr<UTexture2D>& icon)
	{
		return icon.IsNull() ? nullptr : icon.LoadSynchronous();
	};

	if (Item.ItemType != EScenarioEditorOutlinerItemType::Placeable)
	{
		return nullptr;
	}

	if (Item.ParentKey == CorridorOutlinerKey)
	{
		return loadIcon(CorridorIcon);
	}
	if (Item.ParentKey == RobotOutlinerKey)
	{
		return loadIcon(RobotIcon);
	}
	if (Item.ParentKey == ObstaclesOutlinerKey)
	{
		return loadIcon(ObstacleIcon);
	}
	if (Item.ParentKey == PedestriansOutlinerKey)
	{
		return loadIcon(PedestrianIcon);
	}

	switch (Item.ActorCategory)
	{
	case EScenarioActorCategory::DeliveryBot:
		return loadIcon(RobotIcon);
	case EScenarioActorCategory::StaticObstacle:
		return loadIcon(ObstacleIcon);
	case EScenarioActorCategory::Pedestrian:
		return loadIcon(PedestrianIcon);
	case EScenarioActorCategory::GroundRegion:
		return loadIcon(CorridorIcon);
	default:
		return nullptr;
	}
}
