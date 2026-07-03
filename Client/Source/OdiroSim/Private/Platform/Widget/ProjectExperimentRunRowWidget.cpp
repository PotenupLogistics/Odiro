#include "Platform/Widget/ProjectExperimentRunRowWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Misc/Paths.h"
#include "Platform/ViewModel/OdiroListItemViewModel.h"
#include "UI/BaseButtonWidget.h"
#include "UI/BaseProgressBarWidget.h"

namespace
{
	// Numeric project run ids are stored zero-padded on disk but shown without leading zeros.
	FString FormatProjectExperimentRunRowDisplayId(const FString& runId)
	{
		const FString trimmedRunId = runId.TrimStartAndEnd();
		if (trimmedRunId.IsEmpty())
		{
			return FString();
		}

		bool bAllDigits = true;
		int32 firstNonZeroIndex = INDEX_NONE;
		for (int32 index = 0; index < trimmedRunId.Len(); ++index)
		{
			const TCHAR character = trimmedRunId[index];
			if (!FChar::IsDigit(character))
			{
				bAllDigits = false;
				break;
			}
			if (character != TEXT('0') && firstNonZeroIndex == INDEX_NONE)
			{
				firstNonZeroIndex = index;
			}
		}

		if (!bAllDigits)
		{
			return trimmedRunId;
		}
		return firstNonZeroIndex == INDEX_NONE ? FString(TEXT("0")) : trimmedRunId.Mid(firstNonZeroIndex);
	}

	FText MakeProjectExperimentRunLabel(const FString& runId)
	{
		const FString displayId = FormatProjectExperimentRunRowDisplayId(runId);
		return displayId.IsEmpty()
			? FText::GetEmpty()
			: FText::FromString(displayId);
	}

	FText MakeProgressCountLabel(const bool bCompleted, const int32 totalCount)
	{
		const int32 clampedTotal = FMath::Max(0, totalCount);
		if (clampedTotal <= 0)
		{
			return NSLOCTEXT("ProjectExperimentRunRow", "ProgressCountUnavailable", "- / -");
		}

		const int32 completedCount = bCompleted ? clampedTotal : 0;
		return FText::FromString(FString::Printf(TEXT("%d / %d"), completedCount, clampedTotal));
	}

	EBaseWidgetState MakeProgressBarState(const ESimulationRunState state)
	{
		switch (state)
		{
		case ESimulationRunState::Completed:
			return EBaseWidgetState::Success;
		case ESimulationRunState::Failed:
		case ESimulationRunState::Canceled:
			return EBaseWidgetState::Error;
		case ESimulationRunState::Running:
			return EBaseWidgetState::Loading;
		case ESimulationRunState::Pending:
		default:
			return EBaseWidgetState::Default;
		}
	}
}

void UProjectExperimentRunRowWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyDisplayTexts();
}

void UProjectExperimentRunRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyDisplayTexts();

	if (AnalyzeButton)
	{
		AnalyzeButton->OnBaseClicked.RemoveDynamic(this, &UProjectExperimentRunRowWidget::HandleAnalyzeClicked);
		AnalyzeButton->OnBaseClicked.AddDynamic(this, &UProjectExperimentRunRowWidget::HandleAnalyzeClicked);
	}
}

void UProjectExperimentRunRowWidget::NativeDestruct()
{
	if (AnalyzeButton)
	{
		AnalyzeButton->OnBaseClicked.RemoveDynamic(this, &UProjectExperimentRunRowWidget::HandleAnalyzeClicked);
	}

	OnAnalyzeRequested.Clear();
	Super::NativeDestruct();
}

