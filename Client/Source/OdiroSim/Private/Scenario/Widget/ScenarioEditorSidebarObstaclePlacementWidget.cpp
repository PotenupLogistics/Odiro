#include "Scenario/Widget/ScenarioEditorSidebarObstaclePlacementWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"

namespace
{
	constexpr float ObstaclePlacementWidgetPadding = 6.0f;

	void AddObstaclePlacementWidgetToBox(UVerticalBox* box, UWidget* widget)
	{
		if (!box || !widget)
		{
			return;
		}

		if (UVerticalBoxSlot* slot = box->AddChildToVerticalBox(widget))
		{
			slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, ObstaclePlacementWidgetPadding));
			slot->SetHorizontalAlignment(HAlign_Fill);
		}
	}

	void AddObstaclePlacementFieldRow(
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
		AddObstaclePlacementWidgetToBox(parent, fieldRow.Get());
	}
}

TSharedRef<SWidget> UScenarioEditorSidebarObstaclePlacementWidget::RebuildWidget()
{
	Initialize();
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void UScenarioEditorSidebarObstaclePlacementWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindFieldRows();
	ConfigureFieldRows();
	ApplyCachedPlacementToRows();
}

void UScenarioEditorSidebarObstaclePlacementWidget::NativeDestruct()
{
	UnbindFieldRows();
	Super::NativeDestruct();
}

void UScenarioEditorSidebarObstaclePlacementWidget::SetPlacementIndex(const int32 inPlacementIndex)
{
	PlacementIndex = inPlacementIndex;
	ConfigureFieldRows();
}

void UScenarioEditorSidebarObstaclePlacementWidget::SetTextStyleCatalog(
	TSoftObjectPtr<UWidgetTextStyleCatalog> catalog)
{
	TextStyleCatalog = catalog;
	ApplyTextStyles();
}

void UScenarioEditorSidebarObstaclePlacementWidget::RefreshFromPlacement(
	const FScenarioTemplateObstaclePlacement& placement)
{
	CachedPlacement = placement;
	bHasCachedPlacement = true;
	ConfigureFieldRows();
	ApplyCachedPlacementToRows();
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandlePlacementIdCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	OnPlacementIdCommitted.Broadcast(PlacementIndex, text, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandlePropCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	OnPropCommitted.Broadcast(PlacementIndex, text, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleSegmentCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	OnSegmentCommitted.Broadcast(PlacementIndex, text, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleAlongCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	OnAlongCommitted.Broadcast(PlacementIndex, text, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleOffsetCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	OnOffsetCommitted.Broadcast(PlacementIndex, text, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::HandleAllowBlockingCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	OnAllowBlockingCommitted.Broadcast(PlacementIndex, text, commitMethod);
}

void UScenarioEditorSidebarObstaclePlacementWidget::BuildDefaultWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	PlacementBlockWidget = WidgetTree->ConstructWidget<UScenarioEditorSidebarBlockWidget>(
		UScenarioEditorSidebarBlockWidget::StaticClass(),
		TEXT("PlacementBlockWidget"));
	if (!PlacementBlockWidget)
	{
		return;
	}

	WidgetTree->RootWidget = PlacementBlockWidget;
	PlacementBlockWidget->SetNested(true);
	PlacementBlockWidget->SetShowNormalOutline(false);

	UVerticalBox* placementBody = PlacementBlockWidget->GetBodyBox();
	AddObstaclePlacementFieldRow(WidgetTree, placementBody, PlacementIdFieldRow, TEXT("PlacementIdFieldRow"));
	AddObstaclePlacementFieldRow(WidgetTree, placementBody, KindFieldRow, TEXT("KindFieldRow"));
	AddObstaclePlacementFieldRow(WidgetTree, placementBody, PropFieldRow, TEXT("PropFieldRow"));
	AddObstaclePlacementFieldRow(WidgetTree, placementBody, SegmentFieldRow, TEXT("SegmentFieldRow"));
	AddObstaclePlacementFieldRow(WidgetTree, placementBody, AlongFieldRow, TEXT("AlongFieldRow"));
	AddObstaclePlacementFieldRow(WidgetTree, placementBody, OffsetFieldRow, TEXT("OffsetFieldRow"));
	AddObstaclePlacementFieldRow(WidgetTree, placementBody, AllowBlockingFieldRow, TEXT("AllowBlockingFieldRow"));
}

void UScenarioEditorSidebarObstaclePlacementWidget::BindFieldRows()
{
	if (PlacementIdFieldRow)
	{
		PlacementIdFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandlePlacementIdCommitted);
		PlacementIdFieldRow->OnValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandlePlacementIdCommitted);
	}
	if (PropFieldRow)
	{
		PropFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandlePropCommitted);
		PropFieldRow->OnValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandlePropCommitted);
	}
	if (SegmentFieldRow)
	{
		SegmentFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandleSegmentCommitted);
		SegmentFieldRow->OnValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandleSegmentCommitted);
	}
	if (AlongFieldRow)
	{
		AlongFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandleAlongCommitted);
		AlongFieldRow->OnValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandleAlongCommitted);
	}
	if (OffsetFieldRow)
	{
		OffsetFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandleOffsetCommitted);
		OffsetFieldRow->OnValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandleOffsetCommitted);
	}
	if (AllowBlockingFieldRow)
	{
		AllowBlockingFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandleAllowBlockingCommitted);
		AllowBlockingFieldRow->OnValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandleAllowBlockingCommitted);
	}
}

