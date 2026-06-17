#include "Scenario/Widget/ScenarioEditorSidebarMainPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Scenario/Editor/ScenarioAuthoringSubsystem.h"
#include "Scenario/Widget/ScenarioEditorSidebarFieldRow.h"

namespace
{
	FString JoinMainPanelDiagnostics(const TArray<FString>& diagnostics)
	{
		return diagnostics.IsEmpty() ? FString(TEXT("Unknown edit failure.")) : FString::Join(diagnostics, TEXT("\n"));
	}
}

TSharedRef<SWidget> UScenarioEditorSidebarMainPanel::RebuildWidget()
{
	Initialize();
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void UScenarioEditorSidebarMainPanel::NativeConstruct()
{
	Super::NativeConstruct();
	BindFieldRows();
	ConfigureFieldRows();
	RefreshFromDraft();
}

void UScenarioEditorSidebarMainPanel::NativeDestruct()
{
	UnbindFieldRows();
	Super::NativeDestruct();
}

void UScenarioEditorSidebarMainPanel::RefreshFromDraft()
{
	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioAuthoringSubsystem unavailable."));
		return;
	}

	RefreshFromTemplate(authoringSubsystem->GetDraftScenarioTemplate());
}

void UScenarioEditorSidebarMainPanel::RefreshFromTemplate(const FScenarioTemplateDocument& scenarioTemplate)
{
	ConfigureFieldRows();

	if (TemplateIdFieldRow)
	{
		TemplateIdFieldRow->SetValueText(scenarioTemplate.TemplateId);
	}
	if (VersionFieldRow)
	{
		VersionFieldRow->SetValueText(FString::FromInt(scenarioTemplate.Version));
	}
	if (IntentFieldRow)
	{
		IntentFieldRow->SetValueText(scenarioTemplate.Intent);
	}
	if (RobotStartFieldRow)
	{
		RobotStartFieldRow->SetValueText(FormatRobotAnchor(scenarioTemplate.Robot.Start));
	}
	if (RobotGoalFieldRow)
	{
		RobotGoalFieldRow->SetValueText(FormatRobotAnchor(scenarioTemplate.Robot.Goal));
	}

	SetDiagnosticsText(TEXT(""));
}

void UScenarioEditorSidebarMainPanel::HandleTemplateIdCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromDraft();
		return;
	}

	CommitTemplateIdText(text);
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

	CommitIntentText(text);
}

void UScenarioEditorSidebarMainPanel::BuildDefaultWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UVerticalBox* rootBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("GeneratedMainPanelRoot"));
	if (!rootBox)
	{
		return;
	}

	WidgetTree->RootWidget = rootBox;

	auto addFieldRow = [this, rootBox](
		TObjectPtr<UScenarioEditorSidebarFieldRow>& fieldRow,
		const TCHAR* widgetName)
	{
		fieldRow = WidgetTree->ConstructWidget<UScenarioEditorSidebarFieldRow>(
			UScenarioEditorSidebarFieldRow::StaticClass(),
			FName(widgetName));
		if (!fieldRow)
		{
			return;
		}

		if (UVerticalBoxSlot* slot = rootBox->AddChildToVerticalBox(fieldRow))
		{
			slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		}
	};

	addFieldRow(TemplateIdFieldRow, TEXT("TemplateIdFieldRow"));
	addFieldRow(VersionFieldRow, TEXT("VersionFieldRow"));
	addFieldRow(IntentFieldRow, TEXT("IntentFieldRow"));
	addFieldRow(RobotStartFieldRow, TEXT("RobotStartFieldRow"));
	addFieldRow(RobotGoalFieldRow, TEXT("RobotGoalFieldRow"));

	DiagnosticsTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("DiagnosticsTextBlock"));
	if (DiagnosticsTextBlock)
	{
		if (UVerticalBoxSlot* slot = rootBox->AddChildToVerticalBox(DiagnosticsTextBlock))
		{
			slot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
		}
	}
}

void UScenarioEditorSidebarMainPanel::BindFieldRows()
{
	if (TemplateIdFieldRow)
	{
		TemplateIdFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarMainPanel::HandleTemplateIdCommitted);
		TemplateIdFieldRow->OnValueTextCommitted.AddDynamic(
			this,
			&UScenarioEditorSidebarMainPanel::HandleTemplateIdCommitted);
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
}

void UScenarioEditorSidebarMainPanel::UnbindFieldRows()
{
	if (TemplateIdFieldRow)
	{
		TemplateIdFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarMainPanel::HandleTemplateIdCommitted);
	}

	if (IntentFieldRow)
	{
		IntentFieldRow->OnValueTextCommitted.RemoveDynamic(
			this,
			&UScenarioEditorSidebarMainPanel::HandleIntentCommitted);
	}
}

