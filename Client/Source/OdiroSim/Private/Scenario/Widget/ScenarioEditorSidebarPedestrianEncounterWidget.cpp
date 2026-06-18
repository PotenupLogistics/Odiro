#include "Scenario/Widget/ScenarioEditorSidebarPedestrianEncounterWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"

namespace
{
	constexpr float PedestrianEncounterWidgetPadding = 6.0f;

	void AddPedestrianEncounterWidgetToBox(UVerticalBox* box, UWidget* widget)
	{
		if (!box || !widget)
		{
			return;
		}

		if (UVerticalBoxSlot* slot = box->AddChildToVerticalBox(widget))
		{
			slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, PedestrianEncounterWidgetPadding));
			slot->SetHorizontalAlignment(HAlign_Fill);
		}
	}

	void AddPedestrianEncounterFieldRow(
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
		AddPedestrianEncounterWidgetToBox(parent, fieldRow.Get());
	}
}

TSharedRef<SWidget> UScenarioEditorSidebarPedestrianEncounterWidget::RebuildWidget()
{
	Initialize();
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void UScenarioEditorSidebarPedestrianEncounterWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindFieldRows();
	ConfigureFieldRows();
	ApplyCachedEncounterToRows();
}

void UScenarioEditorSidebarPedestrianEncounterWidget::NativeDestruct()
{
	UnbindFieldRows();
	Super::NativeDestruct();
}

void UScenarioEditorSidebarPedestrianEncounterWidget::SetEncounterIndex(const int32 inEncounterIndex)
{
	EncounterIndex = inEncounterIndex;
	ConfigureFieldRows();
}

void UScenarioEditorSidebarPedestrianEncounterWidget::SetTextStyleCatalog(
	TSoftObjectPtr<UWidgetTextStyleCatalog> catalog)
{
	TextStyleCatalog = catalog;
	ApplyTextStyles();
}

void UScenarioEditorSidebarPedestrianEncounterWidget::RefreshFromEncounter(
	const FScenarioTemplatePedestrianEncounter& encounter)
{
	CachedEncounter = encounter;
	bHasCachedEncounter = true;
	ConfigureFieldRows();
	ApplyCachedEncounterToRows();
}

void UScenarioEditorSidebarPedestrianEncounterWidget::HandleEncounterIdCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarPedestrianEncounterField::EncounterId, text, commitMethod);
}

void UScenarioEditorSidebarPedestrianEncounterWidget::HandleTypeCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarPedestrianEncounterField::Type, text, commitMethod);
}

void UScenarioEditorSidebarPedestrianEncounterWidget::HandleAtSegmentCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarPedestrianEncounterField::AtSegment, text, commitMethod);
}

void UScenarioEditorSidebarPedestrianEncounterWidget::HandlePersonaCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarPedestrianEncounterField::Persona, text, commitMethod);
}

void UScenarioEditorSidebarPedestrianEncounterWidget::HandleMeetOffsetCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarPedestrianEncounterField::MeetOffset, text, commitMethod);
}

void UScenarioEditorSidebarPedestrianEncounterWidget::HandleMeetOffsetRangeCommitted(
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	BroadcastRange(EScenarioEditorSidebarPedestrianEncounterField::MeetOffset, minText, maxText, commitMethod);
}

void UScenarioEditorSidebarPedestrianEncounterWidget::HandleCooperationCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarPedestrianEncounterField::Cooperation, text, commitMethod);
}

void UScenarioEditorSidebarPedestrianEncounterWidget::HandleCooperationRangeCommitted(
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	BroadcastRange(EScenarioEditorSidebarPedestrianEncounterField::Cooperation, minText, maxText, commitMethod);
}

void UScenarioEditorSidebarPedestrianEncounterWidget::HandleEvasivenessCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarPedestrianEncounterField::Evasiveness, text, commitMethod);
}

void UScenarioEditorSidebarPedestrianEncounterWidget::HandleEvasivenessRangeCommitted(
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	BroadcastRange(EScenarioEditorSidebarPedestrianEncounterField::Evasiveness, minText, maxText, commitMethod);
}

void UScenarioEditorSidebarPedestrianEncounterWidget::HandlePersonalSpaceCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarPedestrianEncounterField::PersonalSpace, text, commitMethod);
}

void UScenarioEditorSidebarPedestrianEncounterWidget::HandlePersonalSpaceRangeCommitted(
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	BroadcastRange(EScenarioEditorSidebarPedestrianEncounterField::PersonalSpace, minText, maxText, commitMethod);
}

