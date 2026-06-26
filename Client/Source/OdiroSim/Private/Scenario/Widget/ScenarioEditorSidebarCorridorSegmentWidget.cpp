#include "Scenario/Widget/ScenarioEditorSidebarCorridorSegmentWidget.h"

#include "Engine/World.h"
#include "Scenario/ScenarioEditorUiSubsystem.h"
#include "Scenario/ViewModel/ScenarioTemplateFieldRowViewModel.h"
#include "Scenario/ViewModel/ScenarioTemplateSidebarViewModel.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarWidgetHelpers.h"

namespace SidebarWidgetHelpers = ScenarioEditorSidebarWidgetHelpers;

void UScenarioEditorSidebarCorridorSegmentWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindFieldRows();
	ConfigureFieldRows();
	RefreshFieldItemsFromViewModel();
	ApplyCachedSegmentToRows();
}

void UScenarioEditorSidebarCorridorSegmentWidget::NativeDestruct()
{
	UnbindFieldRows();
	Super::NativeDestruct();
}

void UScenarioEditorSidebarCorridorSegmentWidget::SetSegmentIndex(const int32 inSegmentIndex)
{
	SegmentIndex = inSegmentIndex;
	if (bHasCachedSegment)
	{
		RefreshFieldItemsFromViewModel();
	}
	ConfigureFieldRows();
	ApplyCachedSegmentToRows();
}

void UScenarioEditorSidebarCorridorSegmentWidget::SetTextStyleCatalog(
	TSoftObjectPtr<UWidgetTextStyleCatalog> catalog)
{
	TextStyleCatalog = catalog;
	ApplyTextStyles();
}

void UScenarioEditorSidebarCorridorSegmentWidget::SetSurfaceOptions(
	const TArray<FString>& surfaceIds)
{
	SurfaceOptions = surfaceIds;
	if (bHasCachedSegment)
	{
		RefreshFieldItemsFromViewModel();
	}
	ApplyCachedSegmentToRows();
}

void UScenarioEditorSidebarCorridorSegmentWidget::RefreshFromSegment(
	const FScenarioTemplateSegment& segment)
{
	CachedSegment = segment;
	bHasCachedSegment = true;
	RefreshFieldItemsFromViewModel();
	ConfigureFieldRows();
	ApplyCachedSegmentToRows();
}

void UScenarioEditorSidebarCorridorSegmentWidget::HandleIdCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	OnIdCommitted.Broadcast(SegmentIndex, text, commitMethod);
}

void UScenarioEditorSidebarCorridorSegmentWidget::HandleTypeCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	OnTypeCommitted.Broadcast(SegmentIndex, text, commitMethod);
}

void UScenarioEditorSidebarCorridorSegmentWidget::HandleAlongRangeCommitted(
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	OnAlongRangeCommitted.Broadcast(SegmentIndex, minText, maxText, commitMethod);
}

void UScenarioEditorSidebarCorridorSegmentWidget::HandleReplacedByCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	OnReplacedByCommitted.Broadcast(SegmentIndex, text, commitMethod);
}

void UScenarioEditorSidebarCorridorSegmentWidget::HandleAddSegmentRequested()
{
	OnAddSegmentRequested.Broadcast(SegmentIndex);
}

void UScenarioEditorSidebarCorridorSegmentWidget::HandleRemoveSegmentRequested()
{
	OnRemoveSegmentRequested.Broadcast(SegmentIndex);
}

void UScenarioEditorSidebarCorridorSegmentWidget::BindFieldRows()
{
	if (IdFieldRow)
	{
		IdFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorSegmentWidget::HandleIdCommitted);
		IdFieldRow->OnValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarCorridorSegmentWidget::HandleIdCommitted);
	}

	if (TypeFieldRow)
	{
		TypeFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorSegmentWidget::HandleTypeCommitted);
		TypeFieldRow->OnValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarCorridorSegmentWidget::HandleTypeCommitted);
	}

	if (AlongRangeFieldRow)
	{
		AlongRangeFieldRow->OnRangeValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorSegmentWidget::HandleAlongRangeCommitted);
		AlongRangeFieldRow->OnRangeValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarCorridorSegmentWidget::HandleAlongRangeCommitted);
	}

	if (ReplacedByFieldRow)
	{
		ReplacedByFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorSegmentWidget::HandleReplacedByCommitted);
		ReplacedByFieldRow->OnValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarCorridorSegmentWidget::HandleReplacedByCommitted);
	}
	if (SegmentBlockWidget)
	{
		SegmentBlockWidget->OnRemoveActionRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorSegmentWidget::HandleRemoveSegmentRequested);
		SegmentBlockWidget->OnRemoveActionRequested.AddDynamic(
			this,
			&UScenarioEditorSidebarCorridorSegmentWidget::HandleRemoveSegmentRequested);
	}
}

