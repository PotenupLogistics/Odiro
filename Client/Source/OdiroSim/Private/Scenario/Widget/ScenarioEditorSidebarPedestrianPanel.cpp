#include "Scenario/Widget/ScenarioEditorSidebarPedestrianPanel.h"

#include "Components/TextBlock.h"
#include "Components/PanelSlot.h"
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

void UScenarioEditorSidebarPedestrianPanel::NativeConstruct()
{
	Super::NativeConstruct();
	if (WidgetClassCatalog.IsNull())
	{
		WidgetClassCatalog = UScenarioEditorWidgetClassCatalog::MakeDefaultCatalogReference();
	}
	SidebarWidgetHelpers::ApplyPanelRootPadding(this, FName(TEXT("PedestrianPanelRootBox")));
	BindControls();
	ConfigureFieldRows();
	RefreshFromDraft();

	TArray<UScenarioEditorSidebarBlockWidget*> blockWidgets;
	CollectBlockWidgets(blockWidgets);
	SidebarWidgetHelpers::ApplyPanelBlockSpacing(blockWidgets);
}

void UScenarioEditorSidebarPedestrianPanel::NativeDestruct()
{
	UnbindControls();
	Super::NativeDestruct();
}

void UScenarioEditorSidebarPedestrianPanel::SetTextStyleCatalog(
	TSoftObjectPtr<UWidgetTextStyleCatalog> catalog)
{
	TextStyleCatalog = catalog;
	ApplyTextStyles();
}

void UScenarioEditorSidebarPedestrianPanel::SetWidgetClassCatalog(
	TSoftObjectPtr<UScenarioEditorWidgetClassCatalog> catalog)
{
	WidgetClassCatalog = catalog.IsNull()
		? UScenarioEditorWidgetClassCatalog::MakeDefaultCatalogReference()
		: catalog;
}

void UScenarioEditorSidebarPedestrianPanel::RefreshFromDraft()
{
	UScenarioTemplateSidebarViewModel* templateSidebarViewModel = GetTemplateSidebarViewModel();
	FScenarioDocument scenarioTemplate;
	FString failureReason;
	if (!templateSidebarViewModel || !templateSidebarViewModel->TryGetDraftScenario(scenarioTemplate, failureReason))
	{
		SetDiagnosticsText(
			failureReason.IsEmpty() ? TEXT("ScenarioTemplateSidebarViewModel unavailable.") : failureReason);
		return;
	}

	RefreshFromTemplate(scenarioTemplate);
}

void UScenarioEditorSidebarPedestrianPanel::RefreshFromTemplate(
	const FScenarioDocument& scenarioTemplate)
{
	ConfigureFieldRows();

	UScenarioTemplateSidebarViewModel* templateSidebarViewModel = GetTemplateSidebarViewModel();
	if (!templateSidebarViewModel)
	{
		SetDiagnosticsText(TEXT("ScenarioTemplateSidebarViewModel unavailable."));
		return;
	}

	templateSidebarViewModel->RefreshPedestrianFieldItemsFromTemplate(scenarioTemplate);
	ApplyPedestrianFieldItems();
	RefreshSpawnSegmentRows(
		scenarioTemplate.Pedestrians.Background.SpawnSegmentIds,
		scenarioTemplate.Corridor.Segments);
	RefreshEncounterRows(scenarioTemplate.Pedestrians.Encounters);
	SetDiagnosticsText(TEXT(""));
	ApplySelectedBlockPath();
}

void UScenarioEditorSidebarPedestrianPanel::HandleEncounterCollectionAddRequested()
{
	ExecuteTemplateCommand([](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->AddPedestrianEncounterAfter(INDEX_NONE, statusText);
	}, true);
}

void UScenarioEditorSidebarPedestrianPanel::HandleEncounterRemoveRequested(const int32 encounterIndex)
{
	ExecuteTemplateCommand([encounterIndex](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->RemovePedestrianEncounterAt(encounterIndex, statusText);
	}, true);
}