void UScenarioEditorSidebarPedestrianEncounterWidget::HandleAwarenessHorizonCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarPedestrianEncounterField::AwarenessHorizon, text, commitMethod);
}

void UScenarioEditorSidebarPedestrianEncounterWidget::HandleAwarenessHorizonRangeCommitted(
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	BroadcastRange(EScenarioEditorSidebarPedestrianEncounterField::AwarenessHorizon, minText, maxText, commitMethod);
}

void UScenarioEditorSidebarPedestrianEncounterWidget::HandleMaxYieldWaitCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarPedestrianEncounterField::MaxYieldWait, text, commitMethod);
}

void UScenarioEditorSidebarPedestrianEncounterWidget::HandleMaxYieldWaitRangeCommitted(
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	BroadcastRange(EScenarioEditorSidebarPedestrianEncounterField::MaxYieldWait, minText, maxText, commitMethod);
}

void UScenarioEditorSidebarPedestrianEncounterWidget::HandleSidestepDistanceCommitted(
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	BroadcastText(EScenarioEditorSidebarPedestrianEncounterField::SidestepDistance, text, commitMethod);
}

void UScenarioEditorSidebarPedestrianEncounterWidget::HandleSidestepDistanceRangeCommitted(
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	BroadcastRange(EScenarioEditorSidebarPedestrianEncounterField::SidestepDistance, minText, maxText, commitMethod);
}

void UScenarioEditorSidebarPedestrianEncounterWidget::HandleAddRequested()
{
	OnAddRequested.Broadcast(EncounterIndex);
}

void UScenarioEditorSidebarPedestrianEncounterWidget::HandleRemoveRequested()
{
	OnRemoveRequested.Broadcast(EncounterIndex);
}

void UScenarioEditorSidebarPedestrianEncounterWidget::BuildDefaultWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	EncounterBlockWidget = WidgetTree->ConstructWidget<UScenarioEditorSidebarBlockWidget>(
		UScenarioEditorSidebarBlockWidget::StaticClass(),
		TEXT("EncounterBlockWidget"));
	if (!EncounterBlockWidget)
	{
		return;
	}

	WidgetTree->RootWidget = EncounterBlockWidget;
	EncounterBlockWidget->SetNested(true);
	EncounterBlockWidget->SetShowNormalOutline(false);

	UVerticalBox* encounterBody = EncounterBlockWidget->GetBodyBox();
	AddPedestrianEncounterFieldRow(WidgetTree, encounterBody, EncounterIdFieldRow, TEXT("EncounterIdFieldRow"));
	AddPedestrianEncounterFieldRow(WidgetTree, encounterBody, TypeFieldRow, TEXT("TypeFieldRow"));
	AddPedestrianEncounterFieldRow(WidgetTree, encounterBody, AtSegmentFieldRow, TEXT("AtSegmentFieldRow"));
	AddPedestrianEncounterFieldRow(WidgetTree, encounterBody, PersonaFieldRow, TEXT("PersonaFieldRow"));
	AddPedestrianEncounterFieldRow(WidgetTree, encounterBody, MeetOffsetFieldRow, TEXT("MeetOffsetFieldRow"));
	AddPedestrianEncounterFieldRow(WidgetTree, encounterBody, CooperationFieldRow, TEXT("CooperationFieldRow"));
	AddPedestrianEncounterFieldRow(WidgetTree, encounterBody, EvasivenessFieldRow, TEXT("EvasivenessFieldRow"));
	AddPedestrianEncounterFieldRow(WidgetTree, encounterBody, PersonalSpaceFieldRow, TEXT("PersonalSpaceFieldRow"));
	AddPedestrianEncounterFieldRow(WidgetTree, encounterBody, AwarenessHorizonFieldRow, TEXT("AwarenessHorizonFieldRow"));
	AddPedestrianEncounterFieldRow(WidgetTree, encounterBody, MaxYieldWaitFieldRow, TEXT("MaxYieldWaitFieldRow"));
	AddPedestrianEncounterFieldRow(WidgetTree, encounterBody, SidestepDistanceFieldRow, TEXT("SidestepDistanceFieldRow"));
}

