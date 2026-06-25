#include "Scenario/Widget/ScenarioEditorSidebarObstaclePanel.h"

#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Scenario/Data/ScenarioEditorWidgetClassCatalog.h"
#include "Scenario/ScenarioEditorUiSubsystem.h"
#include "Scenario/ViewModel/ScenarioEditorShellViewModel.h"
#include "Scenario/ViewModel/ScenarioTemplateFieldRowViewModel.h"
#include "Scenario/ViewModel/ScenarioTemplateSidebarViewModel.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarWidgetHelpers.h"
#include "Scenario/Data/WidgetTextStyleCatalog.h"

namespace SidebarWidgetHelpers = ScenarioEditorSidebarWidgetHelpers;

void UScenarioEditorSidebarObstaclePanel::NativeConstruct()
{
	Super::NativeConstruct();
	if (WidgetClassCatalog.IsNull())
	{
		WidgetClassCatalog = UScenarioEditorWidgetClassCatalog::MakeDefaultCatalogReference();
	}
	SidebarWidgetHelpers::ApplyPanelRootPadding(this, FName(TEXT("ObstaclePanelRootBox")));
	BindFieldRows();
	ConfigureFieldRows();
	RefreshFromDraft();

	TArray<UScenarioEditorSidebarBlockWidget*> blockWidgets;
	CollectBlockWidgets(blockWidgets);
	SidebarWidgetHelpers::ApplyPanelBlockSpacing(blockWidgets);
}

void UScenarioEditorSidebarObstaclePanel::NativeDestruct()
{
	UnbindFieldRows();
	Super::NativeDestruct();
}

void UScenarioEditorSidebarObstaclePanel::SetTextStyleCatalog(
	TSoftObjectPtr<UWidgetTextStyleCatalog> catalog)
{
	TextStyleCatalog = catalog;
	ApplyTextStyles();
}

void UScenarioEditorSidebarObstaclePanel::SetWidgetClassCatalog(
	TSoftObjectPtr<UScenarioEditorWidgetClassCatalog> catalog)
{
	WidgetClassCatalog = catalog.IsNull()
		? UScenarioEditorWidgetClassCatalog::MakeDefaultCatalogReference()
		: catalog;
}

void UScenarioEditorSidebarObstaclePanel::RefreshFromDraft()
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

void UScenarioEditorSidebarObstaclePanel::RefreshFromTemplate(
	const FScenarioDocument& scenarioTemplate)
{
	ConfigureFieldRows();

	UScenarioTemplateSidebarViewModel* templateSidebarViewModel = GetTemplateSidebarViewModel();
	if (!templateSidebarViewModel)
	{
		SetDiagnosticsText(TEXT("ScenarioTemplateSidebarViewModel unavailable."));
		return;
	}

	templateSidebarViewModel->RefreshObstacleFieldItemsFromTemplate(scenarioTemplate);
	ApplyObstacleFieldItems();
	RefreshPlacementRows(scenarioTemplate.Obstacles.Placements);
	ApplySelectedBlockPath();
	SetDiagnosticsText(TEXT(""));
}

void UScenarioEditorSidebarObstaclePanel::CollectBlockWidgets(
	TArray<UScenarioEditorSidebarBlockWidget*>& outBlockWidgets) const
{
	for (UScenarioEditorSidebarBlockWidget* blockWidget : {
		ObstacleBlockWidget.Get(),
		MinClearWidthBlockWidget.Get(),
		PlacementsBlockWidget.Get() })
	{
		if (blockWidget)
		{
			outBlockWidgets.Add(blockWidget);
		}
	}
	for (UScenarioEditorSidebarObstaclePlacementWidget* placementWidget : PlacementWidgets)
	{
		if (placementWidget && placementWidget->PlacementBlockWidget)
		{
			outBlockWidgets.Add(placementWidget->PlacementBlockWidget.Get());
		}
	}
}