void UScenarioEditorSidebarObstaclePlacementWidget::UnbindFieldRows()
{
	if (PlacementIdFieldRow)
	{
		PlacementIdFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandlePlacementIdCommitted);
	}
	if (PropFieldRow)
	{
		PropFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandlePropCommitted);
	}
	if (SegmentFieldRow)
	{
		SegmentFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandleSegmentCommitted);
	}
	if (AlongFieldRow)
	{
		AlongFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandleAlongCommitted);
	}
	if (OffsetFieldRow)
	{
		OffsetFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandleOffsetCommitted);
	}
	if (AllowBlockingFieldRow)
	{
		AllowBlockingFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePlacementWidget::HandleAllowBlockingCommitted);
	}
}

void UScenarioEditorSidebarObstaclePlacementWidget::ConfigureFieldRows()
{
	const bool bFixedPlacement = IsFixedPlacement();
	if (PlacementBlockWidget)
	{
		PlacementBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		PlacementBlockWidget->SetBlockMetadata(
			bHasCachedPlacement && !CachedPlacement.PlacementId.IsEmpty()
				? CachedPlacement.PlacementId
				: FString::Printf(TEXT("placement[%d]"), PlacementIndex),
			TEXT("root.obstacles.placements[]"),
			TEXT("Detail"));
		PlacementBlockWidget->SetNested(true);
		PlacementBlockWidget->SetShowNormalOutline(false);
	}

	if (PlacementIdFieldRow)
	{
		PlacementIdFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		PlacementIdFieldRow->SetFieldLabel(TEXT("placement_id"));
		PlacementIdFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Text);
		PlacementIdFieldRow->SetEditable(bFixedPlacement);
	}
	if (KindFieldRow)
	{
		KindFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		KindFieldRow->SetFieldLabel(TEXT("kind"));
		KindFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::EnumText);
		KindFieldRow->SetEditable(false);
	}
	if (PropFieldRow)
	{
		PropFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		PropFieldRow->SetFieldLabel(TEXT("prop"));
		PropFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Text);
		PropFieldRow->SetEditable(bFixedPlacement);
	}
	if (SegmentFieldRow)
	{
		SegmentFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		SegmentFieldRow->SetFieldLabel(TEXT("at.segment"));
		SegmentFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Text);
		SegmentFieldRow->SetEditable(bFixedPlacement);
	}
	if (AlongFieldRow)
	{
		AlongFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		AlongFieldRow->SetFieldLabel(TEXT("at.along_m"));
		AlongFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Number);
		AlongFieldRow->SetEditable(bFixedPlacement);
	}
	if (OffsetFieldRow)
	{
		OffsetFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		OffsetFieldRow->SetFieldLabel(TEXT("at.offset_m"));
		OffsetFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Number);
		OffsetFieldRow->SetEditable(bFixedPlacement);
	}
	if (AllowBlockingFieldRow)
	{
		AllowBlockingFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		AllowBlockingFieldRow->SetFieldLabel(TEXT("allow_blocking"));
		AllowBlockingFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::EnumText);
		AllowBlockingFieldRow->SetEditable(bFixedPlacement);
	}
}

