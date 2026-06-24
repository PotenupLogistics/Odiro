#include "Scenario/Widget/ScenarioEditorSidebarMainPanel.h"

#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "Scenario/ScenarioEditorUiSubsystem.h"
#include "Scenario/ViewModel/ScenarioTemplateFieldRowViewModel.h"
#include "Scenario/ViewModel/ScenarioTemplateSidebarViewModel.h"
#include "Scenario/Data/ScenarioEditorWidgetClassCatalog.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"
#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"
#include "Scenario/Data/WidgetTextStyleCatalog.h"

void UScenarioEditorSidebarMainPanel::NativeConstruct()
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

void UScenarioEditorSidebarMainPanel::NativeDestruct()
{
	UnbindFieldRows();
	Super::NativeDestruct();
}

void UScenarioEditorSidebarMainPanel::SetTextStyleCatalog(
	TSoftObjectPtr<UWidgetTextStyleCatalog> catalog)
{
	TextStyleCatalog = catalog;
	ApplyTextStyles();
}

void UScenarioEditorSidebarMainPanel::SetWidgetClassCatalog(
	TSoftObjectPtr<UScenarioEditorWidgetClassCatalog> catalog)
{
	WidgetClassCatalog = catalog.IsNull()
		? UScenarioEditorWidgetClassCatalog::MakeDefaultCatalogReference()
		: catalog;
}

void UScenarioEditorSidebarMainPanel::RefreshFromDraft()
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

void UScenarioEditorSidebarMainPanel::RefreshFromTemplate(const FScenarioDocument& scenarioTemplate)
{
	ConfigureFieldRows();

	UScenarioTemplateSidebarViewModel* templateSidebarViewModel = GetTemplateSidebarViewModel();
	if (!templateSidebarViewModel)
	{
		SetDiagnosticsText(TEXT("ScenarioTemplateSidebarViewModel unavailable."));
		return;
	}

	templateSidebarViewModel->RefreshMainFieldItemsFromTemplate(scenarioTemplate);
	ApplyMainFieldItems();
	SetDiagnosticsText(TEXT(""));
}

void UScenarioEditorSidebarMainPanel::HandleScenarioIdCommitted(
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
		return viewModel->CommitScenarioIdText(text, statusText);
	});
}

void UScenarioEditorSidebarMainPanel::HandleIntentCommitted(
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
		return viewModel->CommitIntentText(text, statusText);
	});
}