void UScenarioEditorSidebarPedestrianPanel::ApplySelectedBlockPath()
{
	UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this);
	const UScenarioEditorShellViewModel* shellViewModel = uiSubsystem ? uiSubsystem->GetShellViewModel() : nullptr;
	const FString selectedBlockPath = shellViewModel ? shellViewModel->GetSelectedTemplateBlockPath() : FString();

	SidebarWidgetHelpers::ApplySelectedBlockPath(PedestriansBlockWidget.Get(), selectedBlockPath);
	SidebarWidgetHelpers::ApplySelectedBlockPath(BackgroundBlockWidget.Get(), selectedBlockPath);
	SidebarWidgetHelpers::ApplySelectedBlockPath(SpawnZoneBlockWidget.Get(), selectedBlockPath);
	SidebarWidgetHelpers::ApplySelectedBlockPath(EncountersBlockWidget.Get(), selectedBlockPath);

	if (EncountersBlockWidget && selectedBlockPath.StartsWith(TEXT("root.pedestrians.encounters[")))
	{
		EncountersBlockWidget->SetExpanded(true);
	}

	for (UScenarioEditorSidebarPedestrianEncounterWidget* encounterWidget : EncounterWidgets)
	{
		if (encounterWidget)
		{
			SidebarWidgetHelpers::ApplySelectedBlockPath(encounterWidget->EncounterBlockWidget.Get(), selectedBlockPath);
		}
	}
	ApplyFocusedEncounterDetailLayout(selectedBlockPath);
}

void UScenarioEditorSidebarPedestrianPanel::CollectBlockWidgets(
	TArray<UScenarioEditorSidebarBlockWidget*>& outBlockWidgets) const
{
	for (UScenarioEditorSidebarBlockWidget* blockWidget : {
		PedestriansBlockWidget.Get(),
		BackgroundBlockWidget.Get(),
		SpawnZoneBlockWidget.Get(),
		EncountersBlockWidget.Get() })
	{
		if (blockWidget)
		{
			outBlockWidgets.Add(blockWidget);
		}
	}
	for (UScenarioEditorSidebarPedestrianEncounterWidget* encounterWidget : EncounterWidgets)
	{
		if (encounterWidget && encounterWidget->EncounterBlockWidget)
		{
			outBlockWidgets.Add(encounterWidget->EncounterBlockWidget.Get());
		}
	}
}