void UScenarioEditorSidebarPedestrianEncounterWidget::BindFieldRows()
{
	if (EncounterIdFieldRow)
	{
		EncounterIdFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleEncounterIdCommitted);
		EncounterIdFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleEncounterIdCommitted);
		EncounterIdFieldRow->OnAddItemRequested.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleAddRequested);
		EncounterIdFieldRow->OnAddItemRequested.AddDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleAddRequested);
		EncounterIdFieldRow->OnRemoveItemRequested.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleRemoveRequested);
		EncounterIdFieldRow->OnRemoveItemRequested.AddDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleRemoveRequested);
	}
	if (TypeFieldRow)
	{
		TypeFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleTypeCommitted);
		TypeFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleTypeCommitted);
	}
	if (AtSegmentFieldRow)
	{
		AtSegmentFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleAtSegmentCommitted);
		AtSegmentFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleAtSegmentCommitted);
	}
	if (PersonaFieldRow)
	{
		PersonaFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandlePersonaCommitted);
		PersonaFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandlePersonaCommitted);
	}
	if (MeetOffsetFieldRow)
	{
		MeetOffsetFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleMeetOffsetCommitted);
		MeetOffsetFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleMeetOffsetCommitted);
		MeetOffsetFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleMeetOffsetRangeCommitted);
		MeetOffsetFieldRow->OnRangeValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleMeetOffsetRangeCommitted);
	}
	if (CooperationFieldRow)
	{
		CooperationFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleCooperationCommitted);
		CooperationFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleCooperationCommitted);
		CooperationFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleCooperationRangeCommitted);
		CooperationFieldRow->OnRangeValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleCooperationRangeCommitted);
	}
	if (EvasivenessFieldRow)
	{
		EvasivenessFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleEvasivenessCommitted);
		EvasivenessFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleEvasivenessCommitted);
		EvasivenessFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleEvasivenessRangeCommitted);
		EvasivenessFieldRow->OnRangeValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleEvasivenessRangeCommitted);
	}
	if (PersonalSpaceFieldRow)
	{
		PersonalSpaceFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandlePersonalSpaceCommitted);
		PersonalSpaceFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandlePersonalSpaceCommitted);
		PersonalSpaceFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandlePersonalSpaceRangeCommitted);
		PersonalSpaceFieldRow->OnRangeValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandlePersonalSpaceRangeCommitted);
	}
	if (AwarenessHorizonFieldRow)
	{
		AwarenessHorizonFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleAwarenessHorizonCommitted);
		AwarenessHorizonFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleAwarenessHorizonCommitted);
		AwarenessHorizonFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleAwarenessHorizonRangeCommitted);
		AwarenessHorizonFieldRow->OnRangeValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleAwarenessHorizonRangeCommitted);
	}
	if (MaxYieldWaitFieldRow)
	{
		MaxYieldWaitFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleMaxYieldWaitCommitted);
		MaxYieldWaitFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleMaxYieldWaitCommitted);
		MaxYieldWaitFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleMaxYieldWaitRangeCommitted);
		MaxYieldWaitFieldRow->OnRangeValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleMaxYieldWaitRangeCommitted);
	}
	if (SidestepDistanceFieldRow)
	{
		SidestepDistanceFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleSidestepDistanceCommitted);
		SidestepDistanceFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleSidestepDistanceCommitted);
		SidestepDistanceFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleSidestepDistanceRangeCommitted);
		SidestepDistanceFieldRow->OnRangeValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleSidestepDistanceRangeCommitted);
	}
}

void UScenarioEditorSidebarPedestrianEncounterWidget::UnbindFieldRows()
{
	if (EncounterIdFieldRow)
	{
		EncounterIdFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleEncounterIdCommitted);
		EncounterIdFieldRow->OnAddItemRequested.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleAddRequested);
		EncounterIdFieldRow->OnRemoveItemRequested.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleRemoveRequested);
	}
	if (TypeFieldRow)
	{
		TypeFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleTypeCommitted);
	}
	if (AtSegmentFieldRow)
	{
		AtSegmentFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleAtSegmentCommitted);
	}
	if (PersonaFieldRow)
	{
		PersonaFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandlePersonaCommitted);
	}
	if (MeetOffsetFieldRow)
	{
		MeetOffsetFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleMeetOffsetCommitted);
		MeetOffsetFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleMeetOffsetRangeCommitted);
	}
	if (CooperationFieldRow)
	{
		CooperationFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleCooperationCommitted);
		CooperationFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleCooperationRangeCommitted);
	}
	if (EvasivenessFieldRow)
	{
		EvasivenessFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleEvasivenessCommitted);
		EvasivenessFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleEvasivenessRangeCommitted);
	}
	if (PersonalSpaceFieldRow)
	{
		PersonalSpaceFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandlePersonalSpaceCommitted);
		PersonalSpaceFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandlePersonalSpaceRangeCommitted);
	}
	if (AwarenessHorizonFieldRow)
	{
		AwarenessHorizonFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleAwarenessHorizonCommitted);
		AwarenessHorizonFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleAwarenessHorizonRangeCommitted);
	}
	if (MaxYieldWaitFieldRow)
	{
		MaxYieldWaitFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleMaxYieldWaitCommitted);
		MaxYieldWaitFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleMaxYieldWaitRangeCommitted);
	}
	if (SidestepDistanceFieldRow)
	{
		SidestepDistanceFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleSidestepDistanceCommitted);
		SidestepDistanceFieldRow->OnRangeValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleSidestepDistanceRangeCommitted);
	}
}

