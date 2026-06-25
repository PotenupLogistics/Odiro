#include "Scenario/Widget/ScenarioEditorSidebarPedestrianEncounterWidget.h"

#include "Engine/World.h"
#include "Scenario/ScenarioEditorUiSubsystem.h"
#include "Scenario/ViewModel/ScenarioTemplateFieldRowViewModel.h"
#include "Scenario/ViewModel/ScenarioTemplateSidebarViewModel.h"
#include "Scenario/Widget/ScenarioEditorSidebarBlockWidget.h"

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
	if (bHasCachedEncounter)
	{
		RefreshFieldItemsFromViewModel();
	}
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
	RefreshFieldItemsFromViewModel();
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

void UScenarioEditorSidebarPedestrianEncounterWidget::BindFieldRows()
{
	if (EncounterIdFieldRow)
	{
		EncounterIdFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleEncounterIdCommitted);
		EncounterIdFieldRow->OnValueTextCommitted.AddDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleEncounterIdCommitted);
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
	if (EncounterBlockWidget)
	{
		EncounterBlockWidget->OnRemoveActionRequested.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleRemoveRequested);
		EncounterBlockWidget->OnRemoveActionRequested.AddDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleRemoveRequested);
	}
}

void UScenarioEditorSidebarPedestrianEncounterWidget::UnbindFieldRows()
{
	if (EncounterIdFieldRow)
	{
		EncounterIdFieldRow->OnValueTextCommitted.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleEncounterIdCommitted);
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
	if (EncounterBlockWidget)
	{
		EncounterBlockWidget->OnRemoveActionRequested.RemoveDynamic(this, &UScenarioEditorSidebarPedestrianEncounterWidget::HandleRemoveRequested);
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
				: FString::Printf(TEXT("상호작용 %d"), EncounterIndex + 1),
			TEXT("root.pedestrians.encounters[]"),
			TEXT("세부"));
		EncounterBlockWidget->SetNested(true);
		EncounterBlockWidget->SetShowNormalOutline(false);
		EncounterBlockWidget->SetAddActionVisible(false);
		EncounterBlockWidget->SetRemoveActionVisible(true);
	}

	if (EncounterIdFieldRow)
	{
		EncounterIdFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (TypeFieldRow)
	{
		TypeFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (AtSegmentFieldRow)
	{
		AtSegmentFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}
	if (PersonaFieldRow)
	{
		PersonaFieldRow->SetTextStyleCatalog(TextStyleCatalog);
	}

	const TArray<UScenarioEditorSidebarFieldRow*> numberRows = {
		MeetOffsetFieldRow.Get(),
		CooperationFieldRow.Get(),
		EvasivenessFieldRow.Get(),
		PersonalSpaceFieldRow.Get(),
		AwarenessHorizonFieldRow.Get(),
		MaxYieldWaitFieldRow.Get(),
		SidestepDistanceFieldRow.Get()
	};
	for (UScenarioEditorSidebarFieldRow* numberRow : numberRows)
	{
		if (numberRow)
		{
			numberRow->SetTextStyleCatalog(TextStyleCatalog);
		}
	}
}

void UScenarioEditorSidebarPedestrianEncounterWidget::ApplyCachedEncounterToRows()
{
	if (!bHasCachedEncounter)
	{
		return;
	}
	if (CachedFieldItems.IsEmpty())
	{
		RefreshFieldItemsFromViewModel();
	}

	if (EncounterBlockWidget)
	{
		EncounterBlockWidget->SetBlockMetadata(
			CachedEncounter.EncounterId.IsEmpty()
				? FString::Printf(TEXT("상호작용 %d"), EncounterIndex + 1)
				: CachedEncounter.EncounterId,
			TEXT("root.pedestrians.encounters[]"),
			TEXT("세부"));
	}
	if (EncounterIdFieldRow)
	{
		EncounterIdFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("EncounterId")));
	}
	if (TypeFieldRow)
	{
		TypeFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("EncounterType")));
	}
	if (AtSegmentFieldRow)
	{
		AtSegmentFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("EncounterAtSegment")));
	}
	if (PersonaFieldRow)
	{
		PersonaFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("EncounterPersona")));
	}
	if (MeetOffsetFieldRow)
	{
		MeetOffsetFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("EncounterMeetOffset")));
	}
	if (CooperationFieldRow)
	{
		CooperationFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("EncounterCooperation")));
	}
	if (EvasivenessFieldRow)
	{
		EvasivenessFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("EncounterEvasiveness")));
	}
	if (PersonalSpaceFieldRow)
	{
		PersonalSpaceFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("EncounterPersonalSpace")));
	}
	if (AwarenessHorizonFieldRow)
	{
		AwarenessHorizonFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("EncounterAwarenessHorizon")));
	}
	if (MaxYieldWaitFieldRow)
	{
		MaxYieldWaitFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("EncounterMaxYieldWait")));
	}
	if (SidestepDistanceFieldRow)
	{
		SidestepDistanceFieldRow->InitializeFromItemViewModel(FindCachedFieldItem(TEXT("EncounterSidestepDistance")));
	}
}

void UScenarioEditorSidebarPedestrianEncounterWidget::RefreshFieldItemsFromViewModel()
{
	CachedFieldItems.Reset();
	if (!bHasCachedEncounter)
	{
		return;
	}

	UScenarioTemplateSidebarViewModel* templateSidebarViewModel = GetTemplateSidebarViewModel();
	if (!templateSidebarViewModel)
	{
		return;
	}

	for (UScenarioTemplateFieldRowViewModel* fieldItem :
		templateSidebarViewModel->CreatePedestrianEncounterFieldItems(EncounterIndex, CachedEncounter))
	{
		CachedFieldItems.Add(fieldItem);
	}
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

UScenarioTemplateSidebarViewModel* UScenarioEditorSidebarPedestrianEncounterWidget::GetTemplateSidebarViewModel() const
{
	UScenarioEditorUiSubsystem* uiSubsystem = UScenarioEditorUiSubsystem::ResolveForWorldContext(this);
	return uiSubsystem ? uiSubsystem->GetTemplateSidebarViewModel() : nullptr;
}

UScenarioTemplateFieldRowViewModel* UScenarioEditorSidebarPedestrianEncounterWidget::FindCachedFieldItem(
	const FString& fieldId) const
{
	for (UScenarioTemplateFieldRowViewModel* fieldItem : CachedFieldItems)
	{
		if (fieldItem && fieldItem->GetItemId() == fieldId)
		{
			return fieldItem;
		}
	}
	return nullptr;
}
