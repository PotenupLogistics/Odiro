#include "Scenario/Widget/ScenarioEditorSidebarCorridorPointWidget.h"

#include "Engine/World.h"
#include "Scenario/ScenarioEditorUiSubsystem.h"
#include "Scenario/ViewModel/ScenarioTemplateFieldRowViewModel.h"
#include "Scenario/ViewModel/ScenarioTemplateSidebarViewModel.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarWidgetHelpers.h"

namespace SidebarWidgetHelpers = ScenarioEditorSidebarWidgetHelpers;

void UScenarioEditorSidebarCorridorPointWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindFieldRows();
	ConfigureFieldRows();
	RefreshFieldItemsFromViewModel();
	ApplyCachedPointToRows();
}

void UScenarioEditorSidebarCorridorPointWidget::NativeDestruct()
{
	UnbindFieldRows();
	Super::NativeDestruct();
}

void UScenarioEditorSidebarCorridorPointWidget::SetPointIndex(const int32 inPointIndex)
{
	PointIndex = inPointIndex;
	if (bHasCachedPoint)
	{
		RefreshFieldItemsFromViewModel();
	}
	ConfigureFieldRows();
	ApplyCachedPointToRows();
}

void UScenarioEditorSidebarCorridorPointWidget::SetTextStyleCatalog(
	TSoftObjectPtr<UWidgetTextStyleCatalog> catalog)
{
	TextStyleCatalog = catalog;
	ApplyTextStyles();
}

void UScenarioEditorSidebarCorridorPointWidget::RefreshFromPoint(const FVector2D& pointMeters)
{
	CachedPointMeters = pointMeters;
	bHasCachedPoint = true;
	RefreshFieldItemsFromViewModel();
	ConfigureFieldRows();
	ApplyCachedPointToRows();
}

void UScenarioEditorSidebarCorridorPointWidget::HandleXCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	OnXCommitted.Broadcast(PointIndex, text, commitMethod);
}

void UScenarioEditorSidebarCorridorPointWidget::HandleYCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	OnYCommitted.Broadcast(PointIndex, text, commitMethod);
}

void UScenarioEditorSidebarCorridorPointWidget::HandleAddPointRequested()
{
	OnAddPointRequested.Broadcast(PointIndex);
}

void UScenarioEditorSidebarCorridorPointWidget::HandleRemovePointRequested()
{
	OnRemovePointRequested.Broadcast(PointIndex);
}

void UScenarioEditorSidebarCorridorPointWidget::BindFieldRows()
{
	if (XFieldRow)
	{
		XFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPointWidget::HandleXCommitted);
		XFieldRow->OnValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarCorridorPointWidget::HandleXCommitted);
	}

	if (YFieldRow)
	{
		YFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPointWidget::HandleYCommitted);
		YFieldRow->OnValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarCorridorPointWidget::HandleYCommitted);
	}
	if (PointBlockWidget)
	{
		PointBlockWidget->OnRemoveActionRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPointWidget::HandleRemovePointRequested);
		PointBlockWidget->OnRemoveActionRequested.AddDynamic(
			this,
			&UScenarioEditorSidebarCorridorPointWidget::HandleRemovePointRequested);
	}
}

void UScenarioEditorSidebarCorridorPointWidget::UnbindFieldRows()
{
	if (XFieldRow)
	{
		XFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPointWidget::HandleXCommitted);
	}
	if (YFieldRow)
	{
		YFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPointWidget::HandleYCommitted);
	}
	if (PointBlockWidget)
	{
		PointBlockWidget->OnRemoveActionRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPointWidget::HandleRemovePointRequested);
	}
}

void UScenarioEditorSidebarCorridorPointWidget::ConfigureFieldRows()
{
	if (PointBlockWidget)
	{
		SidebarWidgetHelpers::ConfigureBlock(PointBlockWidget.Get(), TextStyleCatalog, {
			FString::Printf(TEXT("경로 점 %d"), PointIndex + 1),
			SidebarWidgetHelpers::MakeIndexedBlockPath(TEXT("root.corridor.axis.points_m"), PointIndex),
			TEXT("세부"),
			false,
			true,
			false });
		PointBlockWidget->SetAddActionVisible(false);
		PointBlockWidget->SetRemoveActionVisible(true);
	}

	if (XFieldRow)
	{
		XFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (YFieldRow)
	{
		YFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
}

void UScenarioEditorSidebarCorridorPointWidget::RefreshFieldItemsFromViewModel()
{
	if (!bHasCachedPoint)
	{
		return;
	}

	CachedFieldItems.Reset();
	if (UScenarioTemplateSidebarViewModel* templateSidebarViewModel = GetTemplateSidebarViewModel())
	{
		for (UScenarioTemplateFieldRowViewModel* fieldItem :
			templateSidebarViewModel->CreateCorridorPointFieldItems(PointIndex, CachedPointMeters))
		{
			CachedFieldItems.Add(fieldItem);
		}
	}
}

void UScenarioEditorSidebarCorridorPointWidget::ApplyCachedPointToRows()
{
	if (XFieldRow)
	{
		XFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("CorridorPointX")));
		XFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (YFieldRow)
	{
		YFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("CorridorPointY")));
		YFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
}

void UScenarioEditorSidebarCorridorPointWidget::ApplyTextStyles()
{
	if (PointBlockWidget)
	{
		PointBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (XFieldRow)
	{
		XFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (YFieldRow)
	{
		YFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
}

UScenarioTemplateSidebarViewModel* UScenarioEditorSidebarCorridorPointWidget::GetTemplateSidebarViewModel() const
{
	UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this);
	return uiSubsystem ? uiSubsystem->GetTemplateSidebarViewModel() : nullptr;
}

UScenarioTemplateFieldRowViewModel* UScenarioEditorSidebarCorridorPointWidget::FindCachedFieldItem(
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
