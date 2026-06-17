#include "Scenario/Widget/ScenarioEditorSidebarCorridorSegmentWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"

namespace
{
	constexpr float CorridorSegmentWidgetPadding = 6.0f;

	void AddSegmentWidgetToBox(UVerticalBox* box, UWidget* widget)
	{
		if (!box || !widget)
		{
			return;
		}

		if (UVerticalBoxSlot* slot = box->AddChildToVerticalBox(widget))
		{
			slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, CorridorSegmentWidgetPadding));
			slot->SetHorizontalAlignment(HAlign_Fill);
		}
	}

	void AddSegmentFieldRow(
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
		AddSegmentWidgetToBox(parent, fieldRow.Get());
	}
}

TSharedRef<SWidget> UScenarioEditorSidebarCorridorSegmentWidget::RebuildWidget()
{
	Initialize();
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void UScenarioEditorSidebarCorridorSegmentWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindFieldRows();
	ConfigureFieldRows();
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
	ConfigureFieldRows();
}

void UScenarioEditorSidebarCorridorSegmentWidget::SetTextStyleCatalog(
	TSoftObjectPtr<UWidgetTextStyleCatalog> catalog)
{
	TextStyleCatalog = catalog;
	ApplyTextStyles();
}

void UScenarioEditorSidebarCorridorSegmentWidget::RefreshFromSegment(
	const FScenarioTemplateSegment& segment)
{
	CachedSegment = segment;
	bHasCachedSegment = true;
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

void UScenarioEditorSidebarCorridorSegmentWidget::BuildDefaultWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	SegmentBlockWidget = WidgetTree->ConstructWidget<UScenarioEditorSidebarBlockWidget>(
		UScenarioEditorSidebarBlockWidget::StaticClass(),
		TEXT("SegmentBlockWidget"));
	if (!SegmentBlockWidget)
	{
		return;
	}

	WidgetTree->RootWidget = SegmentBlockWidget;
	SegmentBlockWidget->SetNested(true);
	SegmentBlockWidget->SetShowNormalOutline(false);

	UVerticalBox* segmentBody = SegmentBlockWidget->GetBodyBox();
	AddSegmentFieldRow(WidgetTree, segmentBody, IdFieldRow, TEXT("IdFieldRow"));
	AddSegmentFieldRow(WidgetTree, segmentBody, TypeFieldRow, TEXT("TypeFieldRow"));
	AddSegmentFieldRow(WidgetTree, segmentBody, AlongRangeFieldRow, TEXT("AlongRangeFieldRow"));
	AddSegmentFieldRow(WidgetTree, segmentBody, ReplacedByFieldRow, TEXT("ReplacedByFieldRow"));
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
		IdFieldRow->OnAddItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorSegmentWidget::HandleAddSegmentRequested);
		IdFieldRow->OnAddItemRequested.AddDynamic(
			this,
			&UScenarioEditorSidebarCorridorSegmentWidget::HandleAddSegmentRequested);
		IdFieldRow->OnRemoveItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorSegmentWidget::HandleRemoveSegmentRequested);
		IdFieldRow->OnRemoveItemRequested.AddDynamic(
			this,
			&UScenarioEditorSidebarCorridorSegmentWidget::HandleRemoveSegmentRequested);
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
}

void UScenarioEditorSidebarCorridorSegmentWidget::UnbindFieldRows()
{
	if (IdFieldRow)
	{
		IdFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorSegmentWidget::HandleIdCommitted);
		IdFieldRow->OnAddItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorSegmentWidget::HandleAddSegmentRequested);
		IdFieldRow->OnRemoveItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorSegmentWidget::HandleRemoveSegmentRequested);
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
}