UScenarioEditorSidebarBlockWidget* UScenarioEditorSidebarPedestrianPanel::FindBlockWidgetByPath(
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

void UScenarioEditorSidebarPedestrianPanel::ConfigureFieldRows()
{
	if (PedestriansBlockWidget)
	{
		PedestriansBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		PedestriansBlockWidget->SetBlockMetadata(TEXT("보행자"), TEXT("root.pedestrians"), TEXT("구성"));
		PedestriansBlockWidget->SetSelected(true);
	}
	if (BackgroundBlockWidget)
	{
		BackgroundBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		BackgroundBlockWidget->SetBlockMetadata(TEXT("배경 보행자"), TEXT("root.pedestrians.background"), TEXT("속성"));
		BackgroundBlockWidget->SetNested(true);
		BackgroundBlockWidget->SetShowNormalOutline(false);
	}
	if (SpawnZoneBlockWidget)
	{
		SpawnZoneBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		SpawnZoneBlockWidget->SetBlockMetadata(
			TEXT("스폰 구역"),
			TEXT("root.pedestrians.background.spawn_zone"),
			TEXT("세부"));
		SpawnZoneBlockWidget->SetNested(true);
		SpawnZoneBlockWidget->SetShowNormalOutline(false);
		SpawnZoneBlockWidget->SetAddActionVisible(true);
		SpawnZoneBlockWidget->SetRemoveActionVisible(false);
	}
	if (EncountersBlockWidget)
	{
		EncountersBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		EncountersBlockWidget->SetBlockMetadata(TEXT("상호작용 상황"), TEXT("root.pedestrians.encounters[]"), TEXT("속성"));
		EncountersBlockWidget->SetNested(true);
		EncountersBlockWidget->SetShowNormalOutline(false);
		EncountersBlockWidget->SetAddActionVisible(true);
		EncountersBlockWidget->SetRemoveActionVisible(false);
	}
	if (BackgroundCountFieldRow)
	{
		BackgroundCountFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (BackgroundSpeedFieldRow)
	{
		BackgroundSpeedFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (SpawnSegmentsFieldRow)
	{
		SpawnSegmentsFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		SpawnSegmentsFieldRow->SetEditable(false);
		SpawnSegmentsFieldRow->SetArrayControlsEnabled(false);
		SpawnSegmentsFieldRow->SetAddItemControlVisible(false);
		SpawnSegmentsFieldRow->SetRemoveItemControlVisible(false);
	}
	ApplyPedestrianFieldItems();
}

void UScenarioEditorSidebarPedestrianPanel::ApplyTextStyles()
{
	for (UScenarioEditorSidebarBlockWidget* blockWidget : {
		PedestriansBlockWidget.Get(),
		BackgroundBlockWidget.Get(),
		SpawnZoneBlockWidget.Get(),
		EncountersBlockWidget.Get() })
	{
		if (blockWidget)
		{
			blockWidget->SetTextStyleCatalog(TextStyleCatalog);
		}
	}
	for (UScenarioEditorSidebarFieldRow* fieldRow : {
		BackgroundCountFieldRow.Get(),
		BackgroundSpeedFieldRow.Get(),
		SpawnSegmentsFieldRow.Get(),
		EncountersCountFieldRow.Get() })
	{
		if (fieldRow)
		{
			fieldRow->SetTextStyleCatalog(TextStyleCatalog);
		}
	}
	for (UScenarioEditorSidebarFieldRow* fieldRow : SpawnSegmentItemRows)
	{
		if (fieldRow)
		{
			fieldRow->SetTextStyleCatalog(TextStyleCatalog);
		}
	}
	for (UScenarioEditorSidebarPedestrianEncounterWidget* encounterWidget : EncounterWidgets)
	{
		if (encounterWidget)
		{
			encounterWidget->SetTextStyleCatalog(TextStyleCatalog);
		}
	}

	if (DiagnosticsTextBlock)
	{
		DiagnosticsTextBlock->SetVisibility(DiagnosticsTextBlock->GetText().IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}
}

void UScenarioEditorSidebarPedestrianPanel::ApplyPedestrianFieldItems()
{
	UScenarioTemplateSidebarViewModel* templateSidebarViewModel = GetTemplateSidebarViewModel();
	if (!templateSidebarViewModel)
	{
		return;
	}

	if (BackgroundCountFieldRow)
	{
		BackgroundCountFieldRow->InitializeFromItemViewModel(
			templateSidebarViewModel->FindPedestrianFieldItem(TEXT("BackgroundCount")));
	}
	if (BackgroundSpeedFieldRow)
	{
		BackgroundSpeedFieldRow->InitializeFromItemViewModel(
			templateSidebarViewModel->FindPedestrianFieldItem(TEXT("BackgroundSpeed")));
	}
	if (SpawnSegmentsFieldRow)
	{
		SpawnSegmentsFieldRow->InitializeFromItemViewModel(
			templateSidebarViewModel->FindPedestrianFieldItem(TEXT("SpawnSegments")));
		SpawnSegmentsFieldRow->SetEditable(false);
		SpawnSegmentsFieldRow->SetArrayControlsEnabled(false);
		SpawnSegmentsFieldRow->SetAddItemControlVisible(false);
		SpawnSegmentsFieldRow->SetRemoveItemControlVisible(false);
	}
	if (EncountersCountFieldRow)
	{
		EncountersCountFieldRow->InitializeFromItemViewModel(
			templateSidebarViewModel->FindPedestrianFieldItem(TEXT("EncountersCount")));
	}
}

void UScenarioEditorSidebarPedestrianPanel::ApplyFocusedEncounterDetailLayout(
	const FString& selectedBlockPath)
{
	const bool bFocusEncounter = selectedBlockPath.StartsWith(TEXT("root.pedestrians.encounters["));

	if (PedestriansBlockWidget)
	{
		PedestriansBlockWidget->SetVisibility(ESlateVisibility::Visible);
		PedestriansBlockWidget->SetDetailHostLayout(bFocusEncounter);
		if (bFocusEncounter)
		{
			PedestriansBlockWidget->SetExpanded(true);
		}
	}
	if (BackgroundBlockWidget)
	{
		BackgroundBlockWidget->SetVisibility(bFocusEncounter
			? ESlateVisibility::Collapsed
			: ESlateVisibility::Visible);
		BackgroundBlockWidget->SetDetailHostLayout(false);
	}
	if (SpawnZoneBlockWidget)
	{
		SpawnZoneBlockWidget->SetVisibility(bFocusEncounter
			? ESlateVisibility::Collapsed
			: ESlateVisibility::Visible);
		SpawnZoneBlockWidget->SetDetailHostLayout(false);
	}
	if (EncountersBlockWidget)
	{
		EncountersBlockWidget->SetVisibility(ESlateVisibility::Visible);
		EncountersBlockWidget->SetDetailHostLayout(bFocusEncounter);
		if (bFocusEncounter)
		{
			EncountersBlockWidget->SetExpanded(true);
		}
	}
	if (EncountersCountFieldRow)
	{
		if (bFocusEncounter)
		{
			EncountersCountFieldRow->SetVisibility(ESlateVisibility::Collapsed);
		}
		else if (UScenarioTemplateSidebarViewModel* templateSidebarViewModel = GetTemplateSidebarViewModel())
		{
			EncountersCountFieldRow->InitializeFromItemViewModel(
				templateSidebarViewModel->FindPedestrianFieldItem(TEXT("EncountersCount")));
		}
	}

	for (UScenarioEditorSidebarPedestrianEncounterWidget* encounterWidget : EncounterWidgets)
	{
		if (!encounterWidget || !encounterWidget->EncounterBlockWidget)
		{
			continue;
		}

		const bool bSelectedEncounter = encounterWidget->EncounterBlockWidget->BlockPath == selectedBlockPath;
		encounterWidget->SetVisibility(!bFocusEncounter || bSelectedEncounter
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
		encounterWidget->EncounterBlockWidget->SetFocusedDetailLayout(bFocusEncounter && bSelectedEncounter);
		if (bFocusEncounter && bSelectedEncounter)
		{
			encounterWidget->EncounterBlockWidget->SetExpanded(true);
		}
	}
}

void UScenarioEditorSidebarPedestrianPanel::BindControls()
{
	if (EncountersBlockWidget)
	{
		EncountersBlockWidget->OnAddActionRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarPedestrianPanel::HandleEncounterCollectionAddRequested);
		EncountersBlockWidget->OnAddActionRequested.AddDynamic(
			this,
			&UScenarioEditorSidebarPedestrianPanel::HandleEncounterCollectionAddRequested);
	}
	if (SpawnZoneBlockWidget)
	{
		SpawnZoneBlockWidget->OnAddActionRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarPedestrianPanel::HandleSpawnSegmentAddRequested);
		SpawnZoneBlockWidget->OnAddActionRequested.AddDynamic(
			this,
			&UScenarioEditorSidebarPedestrianPanel::HandleSpawnSegmentAddRequested);
	}
}

void UScenarioEditorSidebarPedestrianPanel::UnbindControls()
{
	if (EncountersBlockWidget)
	{
		EncountersBlockWidget->OnAddActionRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarPedestrianPanel::HandleEncounterCollectionAddRequested);
	}
	if (SpawnZoneBlockWidget)
	{
		SpawnZoneBlockWidget->OnAddActionRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarPedestrianPanel::HandleSpawnSegmentAddRequested);
	}
	for (UScenarioEditorSidebarFieldRow* fieldRow : SpawnSegmentItemRows)
	{
		if (fieldRow)
		{
			fieldRow->OnIndexedValueTextCommitted.RemoveDynamic(
				this,
				&UScenarioEditorSidebarPedestrianPanel::HandleSpawnSegmentTextCommitted);
			fieldRow->OnIndexedRemoveItemRequested.RemoveDynamic(
				this,
				&UScenarioEditorSidebarPedestrianPanel::HandleSpawnSegmentRemoveRequested);
		}
	}
	for (UScenarioEditorSidebarPedestrianEncounterWidget* encounterWidget : EncounterWidgets)
	{
		if (encounterWidget)
		{
			encounterWidget->OnRemoveRequested.RemoveDynamic(
				this,
				&UScenarioEditorSidebarPedestrianPanel::HandleEncounterRemoveRequested);
		}
	}
}

void UScenarioEditorSidebarPedestrianPanel::HandleSpawnSegmentAddRequested()
{
	ExecuteTemplateCommand([](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->AddPedestrianSpawnSegmentAfter(INDEX_NONE, statusText);
	});
}

void UScenarioEditorSidebarPedestrianPanel::HandleSpawnSegmentTextCommitted(
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
		return viewModel->CommitPedestrianSpawnSegmentText(segmentIndex, text, statusText);
	});
}

