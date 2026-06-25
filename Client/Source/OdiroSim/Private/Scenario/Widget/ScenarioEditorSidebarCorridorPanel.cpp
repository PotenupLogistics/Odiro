#include "Scenario/Widget/ScenarioEditorSidebarCorridorPanel.h"

#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Scenario/Data/ScenarioCorridorSurfaceCatalog.h"
#include "Scenario/Data/ScenarioEditorWidgetClassCatalog.h"
#include "Scenario/ScenarioEditorUiSubsystem.h"
#include "Scenario/ViewModel/ScenarioEditorShellViewModel.h"
#include "Scenario/ViewModel/ScenarioTemplateFieldRowViewModel.h"
#include "Scenario/ViewModel/ScenarioTemplateSidebarViewModel.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarCorridorLaneWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarCorridorPointWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarCorridorSegmentWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarWidgetHelpers.h"
#include "Scenario/Data/WidgetTextStyleCatalog.h"

namespace SidebarWidgetHelpers = ScenarioEditorSidebarWidgetHelpers;

void UScenarioEditorSidebarCorridorPanel::NativeConstruct()
{
	Super::NativeConstruct();
	if (WidgetClassCatalog.IsNull())
	{
		WidgetClassCatalog = UScenarioEditorWidgetClassCatalog::MakeDefaultCatalogReference();
	}
	BindFieldRows();
	ConfigureFieldRows();
	RefreshFromDraft();
}

void UScenarioEditorSidebarCorridorPanel::NativeDestruct()
{
	UnbindFieldRows();
	Super::NativeDestruct();
}

void UScenarioEditorSidebarCorridorPanel::SetTextStyleCatalog(
	TSoftObjectPtr<UWidgetTextStyleCatalog> catalog)
{
	TextStyleCatalog = catalog;
	ApplyTextStyles();
}

void UScenarioEditorSidebarCorridorPanel::SetWidgetClassCatalog(
	TSoftObjectPtr<UScenarioEditorWidgetClassCatalog> catalog)
{
	WidgetClassCatalog = catalog.IsNull()
		? UScenarioEditorWidgetClassCatalog::MakeDefaultCatalogReference()
		: catalog;
}

void UScenarioEditorSidebarCorridorPanel::RefreshFromDraft()
{
	UScenarioTemplateSidebarViewModel* templateSidebarViewModel = GetTemplateSidebarViewModel();
	FScenarioDocument scenarioTemplate;
	FString failureReason;
	if (!templateSidebarViewModel || !templateSidebarViewModel->TryGetDraftScenario(scenarioTemplate, failureReason))
	{
		SetDiagnosticsText(failureReason.IsEmpty() ? TEXT("ScenarioTemplateSidebarViewModel unavailable.") : failureReason);
		return;
	}

	RefreshFromTemplate(scenarioTemplate);
}

void UScenarioEditorSidebarCorridorPanel::RefreshFromTemplate(
	const FScenarioDocument& scenarioTemplate)
{
	ConfigureFieldRows();

	if (UScenarioTemplateSidebarViewModel* templateSidebarViewModel = GetTemplateSidebarViewModel())
	{
		templateSidebarViewModel->RefreshCorridorFieldItemsFromTemplate(scenarioTemplate);
	}
	ApplyCorridorFieldItems();

	const FScenarioTemplateCorridor& corridor = scenarioTemplate.Corridor;
	RefreshAxisPointRows(corridor.Axis.PointsMeters);

	RefreshLaneProfileRows(
		EScenarioEditorCorridorSide::Building,
		BuildingSideBlockWidget.Get(),
		corridor.BuildingSide);
	RefreshLaneProfileRows(
		EScenarioEditorCorridorSide::Curb,
		CurbSideBlockWidget.Get(),
		corridor.CurbSide);
	RefreshSegmentRows(corridor.Segments);
	ApplySelectedBlockPath();
	SetDiagnosticsText(TEXT(""));
}

void UScenarioEditorSidebarCorridorPanel::CollectBlockWidgets(
	TArray<UScenarioEditorSidebarBlockWidget*>& outBlockWidgets) const
{
	for (UScenarioEditorSidebarBlockWidget* blockWidget : {
		CorridorBlockWidget.Get(),
		AxisBlockWidget.Get(),
		AxisPointsBlockWidget.Get(),
		WalkwayWidthBlockWidget.Get(),
		BuildingSideBlockWidget.Get(),
		CurbSideBlockWidget.Get(),
		SegmentsBlockWidget.Get() })
	{
		if (blockWidget)
		{
			outBlockWidgets.Add(blockWidget);
		}
	}
	for (UScenarioEditorSidebarCorridorLaneWidget* laneWidget : BuildingSideLaneWidgets)
	{
		if (laneWidget && laneWidget->LaneBlockWidget)
		{
			outBlockWidgets.Add(laneWidget->LaneBlockWidget.Get());
		}
	}
	for (UScenarioEditorSidebarCorridorLaneWidget* laneWidget : CurbSideLaneWidgets)
	{
		if (laneWidget && laneWidget->LaneBlockWidget)
		{
			outBlockWidgets.Add(laneWidget->LaneBlockWidget.Get());
		}
	}
	for (UScenarioEditorSidebarCorridorPointWidget* pointWidget : AxisPointWidgets)
	{
		if (pointWidget && pointWidget->PointBlockWidget)
		{
			outBlockWidgets.Add(pointWidget->PointBlockWidget.Get());
		}
	}
	for (UScenarioEditorSidebarCorridorSegmentWidget* segmentWidget : SegmentWidgets)
	{
		if (segmentWidget && segmentWidget->SegmentBlockWidget)
		{
			outBlockWidgets.Add(segmentWidget->SegmentBlockWidget.Get());
		}
	}
}