void UScenarioEditorSidebarCorridorSegmentWidget::ConfigureFieldRows()
{
	if (SegmentBlockWidget)
	{
		SegmentBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		SegmentBlockWidget->SetBlockMetadata(
			CachedSegment.SegmentId.IsEmpty()
				? FString::Printf(TEXT("segment[%d]"), SegmentIndex)
				: CachedSegment.SegmentId,
			TEXT("root.corridor.segments[]"),
			TEXT("Detail"));
		SegmentBlockWidget->SetNested(true);
		SegmentBlockWidget->SetShowNormalOutline(false);
	}

	if (IdFieldRow)
	{
		IdFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		IdFieldRow->SetFieldLabel(TEXT("id"));
		IdFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Text);
		IdFieldRow->SetEditable(true);
		IdFieldRow->SetArrayControlsEnabled(true);
	}
	if (TypeFieldRow)
	{
		TypeFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		TypeFieldRow->SetFieldLabel(TEXT("type"));
		TypeFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::EnumText);
		TypeFieldRow->SetEditable(true);
	}
	if (AlongRangeFieldRow)
	{
		AlongRangeFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		AlongRangeFieldRow->SetFieldLabel(TEXT("along_range_m"));
		AlongRangeFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Range);
		AlongRangeFieldRow->SetEditable(true);
		AlongRangeFieldRow->SetRangeInputEnabled(true);
	}
	if (ReplacedByFieldRow)
	{
		ReplacedByFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		ReplacedByFieldRow->SetFieldLabel(TEXT("replaced_by"));
		ReplacedByFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Text);
		ReplacedByFieldRow->SetEditable(true);
	}
}

void UScenarioEditorSidebarCorridorSegmentWidget::ApplyCachedSegmentToRows()
{
	if (!bHasCachedSegment)
	{
		return;
	}

	if (SegmentBlockWidget)
	{
		SegmentBlockWidget->SetBlockMetadata(
			CachedSegment.SegmentId.IsEmpty()
				? FString::Printf(TEXT("segment[%d]"), SegmentIndex)
				: CachedSegment.SegmentId,
			TEXT("root.corridor.segments[]"),
			TEXT("Detail"));
	}
	if (IdFieldRow)
	{
		IdFieldRow->SetValueText(CachedSegment.SegmentId);
	}
	if (TypeFieldRow)
	{
		TypeFieldRow->SetValueText(SegmentTypeToString(CachedSegment.Type));
	}
	if (AlongRangeFieldRow)
	{
		AlongRangeFieldRow->SetRangeValueText(
			FormatEditableNumber(CachedSegment.AlongRangeMeters.StartMeters),
			FormatEditableNumber(CachedSegment.AlongRangeMeters.EndMeters));
		AlongRangeFieldRow->SetValueText(FString::Printf(
			TEXT("%s..%s"),
			*FormatEditableNumber(CachedSegment.AlongRangeMeters.StartMeters),
			*FormatEditableNumber(CachedSegment.AlongRangeMeters.EndMeters)));
		AlongRangeFieldRow->SetRangeInputEnabled(true);
	}
	if (ReplacedByFieldRow)
	{
		ReplacedByFieldRow->SetValueText(FormatEditableStringValue(CachedSegment.ReplacedBySurfaceId));
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

FString UScenarioEditorSidebarCorridorSegmentWidget::SegmentTypeToString(
	const EScenarioTemplateSegmentType type)
{
	switch (type)
	{
	case EScenarioTemplateSegmentType::Straight:
		return TEXT("straight");
	case EScenarioTemplateSegmentType::Narrowing:
		return TEXT("narrowing");
	case EScenarioTemplateSegmentType::Crosswalk:
		return TEXT("crosswalk");
	case EScenarioTemplateSegmentType::Entrance:
		return TEXT("entrance");
	default:
		return TEXT("unknown");
	}
}

FString UScenarioEditorSidebarCorridorSegmentWidget::FormatEditableStringValue(
	const FScenarioTemplateStringValue& value)
{
	if (!value.bIsSet)
	{
		return FString();
	}
	if (value.Mode == EScenarioTemplateStringValueMode::Choices)
	{
		return value.Choices.IsEmpty() ? FString() : value.Choices[0];
	}

	return value.FixedValue;
}

FString UScenarioEditorSidebarCorridorSegmentWidget::FormatEditableNumber(const double value)
{
	return FString::Printf(TEXT("%.2f"), value);
}