void UProjectExperimentRunRowWidget::InitializeRunRow(
	const FString& runDirectory,
	const FString& runId,
	const ESimulationRunState state,
	const bool bCompleted,
	const int32 progressTotalCount,
	const FString& successRateLabel,
	const FString& totalDurationLabel,
	const bool bCanAnalyze)
{
	ItemViewModel = nullptr;
	RunDirectory = runDirectory;
	bAnalyzeEnabled = bCanAnalyze;
	bHasRunData = true;

	SetDisplayTexts(
		MakeProjectExperimentRunLabel(runId.IsEmpty() ? FPaths::GetBaseFilename(runDirectory) : runId),
		FText::GetEmpty(),
		MakeProgressCountLabel(bCompleted, progressTotalCount),
		FText::FromString(successRateLabel),
		FText::FromString(totalDurationLabel),
		ActionDisplayText);
	SetProgressPresentation(true, true);
	SetActionPresentation(false, true);

	if (RunIdText)
	{
		const FString sourceRunId = runId.IsEmpty() ? FPaths::GetBaseFilename(runDirectory) : runId;
		RunIdText->SetText(MakeProjectExperimentRunLabel(sourceRunId));
	}

	if (ProgressCountText)
	{
		ProgressCountText->SetText(MakeProgressCountLabel(bCompleted, progressTotalCount));
	}

	if (ProgressBar)
	{
		ProgressBar->SetProgressPercent(bCompleted ? 100.0f : 0.0f);
		ProgressBar->SetBaseState(MakeProgressBarState(state));
	}

	if (SuccessRateText)
	{
		SuccessRateText->SetText(FText::FromString(successRateLabel));
	}

	if (TotalDurationText)
	{
		TotalDurationText->SetText(FText::FromString(totalDurationLabel));
	}

	if (AnalyzeButton)
	{
		AnalyzeButton->SetDisabled(!bAnalyzeEnabled);
		AnalyzeButton->SetVisibility(bAnalyzeEnabled && bShowAnalyzeButton ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (ActionLabelText)
	{
		ActionLabelText->SetVisibility(ESlateVisibility::Collapsed);
	}

	RefreshStateVisibility(state);
}

void UProjectExperimentRunRowWidget::InitializeFromItemViewModel(
	UOdiroListItemViewModel* itemViewModel,
	const ESimulationRunState state,
	const bool bCompleted,
	const int32 progressTotalCount,
	const FString& successRateLabel,
	const FString& totalDurationLabel,
	const bool bCanAnalyze)
{
	ItemViewModel = itemViewModel;
	InitializeRunRow(
		ItemViewModel ? ItemViewModel->GetPayloadPath() : FString(),
		ItemViewModel ? ItemViewModel->GetItemId() : FString(),
		state,
		bCompleted,
		progressTotalCount,
		successRateLabel,
		totalDurationLabel,
		bCanAnalyze && (!ItemViewModel || ItemViewModel->IsEnabled()));
	ItemViewModel = itemViewModel;
}

void UProjectExperimentRunRowWidget::SetDisplayTexts(
	const FText runIdText,
	const FText progressText,
	const FText progressCountText,
	const FText successRateText,
	const FText totalDurationText,
	const FText actionText)
{
	RunIdDisplayText = runIdText;
	ProgressDisplayText = progressText;
	ProgressCountDisplayText = progressCountText;
	SuccessRateDisplayText = successRateText;
	TotalDurationDisplayText = totalDurationText;
	ActionDisplayText = actionText;
	ApplyDisplayTexts();
}

void UProjectExperimentRunRowWidget::SetProgressPresentation(
	const bool bInShowProgressBar,
	const bool bInShowProgressCountText)
{
	bShowProgressBar = bInShowProgressBar;
	bShowProgressCountText = bInShowProgressCountText;
	ApplyDisplayTexts();
}

void UProjectExperimentRunRowWidget::SetActionPresentation(
	const bool bInShowActionText,
	const bool bInShowAnalyzeButton)
{
	bShowActionText = bInShowActionText;
	bShowAnalyzeButton = bInShowAnalyzeButton;
	ApplyDisplayTexts();
}

void UProjectExperimentRunRowWidget::HandleAnalyzeClicked(UBaseButtonWidget* button)
{
	if (!bAnalyzeEnabled || button != AnalyzeButton.Get())
	{
		return;
	}

	OnAnalyzeRequested.Broadcast(this);
}

void UProjectExperimentRunRowWidget::ApplyDisplayTexts() const
{
	if (RunIdText && !RunIdDisplayText.IsEmpty())
	{
		RunIdText->SetText(RunIdDisplayText);
	}
	if (ProgressCountText)
	{
		if (!ProgressCountDisplayText.IsEmpty())
		{
			ProgressCountText->SetText(ProgressCountDisplayText);
		}
		else if (!ProgressDisplayText.IsEmpty())
		{
			ProgressCountText->SetText(ProgressDisplayText);
		}
		ProgressCountText->SetVisibility(bShowProgressCountText ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (ProgressBar)
	{
		ProgressBar->SetVisibility(bShowProgressBar ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (SuccessRateText && !SuccessRateDisplayText.IsEmpty())
	{
		SuccessRateText->SetText(SuccessRateDisplayText);
	}
	if (TotalDurationText && !TotalDurationDisplayText.IsEmpty())
	{
		TotalDurationText->SetText(TotalDurationDisplayText);
	}
	if (ActionLabelText)
	{
		ActionLabelText->SetText(ActionDisplayText);
		ActionLabelText->SetVisibility(bShowActionText ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (AnalyzeButton)
	{
		if (!ActionDisplayText.IsEmpty())
		{
			AnalyzeButton->SetLabel(ActionDisplayText);
		}
		AnalyzeButton->SetVisibility(bShowAnalyzeButton ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UProjectExperimentRunRowWidget::RefreshStateVisibility(const ESimulationRunState state) const
{
	SetStateBoxVisibility(PendingStateBox.Get(), state == ESimulationRunState::Pending);
	SetStateBoxVisibility(RunningStateBox.Get(), state == ESimulationRunState::Running);
	SetStateBoxVisibility(CompletedStateBox.Get(), state == ESimulationRunState::Completed);
	SetStateBoxVisibility(FailedStateBox.Get(), state == ESimulationRunState::Failed);
	SetStateBoxVisibility(CanceledStateBox.Get(), state == ESimulationRunState::Canceled);
}

void UProjectExperimentRunRowWidget::SetStateBoxVisibility(UWidget* stateBox, const bool bVisible)
{
	if (!stateBox)
	{
		return;
	}

	stateBox->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}