UScenarioEditorSidebarBlockWidget* UScenarioEditorSidebarCorridorPanel::FindBlockWidgetByPath(
	const FString& blockPath) const
{
	TArray<UScenarioEditorSidebarBlockWidget*> blockWidgets;
	CollectBlockWidgets(blockWidgets);
	for (UScenarioEditorSidebarBlockWidget* blockWidget : blockWidgets)
	{
		if (blockWidget && blockWidget->BlockPath == blockPath)
		{
			return blockWidget;
		}
	}
	return nullptr;
}

void UScenarioEditorSidebarCorridorPanel::HandleWalkwayWidthCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	ExecuteTemplateCommand([&text](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->CommitCorridorWalkwayWidthText(text, statusText);
	});
}

void UScenarioEditorSidebarCorridorPanel::HandleWalkwayWidthRangeCommitted(
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	ExecuteTemplateCommand([&minText, &maxText](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->CommitCorridorWalkwayWidthRangeText(minText, maxText, statusText);
	});
}

void UScenarioEditorSidebarCorridorPanel::HandleLaneSurfaceCommitted(
	const EScenarioEditorCorridorSide side,
	const int32 laneIndex,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	ExecuteTemplateCommand(
		[side, laneIndex, &text](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
		{
			return viewModel->CommitCorridorLaneSurfaceText(side, laneIndex, text, statusText);
		});
}

void UScenarioEditorSidebarCorridorPanel::HandleLaneWidthCommitted(
	const EScenarioEditorCorridorSide side,
	const int32 laneIndex,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	ExecuteTemplateCommand(
		[side, laneIndex, &text](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
		{
			return viewModel->CommitCorridorLaneWidthText(side, laneIndex, text, statusText);
		});
}

void UScenarioEditorSidebarCorridorPanel::HandleLaneWidthRangeCommitted(
	const EScenarioEditorCorridorSide side,
	const int32 laneIndex,
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	ExecuteTemplateCommand(
		[side, laneIndex, &minText, &maxText](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
		{
			return viewModel->CommitCorridorLaneWidthRangeText(
				side,
				laneIndex,
				minText,
				maxText,
				statusText);
		});
}

void UScenarioEditorSidebarCorridorPanel::HandleLaneAddRequested(
	const EScenarioEditorCorridorSide side,
	const int32 laneIndex)
{
	ExecuteTemplateCommand([side, laneIndex](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->AddCorridorLaneAfter(side, laneIndex, statusText);
	});
}

void UScenarioEditorSidebarCorridorPanel::HandleLaneRemoveRequested(
	const EScenarioEditorCorridorSide side,
	const int32 laneIndex)
{
	ExecuteTemplateCommand([side, laneIndex](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->RemoveCorridorLaneAt(side, laneIndex, statusText);
	});
}

void UScenarioEditorSidebarCorridorPanel::HandleBuildingSideCountAddRequested()
{
	ExecuteTemplateCommand([](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->AddCorridorLaneAfter(EScenarioEditorCorridorSide::Building, INDEX_NONE, statusText);
	});
}

void UScenarioEditorSidebarCorridorPanel::HandleBuildingSideCountRemoveRequested()
{
	ExecuteTemplateCommand([](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->RemoveCorridorLaneAt(EScenarioEditorCorridorSide::Building, INDEX_NONE, statusText);
	});
}

void UScenarioEditorSidebarCorridorPanel::HandleCurbSideCountAddRequested()
{
	ExecuteTemplateCommand([](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->AddCorridorLaneAfter(EScenarioEditorCorridorSide::Curb, INDEX_NONE, statusText);
	});
}

void UScenarioEditorSidebarCorridorPanel::HandleCurbSideCountRemoveRequested()
{
	ExecuteTemplateCommand([](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->RemoveCorridorLaneAt(EScenarioEditorCorridorSide::Curb, INDEX_NONE, statusText);
	});
}

void UScenarioEditorSidebarCorridorPanel::HandleAxisPointXCommitted(
	const int32 pointIndex,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	ExecuteTemplateCommand([pointIndex, &text](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->CommitCorridorAxisPointXText(pointIndex, text, statusText);
	});
}

void UScenarioEditorSidebarCorridorPanel::HandleAxisPointYCommitted(
	const int32 pointIndex,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	ExecuteTemplateCommand([pointIndex, &text](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->CommitCorridorAxisPointYText(pointIndex, text, statusText);
	});
}

void UScenarioEditorSidebarCorridorPanel::HandleAxisPointAddRequested(const int32 pointIndex)
{
	ExecuteTemplateCommand([pointIndex](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->AddCorridorAxisPointAfter(pointIndex, statusText);
	});
}

void UScenarioEditorSidebarCorridorPanel::HandleAxisPointRemoveRequested(const int32 pointIndex)
{
	ExecuteTemplateCommand([pointIndex](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->RemoveCorridorAxisPointAt(pointIndex, statusText);
	});
}

void UScenarioEditorSidebarCorridorPanel::HandleAxisPointsCountAddRequested()
{
	ExecuteTemplateCommand([](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->AddCorridorAxisPointAfter(INDEX_NONE, statusText);
	});
}

void UScenarioEditorSidebarCorridorPanel::HandleAxisPointsCountRemoveRequested()
{
	ExecuteTemplateCommand([](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->RemoveCorridorAxisPointAt(INDEX_NONE, statusText);
	});
}

void UScenarioEditorSidebarCorridorPanel::HandleSegmentIdCommitted(
	const int32 segmentIndex,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	ExecuteTemplateCommand([segmentIndex, &text](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->CommitCorridorSegmentIdText(segmentIndex, text, statusText);
	});
}

void UScenarioEditorSidebarCorridorPanel::HandleSegmentTypeCommitted(
	const int32 segmentIndex,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	ExecuteTemplateCommand([segmentIndex, &text](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->CommitCorridorSegmentTypeText(segmentIndex, text, statusText);
	});
}

void UScenarioEditorSidebarCorridorPanel::HandleSegmentAlongRangeCommitted(
	const int32 segmentIndex,
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	ExecuteTemplateCommand(
		[segmentIndex, &minText, &maxText](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
		{
			return viewModel->CommitCorridorSegmentAlongRangeText(
				segmentIndex,
				minText,
				maxText,
				statusText);
		});
}

void UScenarioEditorSidebarCorridorPanel::HandleSegmentReplacedByCommitted(
	const int32 segmentIndex,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	ExecuteTemplateCommand([segmentIndex, &text](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->CommitCorridorSegmentReplacedByText(segmentIndex, text, statusText);
	});
}

void UScenarioEditorSidebarCorridorPanel::HandleSegmentAddRequested(const int32 segmentIndex)
{
	ExecuteTemplateCommand([segmentIndex](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->AddCorridorSegmentAfter(segmentIndex, statusText);
	});
}

void UScenarioEditorSidebarCorridorPanel::HandleSegmentRemoveRequested(const int32 segmentIndex)
{
	ExecuteTemplateCommand([segmentIndex](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->RemoveCorridorSegmentAt(segmentIndex, statusText);
	});
}

void UScenarioEditorSidebarCorridorPanel::HandleSegmentsCountAddRequested()
{
	ExecuteTemplateCommand([](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->AddCorridorSegmentAfter(INDEX_NONE, statusText);
	});
}

void UScenarioEditorSidebarCorridorPanel::HandleSegmentsCountRemoveRequested()
{
	ExecuteTemplateCommand([](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->RemoveCorridorSegmentAt(INDEX_NONE, statusText);
	});
}

void UScenarioEditorSidebarCorridorPanel::BindFieldRows()
{
	if (WalkwayWidthFieldRow)
	{
		WalkwayWidthFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleWalkwayWidthCommitted);
		WalkwayWidthFieldRow->OnValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleWalkwayWidthCommitted);
		WalkwayWidthFieldRow->OnRangeValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleWalkwayWidthRangeCommitted);
		WalkwayWidthFieldRow->OnRangeValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleWalkwayWidthRangeCommitted);
	}
}