UScenarioEditorSidebarBlockWidget* UScenarioEditorSidebarObstaclePanel::FindBlockWidgetByPath(
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

void UScenarioEditorSidebarObstaclePanel::HandleMinClearWidthCommitted(
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
		return viewModel->CommitObstacleMinClearWidthText(text, statusText);
	});
}

void UScenarioEditorSidebarObstaclePanel::HandleMinClearWidthRangeCommitted(
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
		return viewModel->CommitObstacleMinClearWidthRangeText(minText, maxText, statusText);
	});
}

void UScenarioEditorSidebarObstaclePanel::HandlePlacementsCountAddRequested()
{
	ExecuteTemplateCommand([](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->AddObstaclePlacementAfter(INDEX_NONE, statusText);
	});
}

void UScenarioEditorSidebarObstaclePanel::HandlePlacementsCountRemoveRequested()
{
	ExecuteTemplateCommand([](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->RemoveObstaclePlacementAt(INDEX_NONE, statusText);
	});
}

void UScenarioEditorSidebarObstaclePanel::HandlePlacementFieldTextCommitted(
	const int32 placementIndex,
	const EScenarioEditorSidebarObstaclePlacementField field,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	ExecuteTemplateCommand(
		[placementIndex, field, &text](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
		{
			return viewModel->CommitObstaclePlacementText(placementIndex, field, text, statusText);
		});
}

void UScenarioEditorSidebarObstaclePanel::HandlePlacementFieldRangeCommitted(
	const int32 placementIndex,
	const EScenarioEditorSidebarObstaclePlacementField field,
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
		[placementIndex, field, &minText, &maxText](
			UScenarioTemplateSidebarViewModel* viewModel,
			FString& statusText)
		{
			return viewModel->CommitObstaclePlacementRange(
				placementIndex,
				field,
				minText,
				maxText,
				statusText);
		});
}

void UScenarioEditorSidebarObstaclePanel::HandlePlacementAddRequested(const int32 placementIndex)
{
	ExecuteTemplateCommand([placementIndex](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->AddObstaclePlacementAfter(placementIndex, statusText);
	});
}

void UScenarioEditorSidebarObstaclePanel::HandlePlacementRemoveRequested(const int32 placementIndex)
{
	ExecuteTemplateCommand([placementIndex](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->RemoveObstaclePlacementAt(placementIndex, statusText);
	});
}

void UScenarioEditorSidebarObstaclePanel::BindFieldRows()
{
	if (MinClearWidthFieldRow)
	{
		MinClearWidthFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandleMinClearWidthCommitted);
		MinClearWidthFieldRow->OnValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandleMinClearWidthCommitted);
		MinClearWidthFieldRow->OnRangeValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandleMinClearWidthRangeCommitted);
		MinClearWidthFieldRow->OnRangeValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandleMinClearWidthRangeCommitted);
	}
}

void UScenarioEditorSidebarObstaclePanel::UnbindFieldRows()
{
	if (MinClearWidthFieldRow)
	{
		MinClearWidthFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandleMinClearWidthCommitted);
		MinClearWidthFieldRow->OnRangeValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandleMinClearWidthRangeCommitted);
	}
	if (PlacementsCountFieldRow)
	{
		SidebarWidgetHelpers::UnbindFieldRowActions(PlacementsCountFieldRow.Get(), this);
	}

	for (UScenarioEditorSidebarObstaclePlacementWidget* placementWidget : PlacementWidgets)
	{
		if (!placementWidget)
		{
			continue;
		}

		placementWidget->OnFieldTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandlePlacementFieldTextCommitted);
		placementWidget->OnFieldRangeCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandlePlacementFieldRangeCommitted);
		placementWidget->OnAddRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandlePlacementAddRequested);
		placementWidget->OnRemoveRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarObstaclePanel::HandlePlacementRemoveRequested);
	}
}

