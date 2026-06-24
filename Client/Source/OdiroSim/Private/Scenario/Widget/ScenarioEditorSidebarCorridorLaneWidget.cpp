#include "Scenario/Widget/ScenarioEditorSidebarCorridorLaneWidget.h"

#include "Engine/World.h"
#include "Scenario/ScenarioEditorUiSubsystem.h"
#include "Scenario/ViewModel/ScenarioTemplateFieldRowViewModel.h"
#include "Scenario/ViewModel/ScenarioTemplateSidebarViewModel.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"

void UScenarioEditorSidebarCorridorLaneWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindFieldRows();
	ConfigureFieldRows();
	RefreshFieldItemsFromViewModel();
	ApplyCachedLaneToRows();
}

void UScenarioEditorSidebarCorridorLaneWidget::NativeDestruct()
{
	UnbindFieldRows();
	Super::NativeDestruct();
}

void UScenarioEditorSidebarCorridorLaneWidget::SetLaneContext(
	const EScenarioEditorCorridorSide inSide,
	const int32 inLaneIndex)
{
	Side = inSide;
	LaneIndex = inLaneIndex;
	if (bHasCachedLane)
	{
		RefreshFieldItemsFromViewModel();
	}
	ConfigureFieldRows();
	ApplyCachedLaneToRows();
}

void UScenarioEditorSidebarCorridorLaneWidget::SetTextStyleCatalog(
	TSoftObjectPtr<UWidgetTextStyleCatalog> catalog)
{
	TextStyleCatalog = catalog;
	ApplyTextStyles();
}

void UScenarioEditorSidebarCorridorLaneWidget::SetSurfaceOptions(
	const TArray<FString>& surfaceIds)
{
	SurfaceOptions = surfaceIds;
	if (bHasCachedLane)
	{
		RefreshFieldItemsFromViewModel();
	}
	ApplyCachedLaneToRows();
}

void UScenarioEditorSidebarCorridorLaneWidget::RefreshFromLane(
	const FScenarioTemplateLaneRule& lane)
{
	CachedLane = lane;
	bHasCachedLane = true;
	RefreshFieldItemsFromViewModel();
	ConfigureFieldRows();
	ApplyCachedLaneToRows();
}

void UScenarioEditorSidebarCorridorLaneWidget::HandleSurfaceCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	OnSurfaceCommitted.Broadcast(Side, LaneIndex, text, commitMethod);
}

void UScenarioEditorSidebarCorridorLaneWidget::HandleWidthCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	OnWidthCommitted.Broadcast(Side, LaneIndex, text, commitMethod);
}

void UScenarioEditorSidebarCorridorLaneWidget::HandleWidthRangeCommitted(
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	OnWidthRangeCommitted.Broadcast(Side, LaneIndex, minText, maxText, commitMethod);
}

void UScenarioEditorSidebarCorridorLaneWidget::HandleAddLaneRequested()
{
	OnAddLaneRequested.Broadcast(Side, LaneIndex);
}

void UScenarioEditorSidebarCorridorLaneWidget::HandleRemoveLaneRequested()
{
	OnRemoveLaneRequested.Broadcast(Side, LaneIndex);
}

void UScenarioEditorSidebarCorridorLaneWidget::BindFieldRows()
{
	if (SurfaceFieldRow)
	{
		SurfaceFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorLaneWidget::HandleSurfaceCommitted);
		SurfaceFieldRow->OnValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarCorridorLaneWidget::HandleSurfaceCommitted);
		SurfaceFieldRow->OnAddItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorLaneWidget::HandleAddLaneRequested);
		SurfaceFieldRow->OnAddItemRequested.AddDynamic(
			this,
			&UScenarioEditorSidebarCorridorLaneWidget::HandleAddLaneRequested);
		SurfaceFieldRow->OnRemoveItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorLaneWidget::HandleRemoveLaneRequested);
		SurfaceFieldRow->OnRemoveItemRequested.AddDynamic(
			this,
			&UScenarioEditorSidebarCorridorLaneWidget::HandleRemoveLaneRequested);
	}

	if (WidthFieldRow)
	{
		WidthFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorLaneWidget::HandleWidthCommitted);
		WidthFieldRow->OnValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarCorridorLaneWidget::HandleWidthCommitted);
		WidthFieldRow->OnRangeValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorLaneWidget::HandleWidthRangeCommitted);
		WidthFieldRow->OnRangeValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarCorridorLaneWidget::HandleWidthRangeCommitted);
	}
}