void UScenarioEditorSidebarCorridorPanel::UnbindFieldRows()
{
	if (WalkwayWidthFieldRow)
	{
		WalkwayWidthFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleWalkwayWidthCommitted);
		WalkwayWidthFieldRow->OnRangeValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleWalkwayWidthRangeCommitted);
	}
	if (BuildingSideCountFieldRow)
	{
		SidebarWidgetHelpers::UnbindFieldRowActions(BuildingSideCountFieldRow.Get(), this);
	}
	if (CurbSideCountFieldRow)
	{
		SidebarWidgetHelpers::UnbindFieldRowActions(CurbSideCountFieldRow.Get(), this);
	}
	if (AxisPointsFieldRow)
	{
		SidebarWidgetHelpers::UnbindFieldRowActions(AxisPointsFieldRow.Get(), this);
	}
	if (SegmentsCountFieldRow)
	{
		SidebarWidgetHelpers::UnbindFieldRowActions(SegmentsCountFieldRow.Get(), this);
	}
	for (UScenarioEditorSidebarCorridorLaneWidget* laneWidget : BuildingSideLaneWidgets)
	{
		if (!laneWidget)
		{
			continue;
		}

		laneWidget->OnSurfaceCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleLaneSurfaceCommitted);
		laneWidget->OnWidthCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleLaneWidthCommitted);
		laneWidget->OnWidthRangeCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleLaneWidthRangeCommitted);
		laneWidget->OnAddLaneRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleLaneAddRequested);
		laneWidget->OnRemoveLaneRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleLaneRemoveRequested);
	}
	for (UScenarioEditorSidebarCorridorLaneWidget* laneWidget : CurbSideLaneWidgets)
	{
		if (!laneWidget)
		{
			continue;
		}

		laneWidget->OnSurfaceCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleLaneSurfaceCommitted);
		laneWidget->OnWidthCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleLaneWidthCommitted);
		laneWidget->OnWidthRangeCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleLaneWidthRangeCommitted);
		laneWidget->OnAddLaneRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleLaneAddRequested);
		laneWidget->OnRemoveLaneRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleLaneRemoveRequested);
	}
	for (UScenarioEditorSidebarCorridorPointWidget* pointWidget : AxisPointWidgets)
	{
		if (!pointWidget)
		{
			continue;
		}

		pointWidget->OnXCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleAxisPointXCommitted);
		pointWidget->OnYCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleAxisPointYCommitted);
		pointWidget->OnAddPointRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleAxisPointAddRequested);
		pointWidget->OnRemovePointRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleAxisPointRemoveRequested);
	}
	for (UScenarioEditorSidebarCorridorSegmentWidget* segmentWidget : SegmentWidgets)
	{
		if (!segmentWidget)
		{
			continue;
		}

		segmentWidget->OnIdCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleSegmentIdCommitted);
		segmentWidget->OnTypeCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleSegmentTypeCommitted);
		segmentWidget->OnAlongRangeCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleSegmentAlongRangeCommitted);
		segmentWidget->OnReplacedByCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleSegmentReplacedByCommitted);
		segmentWidget->OnAddSegmentRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleSegmentAddRequested);
		segmentWidget->OnRemoveSegmentRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarCorridorPanel::HandleSegmentRemoveRequested);
	}
}