void UScenarioEditorSidebarPedestrianEncounterWidget::ConfigureFieldRows()
{
	if (EncounterBlockWidget)
	{
		EncounterBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
		EncounterBlockWidget->SetBlockMetadata(
			bHasCachedEncounter && !CachedEncounter.EncounterId.IsEmpty()
				? CachedEncounter.EncounterId
				: FString::Printf(TEXT("encounter[%d]"), EncounterIndex),
			TEXT("root.pedestrians.encounters[]"),
			TEXT("Detail"));
		EncounterBlockWidget->SetNested(true);
		EncounterBlockWidget->SetShowNormalOutline(false);
	}

	if (EncounterIdFieldRow)
	{
		EncounterIdFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		EncounterIdFieldRow->SetFieldLabel(TEXT("id"));
		EncounterIdFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Text);
		EncounterIdFieldRow->SetEditable(true);
		EncounterIdFieldRow->SetArrayControlsEnabled(false);
	}
	if (TypeFieldRow)
	{
		TypeFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		TypeFieldRow->SetFieldLabel(TEXT("type"));
		TypeFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::EnumText);
		TypeFieldRow->SetEditable(true);
	}
	if (AtSegmentFieldRow)
	{
		AtSegmentFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		AtSegmentFieldRow->SetFieldLabel(TEXT("at"));
		AtSegmentFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Text);
		AtSegmentFieldRow->SetEditable(true);
	}
	if (PersonaFieldRow)
	{
		PersonaFieldRow->SetTextStyleCatalog(TextStyleCatalog);
		PersonaFieldRow->SetFieldLabel(TEXT("persona"));
		PersonaFieldRow->SetInputType(EScenarioEditorSidebarFieldInputType::Text);
		PersonaFieldRow->SetEditable(true);
	}

	const TArray<TPair<UScenarioEditorSidebarFieldRow*, FString>> numberRows = {
		{ MeetOffsetFieldRow.Get(), TEXT("meet_offset_m") },
		{ CooperationFieldRow.Get(), TEXT("overrides.cooperation") },
		{ EvasivenessFieldRow.Get(), TEXT("overrides.evasiveness") },
		{ PersonalSpaceFieldRow.Get(), TEXT("overrides.personal_space_m") },
		{ AwarenessHorizonFieldRow.Get(), TEXT("overrides.awareness_horizon_s") },
		{ MaxYieldWaitFieldRow.Get(), TEXT("overrides.max_yield_wait_s") },
		{ SidestepDistanceFieldRow.Get(), TEXT("overrides.sidestep_distance_m") }
	};
	for (const TPair<UScenarioEditorSidebarFieldRow*, FString>& numberRow : numberRows)
	{
		if (numberRow.Key)
		{
			numberRow.Key->SetTextStyleCatalog(TextStyleCatalog);
			numberRow.Key->SetFieldLabel(numberRow.Value);
			numberRow.Key->SetInputType(EScenarioEditorSidebarFieldInputType::Range);
			numberRow.Key->SetEditable(true);
		}
	}
}

void UScenarioEditorSidebarPedestrianEncounterWidget::ApplyCachedEncounterToRows()
{
	if (!bHasCachedEncounter)
	{
		return;
	}

	if (EncounterBlockWidget)
	{
		EncounterBlockWidget->SetBlockMetadata(
			CachedEncounter.EncounterId.IsEmpty()
				? FString::Printf(TEXT("encounter[%d]"), EncounterIndex)
				: CachedEncounter.EncounterId,
			TEXT("root.pedestrians.encounters[]"),
			TEXT("Detail"));
	}
	if (EncounterIdFieldRow)
	{
		EncounterIdFieldRow->SetValueText(CachedEncounter.EncounterId);
	}
	if (TypeFieldRow)
	{
		TypeFieldRow->SetValueText(EncounterTypeToString(CachedEncounter.Type));
	}
	if (AtSegmentFieldRow)
	{
		AtSegmentFieldRow->SetValueText(CachedEncounter.AtSegmentId);
	}
	if (PersonaFieldRow)
	{
		PersonaFieldRow->SetValueText(CachedEncounter.PersonaId);
	}
	SetNumberRowValue(MeetOffsetFieldRow.Get(), CachedEncounter.MeetOffsetMeters);
	SetNumberRowValue(CooperationFieldRow.Get(), CachedEncounter.Overrides.Cooperation);
	SetNumberRowValue(EvasivenessFieldRow.Get(), CachedEncounter.Overrides.Evasiveness);
	SetNumberRowValue(PersonalSpaceFieldRow.Get(), CachedEncounter.Overrides.PersonalSpaceMeters);
	SetNumberRowValue(AwarenessHorizonFieldRow.Get(), CachedEncounter.Overrides.AwarenessHorizonSeconds);
	SetNumberRowValue(MaxYieldWaitFieldRow.Get(), CachedEncounter.Overrides.MaxYieldWaitSeconds);
	SetNumberRowValue(SidestepDistanceFieldRow.Get(), CachedEncounter.Overrides.SidestepDistanceMeters);
}