void UScenarioEditorSidebarMainPanel::HandleRobotStartTypeCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorTextCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Start,
		EScenarioEditorSidebarRobotAnchorField::Type,
		text,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotStartSegmentCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorTextCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Start,
		EScenarioEditorSidebarRobotAnchorField::Segment,
		text,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotStartAlongCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorTextCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Start,
		EScenarioEditorSidebarRobotAnchorField::Along,
		text,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotStartAlongRangeCommitted(
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorRangeCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Start,
		EScenarioEditorSidebarRobotAnchorField::Along,
		minText,
		maxText,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotStartOffsetCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorTextCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Start,
		EScenarioEditorSidebarRobotAnchorField::Offset,
		text,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotStartOffsetRangeCommitted(
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorRangeCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Start,
		EScenarioEditorSidebarRobotAnchorField::Offset,
		minText,
		maxText,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotStartLaneCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorTextCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Start,
		EScenarioEditorSidebarRobotAnchorField::Lane,
		text,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotStartHeadingCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorTextCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Start,
		EScenarioEditorSidebarRobotAnchorField::Heading,
		text,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotGoalTypeCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorTextCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Goal,
		EScenarioEditorSidebarRobotAnchorField::Type,
		text,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotGoalSegmentCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorTextCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Goal,
		EScenarioEditorSidebarRobotAnchorField::Segment,
		text,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotGoalAlongCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorTextCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Goal,
		EScenarioEditorSidebarRobotAnchorField::Along,
		text,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotGoalAlongRangeCommitted(
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorRangeCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Goal,
		EScenarioEditorSidebarRobotAnchorField::Along,
		minText,
		maxText,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotGoalOffsetCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorTextCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Goal,
		EScenarioEditorSidebarRobotAnchorField::Offset,
		text,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotGoalOffsetRangeCommitted(
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorRangeCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Goal,
		EScenarioEditorSidebarRobotAnchorField::Offset,
		minText,
		maxText,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotGoalLaneCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorTextCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Goal,
		EScenarioEditorSidebarRobotAnchorField::Lane,
		text,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::HandleRobotGoalHeadingCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	HandleRobotAnchorTextCommitted(
		EScenarioEditorSidebarRobotAnchorTarget::Goal,
		EScenarioEditorSidebarRobotAnchorField::Heading,
		text,
		commitMethod);
}

void UScenarioEditorSidebarMainPanel::BindFieldRows()
{
	if (ScenarioIdFieldRow)
	{
		ScenarioIdFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarMainPanel::HandleScenarioIdCommitted);
		ScenarioIdFieldRow->OnValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarMainPanel::HandleScenarioIdCommitted);
	}

	if (IntentFieldRow)
	{
		IntentFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarMainPanel::HandleIntentCommitted);
		IntentFieldRow->OnValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarMainPanel::HandleIntentCommitted);
	}

	if (RobotStartTypeFieldRow)
	{
		RobotStartTypeFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartTypeCommitted);
		RobotStartTypeFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartTypeCommitted);
	}
	if (RobotStartSegmentFieldRow)
	{
		RobotStartSegmentFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartSegmentCommitted);
		RobotStartSegmentFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartSegmentCommitted);
	}
	if (RobotStartAlongFieldRow)
	{
		RobotStartAlongFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartAlongCommitted);
		RobotStartAlongFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartAlongCommitted);
		RobotStartAlongFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartAlongRangeCommitted);
		RobotStartAlongFieldRow->OnRangeValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartAlongRangeCommitted);
	}
	if (RobotStartOffsetFieldRow)
	{
		RobotStartOffsetFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartOffsetCommitted);
		RobotStartOffsetFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartOffsetCommitted);
		RobotStartOffsetFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartOffsetRangeCommitted);
		RobotStartOffsetFieldRow->OnRangeValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartOffsetRangeCommitted);
	}
	if (RobotStartLaneFieldRow)
	{
		RobotStartLaneFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartLaneCommitted);
		RobotStartLaneFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartLaneCommitted);
	}
	if (RobotStartHeadingFieldRow)
	{
		RobotStartHeadingFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartHeadingCommitted);
		RobotStartHeadingFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartHeadingCommitted);
	}
	if (RobotGoalTypeFieldRow)
	{
		RobotGoalTypeFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalTypeCommitted);
		RobotGoalTypeFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalTypeCommitted);
	}
	if (RobotGoalSegmentFieldRow)
	{
		RobotGoalSegmentFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalSegmentCommitted);
		RobotGoalSegmentFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalSegmentCommitted);
	}
	if (RobotGoalAlongFieldRow)
	{
		RobotGoalAlongFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalAlongCommitted);
		RobotGoalAlongFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalAlongCommitted);
		RobotGoalAlongFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalAlongRangeCommitted);
		RobotGoalAlongFieldRow->OnRangeValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalAlongRangeCommitted);
	}
	if (RobotGoalOffsetFieldRow)
	{
		RobotGoalOffsetFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalOffsetCommitted);
		RobotGoalOffsetFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalOffsetCommitted);
		RobotGoalOffsetFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalOffsetRangeCommitted);
		RobotGoalOffsetFieldRow->OnRangeValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalOffsetRangeCommitted);
	}
	if (RobotGoalLaneFieldRow)
	{
		RobotGoalLaneFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalLaneCommitted);
		RobotGoalLaneFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalLaneCommitted);
	}
	if (RobotGoalHeadingFieldRow)
	{
		RobotGoalHeadingFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalHeadingCommitted);
		RobotGoalHeadingFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalHeadingCommitted);
	}
}