void UScenarioEditorSidebarCorridorPanel::ConfigureFieldRows()
{
	if (CorridorBlockWidget)
	{
		SidebarWidgetHelpers::ConfigureBlock(CorridorBlockWidget.Get(), TextStyleCatalog, {
			TEXT("통로"),
			TEXT("root.corridor"),
			TEXT("구성"),
			true,
			false,
			true });
	}
	if (AxisBlockWidget)
	{
		SidebarWidgetHelpers::ConfigureBlock(AxisBlockWidget.Get(), TextStyleCatalog, {
			TEXT("중심 경로"),
			TEXT("root.corridor.axis"),
			TEXT("속성"),
			false,
			true,
			false });
	}
	if (AxisPointsBlockWidget)
	{
		SidebarWidgetHelpers::ConfigureBlock(AxisPointsBlockWidget.Get(), TextStyleCatalog, {
			TEXT("경로 점"),
			TEXT("root.corridor.axis.points_m[]"),
			TEXT("속성"),
			false,
			true,
			false });
	}
	if (WalkwayWidthBlockWidget)
	{
		SidebarWidgetHelpers::ConfigureBlock(WalkwayWidthBlockWidget.Get(), TextStyleCatalog, {
			TEXT("보행로 폭"),
			TEXT("root.corridor.walkway_width_m"),
			TEXT("속성"),
			false,
			true,
			false });
	}
	if (BuildingSideBlockWidget)
	{
		SidebarWidgetHelpers::ConfigureBlock(BuildingSideBlockWidget.Get(), TextStyleCatalog, {
			TEXT("건물측 영역"),
			TEXT("root.corridor.building_side[]"),
			TEXT("속성"),
			false,
			true,
			false });
	}
	if (CurbSideBlockWidget)
	{
		SidebarWidgetHelpers::ConfigureBlock(CurbSideBlockWidget.Get(), TextStyleCatalog, {
			TEXT("도로측 영역"),
			TEXT("root.corridor.curb_side[]"),
			TEXT("속성"),
			false,
			true,
			false });
	}
	if (SegmentsBlockWidget)
	{
		SidebarWidgetHelpers::ConfigureBlock(SegmentsBlockWidget.Get(), TextStyleCatalog, {
			TEXT("구간"),
			TEXT("root.corridor.segments[]"),
			TEXT("속성"),
			false,
			true,
			false });
	}
	if (AxisTypeFieldRow)
	{
		AxisTypeFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (AxisPointsFieldRow)
	{
		AxisPointsFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (WalkwayWidthFieldRow)
	{
		WalkwayWidthFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}

	ApplyTextStyles();
}

void UScenarioEditorSidebarCorridorPanel::ApplyCorridorFieldItems()
{
	UScenarioTemplateSidebarViewModel* templateSidebarViewModel = GetTemplateSidebarViewModel();
	if (!templateSidebarViewModel)
	{
		return;
	}

	if (AxisTypeFieldRow)
	{
		AxisTypeFieldRow->InitializeFromItemViewModel(
			templateSidebarViewModel->FindCorridorFieldItem(TEXT("AxisType")));
		AxisTypeFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (AxisPointsFieldRow)
	{
		AxisPointsFieldRow->InitializeFromItemViewModel(
			templateSidebarViewModel->FindCorridorFieldItem(TEXT("AxisPointsCount")));
		AxisPointsFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (WalkwayWidthFieldRow)
	{
		WalkwayWidthFieldRow->InitializeFromItemViewModel(
			templateSidebarViewModel->FindCorridorFieldItem(TEXT("WalkwayWidth")));
		WalkwayWidthFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
}

void UScenarioEditorSidebarCorridorPanel::ApplySelectedBlockPath()
{
	UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this);
	const UScenarioEditorShellViewModel* shellViewModel = uiSubsystem ? uiSubsystem->GetShellViewModel() : nullptr;
	const FString selectedBlockPath = shellViewModel ? shellViewModel->GetSelectedTemplateBlockPath() : FString();

	SidebarWidgetHelpers::ApplySelectedBlockPath(CorridorBlockWidget.Get(), selectedBlockPath);
	SidebarWidgetHelpers::ApplySelectedBlockPath(AxisBlockWidget.Get(), selectedBlockPath);
	SidebarWidgetHelpers::ApplySelectedBlockPath(AxisPointsBlockWidget.Get(), selectedBlockPath);
	SidebarWidgetHelpers::ApplySelectedBlockPath(WalkwayWidthBlockWidget.Get(), selectedBlockPath);
	SidebarWidgetHelpers::ApplySelectedBlockPath(BuildingSideBlockWidget.Get(), selectedBlockPath);
	SidebarWidgetHelpers::ApplySelectedBlockPath(CurbSideBlockWidget.Get(), selectedBlockPath);
	SidebarWidgetHelpers::ApplySelectedBlockPath(SegmentsBlockWidget.Get(), selectedBlockPath);

	if (selectedBlockPath.StartsWith(TEXT("root.corridor.axis.points_m[")))
	{
		if (AxisBlockWidget)
		{
			AxisBlockWidget->SetExpanded(true);
		}
		if (AxisPointsBlockWidget)
		{
			AxisPointsBlockWidget->SetExpanded(true);
		}
	}
	else if (selectedBlockPath.StartsWith(TEXT("root.corridor.segments[")) && SegmentsBlockWidget)
	{
		SegmentsBlockWidget->SetExpanded(true);
	}

	for (UScenarioEditorSidebarCorridorPointWidget* pointWidget : AxisPointWidgets)
	{
		if (pointWidget)
		{
			SidebarWidgetHelpers::ApplySelectedBlockPath(pointWidget->PointBlockWidget.Get(), selectedBlockPath);
		}
	}
	for (UScenarioEditorSidebarCorridorSegmentWidget* segmentWidget : SegmentWidgets)
	{
		if (segmentWidget)
		{
			SidebarWidgetHelpers::ApplySelectedBlockPath(segmentWidget->SegmentBlockWidget.Get(), selectedBlockPath);
		}
	}
	for (UScenarioEditorSidebarCorridorLaneWidget* laneWidget : BuildingSideLaneWidgets)
	{
		if (laneWidget)
		{
			SidebarWidgetHelpers::ApplySelectedBlockPath(laneWidget->LaneBlockWidget.Get(), selectedBlockPath);
		}
	}
	for (UScenarioEditorSidebarCorridorLaneWidget* laneWidget : CurbSideLaneWidgets)
	{
		if (laneWidget)
		{
			SidebarWidgetHelpers::ApplySelectedBlockPath(laneWidget->LaneBlockWidget.Get(), selectedBlockPath);
		}
	}
}

void UScenarioEditorSidebarCorridorPanel::ApplyTextStyles()
{
	for (UScenarioEditorSidebarBlockWidget* blockWidget : {
		CorridorBlockWidget.Get(),
		AxisBlockWidget.Get(),
		AxisPointsBlockWidget.Get(),
		WalkwayWidthBlockWidget.Get(),
		BuildingSideBlockWidget.Get(),
		CurbSideBlockWidget.Get(),
		SegmentsBlockWidget.Get() })
	{
		if (blockWidget)
		{
			blockWidget->SetTextStyleCatalog(TextStyleCatalog);
		}
	}

	for (UScenarioEditorSidebarFieldRow* fieldRow : {
		AxisTypeFieldRow.Get(),
		AxisPointsFieldRow.Get(),
		WalkwayWidthFieldRow.Get(),
		BuildingSideCountFieldRow.Get(),
		CurbSideCountFieldRow.Get(),
		SegmentsCountFieldRow.Get() })
	{
		if (fieldRow)
		{
			fieldRow->SetTextStyleCatalog(TextStyleCatalog);
		}
	}

	for (UScenarioEditorSidebarCorridorLaneWidget* laneWidget : BuildingSideLaneWidgets)
	{
		if (laneWidget)
		{
			laneWidget->SetTextStyleCatalog(TextStyleCatalog);
		}
	}
	for (UScenarioEditorSidebarCorridorLaneWidget* laneWidget : CurbSideLaneWidgets)
	{
		if (laneWidget)
		{
			laneWidget->SetTextStyleCatalog(TextStyleCatalog);
		}
	}
	for (UScenarioEditorSidebarCorridorPointWidget* pointWidget : AxisPointWidgets)
	{
		if (pointWidget)
		{
			pointWidget->SetTextStyleCatalog(TextStyleCatalog);
		}
	}
	for (UScenarioEditorSidebarCorridorSegmentWidget* segmentWidget : SegmentWidgets)
	{
		if (segmentWidget)
		{
			segmentWidget->SetTextStyleCatalog(TextStyleCatalog);
		}
	}

	if (DiagnosticsTextBlock)
	{
		DiagnosticsTextBlock->SetVisibility(DiagnosticsTextBlock->GetText().IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}
}

void UScenarioEditorSidebarCorridorPanel::RefreshAxisPointRows(
	const TArray<FVector2D>& pointsMeters)
{
	AxisPointWidgets.Reset();

	if (AxisPointsFieldRow)
	{
		if (UScenarioTemplateSidebarViewModel* templateSidebarViewModel = GetTemplateSidebarViewModel())
		{
			AxisPointsFieldRow->InitializeFromItemViewModel(
				templateSidebarViewModel->FindCorridorFieldItem(TEXT("AxisPointsCount")));
		}
		AxisPointsFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		SidebarWidgetHelpers::BindFieldRowActions(
			AxisPointsFieldRow.Get(),
			this,
			GET_FUNCTION_NAME_CHECKED(
				UScenarioEditorSidebarCorridorPanel,
				HandleAxisPointsCountAddRequested),
			GET_FUNCTION_NAME_CHECKED(
				UScenarioEditorSidebarCorridorPanel,
				HandleAxisPointsCountRemoveRequested));
	}

	if (!AxisPointsBlockWidget)
	{
		return;
	}

	AxisPointsBlockWidget->ClearBodyChildren();
	if (AxisPointsFieldRow)
	{
		AxisPointsBlockWidget->AddBodyChild(AxisPointsFieldRow.Get());
	}

	for (int32 pointIndex = 0; pointIndex < pointsMeters.Num(); ++pointIndex)
	{
		if (UScenarioEditorSidebarCorridorPointWidget* pointWidget =
			AddAxisPointWidget(pointIndex, pointsMeters[pointIndex], AxisPointsBlockWidget.Get()))
		{
			AxisPointWidgets.Add(pointWidget);
		}
	}
}

void UScenarioEditorSidebarCorridorPanel::RefreshLaneProfileRows(
	const EScenarioEditorCorridorSide side,
	UScenarioEditorSidebarBlockWidget* sideBlockWidget,
	const TArray<FScenarioTemplateLaneRule>& lanes)
{
	if (!sideBlockWidget)
	{
		return;
	}

	TArray<TObjectPtr<UScenarioEditorSidebarCorridorLaneWidget>>& laneWidgets =
		side == EScenarioEditorCorridorSide::Building
			? BuildingSideLaneWidgets
			: CurbSideLaneWidgets;
	laneWidgets.Reset();

	sideBlockWidget->ClearBodyChildren();
	const TArray<FString> surfaceOptions = GetCorridorSurfaceIdOptions();
	UScenarioTemplateSidebarViewModel* templateSidebarViewModel = GetTemplateSidebarViewModel();
	UScenarioEditorSidebarFieldRow* countRow = AddReadOnlyFieldRow(
		sideBlockWidget,
		templateSidebarViewModel
			? templateSidebarViewModel->FindCorridorFieldItem(
				side == EScenarioEditorCorridorSide::Building
					? TEXT("BuildingSideCount")
					: TEXT("CurbSideCount"))
			: nullptr);
	if (countRow)
	{
		if (side == EScenarioEditorCorridorSide::Building)
		{
			BuildingSideCountFieldRow = countRow;
			SidebarWidgetHelpers::BindFieldRowActions(
				countRow,
				this,
				GET_FUNCTION_NAME_CHECKED(
					UScenarioEditorSidebarCorridorPanel,
					HandleBuildingSideCountAddRequested),
				GET_FUNCTION_NAME_CHECKED(
					UScenarioEditorSidebarCorridorPanel,
					HandleBuildingSideCountRemoveRequested));
		}
		else
		{
			CurbSideCountFieldRow = countRow;
			SidebarWidgetHelpers::BindFieldRowActions(
				countRow,
				this,
				GET_FUNCTION_NAME_CHECKED(
					UScenarioEditorSidebarCorridorPanel,
					HandleCurbSideCountAddRequested),
				GET_FUNCTION_NAME_CHECKED(
					UScenarioEditorSidebarCorridorPanel,
					HandleCurbSideCountRemoveRequested));
		}
	}

	for (int32 laneIndex = 0; laneIndex < lanes.Num(); ++laneIndex)
	{
		if (UScenarioEditorSidebarCorridorLaneWidget* laneWidget =
			AddLaneWidget(side, laneIndex, lanes[laneIndex], surfaceOptions, sideBlockWidget))
		{
			laneWidgets.Add(laneWidget);
		}
	}
}

void UScenarioEditorSidebarCorridorPanel::RefreshSegmentRows(
	const TArray<FScenarioTemplateSegment>& segments)
{
	if (!SegmentsBlockWidget)
	{
		return;
	}

	SegmentWidgets.Reset();
	SegmentsBlockWidget->ClearBodyChildren();
	const TArray<FString> surfaceOptions = GetCorridorSurfaceIdOptions();
	UScenarioTemplateSidebarViewModel* templateSidebarViewModel = GetTemplateSidebarViewModel();
	UScenarioEditorSidebarFieldRow* countRow = AddReadOnlyFieldRow(
		SegmentsBlockWidget.Get(),
		templateSidebarViewModel
			? templateSidebarViewModel->FindCorridorFieldItem(TEXT("SegmentsCount"))
			: nullptr);
	if (countRow)
	{
		SegmentsCountFieldRow = countRow;
		SidebarWidgetHelpers::BindFieldRowActions(
			countRow,
			this,
			GET_FUNCTION_NAME_CHECKED(
				UScenarioEditorSidebarCorridorPanel,
				HandleSegmentsCountAddRequested),
			GET_FUNCTION_NAME_CHECKED(
				UScenarioEditorSidebarCorridorPanel,
				HandleSegmentsCountRemoveRequested));
	}

	for (int32 segmentIndex = 0; segmentIndex < segments.Num(); ++segmentIndex)
	{
		if (UScenarioEditorSidebarCorridorSegmentWidget* segmentWidget =
			AddSegmentWidget(segmentIndex, segments[segmentIndex], surfaceOptions, SegmentsBlockWidget.Get()))
		{
			SegmentWidgets.Add(segmentWidget);
		}
	}
}

UScenarioEditorSidebarFieldRow* UScenarioEditorSidebarCorridorPanel::AddReadOnlyFieldRow(
	UScenarioEditorSidebarBlockWidget* parentBlockWidget,
	UScenarioTemplateFieldRowViewModel* fieldItemViewModel) const
{
	UScenarioEditorSidebarFieldRow* fieldRow = SidebarWidgetHelpers::CreateFieldRow(
		GetWorld(),
		WidgetClassCatalog,
		TextStyleCatalog,
		fieldItemViewModel,
		parentBlockWidget);
	if (!fieldRow)
	{
		SetDiagnosticsText(TEXT("Scenario editor field row widget class is missing."));
		return nullptr;
	}

	return fieldRow;
}

UScenarioEditorSidebarCorridorPointWidget* UScenarioEditorSidebarCorridorPanel::AddAxisPointWidget(
	const int32 pointIndex,
	const FVector2D& pointMeters,
	UScenarioEditorSidebarBlockWidget* parentBlockWidget)
{
	if (!GetWorld() || !parentBlockWidget)
	{
		return nullptr;
	}

	UScenarioEditorSidebarCorridorPointWidget* pointWidget =
		CreateWidget<UScenarioEditorSidebarCorridorPointWidget>(
			GetWorld(),
			UScenarioEditorWidgetClassCatalog::ResolveSidebarCorridorPointWidgetClass(WidgetClassCatalog));
	if (!pointWidget)
	{
		SetDiagnosticsText(TEXT("Scenario editor corridor point widget class is missing."));
		return nullptr;
	}

	pointWidget->SetTextStyleCatalog(TextStyleCatalog);
	pointWidget->SetPointIndex(pointIndex);
	pointWidget->RefreshFromPoint(pointMeters);
	pointWidget->OnXCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleAxisPointXCommitted);
	pointWidget->OnXCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleAxisPointXCommitted);
	pointWidget->OnYCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleAxisPointYCommitted);
	pointWidget->OnYCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleAxisPointYCommitted);
	pointWidget->OnAddPointRequested.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleAxisPointAddRequested);
	pointWidget->OnAddPointRequested.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleAxisPointAddRequested);
	pointWidget->OnRemovePointRequested.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleAxisPointRemoveRequested);
	pointWidget->OnRemovePointRequested.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleAxisPointRemoveRequested);
	parentBlockWidget->AddBodyChild(pointWidget);
	return pointWidget;
}

