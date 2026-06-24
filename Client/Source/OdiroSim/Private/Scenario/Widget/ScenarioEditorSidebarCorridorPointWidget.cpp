#include "Scenario/Widget/ScenarioEditorSidebarCorridorPointWidget.h"

#include "Engine/World.h"
#include "Scenario/ScenarioEditorUiSubsystem.h"
#include "Scenario/ViewModel/ScenarioTemplateFieldRowViewModel.h"
#include "Scenario/ViewModel/ScenarioTemplateSidebarViewModel.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"

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
		XFieldRow->OnAddItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPointWidget::HandleAddPointRequested);
		XFieldRow->OnAddItemRequested.AddDynamic(
			this,
			&UScenarioEditorSidebarCorridorPointWidget::HandleAddPointRequested);
		XFieldRow->OnRemoveItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPointWidget::HandleRemovePointRequested);
		XFieldRow->OnRemoveItemRequested.AddDynamic(
			this,
			&UScenarioEditorSidebarCorridorPointWidget::HandleRemovePointRequested);
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
}

void UScenarioEditorSidebarCorridorPointWidget::UnbindFieldRows()
{
	if (XFieldRow)
	{
		XFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPointWidget::HandleXCommitted);
		XFieldRow->OnAddItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPointWidget::HandleAddPointRequested);
		XFieldRow->OnRemoveItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPointWidget::HandleRemovePointRequested);
	}
	if (YFieldRow)
	{
		YFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPointWidget::HandleYCommitted);
	}
}

void UScenarioEditorSidebarCorridorPointWidget::ConfigureFieldRows()
{
	if (PointBlockWidget)
	{
		PointBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		PointBlockWidget->SetBlockMetadata(
			FString::Printf(TEXT("point[%d]"), PointIndex),
			TEXT("root.corridor.axis.points_m[]"),
			TEXT("Detail"));
		PointBlockWidget->SetNested(true);
		PointBlockWidget->SetShowNormalOutline(false);
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
