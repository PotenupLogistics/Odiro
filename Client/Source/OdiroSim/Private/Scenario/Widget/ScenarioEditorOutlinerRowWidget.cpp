#include "Scenario/Widget/ScenarioEditorOutlinerRowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "Scenario/ViewModel/ScenarioEditorListItemViewModel.h"

namespace
{
	constexpr float MinimumIndentSpacerHeight = 1.0f;
	constexpr float OutlinerIconRightPadding = 6.0f;
	const FString CorridorOutlinerKey = TEXT("Scenario/Corridor");
	const FString RobotOutlinerKey = TEXT("Scenario/Robot");
	const FString ObstaclesOutlinerKey = TEXT("Scenario/Obstacles");
	const FString PedestriansOutlinerKey = TEXT("Scenario/Pedestrians");
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