UScenarioEditorSidebarCorridorLaneWidget* UScenarioEditorSidebarCorridorPanel::AddLaneWidget(
	const EScenarioEditorCorridorSide side,
	const int32 laneIndex,
	const FScenarioTemplateLaneRule& lane,
	const TArray<FString>& surfaceOptions,
	UScenarioEditorSidebarBlockWidget* parentBlockWidget)
{
	if (!GetWorld() || !parentBlockWidget)
	{
		return nullptr;
	}

	UScenarioEditorSidebarCorridorLaneWidget* laneWidget =
		CreateWidget<UScenarioEditorSidebarCorridorLaneWidget>(
			GetWorld(),
			UScenarioEditorWidgetClassCatalog::ResolveSidebarCorridorLaneWidgetClass(WidgetClassCatalog));
	if (!laneWidget)
	{
		SetDiagnosticsText(TEXT("Scenario editor corridor lane widget class is missing."));
		return nullptr;
	}

	laneWidget->SetTextStyleCatalog(TextStyleCatalog);
	laneWidget->SetLaneContext(side, laneIndex);
	laneWidget->SetSurfaceOptions(surfaceOptions);
	laneWidget->RefreshFromLane(lane);
	laneWidget->OnSurfaceCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleLaneSurfaceCommitted);
	laneWidget->OnSurfaceCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleLaneSurfaceCommitted);
	laneWidget->OnWidthCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleLaneWidthCommitted);
	laneWidget->OnWidthCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleLaneWidthCommitted);
	laneWidget->OnWidthRangeCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleLaneWidthRangeCommitted);
	laneWidget->OnWidthRangeCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleLaneWidthRangeCommitted);
	laneWidget->OnAddLaneRequested.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleLaneAddRequested);
	laneWidget->OnAddLaneRequested.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleLaneAddRequested);
	laneWidget->OnRemoveLaneRequested.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleLaneRemoveRequested);
	laneWidget->OnRemoveLaneRequested.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleLaneRemoveRequested);
	parentBlockWidget->AddBodyChild(laneWidget);
	return laneWidget;
}