void UScenarioEditorSidebarCorridorSegmentWidget::UnbindFieldRows()
{
	if (IdFieldRow)
	{
		IdFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorSegmentWidget::HandleIdCommitted);
	}
	if (TypeFieldRow)
	{
		TypeFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorSegmentWidget::HandleTypeCommitted);
	}
	if (AlongRangeFieldRow)
	{
		AlongRangeFieldRow->OnRangeValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorSegmentWidget::HandleAlongRangeCommitted);
	}
	if (ReplacedByFieldRow)
	{
		ReplacedByFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorSegmentWidget::HandleReplacedByCommitted);
	}
	if (SegmentBlockWidget)
	{
		SegmentBlockWidget->OnRemoveActionRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorSegmentWidget::HandleRemoveSegmentRequested);
	}
}

void UScenarioEditorSidebarCorridorSegmentWidget::ConfigureFieldRows()
{
	if (SegmentBlockWidget)
	{
		SidebarWidgetHelpers::ConfigureBlock(SegmentBlockWidget.Get(), TextStyleCatalog, {
			CachedSegment.SegmentId.IsEmpty()
				? FString::Printf(TEXT("구간 %d"), SegmentIndex + 1)
				: CachedSegment.SegmentId,
			SidebarWidgetHelpers::MakeIndexedBlockPath(TEXT("root.corridor.segments"), SegmentIndex),
			TEXT("세부"),
			false,
			true,
			false });
		SegmentBlockWidget->SetAddActionVisible(false);
		SegmentBlockWidget->SetRemoveActionVisible(true);
	}

	if (IdFieldRow)
	{
		IdFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (TypeFieldRow)
	{
		TypeFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (AlongRangeFieldRow)
	{
		AlongRangeFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (ReplacedByFieldRow)
	{
		ReplacedByFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
}

void UScenarioEditorSidebarCorridorSegmentWidget::RefreshFieldItemsFromViewModel()
{
	if (!bHasCachedSegment)
	{
		return;
	}

	CachedFieldItems.Reset();
	if (UScenarioTemplateSidebarViewModel* templateSidebarViewModel = GetTemplateSidebarViewModel())
	{
		for (UScenarioTemplateFieldRowViewModel* fieldItem :
			templateSidebarViewModel->CreateCorridorSegmentFieldItems(
				SegmentIndex,
				CachedSegment,
				SurfaceOptions))
		{
			CachedFieldItems.Add(fieldItem);
		}
	}
}

void UScenarioEditorSidebarCorridorSegmentWidget::ApplyCachedSegmentToRows()
{
	if (SegmentBlockWidget)
	{
		SegmentBlockWidget->SetBlockMetadata(
			CachedSegment.SegmentId.IsEmpty()
				? FString::Printf(TEXT("구간 %d"), SegmentIndex + 1)
				: CachedSegment.SegmentId,
			SidebarWidgetHelpers::MakeIndexedBlockPath(TEXT("root.corridor.segments"), SegmentIndex),
			TEXT("세부"));
	}
	if (IdFieldRow)
	{
		IdFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("CorridorSegmentId")));
		IdFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (TypeFieldRow)
	{
		TypeFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("CorridorSegmentType")));
		TypeFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (AlongRangeFieldRow)
	{
		AlongRangeFieldRow->InitializeFromItemViewModel(
			FindCachedFieldItem(TEXT("CorridorSegmentAlongRange")));
		AlongRangeFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (ReplacedByFieldRow)
	{
		ReplacedByFieldRow->InitializeFromItemViewModel(
			FindCachedFieldItem(TEXT("CorridorSegmentReplacedBy")));
		ReplacedByFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
}

void UScenarioEditorSidebarCorridorSegmentWidget::ApplyTextStyles()
{
	if (SegmentBlockWidget)
	{
		SegmentBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
	}
	for (UScenarioEditorSidebarFieldRow* fieldRow : {
		IdFieldRow.Get(),
		TypeFieldRow.Get(),
		AlongRangeFieldRow.Get(),
		ReplacedByFieldRow.Get() })
	{
		if (fieldRow)
		{
			fieldRow->SetTextStyleCatalog(TextStyleCatalog);
		}
	}
}

UScenarioTemplateSidebarViewModel* UScenarioEditorSidebarCorridorSegmentWidget::GetTemplateSidebarViewModel() const
{
	UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this);
	return uiSubsystem ? uiSubsystem->GetTemplateSidebarViewModel() : nullptr;
}

UScenarioTemplateFieldRowViewModel* UScenarioEditorSidebarCorridorSegmentWidget::FindCachedFieldItem(
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