void UScenarioEditorSidebarObstaclePanel::ConfigureFieldRows()
{
	if (ObstacleBlockWidget)
	{
		SidebarWidgetHelpers::ConfigureBlock(ObstacleBlockWidget.Get(), TextStyleCatalog, {
			TEXT("장애물"),
			TEXT("root.obstacles"),
			TEXT("구성"),
			true,
			false,
			true });
	}
	if (MinClearWidthBlockWidget)
	{
		SidebarWidgetHelpers::ConfigureBlock(MinClearWidthBlockWidget.Get(), TextStyleCatalog, {
			TEXT("최소 통행 폭"),
			TEXT("root.obstacles.min_clear_width_m"),
			TEXT("속성"),
			false,
			true,
			false });
	}
	if (PlacementsBlockWidget)
	{
		SidebarWidgetHelpers::ConfigureBlock(PlacementsBlockWidget.Get(), TextStyleCatalog, {
			TEXT("배치된 장애물"),
			TEXT("root.obstacles.placements[]"),
			TEXT("속성"),
			false,
			true,
			false });
	}

	if (MinClearWidthFieldRow)
	{
		MinClearWidthFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	ApplyObstacleFieldItems();
}

void UScenarioEditorSidebarObstaclePanel::ApplyTextStyles()
{
	for (UScenarioEditorSidebarBlockWidget* blockWidget : {
		ObstacleBlockWidget.Get(),
		MinClearWidthBlockWidget.Get(),
		PlacementsBlockWidget.Get() })
	{
		if (blockWidget)
		{
			blockWidget->SetTextStyleCatalog(TextStyleCatalog);
		}
	}
	if (MinClearWidthFieldRow)
	{
		MinClearWidthFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (PlacementsCountFieldRow)
	{
		PlacementsCountFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	for (UScenarioEditorSidebarObstaclePlacementWidget* placementWidget : PlacementWidgets)
	{
		if (placementWidget)
		{
			placementWidget->SetTextStyleCatalog(TextStyleCatalog);
		}
	}

	if (DiagnosticsTextBlock)
	{
		DiagnosticsTextBlock->SetVisibility(DiagnosticsTextBlock->GetText().IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}
}

void UScenarioEditorSidebarObstaclePanel::ApplyObstacleFieldItems()
{
	UScenarioTemplateSidebarViewModel* templateSidebarViewModel = GetTemplateSidebarViewModel();
	if (!templateSidebarViewModel)
	{
		return;
	}

	if (MinClearWidthFieldRow)
	{
		MinClearWidthFieldRow->InitializeFromItemViewModel(
			templateSidebarViewModel->FindObstacleFieldItem(TEXT("MinClearWidth")));
	}
	if (PlacementsCountFieldRow)
	{
		PlacementsCountFieldRow->InitializeFromItemViewModel(
			templateSidebarViewModel->FindObstacleFieldItem(TEXT("PlacementsCount")));
	}
}

void UScenarioEditorSidebarObstaclePanel::ApplySelectedBlockPath()
{
	UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this);
	const UScenarioEditorShellViewModel* shellViewModel = uiSubsystem ? uiSubsystem->GetShellViewModel() : nullptr;
	const FString selectedBlockPath = shellViewModel ? shellViewModel->GetSelectedTemplateBlockPath() : FString();

	SidebarWidgetHelpers::ApplySelectedBlockPath(ObstacleBlockWidget.Get(), selectedBlockPath);
	SidebarWidgetHelpers::ApplySelectedBlockPath(MinClearWidthBlockWidget.Get(), selectedBlockPath);
	SidebarWidgetHelpers::ApplySelectedBlockPath(PlacementsBlockWidget.Get(), selectedBlockPath);

	const bool bSelectedPlacementItem = selectedBlockPath.StartsWith(TEXT("root.obstacles.placements["));
	if (PlacementsBlockWidget && bSelectedPlacementItem)
	{
		PlacementsBlockWidget->SetExpanded(true);
	}

	for (UScenarioEditorSidebarObstaclePlacementWidget* placementWidget : PlacementWidgets)
	{
		if (placementWidget)
		{
			SidebarWidgetHelpers::ApplySelectedBlockPath(
				placementWidget->PlacementBlockWidget.Get(),
				selectedBlockPath);
		}
	}
}

void UScenarioEditorSidebarObstaclePanel::RefreshPlacementRows(
	const TArray<FScenarioTemplateObstaclePlacement>& placements)
{
	if (!PlacementsBlockWidget)
	{
		return;
	}

	PlacementWidgets.Reset();
	PlacementsBlockWidget->ClearBodyChildren();
	PlacementsCountFieldRow = AddFieldRow(
		PlacementsBlockWidget.Get(),
		GetTemplateSidebarViewModel()
			? GetTemplateSidebarViewModel()->FindObstacleFieldItem(TEXT("PlacementsCount"))
			: nullptr);
	if (PlacementsCountFieldRow)
	{
		SidebarWidgetHelpers::BindFieldRowActions(
			PlacementsCountFieldRow.Get(),
			this,
			GET_FUNCTION_NAME_CHECKED(
				UScenarioEditorSidebarObstaclePanel,
				HandlePlacementsCountAddRequested),
			GET_FUNCTION_NAME_CHECKED(
				UScenarioEditorSidebarObstaclePanel,
				HandlePlacementsCountRemoveRequested));
	}

	for (int32 placementIndex = 0; placementIndex < placements.Num(); ++placementIndex)
	{
		if (UScenarioEditorSidebarObstaclePlacementWidget* placementWidget =
			AddPlacementWidget(placementIndex, placements[placementIndex], PlacementsBlockWidget.Get()))
		{
			PlacementWidgets.Add(placementWidget);
		}
	}
}

UScenarioEditorSidebarFieldRow* UScenarioEditorSidebarObstaclePanel::AddFieldRow(
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

UScenarioEditorSidebarObstaclePlacementWidget* UScenarioEditorSidebarObstaclePanel::AddPlacementWidget(
	const int32 placementIndex,
	const FScenarioTemplateObstaclePlacement& placement,
	UScenarioEditorSidebarBlockWidget* parentBlockWidget)
{
	if (!GetWorld() || !parentBlockWidget)
	{
		return nullptr;
	}

	UScenarioEditorSidebarObstaclePlacementWidget* placementWidget =
		CreateWidget<UScenarioEditorSidebarObstaclePlacementWidget>(
			GetWorld(),
			UScenarioEditorWidgetClassCatalog::ResolveSidebarObstaclePlacementWidgetClass(WidgetClassCatalog));
	if (!placementWidget)
	{
		SetDiagnosticsText(TEXT("Scenario editor obstacle placement widget class is missing."));
		return nullptr;
	}

	placementWidget->SetTextStyleCatalog(TextStyleCatalog);
	placementWidget->SetPlacementIndex(placementIndex);
	placementWidget->RefreshFromPlacement(placement);
	placementWidget->OnFieldTextCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementFieldTextCommitted);
	placementWidget->OnFieldTextCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementFieldTextCommitted);
	placementWidget->OnFieldRangeCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementFieldRangeCommitted);
	placementWidget->OnFieldRangeCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementFieldRangeCommitted);
	placementWidget->OnAddRequested.RemoveDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementAddRequested);
	placementWidget->OnAddRequested.AddDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementAddRequested);
	placementWidget->OnRemoveRequested.RemoveDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementRemoveRequested);
	placementWidget->OnRemoveRequested.AddDynamic(
		this,
		&UScenarioEditorSidebarObstaclePanel::HandlePlacementRemoveRequested);
	parentBlockWidget->AddBodyChild(placementWidget);
	return placementWidget;
}

UScenarioTemplateSidebarViewModel* UScenarioEditorSidebarObstaclePanel::GetTemplateSidebarViewModel() const
{
	UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this);
	return uiSubsystem ? uiSubsystem->GetTemplateSidebarViewModel() : nullptr;
}

void UScenarioEditorSidebarObstaclePanel::ExecuteTemplateCommand(
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

void UScenarioEditorSidebarObstaclePanel::SetDiagnosticsText(const FString& text) const
{
	if (DiagnosticsTextBlock)
	{
		DiagnosticsTextBlock->SetText(FText::FromString(text));
	}
}