void UScenarioEditorSidebarCorridorLaneWidget::UnbindFieldRows()
{
	if (SurfaceFieldRow)
	{
		SurfaceFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorLaneWidget::HandleSurfaceCommitted);
		SurfaceFieldRow->OnAddItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorLaneWidget::HandleAddLaneRequested);
		SurfaceFieldRow->OnRemoveItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorLaneWidget::HandleRemoveLaneRequested);
	}

	if (WidthFieldRow)
	{
		WidthFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorLaneWidget::HandleWidthCommitted);
		WidthFieldRow->OnRangeValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorLaneWidget::HandleWidthRangeCommitted);
	}
}

void UScenarioEditorSidebarCorridorLaneWidget::ConfigureFieldRows()
{
	if (LaneBlockWidget)
	{
		const TCHAR* sideLabel = Side == EScenarioEditorCorridorSide::Building ? TEXT("건물측") : TEXT("도로측");
		LaneBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		LaneBlockWidget->SetBlockMetadata(
			FString::Printf(TEXT("%s 영역 %d"), sideLabel, LaneIndex + 1),
			MakeLanePath(Side),
			TEXT("세부"));
		LaneBlockWidget->SetNested(true);
		LaneBlockWidget->SetShowNormalOutline(false);
	}

	if (SurfaceFieldRow)
	{
		SurfaceFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}

	if (WidthFieldRow)
	{
		WidthFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
}

void UScenarioEditorSidebarCorridorLaneWidget::RefreshFieldItemsFromViewModel()
{
	if (!bHasCachedLane)
	{
		return;
	}

	CachedFieldItems.Reset();
	if (UScenarioTemplateSidebarViewModel* templateSidebarViewModel = GetTemplateSidebarViewModel())
	{
		for (UScenarioTemplateFieldRowViewModel* fieldItem :
			templateSidebarViewModel->CreateCorridorLaneFieldItems(
				Side,
				LaneIndex,
				CachedLane,
				SurfaceOptions))
		{
			CachedFieldItems.Add(fieldItem);
		}
	}
}

void UScenarioEditorSidebarCorridorLaneWidget::ApplyCachedLaneToRows()
{
	if (SurfaceFieldRow)
	{
		SurfaceFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("CorridorLaneSurface")));
		SurfaceFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}

	if (WidthFieldRow)
	{
		WidthFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("CorridorLaneWidth")));
		WidthFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
}

void UScenarioEditorSidebarCorridorLaneWidget::ApplyTextStyles()
{
	if (LaneBlockWidget)
	{
		LaneBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (SurfaceFieldRow)
	{
		SurfaceFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (WidthFieldRow)
	{
		WidthFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
}

UScenarioTemplateSidebarViewModel* UScenarioEditorSidebarCorridorLaneWidget::GetTemplateSidebarViewModel() const
{
	UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this);
	return uiSubsystem ? uiSubsystem->GetTemplateSidebarViewModel() : nullptr;
}

UScenarioTemplateFieldRowViewModel* UScenarioEditorSidebarCorridorLaneWidget::FindCachedFieldItem(
	const FString& fieldId) const
{
	for (UScenarioTemplateFieldRowViewModel* fieldItem : CachedFieldItems)
	{
		if (fieldItem && fieldItem->GetItemId() == fieldId)
		{
			return fieldItem;
		}
	}
	return nullptr;
}

FString UScenarioEditorSidebarCorridorLaneWidget::MakeLanePath(
	const EScenarioEditorCorridorSide side)
{
	return side == EScenarioEditorCorridorSide::Building
		? FString(TEXT("root.corridor.building_side[]"))
		: FString(TEXT("root.corridor.curb_side[]"));
}