void UScenarioEditorSidebarMainPanel::UnbindFieldRows()
{
	if (ScenarioIdFieldRow)
	{
		ScenarioIdFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarMainPanel::HandleScenarioIdCommitted);
	}

	if (IntentFieldRow)
	{
		IntentFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarMainPanel::HandleIntentCommitted);
	}

	if (RobotStartTypeFieldRow)
	{
		RobotStartTypeFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartTypeCommitted);
	}
	if (RobotStartSegmentFieldRow)
	{
		RobotStartSegmentFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartSegmentCommitted);
	}
	if (RobotStartAlongFieldRow)
	{
		RobotStartAlongFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartAlongCommitted);
		RobotStartAlongFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartAlongRangeCommitted);
	}
	if (RobotStartOffsetFieldRow)
	{
		RobotStartOffsetFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartOffsetCommitted);
		RobotStartOffsetFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartOffsetRangeCommitted);
	}
	if (RobotStartLaneFieldRow)
	{
		RobotStartLaneFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartLaneCommitted);
	}
	if (RobotStartHeadingFieldRow)
	{
		RobotStartHeadingFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotStartHeadingCommitted);
	}
	if (RobotGoalTypeFieldRow)
	{
		RobotGoalTypeFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalTypeCommitted);
	}
	if (RobotGoalSegmentFieldRow)
	{
		RobotGoalSegmentFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalSegmentCommitted);
	}
	if (RobotGoalAlongFieldRow)
	{
		RobotGoalAlongFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalAlongCommitted);
		RobotGoalAlongFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalAlongRangeCommitted);
	}
	if (RobotGoalOffsetFieldRow)
	{
		RobotGoalOffsetFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalOffsetCommitted);
		RobotGoalOffsetFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalOffsetRangeCommitted);
	}
	if (RobotGoalLaneFieldRow)
	{
		RobotGoalLaneFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalLaneCommitted);
	}
	if (RobotGoalHeadingFieldRow)
	{
		RobotGoalHeadingFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarMainPanel::HandleRobotGoalHeadingCommitted);
	}
}

void UScenarioEditorSidebarMainPanel::ConfigureFieldRows()
{
	if (RootBlockWidget)
	{
		RootBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		RootBlockWidget->SetBlockMetadata(TEXT("Root"), TEXT("scenario"), TEXT("Main"));
		RootBlockWidget->SetSelected(true);
		RootBlockWidget->SetNested(false);
	}
	if (RobotBlockWidget)
	{
		RobotBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		RobotBlockWidget->SetBlockMetadata(TEXT("robot"), TEXT("root.robot"), TEXT("Template"));
		RobotBlockWidget->SetSelected(false);
		RobotBlockWidget->SetNested(true);
	}
	if (RobotStartBlockWidget)
	{
		RobotStartBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		RobotStartBlockWidget->SetBlockMetadata(TEXT("start"), TEXT("root.robot.start"), TEXT("Property"));
		RobotStartBlockWidget->SetNested(true);
	}
	if (RobotGoalBlockWidget)
	{
		RobotGoalBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		RobotGoalBlockWidget->SetBlockMetadata(TEXT("goal"), TEXT("root.robot.goal"), TEXT("Property"));
		RobotGoalBlockWidget->SetNested(true);
	}
	ApplyTextStyles();
}

void UScenarioEditorSidebarMainPanel::ApplyMainFieldItems()
{
	UScenarioTemplateSidebarViewModel* templateSidebarViewModel = GetTemplateSidebarViewModel();
	if (!templateSidebarViewModel)
	{
		return;
	}

	auto applyFieldItem = [this, templateSidebarViewModel](
		UScenarioEditorSidebarFieldRow* fieldRow,
		const TCHAR* fieldId)
	{
		if (!fieldRow)
		{
			return;
		}

		fieldRow->InitializeFromItemViewModel(templateSidebarViewModel->FindMainFieldItem(fieldId));
		fieldRow->SetTextStyleCatalog(TextStyleCatalog);
	};

	applyFieldItem(SchemaFieldRow.Get(), TEXT("Schema"));
	applyFieldItem(ScenarioIdFieldRow.Get(), TEXT("ScenarioId"));
	applyFieldItem(VersionFieldRow.Get(), TEXT("Version"));
	applyFieldItem(IntentFieldRow.Get(), TEXT("Intent"));
	applyFieldItem(RobotStartFieldRow.Get(), TEXT("RobotStart"));
	applyFieldItem(RobotGoalFieldRow.Get(), TEXT("RobotGoal"));
	applyFieldItem(RobotStartTypeFieldRow.Get(), TEXT("RobotStartType"));
	applyFieldItem(RobotStartSegmentFieldRow.Get(), TEXT("RobotStartSegment"));
	applyFieldItem(RobotStartAlongFieldRow.Get(), TEXT("RobotStartAlong"));
	applyFieldItem(RobotStartOffsetFieldRow.Get(), TEXT("RobotStartOffset"));
	applyFieldItem(RobotStartLaneFieldRow.Get(), TEXT("RobotStartLane"));
	applyFieldItem(RobotStartHeadingFieldRow.Get(), TEXT("RobotStartHeading"));
	applyFieldItem(RobotGoalTypeFieldRow.Get(), TEXT("RobotGoalType"));
	applyFieldItem(RobotGoalSegmentFieldRow.Get(), TEXT("RobotGoalSegment"));
	applyFieldItem(RobotGoalAlongFieldRow.Get(), TEXT("RobotGoalAlong"));
	applyFieldItem(RobotGoalOffsetFieldRow.Get(), TEXT("RobotGoalOffset"));
	applyFieldItem(RobotGoalLaneFieldRow.Get(), TEXT("RobotGoalLane"));
	applyFieldItem(RobotGoalHeadingFieldRow.Get(), TEXT("RobotGoalHeading"));
}