UScenarioEditorSidebarCorridorSegmentWidget* UScenarioEditorSidebarCorridorPanel::AddSegmentWidget(
	const int32 segmentIndex,
	const FScenarioTemplateSegment& segment,
	const TArray<FString>& surfaceOptions,
	UScenarioEditorSidebarBlockWidget* parentBlockWidget)
{
	if (!GetWorld() || !parentBlockWidget)
	{
		return nullptr;
	}

	UScenarioEditorSidebarCorridorSegmentWidget* segmentWidget =
		CreateWidget<UScenarioEditorSidebarCorridorSegmentWidget>(
			GetWorld(),
			UScenarioEditorWidgetClassCatalog::ResolveSidebarCorridorSegmentWidgetClass(WidgetClassCatalog));
	if (!segmentWidget)
	{
		SetDiagnosticsText(TEXT("Scenario editor corridor segment widget class is missing."));
		return nullptr;
	}

	segmentWidget->SetTextStyleCatalog(TextStyleCatalog);
	segmentWidget->SetSegmentIndex(segmentIndex);
	segmentWidget->SetSurfaceOptions(surfaceOptions);
	segmentWidget->RefreshFromSegment(segment);
	segmentWidget->OnIdCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleSegmentIdCommitted);
	segmentWidget->OnIdCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleSegmentIdCommitted);
	segmentWidget->OnTypeCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleSegmentTypeCommitted);
	segmentWidget->OnTypeCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleSegmentTypeCommitted);
	segmentWidget->OnAlongRangeCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleSegmentAlongRangeCommitted);
	segmentWidget->OnAlongRangeCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleSegmentAlongRangeCommitted);
	segmentWidget->OnReplacedByCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleSegmentReplacedByCommitted);
	segmentWidget->OnReplacedByCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleSegmentReplacedByCommitted);
	segmentWidget->OnAddSegmentRequested.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleSegmentAddRequested);
	segmentWidget->OnAddSegmentRequested.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleSegmentAddRequested);
	segmentWidget->OnRemoveSegmentRequested.RemoveDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleSegmentRemoveRequested);
	segmentWidget->OnRemoveSegmentRequested.AddDynamic(
		this,
		&UScenarioEditorSidebarCorridorPanel::HandleSegmentRemoveRequested);
	parentBlockWidget->AddBodyChild(segmentWidget);
	return segmentWidget;
}

