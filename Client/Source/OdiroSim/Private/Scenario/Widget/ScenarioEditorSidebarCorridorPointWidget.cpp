#include "Scenario/Widget/ScenarioEditorSidebarCorridorPointWidget.h"

#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"

void UScenarioEditorSidebarCorridorPointWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindFieldRows();
	ConfigureFieldRows();
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
	ConfigureFieldRows();
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
		XFieldRow->SetFieldLabel(TEXT("x"));
		XFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Number);
		XFieldRow->SetEditable(true);
		XFieldRow->SetArrayControlsEnabled(true);
	}
	if (YFieldRow)
	{
		YFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		YFieldRow->SetFieldLabel(TEXT("y"));
		YFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Number);
		YFieldRow->SetEditable(true);
	}
}

void UScenarioEditorSidebarCorridorPointWidget::ApplyCachedPointToRows()
{
	if (!bHasCachedPoint)
	{
		return;
	}

	if (XFieldRow)
	{
		XFieldRow->SetValueText(FormatEditableNumber(CachedPointMeters.X));
	}
	if (YFieldRow)
	{
		YFieldRow->SetValueText(FormatEditableNumber(CachedPointMeters.Y));
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

FString UScenarioEditorSidebarCorridorPointWidget::FormatEditableNumber(const double value)
{
	return FString::Printf(TEXT("%.2f"), value);
}
