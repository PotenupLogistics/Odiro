#include "Platform/Widget/ProjectExperimentRunRowWidget.h"

#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Misc/Paths.h"

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
}

void UProjectExperimentRunRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (AnalyzeButton)
	{
		AnalyzeButton->OnClicked.RemoveDynamic(this, &UProjectExperimentRunRowWidget::HandleAnalyzeClicked);
		AnalyzeButton->OnClicked.AddDynamic(this, &UProjectExperimentRunRowWidget::HandleAnalyzeClicked);
	}
}

void UProjectExperimentRunRowWidget::NativeDestruct()
{
	if (AnalyzeButton)
	{
		AnalyzeButton->OnClicked.RemoveDynamic(this, &UProjectExperimentRunRowWidget::HandleAnalyzeClicked);
	}

	OnAnalyzeRequested.Clear();
	Super::NativeDestruct();
}

void UProjectExperimentRunRowWidget::InitializeRunRow(
	const FString& runDirectory,
	const FString& runId,
	const ESimulationRunState state,
	const float progressPercent,
	const bool bCanAnalyze)
{
	RunDirectory = runDirectory;
	bAnalyzeEnabled = bCanAnalyze;

	if (RunIdText)
	{
		const FString sourceRunId = runId.IsEmpty() ? FPaths::GetBaseFilename(runDirectory) : runId;
		RunIdText->SetText(FText::FromString(FormatProjectExperimentRunRowDisplayId(sourceRunId)));
	}

	const float clampedProgress = FMath::Clamp(progressPercent, 0.0f, 100.0f);
	if (ProgressBar)
	{
		ProgressBar->SetPercent(clampedProgress / 100.0f);
	}

	if (ProgressText)
	{
		const int32 roundedProgress = FMath::RoundToInt(clampedProgress);
		ProgressText->SetText(FText::Format(NSLOCTEXT("ProjectExperimentRunRow", "ProgressPercent", "{0}%"), roundedProgress));
	}

	if (AnalyzeButton)
	{
		AnalyzeButton->SetIsEnabled(bAnalyzeEnabled);
		AnalyzeButton->SetVisibility(bAnalyzeEnabled ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	RefreshStateVisibility(state);
}

void UProjectExperimentRunRowWidget::HandleAnalyzeClicked()
{
	if (!bAnalyzeEnabled)
	{
		return;
	}

	OnAnalyzeRequested.Broadcast(this);
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