void UScenarioEditorSidebarPedestrianPanel::HandleSpawnSegmentRemoveRequested(const int32 segmentIndex)
{
	ExecuteTemplateCommand([segmentIndex](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
	{
		return viewModel->RemovePedestrianSpawnSegmentAt(segmentIndex, statusText);
	});
}

void UScenarioEditorSidebarPedestrianPanel::RefreshEncounterRows(
	const TArray<FScenarioTemplatePedestrianEncounter>& encounters)
{
	if (!EncountersBlockWidget)
	{
		return;
	}

	for (UScenarioEditorSidebarPedestrianEncounterWidget* encounterWidget : EncounterWidgets)
	{
		if (encounterWidget)
		{
			encounterWidget->OnRemoveRequested.RemoveDynamic(
				this,
				&UScenarioEditorSidebarPedestrianPanel::HandleEncounterRemoveRequested);
		}
	}
	EncounterWidgets.Reset();
	EncountersBlockWidget->ClearBodyChildren();
	EncountersCountFieldRow = AddFieldRow(
		EncountersBlockWidget.Get(),
		GetTemplateSidebarViewModel()
			? GetTemplateSidebarViewModel()->FindPedestrianFieldItem(TEXT("EncountersCount"))
			: nullptr);

	for (int32 encounterIndex = 0; encounterIndex < encounters.Num(); ++encounterIndex)
	{
		if (UScenarioEditorSidebarPedestrianEncounterWidget* encounterWidget =
			AddEncounterWidget(encounterIndex, encounters[encounterIndex], EncountersBlockWidget.Get()))
		{
			EncounterWidgets.Add(encounterWidget);
		}
	}
}

void UScenarioEditorSidebarPedestrianPanel::RefreshSpawnSegmentRows(
	const TArray<FString>& spawnSegmentIds,
	const TArray<FScenarioTemplateSegment>& corridorSegments)
{
	for (UScenarioEditorSidebarFieldRow* fieldRow : SpawnSegmentItemRows)
	{
		if (!fieldRow)
		{
			continue;
		}

		fieldRow->OnIndexedValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarPedestrianPanel::HandleSpawnSegmentTextCommitted);
		fieldRow->OnIndexedRemoveItemRequested.RemoveDynamic(
			this,
			&UScenarioEditorSidebarPedestrianPanel::HandleSpawnSegmentRemoveRequested);
		fieldRow->RemoveFromParent();
	}
	SpawnSegmentItemRows.Reset();

	if (!SpawnZoneBlockWidget || !SpawnSegmentsFieldRow)
	{
		return;
	}
	if (SpawnSegmentsFieldRow->GetVisibility() == ESlateVisibility::Collapsed)
	{
		return;
	}

	TArray<FString> segmentOptions;
	segmentOptions.Reserve(corridorSegments.Num());
	for (const FScenarioTemplateSegment& segment : corridorSegments)
	{
		if (!segment.SegmentId.IsEmpty())
		{
			segmentOptions.AddUnique(segment.SegmentId);
		}
	}

	SpawnSegmentsFieldRow->SetValueText(FString::FromInt(spawnSegmentIds.Num()));
	SpawnSegmentsFieldRow->SetEditable(false);
	SpawnSegmentsFieldRow->SetArrayControlsEnabled(false);
	SpawnSegmentsFieldRow->SetAddItemControlVisible(false);
	SpawnSegmentsFieldRow->SetRemoveItemControlVisible(false);

	for (int32 segmentIndex = 0; segmentIndex < spawnSegmentIds.Num(); ++segmentIndex)
	{
		if (UScenarioEditorSidebarFieldRow* fieldRow =
			AddSpawnSegmentItemRow(segmentIndex, spawnSegmentIds[segmentIndex], segmentOptions))
		{
			SpawnSegmentItemRows.Add(fieldRow);
		}
	}
}

