#include "Scenario/Widget/ScenarioEditorSidebarCorridorLaneWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"

namespace
{
	constexpr float CorridorLaneWidgetPadding = 6.0f;

	void AddLaneWidgetToBox(UVerticalBox* box, UWidget* widget)
	{
		if (!box || !widget)
		{
			return;
		}

		if (UVerticalBoxSlot* slot = box->AddChildToVerticalBox(widget))
		{
			slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, CorridorLaneWidgetPadding));
			slot->SetHorizontalAlignment(HAlign_Fill);
		}
	}

	void AddLaneFieldRow(
		UWidgetTree* widgetTree,
		UVerticalBox* parent,
		TObjectPtr<UScenarioEditorSidebarFieldRow>& fieldRow,
		const TCHAR* widgetName)
	{
		if (!widgetTree || !parent)
		{
			return;
		}

		fieldRow = widgetTree->ConstructWidget<UScenarioEditorSidebarFieldRow>(
			UScenarioEditorSidebarFieldRow::StaticClass(),
			FName(widgetName));
		AddLaneWidgetToBox(parent, fieldRow.Get());
	}
}

TSharedRef<SWidget> UScenarioEditorSidebarCorridorLaneWidget::RebuildWidget()
{
	Initialize();
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void UScenarioEditorSidebarCorridorLaneWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindFieldRows();
	ConfigureFieldRows();
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
	ConfigureFieldRows();
}

void UScenarioEditorSidebarCorridorLaneWidget::SetTextStyleCatalog(
	TSoftObjectPtr<UWidgetTextStyleCatalog> catalog)
{
	TextStyleCatalog = catalog;
	ApplyTextStyles();
}

void UScenarioEditorSidebarCorridorLaneWidget::RefreshFromLane(
	const FScenarioTemplateLaneRule& lane)
{
	CachedLane = lane;
	bHasCachedLane = true;
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

void UScenarioEditorSidebarCorridorLaneWidget::BuildDefaultWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	LaneBlockWidget = WidgetTree->ConstructWidget<UScenarioEditorSidebarBlockWidget>(
		UScenarioEditorSidebarBlockWidget::StaticClass(),
		TEXT("LaneBlockWidget"));
	if (!LaneBlockWidget)
	{
		return;
	}

	WidgetTree->RootWidget = LaneBlockWidget;
	LaneBlockWidget->SetNested(true);
	LaneBlockWidget->SetShowNormalOutline(false);

	UVerticalBox* laneBody = LaneBlockWidget->GetBodyBox();
	AddLaneFieldRow(WidgetTree, laneBody, SurfaceFieldRow, TEXT("SurfaceFieldRow"));
	AddLaneFieldRow(WidgetTree, laneBody, WidthFieldRow, TEXT("WidthFieldRow"));
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
		LaneBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		LaneBlockWidget->SetBlockMetadata(
			FString::Printf(TEXT("lane[%d]"), LaneIndex),
			MakeLanePath(Side),
			TEXT("Detail"));
		LaneBlockWidget->SetNested(true);
		LaneBlockWidget->SetShowNormalOutline(false);
	}

	if (SurfaceFieldRow)
	{
		SurfaceFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		SurfaceFieldRow->SetFieldLabel(TEXT("surface"));
		SurfaceFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Text);
		SurfaceFieldRow->SetEditable(true);
		SurfaceFieldRow->SetArrayControlsEnabled(true);
	}

	if (WidthFieldRow)
	{
		WidthFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		WidthFieldRow->SetFieldLabel(TEXT("width_m"));
		WidthFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Range);
		WidthFieldRow->SetEditable(true);
	}
}

void UScenarioEditorSidebarCorridorLaneWidget::ApplyCachedLaneToRows()
{
	if (!bHasCachedLane)
	{
		return;
	}

	if (SurfaceFieldRow)
	{
		SurfaceFieldRow->SetValueText(CachedLane.SurfaceId);
	}

	if (WidthFieldRow)
	{
		const FScenarioTemplateNumberValue& widthMeters = CachedLane.WidthMeters;
		const double fixedDisplayValue = widthMeters.Mode == EScenarioTemplateNumberValueMode::Range
			? (widthMeters.MinValue + widthMeters.MaxValue) * 0.5
			: widthMeters.FixedValue;
		const double minDisplayValue = widthMeters.Mode == EScenarioTemplateNumberValueMode::Range
			? widthMeters.MinValue
			: fixedDisplayValue;
		const double maxDisplayValue = widthMeters.Mode == EScenarioTemplateNumberValueMode::Range
			? widthMeters.MaxValue
			: fixedDisplayValue;

		WidthFieldRow->SetValueText(widthMeters.bIsSet ? FormatEditableNumber(fixedDisplayValue) : FString());
		WidthFieldRow->SetRangeValueText(
			widthMeters.bIsSet ? FormatEditableNumber(minDisplayValue) : FString(),
			widthMeters.bIsSet ? FormatEditableNumber(maxDisplayValue) : FString());
		WidthFieldRow->SetRangeInputEnabled(
			widthMeters.bIsSet
			&& widthMeters.Mode == EScenarioTemplateNumberValueMode::Range);
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

FString UScenarioEditorSidebarCorridorLaneWidget::FormatEditableNumber(const double value)
{
	return FString::Printf(TEXT("%.2f"), value);
}

FString UScenarioEditorSidebarCorridorLaneWidget::MakeLanePath(
	const EScenarioEditorCorridorSide side)
{
	return side == EScenarioEditorCorridorSide::Building
		? FString(TEXT("root.corridor.building_side[]"))
		: FString(TEXT("root.corridor.curb_side[]"));
}