void UScenarioEditorSidebarObstaclePlacementWidget::ApplyCachedPlacementToRows()
{
	if (!bHasCachedPlacement)
	{
		return;
	}

	if (PlacementBlockWidget)
	{
		PlacementBlockWidget->SetBlockMetadata(
			CachedPlacement.PlacementId.IsEmpty()
				? FString::Printf(TEXT("placement[%d]"), PlacementIndex)
				: CachedPlacement.PlacementId,
			TEXT("root.obstacles.placements[]"),
			TEXT("Detail"));
	}
	if (PlacementIdFieldRow)
	{
		PlacementIdFieldRow->SetValueText(CachedPlacement.PlacementId);
	}
	if (KindFieldRow)
	{
		KindFieldRow->SetValueText(PlacementKindToString(CachedPlacement.Kind));
	}
	if (PropFieldRow)
	{
		PropFieldRow->SetValueText(CachedPlacement.PropId);
	}
	if (SegmentFieldRow)
	{
		SegmentFieldRow->SetValueText(CachedPlacement.At.SegmentId);
	}
	if (AlongFieldRow)
	{
		AlongFieldRow->SetValueText(FormatEditableNumber(CachedPlacement.At.AlongMeters));
	}
	if (OffsetFieldRow)
	{
		OffsetFieldRow->SetValueText(FormatEditableNumber(CachedPlacement.At.OffsetMeters));
	}
	if (AllowBlockingFieldRow)
	{
		AllowBlockingFieldRow->SetValueText(CachedPlacement.bAllowBlocking ? TEXT("true") : TEXT("false"));
	}
}

void UScenarioEditorSidebarObstaclePlacementWidget::ApplyTextStyles()
{
	if (PlacementBlockWidget)
	{
		PlacementBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
	}
	for (UScenarioEditorSidebarFieldRow* fieldRow : {
		PlacementIdFieldRow.Get(),
		KindFieldRow.Get(),
		PropFieldRow.Get(),
		SegmentFieldRow.Get(),
		AlongFieldRow.Get(),
		OffsetFieldRow.Get(),
		AllowBlockingFieldRow.Get() })
	{
		if (fieldRow)
		{
			fieldRow->SetTextStyleCatalog(TextStyleCatalog);
		}
	}
}

bool UScenarioEditorSidebarObstaclePlacementWidget::IsFixedPlacement() const
{
	return !bHasCachedPlacement || CachedPlacement.Kind == EScenarioTemplateObstaclePlacementKind::Fixed;
}

FString UScenarioEditorSidebarObstaclePlacementWidget::PlacementKindToString(
	const EScenarioTemplateObstaclePlacementKind kind)
{
	switch (kind)
	{
	case EScenarioTemplateObstaclePlacementKind::Fixed:
		return TEXT("fixed");
	case EScenarioTemplateObstaclePlacementKind::Pattern:
		return TEXT("pattern");
	case EScenarioTemplateObstaclePlacementKind::Scatter:
		return TEXT("scatter");
	default:
		return TEXT("unknown");
	}
}

FString UScenarioEditorSidebarObstaclePlacementWidget::FormatEditableNumber(
	const FScenarioTemplateNumberValue& value)
{
	if (!value.bIsSet)
	{
		return FString();
	}
	if (value.Mode == EScenarioTemplateNumberValueMode::Range)
	{
		return FString::Printf(TEXT("%.2f..%.2f"), value.MinValue, value.MaxValue);
	}
	return FString::Printf(TEXT("%.2f"), value.FixedValue);
}