UScenarioEditorSidebarFieldRow* UScenarioEditorSidebarPedestrianPanel::AddSpawnSegmentItemRow(
	const int32 segmentIndex,
	const FString& segmentId,
	const TArray<FString>& segmentOptions)
{
	if (!GetWorld() || !SpawnZoneBlockWidget)
	{
		return nullptr;
	}

	UScenarioEditorSidebarFieldRow* fieldRow =
		CreateWidget<UScenarioEditorSidebarFieldRow>(
			GetWorld(),
			UScenarioEditorWidgetClassCatalog::ResolveSidebarFieldRowWidgetClass(WidgetClassCatalog));
	if (!fieldRow)
	{
		SetDiagnosticsText(TEXT("Scenario editor field row widget class is missing."));
		return nullptr;
	}

	fieldRow->SetTextStyleCatalog(TextStyleCatalog);
	fieldRow->SetFieldLabel(FString::Printf(TEXT("구간 %d"), segmentIndex + 1));
	fieldRow->SetValueText(segmentId);
	fieldRow->SetInputType(segmentOptions.IsEmpty()
		? EScenarioEditorSidebarFieldInputType::Text
		: EScenarioEditorSidebarFieldInputType::ComboBox);
	fieldRow->SetComboOptions(segmentOptions);
	fieldRow->SetComboAllowsUnset(false, FString());
	fieldRow->SetEditable(true);
	fieldRow->SetArrayControlsEnabled(false);
	fieldRow->SetAddItemControlVisible(false);
	fieldRow->SetRemoveItemControlVisible(true);
	fieldRow->SetActionContextIndex(segmentIndex);
	fieldRow->OnIndexedValueTextCommitted.RemoveDynamic(
		this,
		&UScenarioEditorSidebarPedestrianPanel::HandleSpawnSegmentTextCommitted);
	fieldRow->OnIndexedValueTextCommitted.AddDynamic(
		this,
		&UScenarioEditorSidebarPedestrianPanel::HandleSpawnSegmentTextCommitted);
	fieldRow->OnIndexedRemoveItemRequested.RemoveDynamic(
		this,
		&UScenarioEditorSidebarPedestrianPanel::HandleSpawnSegmentRemoveRequested);
	fieldRow->OnIndexedRemoveItemRequested.AddDynamic(
		this,
		&UScenarioEditorSidebarPedestrianPanel::HandleSpawnSegmentRemoveRequested);

	if (UVerticalBox* bodyBox = SpawnZoneBlockWidget->GetBodyBox())
	{
		int32 anchorIndex = INDEX_NONE;
		for (int32 childIndex = 0; childIndex < bodyBox->GetChildrenCount(); ++childIndex)
		{
			if (bodyBox->GetChildAt(childIndex) == SpawnSegmentsFieldRow.Get())
			{
				anchorIndex = childIndex;
				break;
			}
		}

		UPanelSlot* insertedSlot = anchorIndex == INDEX_NONE
			? bodyBox->AddChild(fieldRow)
			: bodyBox->InsertChildAt(anchorIndex + 1 + segmentIndex, fieldRow);
		if (UVerticalBoxSlot* verticalSlot = Cast<UVerticalBoxSlot>(insertedSlot))
		{
			verticalSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
			verticalSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}
	return fieldRow;
}

UScenarioEditorSidebarFieldRow* UScenarioEditorSidebarPedestrianPanel::AddFieldRow(
	UScenarioEditorSidebarBlockWidget* parentBlockWidget,
	UScenarioTemplateFieldRowViewModel* fieldItemViewModel) const
{
	if (!GetWorld() || !parentBlockWidget)
	{
		return nullptr;
	}

	UScenarioEditorSidebarFieldRow* fieldRow =
		CreateWidget<UScenarioEditorSidebarFieldRow>(
			GetWorld(),
			UScenarioEditorWidgetClassCatalog::ResolveSidebarFieldRowWidgetClass(WidgetClassCatalog));
	if (!fieldRow)
	{
		if (DiagnosticsTextBlock)
		{
			DiagnosticsTextBlock->SetText(FText::FromString(TEXT("Scenario editor field row widget class is missing.")));
		}
		return nullptr;
	}

	fieldRow->SetTextStyleCatalog(TextStyleCatalog);
	fieldRow->InitializeFromItemViewModel(fieldItemViewModel);
	parentBlockWidget->AddBodyChild(fieldRow);
	return fieldRow;
}

UScenarioEditorSidebarPedestrianEncounterWidget* UScenarioEditorSidebarPedestrianPanel::AddEncounterWidget(
	const int32 encounterIndex,
	const FScenarioTemplatePedestrianEncounter& encounter,
	UScenarioEditorSidebarBlockWidget* parentBlockWidget)
{
	if (!GetWorld() || !parentBlockWidget)
	{
		return nullptr;
	}

	UScenarioEditorSidebarPedestrianEncounterWidget* encounterWidget =
		CreateWidget<UScenarioEditorSidebarPedestrianEncounterWidget>(
			GetWorld(),
			UScenarioEditorWidgetClassCatalog::ResolveSidebarPedestrianEncounterWidgetClass(WidgetClassCatalog));
	if (!encounterWidget)
	{
		if (DiagnosticsTextBlock)
		{
			DiagnosticsTextBlock->SetText(FText::FromString(TEXT("Scenario editor pedestrian encounter widget class is missing.")));
		}
		return nullptr;
	}

	encounterWidget->SetTextStyleCatalog(TextStyleCatalog);
	encounterWidget->SetEncounterIndex(encounterIndex);
	encounterWidget->RefreshFromEncounter(encounter);
	encounterWidget->OnRemoveRequested.RemoveDynamic(
		this,
		&UScenarioEditorSidebarPedestrianPanel::HandleEncounterRemoveRequested);
	encounterWidget->OnRemoveRequested.AddDynamic(
		this,
		&UScenarioEditorSidebarPedestrianPanel::HandleEncounterRemoveRequested);
	parentBlockWidget->AddBodyChild(encounterWidget);
	return encounterWidget;
}

UScenarioTemplateSidebarViewModel* UScenarioEditorSidebarPedestrianPanel::GetTemplateSidebarViewModel() const
{
	UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this);
	return uiSubsystem ? uiSubsystem->GetTemplateSidebarViewModel() : nullptr;
}

void UScenarioEditorSidebarPedestrianPanel::ExecuteTemplateCommand(
	TFunctionRef<bool(UScenarioTemplateSidebarViewModel*, FString&)> command,
	const bool bRefreshInspectorOnSuccess)
{
	UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this);
	UScenarioTemplateSidebarViewModel* templateSidebarViewModel = uiSubsystem
		? uiSubsystem->GetTemplateSidebarViewModel()
		: nullptr;
	if (!templateSidebarViewModel)
	{
		SetDiagnosticsText(TEXT("ScenarioTemplateSidebarViewModel unavailable."));
		return;
	}

	FString statusText;
	const bool bCommandSucceeded = command(templateSidebarViewModel, statusText);
	if (bCommandSucceeded && bRefreshInspectorOnSuccess && uiSubsystem)
	{
		uiSubsystem->RefreshEditorRootInspector();
	}
	else
	{
		RefreshFromDraft();
	}
	SetDiagnosticsText(statusText);
}

void UScenarioEditorSidebarPedestrianPanel::SetDiagnosticsText(const FString& text) const
{
	if (DiagnosticsTextBlock)
	{
		DiagnosticsTextBlock->SetText(FText::FromString(text));
	}
}