void UScenarioEditorSidebarMainPanel::ConfigureFieldRows()
{
	if (TemplateIdFieldRow)
	{
		TemplateIdFieldRow->SetFieldLabel(TEXT("template_id"));
		TemplateIdFieldRow->SetEditable(true);
	}
	if (VersionFieldRow)
	{
		VersionFieldRow->SetFieldLabel(TEXT("version"));
		VersionFieldRow->SetEditable(false);
	}
	if (IntentFieldRow)
	{
		IntentFieldRow->SetFieldLabel(TEXT("intent"));
		IntentFieldRow->SetEditable(true);
	}
	if (RobotStartFieldRow)
	{
		RobotStartFieldRow->SetFieldLabel(TEXT("robot.start"));
		RobotStartFieldRow->SetEditable(false);
	}
	if (RobotGoalFieldRow)
	{
		RobotGoalFieldRow->SetFieldLabel(TEXT("robot.goal"));
		RobotGoalFieldRow->SetEditable(false);
	}
}

UScenarioAuthoringSubsystem* UScenarioEditorSidebarMainPanel::GetAuthoringSubsystem() const
{
	UWorld* world = GetWorld();
	return world ? world->GetSubsystem<UScenarioAuthoringSubsystem>() : nullptr;
}

void UScenarioEditorSidebarMainPanel::CommitTemplateIdText(const FText& text)
{
	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioAuthoringSubsystem unavailable."));
		return;
	}

	TArray<FString> diagnostics;
	if (!authoringSubsystem->SetDraftTemplateId(text.ToString(), diagnostics))
	{
		RefreshFromDraft();
		SetDiagnosticsText(JoinMainPanelDiagnostics(diagnostics));
		return;
	}

	RefreshFromDraft();
}

void UScenarioEditorSidebarMainPanel::CommitIntentText(const FText& text)
{
	UScenarioAuthoringSubsystem* authoringSubsystem = GetAuthoringSubsystem();
	if (!authoringSubsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioAuthoringSubsystem unavailable."));
		return;
	}

	TArray<FString> diagnostics;
	if (!authoringSubsystem->SetDraftIntent(text.ToString(), diagnostics))
	{
		RefreshFromDraft();
		SetDiagnosticsText(JoinMainPanelDiagnostics(diagnostics));
		return;
	}

	RefreshFromDraft();
}

void UScenarioEditorSidebarMainPanel::SetDiagnosticsText(const FString& text) const
{
	if (DiagnosticsTextBlock)
	{
		DiagnosticsTextBlock->SetText(FText::FromString(text));
	}
}

FString UScenarioEditorSidebarMainPanel::RobotAnchorTypeToString(
	const EScenarioTemplateRobotAnchorType type)
{
	switch (type)
	{
	case EScenarioTemplateRobotAnchorType::Entry:
		return TEXT("entry");
	case EScenarioTemplateRobotAnchorType::Exit:
		return TEXT("exit");
	case EScenarioTemplateRobotAnchorType::CorridorPose:
		return TEXT("corridor_pose");
	default:
		return TEXT("unknown");
	}
}

FString UScenarioEditorSidebarMainPanel::RobotHeadingToString(
	const EScenarioTemplateRobotHeading heading)
{
	switch (heading)
	{
	case EScenarioTemplateRobotHeading::Forward:
		return TEXT("forward");
	case EScenarioTemplateRobotHeading::Backward:
		return TEXT("backward");
	case EScenarioTemplateRobotHeading::Auto:
		return TEXT("auto");
	default:
		return TEXT("unknown");
	}
}

FString UScenarioEditorSidebarMainPanel::FormatNumberValue(
	const FScenarioTemplateNumberValue& value,
	const FString& suffix)
{
	if (!value.bIsSet)
	{
		return TEXT("(unset)");
	}

	if (value.Mode == EScenarioTemplateNumberValueMode::Range)
	{
		return FString::Printf(TEXT("%.2f..%.2f%s"), value.MinValue, value.MaxValue, *suffix);
	}

	return FString::Printf(TEXT("%.2f%s"), value.FixedValue, *suffix);
}

FString UScenarioEditorSidebarMainPanel::FormatRobotAnchor(
	const FScenarioTemplateRobotAnchor& anchor)
{
	if (anchor.Type != EScenarioTemplateRobotAnchorType::CorridorPose)
	{
		return FString::Printf(
			TEXT("%s | heading: %s"),
			*RobotAnchorTypeToString(anchor.Type),
			*RobotHeadingToString(anchor.Heading));
	}

	return FString::Printf(
		TEXT("corridor_pose | segment: %s | along: %s | offset: %s | lane: %s | heading: %s"),
		anchor.SegmentId.IsEmpty() ? TEXT("(unset)") : *anchor.SegmentId,
		*FormatNumberValue(anchor.AlongMeters, TEXT("m")),
		*FormatNumberValue(anchor.OffsetMeters, TEXT("m")),
		anchor.LaneId.IsEmpty() ? TEXT("(unset)") : *anchor.LaneId,
		*RobotHeadingToString(anchor.Heading));
}