UScenarioTemplateSidebarViewModel* UScenarioEditorSidebarCorridorPanel::GetTemplateSidebarViewModel() const
{
	UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this);
	return uiSubsystem ? uiSubsystem->GetTemplateSidebarViewModel() : nullptr;
}

TArray<FString> UScenarioEditorSidebarCorridorPanel::GetCorridorSurfaceIdOptions() const
{
	TArray<FString> surfaceIds;
	if (UScenarioTemplateSidebarViewModel* templateSidebarViewModel = GetTemplateSidebarViewModel())
	{
		templateSidebarViewModel->GetCorridorSurfaceIdOptions(surfaceIds);
	}

	if (!surfaceIds.IsEmpty())
	{
		return surfaceIds;
	}

	const TArray<FScenarioCorridorSurfaceEntry> surfaceEntries =
		UScenarioCorridorSurfaceCatalog::MakeDefaultEntries();
	TSet<FString> seenSurfaceIds;
	for (const FScenarioCorridorSurfaceEntry& surfaceEntry : surfaceEntries)
	{
		if (surfaceEntry.SurfaceId.IsNone())
		{
			continue;
		}

		const FString surfaceId = surfaceEntry.SurfaceId.ToString();
		if (surfaceId.IsEmpty() || seenSurfaceIds.Contains(surfaceId))
		{
			continue;
		}

		seenSurfaceIds.Add(surfaceId);
		surfaceIds.Add(surfaceId);
	}

	return surfaceIds;
}

void UScenarioEditorSidebarCorridorPanel::ExecuteTemplateCommand(
	TFunctionRef<bool(UScenarioTemplateSidebarViewModel*, FString&)> command)
{
	UScenarioTemplateSidebarViewModel* templateSidebarViewModel = GetTemplateSidebarViewModel();
	if (!templateSidebarViewModel)
	{
		SetDiagnosticsText(TEXT("ScenarioTemplateSidebarViewModel unavailable."));
		return;
	}

	FString statusText;
	command(templateSidebarViewModel, statusText);
	RefreshFromDraft();
	SetDiagnosticsText(statusText);
}

void UScenarioEditorSidebarCorridorPanel::SetDiagnosticsText(const FString& text) const
{
	if (DiagnosticsTextBlock)
	{
		DiagnosticsTextBlock->SetText(FText::FromString(text));
	}
}