void UScenarioEditorSidebarPedestrianEncounterWidget::ApplyTextStyles()
{
	if (EncounterBlockWidget)
	{
		EncounterBlockWidget->SetTextStyleCatalog(TextStyleCatalog);
	}
	for (UScenarioEditorSidebarFieldRow* fieldRow : {
		EncounterIdFieldRow.Get(),
		TypeFieldRow.Get(),
		AtSegmentFieldRow.Get(),
		PersonaFieldRow.Get(),
		MeetOffsetFieldRow.Get(),
		CooperationFieldRow.Get(),
		EvasivenessFieldRow.Get(),
		PersonalSpaceFieldRow.Get(),
		AwarenessHorizonFieldRow.Get(),
		MaxYieldWaitFieldRow.Get(),
		SidestepDistanceFieldRow.Get() })
	{
		if (fieldRow)
		{
			fieldRow->SetTextStyleCatalog(TextStyleCatalog);
		}
	}
}

void UScenarioEditorSidebarPedestrianEncounterWidget::BroadcastText(
	const EScenarioEditorSidebarPedestrianEncounterField field,
	const FText& text,
	const ETextCommit::Type commitMethod)
{
	OnFieldTextCommitted.Broadcast(EncounterIndex, field, text, commitMethod);
}

void UScenarioEditorSidebarPedestrianEncounterWidget::BroadcastRange(
	const EScenarioEditorSidebarPedestrianEncounterField field,
	const FText& minText,
	const FText& maxText,
	const ETextCommit::Type commitMethod)
{
	OnFieldRangeCommitted.Broadcast(EncounterIndex, field, minText, maxText, commitMethod);
}

void UScenarioEditorSidebarPedestrianEncounterWidget::SetNumberRowValue(
	UScenarioEditorSidebarFieldRow* fieldRow,
	const FScenarioTemplateNumberValue& value)
{
	if (!fieldRow)
	{
		return;
	}

	if (!value.bIsSet)
	{
		fieldRow->SetValueText(FString());
		fieldRow->SetRangeValueText(FString(), FString());
		fieldRow->SetRangeInputEnabled(false);
		return;
	}

	const double fixedDisplayValue = value.Mode == EScenarioTemplateNumberValueMode::Range
		? (value.MinValue + value.MaxValue) * 0.5
		: value.FixedValue;
	const double minDisplayValue = value.Mode == EScenarioTemplateNumberValueMode::Range
		? value.MinValue
		: fixedDisplayValue;
	const double maxDisplayValue = value.Mode == EScenarioTemplateNumberValueMode::Range
		? value.MaxValue
		: fixedDisplayValue;

	fieldRow->SetValueText(FormatEditableNumber(fixedDisplayValue));
	fieldRow->SetRangeValueText(FormatEditableNumber(minDisplayValue), FormatEditableNumber(maxDisplayValue));
	fieldRow->SetRangeInputEnabled(value.Mode == EScenarioTemplateNumberValueMode::Range);
}

FString UScenarioEditorSidebarPedestrianEncounterWidget::EncounterTypeToString(
	const EScenarioTemplateEncounterType type)
{
	switch (type)
	{
	case EScenarioTemplateEncounterType::OncomingPass:
		return TEXT("oncoming_pass");
	case EScenarioTemplateEncounterType::Overtake:
		return TEXT("overtake");
	case EScenarioTemplateEncounterType::CrossPath:
		return TEXT("cross_path");
	case EScenarioTemplateEncounterType::StandingGroup:
		return TEXT("standing_group");
	default:
		return TEXT("unknown");
	}
}

FString UScenarioEditorSidebarPedestrianEncounterWidget::FormatEditableNumber(const double value)
{
	return FString::Printf(TEXT("%.2f"), value);
}