void UScenarioEditorSidebarMainPanel::ApplyTextStyles()
{
	if (RootBlockWidget)
	{
		RootBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (RobotBlockWidget)
	{
		RobotBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (RobotStartBlockWidget)
	{
		RobotStartBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (RobotGoalBlockWidget)
	{
		RobotGoalBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (SchemaFieldRow)
	{
		SchemaFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (ScenarioIdFieldRow)
	{
		ScenarioIdFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (VersionFieldRow)
	{
		VersionFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (IntentFieldRow)
	{
		IntentFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (RobotStartFieldRow)
	{
		RobotStartFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (RobotGoalFieldRow)
	{
		RobotGoalFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	for (UScenarioEditorSidebarFieldRow* fieldRow : {
		RobotStartTypeFieldRow.Get(),
		RobotStartSegmentFieldRow.Get(),
		RobotStartAlongFieldRow.Get(),
		RobotStartOffsetFieldRow.Get(),
		RobotStartLaneFieldRow.Get(),
		RobotStartHeadingFieldRow.Get(),
		RobotGoalTypeFieldRow.Get(),
		RobotGoalSegmentFieldRow.Get(),
		RobotGoalAlongFieldRow.Get(),
		RobotGoalOffsetFieldRow.Get(),
		RobotGoalLaneFieldRow.Get(),
		RobotGoalHeadingFieldRow.Get() })
	{
		if (fieldRow)
		{
			fieldRow->SetTextStyleCatalog(TextStyleCatalog);
		}
	}
	if (DiagnosticsTextBlock)
	{
		DiagnosticsTextBlock->SetVisibility(DiagnosticsTextBlock->GetText().IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}
}

UScenarioTemplateSidebarViewModel* UScenarioEditorSidebarMainPanel::GetTemplateSidebarViewModel() const
{
	UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this);
	return uiSubsystem ? uiSubsystem->GetTemplateSidebarViewModel() : nullptr;
}

void UScenarioEditorSidebarMainPanel::ExecuteTemplateCommand(
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

void UScenarioEditorSidebarMainPanel::HandleRobotAnchorTextCommitted(
	const EScenarioEditorSidebarRobotAnchorTarget target,
	const EScenarioEditorSidebarRobotAnchorField field,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	ExecuteTemplateCommand(
		[target, field, &text](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
		{
			return viewModel->CommitRobotAnchorText(target, field, text, statusText);
		});
}

void UScenarioEditorSidebarMainPanel::HandleRobotAnchorRangeCommitted(
	const EScenarioEditorSidebarRobotAnchorTarget target,
	const EScenarioEditorSidebarRobotAnchorField field,
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
		[target, field, &minText, &maxText](UScenarioTemplateSidebarViewModel* viewModel, FString& statusText)
		{
			return viewModel->CommitRobotAnchorRange(target, field, minText, maxText, statusText);
		});
}

void UScenarioEditorSidebarMainPanel::SetDiagnosticsText(const FString& text) const
{
	if (DiagnosticsTextBlock)
	{
		DiagnosticsTextBlock->SetText(FText::FromString(text));
	}
}
