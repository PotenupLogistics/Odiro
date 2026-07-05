#include "Platform/Widget/RunListScreenWidget.h"

#include "Components/VerticalBox.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Platform/ExperimentConfigSettings.h"
#include "Platform/PlatformUiSubsystem.h"
#include "Platform/ProjectRunResultDashboard.h"
#include "Platform/ViewModel/ExperimentConfigViewModel.h"
#include "Platform/ViewModel/OdiroListItemViewModel.h"
#include "Platform/ViewModel/ProjectWorkspaceViewModel.h"
#include "Platform/Widget/ProjectExperimentRunRowWidget.h"
#include "UI/BaseButtonWidget.h"
#include "UI/BaseTextInputWidget.h"
#include "Components/TextBlock.h"

namespace
{
	FString NormalizeRunListPath(FString path)
	{
		path = path.TrimStartAndEnd();
		if (path.IsEmpty())
		{
			return FString();
		}

		path = FPaths::IsRelative(path)
			? FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), path))
			: FPaths::ConvertRelativePathToFull(path);
		FPaths::NormalizeFilename(path);
		return path;
	}

	FString GetInputText(const UBaseTextInputWidget* input)
	{
		return input ? input->GetCurrentText().ToString().TrimStartAndEnd() : FString();
	}

	FText GetInputLabelText(const UTextBlock* labelWidget)
	{
		return labelWidget ? labelWidget->GetText() : FText::GetEmpty();
	}

	FText FormatNumberForDisplay(const double value, const int32 displayDecimals)
	{
		if (displayDecimals < 0)
		{
			return FText::AsNumber(value);
		}

		FNumberFormattingOptions options;
		options.MinimumFractionalDigits = displayDecimals;
		options.MaximumFractionalDigits = displayDecimals;
		return FText::AsNumber(value, &options);
	}

	FText FormatRangeBoundaryForDiagnostics(const double value)
	{
		FString text = FString::SanitizeFloat(value);
		text.RemoveFromEnd(TEXT(".0"));
		return FText::FromString(text);
	}

	void AddFormattedDiagnostic(
		TArray<FString>& outDiagnostics,
		const FText& format,
		const FText& field)
	{
		FFormatNamedArguments args;
		args.Add(TEXT("Field"), field);
		outDiagnostics.Add(FText::Format(format, args).ToString());
	}

	void AddRangeDiagnostic(
		TArray<FString>& outDiagnostics,
		const UBaseTextInputWidget* input,
		const FText& label,
		const FText& format)
	{
		FFormatNamedArguments args;
		args.Add(TEXT("Field"), label);
		args.Add(TEXT("Min"), FormatRangeBoundaryForDiagnostics(input ? input->GetMinValue() : 0.0));
		args.Add(TEXT("Max"), FormatRangeBoundaryForDiagnostics(input ? input->GetMaxValue() : 0.0));
		outDiagnostics.Add(FText::Format(format, args).ToString());
	}

	bool IsWithinInputRange(const UBaseTextInputWidget* input, const double value)
	{
		return !input || (value >= input->GetMinValue() && value <= input->GetMaxValue());
	}

	void SetNumberInputValue(UBaseTextInputWidget* input, const float value)
	{
		if (!input)
		{
			return;
		}

		input->SetNumericValue(value);
	}

	void SetNumberInputValue(UBaseTextInputWidget* input, const int32 value)
	{
		SetNumberInputValue(input, static_cast<float>(value));
	}

	void SetNumberInputValue(UBaseTextInputWidget* input, const int64 value)
	{
		SetNumberInputValue(input, static_cast<float>(value));
	}

	bool TryParseIntegerInput(
		const UBaseTextInputWidget* input,
		const UTextBlock* labelWidget,
		const FText& integerErrorFormat,
		const FText& rangeErrorFormat,
		int32& outValue,
		TArray<FString>& outDiagnostics)
	{
		const FString text = GetInputText(input);
		const FText label = GetInputLabelText(labelWidget);
		if (!LexTryParseString(outValue, *text))
		{
			AddFormattedDiagnostic(outDiagnostics, integerErrorFormat, label);
			return false;
		}

		if (!IsWithinInputRange(input, outValue))
		{
			AddRangeDiagnostic(outDiagnostics, input, label, rangeErrorFormat);
			return false;
		}
		return true;
	}

	bool TryParseNumberInput(
		const UBaseTextInputWidget* input,
		const UTextBlock* labelWidget,
		const FText& numberErrorFormat,
		const FText& rangeErrorFormat,
		float& outValue,
		TArray<FString>& outDiagnostics)
	{
		const FString text = GetInputText(input);
		const FText label = GetInputLabelText(labelWidget);
		if (!LexTryParseString(outValue, *text))
		{
			AddFormattedDiagnostic(outDiagnostics, numberErrorFormat, label);
			return false;
		}

		if (!IsWithinInputRange(input, outValue))
		{
			AddRangeDiagnostic(outDiagnostics, input, label, rangeErrorFormat);
			return false;
		}
		return true;
	}

	bool TryParseInteger64Input(
		const UBaseTextInputWidget* input,
		const UTextBlock* labelWidget,
		const FText& integerErrorFormat,
		int64& outValue,
		TArray<FString>& outDiagnostics)
	{
		const FString text = GetInputText(input);
		const FText label = GetInputLabelText(labelWidget);
		if (!LexTryParseString(outValue, *text))
		{
			AddFormattedDiagnostic(outDiagnostics, integerErrorFormat, label);
			return false;
		}
		return true;
	}

	FString FormatRunListText(const FText& format, const TCHAR* key, const FText& value)
	{
		FFormatNamedArguments args;
		args.Add(key, value);
		return FText::Format(format, args).ToString();
	}

	FText FormatRunListProgressCountLabel(
		const int32 completedCount,
		const int32 totalCount,
		const FText& format)
	{
		FFormatNamedArguments args;
		args.Add(TEXT("Completed"), FText::AsNumber(FMath::Max(0, completedCount)));
		args.Add(TEXT("Total"), FText::AsNumber(FMath::Max(0, totalCount)));
		return FText::Format(format, args);
	}

	struct FRunListTextOptions
	{
		FText EmptyRunMetricText;
		FText SuccessRateFormat;
		int32 SuccessRateDisplayDecimals = 0;
		FText TotalDurationFormat;
		int32 TotalDurationDisplayDecimals = 1;
		FText ProgressCountFormat;
		FText ProgressErrorText;
		FText ProgressStartingText;
		FText ProgressCanceledText;
		FText ProgressCompletedText;
		FText ProgressFailedText;
		FText ProgressPendingText;
	};

	// run summary dashboard에서 row에 표시할 문자열 묶음.
	struct FRunListDashboardLabels
	{
		FString SuccessRateLabel;
		FString TotalDurationLabel;
		int32 EpisodeCount = 0;
		bool bSummaryLoaded = false;
	};

	// Run row progress column에 적용할 표시값 묶음.
	struct FRunListProgressPresentation
	{
		int32 CompletedCount = 0;
		int32 TotalCount = 0;
		float Percent = 0.0f;
		FText Label;
		EProjectExperimentRunRowProgressState State = EProjectExperimentRunRowProgressState::Default;
	};

	// 한 row를 그릴 때 필요한 파일 기반 결과 snapshot.
	struct FRunListRenderedRowData
	{
		UOdiroListItemViewModel* Item = nullptr;
		ESimulationRunState RunState = ESimulationRunState::Pending;
		FRunListDashboardLabels DashboardLabels;
		int32 ProgressCompletedCount = 0;
		int32 ProgressTotalCount = 0;
		float ProgressPercent = 0.0f;
		FText ProgressLabel;
		EProjectExperimentRunRowProgressState ProgressState = EProjectExperimentRunRowProgressState::Default;
	};

	// summary dashboard를 RunList row label 값으로 변환한다.
	FRunListDashboardLabels MakeRunListDashboardLabels(
		const FString& runDirectory,
		const FRunListTextOptions& textOptions)
	{
		FRunListDashboardLabels labels;
		labels.SuccessRateLabel = textOptions.EmptyRunMetricText.ToString();
		labels.TotalDurationLabel = textOptions.EmptyRunMetricText.ToString();

		FProjectRunResultDashboardData dashboardData;
		if (!UPlatformUiSubsystem::LoadProjectRunDashboard(
			NormalizeRunListPath(runDirectory),
			dashboardData))
		{
			return labels;
		}

		labels.bSummaryLoaded = dashboardData.bSummaryLoaded;
		if (dashboardData.EpisodeCount > 0)
		{
			labels.EpisodeCount = dashboardData.EpisodeCount;
			labels.SuccessRateLabel = FormatRunListText(
				textOptions.SuccessRateFormat,
				TEXT("Percent"),
				FormatNumberForDisplay(
					100.0 * static_cast<double>(dashboardData.SuccessCount) / dashboardData.EpisodeCount,
					textOptions.SuccessRateDisplayDecimals));
		}
		labels.TotalDurationLabel = FormatRunListText(
			textOptions.TotalDurationFormat,
			TEXT("Seconds"),
			FormatNumberForDisplay(dashboardData.TotalDurationSeconds, textOptions.TotalDurationDisplayDecimals));
		return labels;
	}

	FRunListProgressPresentation MakeRunListProgressPresentation(
		const FPlatformProjectRunProgressSnapshot& progressSnapshot,
		const FRunListTextOptions& textOptions)
	{
		FRunListProgressPresentation presentation;
		presentation.CompletedCount = FMath::Max(0, progressSnapshot.CompletedCount);
		presentation.TotalCount = FMath::Max(FMath::Max(0, progressSnapshot.TotalCount), presentation.CompletedCount);
		presentation.Percent = FMath::Clamp(progressSnapshot.Percent, 0.0f, 100.0f);

		switch (progressSnapshot.ProgressKind)
		{
		case EPlatformProjectRunProgressKind::Error:
			presentation.Label = textOptions.ProgressErrorText;
			presentation.State = EProjectExperimentRunRowProgressState::Error;
			return presentation;
		case EPlatformProjectRunProgressKind::Starting:
			presentation.Label = textOptions.ProgressStartingText;
			presentation.Percent = 0.0f;
			presentation.State = EProjectExperimentRunRowProgressState::Loading;
			return presentation;
		case EPlatformProjectRunProgressKind::Running:
			presentation.Label = FormatRunListProgressCountLabel(
				presentation.CompletedCount,
				presentation.TotalCount,
				textOptions.ProgressCountFormat);
			presentation.State = EProjectExperimentRunRowProgressState::Loading;
			return presentation;
		case EPlatformProjectRunProgressKind::Canceled:
			presentation.Label = textOptions.ProgressCanceledText;
			presentation.State = EProjectExperimentRunRowProgressState::Warning;
			return presentation;
		case EPlatformProjectRunProgressKind::Completed:
			presentation.Label = textOptions.ProgressCompletedText;
			presentation.Percent = 100.0f;
			presentation.State = EProjectExperimentRunRowProgressState::Success;
			return presentation;
		case EPlatformProjectRunProgressKind::Failed:
			presentation.Label = textOptions.ProgressFailedText;
			presentation.State = EProjectExperimentRunRowProgressState::Error;
			return presentation;
		case EPlatformProjectRunProgressKind::Pending:
		default:
			break;
		}

		presentation.Label = textOptions.ProgressPendingText;
		presentation.Percent = 0.0f;
		presentation.State = EProjectExperimentRunRowProgressState::Default;
		return presentation;
	}

	// ViewModel item과 파일 기반 결과 snapshot을 render-ready row data로 결합한다.
	TArray<FRunListRenderedRowData> BuildRunListRenderedRows(
		const TArray<UOdiroListItemViewModel*>& runItems,
		const UExperimentConfigViewModel* configViewModel,
		const UPlatformUiSubsystem* platformUiSubsystem,
		const FRunListTextOptions& textOptions)
	{
		const int32 configuredEpisodeCount = configViewModel ? FMath::Max(1, configViewModel->GetEpisodeCount()) : 0;
		TArray<FRunListRenderedRowData> renderedRows;
		renderedRows.Reserve(runItems.Num());

		for (UOdiroListItemViewModel* item : runItems)
		{
			if (!item)
			{
				continue;
			}

			FRunListRenderedRowData rowData;
			rowData.Item = item;
			const FString runDirectory = item->GetPayloadPath();
			rowData.DashboardLabels = MakeRunListDashboardLabels(runDirectory, textOptions);
			FPlatformProjectRunProgressSnapshot progressSnapshot = platformUiSubsystem
				? platformUiSubsystem->BuildProjectRunProgressSnapshot(runDirectory, configuredEpisodeCount)
				: FPlatformProjectRunProgressSnapshot();
			if (!platformUiSubsystem)
			{
				progressSnapshot.RunState = rowData.DashboardLabels.bSummaryLoaded
					? ESimulationRunState::Completed
					: ESimulationRunState::Pending;
				progressSnapshot.ProgressKind = rowData.DashboardLabels.bSummaryLoaded
					? EPlatformProjectRunProgressKind::Completed
					: EPlatformProjectRunProgressKind::Pending;
				progressSnapshot.CompletedCount = rowData.DashboardLabels.bSummaryLoaded
					? rowData.DashboardLabels.EpisodeCount
					: 0;
				progressSnapshot.TotalCount = rowData.DashboardLabels.EpisodeCount > 0
					? rowData.DashboardLabels.EpisodeCount
					: configuredEpisodeCount;
				progressSnapshot.Percent = progressSnapshot.ProgressKind == EPlatformProjectRunProgressKind::Completed
					? 100.0f
					: 0.0f;
			}

			rowData.RunState = progressSnapshot.RunState;
			const FRunListProgressPresentation progressPresentation = MakeRunListProgressPresentation(
				progressSnapshot,
				textOptions);
			rowData.ProgressCompletedCount = progressPresentation.CompletedCount;
			rowData.ProgressTotalCount = progressPresentation.TotalCount;
			rowData.ProgressPercent = progressPresentation.Percent;
			rowData.ProgressLabel = progressPresentation.Label;
			rowData.ProgressState = progressPresentation.State;
			renderedRows.Add(rowData);
		}
		return renderedRows;
	}

	// 결과 목록이 실제로 바뀌었는지 비교할 compact signature를 만든다.
	FString BuildRunListResultsSignature(const TArray<FRunListRenderedRowData>& renderedRows)
	{
		TArray<FString> signatureLines;
		signatureLines.Reserve(renderedRows.Num());
		for (const FRunListRenderedRowData& rowData : renderedRows)
		{
			const UOdiroListItemViewModel* item = rowData.Item;
			if (!item)
			{
				continue;
			}

			signatureLines.Add(FString::Printf(
				TEXT("%s|%s|%d|%s|%s|%d|%d|%d|%.2f|%s|%d|%d"),
				*item->GetItemId(),
				*NormalizeRunListPath(item->GetPayloadPath()),
				static_cast<int32>(rowData.RunState),
				*rowData.DashboardLabels.SuccessRateLabel,
				*rowData.DashboardLabels.TotalDurationLabel,
				rowData.DashboardLabels.EpisodeCount,
				rowData.ProgressCompletedCount,
				rowData.ProgressTotalCount,
				rowData.ProgressPercent,
				*rowData.ProgressLabel.ToString(),
				static_cast<int32>(rowData.ProgressState),
				item->IsSelected() ? 1 : 0));
		}
		return FString::Join(signatureLines, TEXT("\n"));
	}
}

void URunListScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (StartRunButton)
	{
		StartRunButton->OnBaseClicked.RemoveDynamic(this, &URunListScreenWidget::HandleRunClicked);
		StartRunButton->OnBaseClicked.AddDynamic(this, &URunListScreenWidget::HandleRunClicked);
	}

	RefreshFromViewModels();
}

void URunListScreenWidget::NativeDestruct()
{
	StopRunResultsPolling();

	if (StartRunButton)
	{
		StartRunButton->OnBaseClicked.RemoveDynamic(this, &URunListScreenWidget::HandleRunClicked);
	}

	ClearRunRows();
	Super::NativeDestruct();
}

void URunListScreenWidget::RefreshFromViewModels()
{
	UProjectWorkspaceViewModel* workspaceViewModel = ResolveWorkspaceViewModel();
	UExperimentConfigViewModel* configViewModel = ResolveExperimentConfigViewModel();

	if (workspaceViewModel)
	{
		workspaceViewModel->RefreshFromProjectSession();
	}

	if (configViewModel && configViewModel->LoadFromActiveProject())
	{
		if (FixedFpsInput)
		{
			SetNumberInputValue(FixedFpsInput.Get(), configViewModel->GetFixedFps());
		}
		if (TimeScaleInput)
		{
			SetNumberInputValue(TimeScaleInput.Get(), configViewModel->GetTimeScale());
		}
		if (MaxDurationInput)
		{
			SetNumberInputValue(MaxDurationInput.Get(), configViewModel->GetMaxDurationSeconds());
		}
		if (EpisodeCountInput)
		{
			SetNumberInputValue(EpisodeCountInput.Get(), configViewModel->GetEpisodeCount());
		}
		if (BaseSeedInput)
		{
			SetNumberInputValue(BaseSeedInput.Get(), configViewModel->GetBaseSeed());
		}
		if (TipOverAngleInput)
		{
			SetNumberInputValue(TipOverAngleInput.Get(), configViewModel->GetTipOverAngleDegrees());
		}
		if (NearMissDistanceInput)
		{
			SetNumberInputValue(NearMissDistanceInput.Get(), configViewModel->GetNearMissDistanceMeters());
		}
		if (GoalAcceptanceRadiusInput)
		{
			SetNumberInputValue(GoalAcceptanceRadiusInput.Get(), configViewModel->GetGoalAcceptanceRadiusMeters());
		}
	}

	RebuildRunRows(true);
	StartRunResultsPolling();
}

bool URunListScreenWidget::StartNewRun()
{
	UProjectWorkspaceViewModel* workspaceViewModel = ResolveWorkspaceViewModel();
	if (!workspaceViewModel || !SaveExperimentSettings())
	{
		return false;
	}

	FString newRunId;
	const bool bStarted = workspaceViewModel->StartNewRun(newRunId);
	RefreshFromViewModels();
	return bStarted;
}

bool URunListScreenWidget::RequestAnalysisForSelectedRun()
{
	UProjectWorkspaceViewModel* workspaceViewModel = ResolveWorkspaceViewModel();
	if (!workspaceViewModel)
	{
		return false;
	}

	const bool bRequested = workspaceViewModel->RequestAiAnalysis();
	if (bRequested)
	{
		OnRunDetailRequested.Broadcast(this, workspaceViewModel->GetSelectedRunId());
	}
	RefreshFromViewModels();
	return bRequested;
}

UProjectWorkspaceViewModel* URunListScreenWidget::ResolveWorkspaceViewModel()
{
	if (ProjectWorkspaceViewModel)
	{
		return ProjectWorkspaceViewModel.Get();
	}

	UPlatformUiSubsystem* platformUiSubsystem = UPlatformUiSubsystem::ResolveForWorldContext(this);
	ProjectWorkspaceViewModel = platformUiSubsystem ? platformUiSubsystem->GetProjectWorkspaceViewModel() : nullptr;
	return ProjectWorkspaceViewModel.Get();
}

UExperimentConfigViewModel* URunListScreenWidget::ResolveExperimentConfigViewModel()
{
	if (ExperimentConfigViewModel)
	{
		return ExperimentConfigViewModel.Get();
	}

	UPlatformUiSubsystem* platformUiSubsystem = UPlatformUiSubsystem::ResolveForWorldContext(this);
	ExperimentConfigViewModel = platformUiSubsystem ? platformUiSubsystem->GetExperimentConfigViewModel() : nullptr;
	return ExperimentConfigViewModel.Get();
}

void URunListScreenWidget::RefreshRunResults()
{
	UProjectWorkspaceViewModel* workspaceViewModel = ResolveWorkspaceViewModel();
	if (!workspaceViewModel)
	{
		ClearRunRows();
		return;
	}

	workspaceViewModel->RefreshProjectRuns();
	RebuildRunRows();
}

void URunListScreenWidget::StartRunResultsPolling()
{
	UWorld* world = GetWorld();
	if (!world || world->WorldType == EWorldType::Editor || !IsVisible())
	{
		return;
	}

	world->GetTimerManager().ClearTimer(RunResultsPollingTimerHandle);
	const float pollingInterval = FMath::Max(RunResultsPollingIntervalSeconds, UE_SMALL_NUMBER);
	world->GetTimerManager().SetTimer(
		RunResultsPollingTimerHandle,
		this,
		&URunListScreenWidget::HandleRunResultsPollingTick,
		pollingInterval,
		true,
		pollingInterval);
}

void URunListScreenWidget::StopRunResultsPolling()
{
	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().ClearTimer(RunResultsPollingTimerHandle);
	}
	RunResultsPollingTimerHandle.Invalidate();
}

void URunListScreenWidget::HandleRunResultsPollingTick()
{
	if (!IsVisible())
	{
		StopRunResultsPolling();
		return;
	}
	RefreshRunResults();
}

bool URunListScreenWidget::SaveExperimentSettings()
{
	UExperimentConfigViewModel* configViewModel = ResolveExperimentConfigViewModel();
	if (!configViewModel)
	{
		return false;
	}

	TArray<FString> diagnostics;
	int32 fixedFps = 0;
	float timeScale = 0.0f;
	float maxDurationSeconds = 0.0f;
	int32 episodeCount = 0;
	int64 baseSeed = 0;
	float tipOverAngleDegrees = 0.0f;
	float nearMissDistanceMeters = 0.0f;
	float goalAcceptanceRadiusMeters = 0.0f;
	const bool bInputsValid =
		TryParseIntegerInput(
			FixedFpsInput.Get(),
			FixedFpsLabel.Get(),
			IntegerValidationErrorFormat,
			RangeValidationErrorFormat,
			fixedFps,
			diagnostics)
		& TryParseNumberInput(
			TimeScaleInput.Get(),
			TimeScaleLabel.Get(),
			NumberValidationErrorFormat,
			RangeValidationErrorFormat,
			timeScale,
			diagnostics)
		& TryParseNumberInput(
			MaxDurationInput.Get(),
			MaxDurationLabel.Get(),
			NumberValidationErrorFormat,
			RangeValidationErrorFormat,
			maxDurationSeconds,
			diagnostics)
		& TryParseIntegerInput(
			EpisodeCountInput.Get(),
			EpisodeCountLabel.Get(),
			IntegerValidationErrorFormat,
			RangeValidationErrorFormat,
			episodeCount,
			diagnostics)
		& TryParseInteger64Input(
			BaseSeedInput.Get(),
			BaseSeedLabel.Get(),
			IntegerValidationErrorFormat,
			baseSeed,
			diagnostics)
		& TryParseNumberInput(
			TipOverAngleInput.Get(),
			TipOverAngleLabel.Get(),
			NumberValidationErrorFormat,
			RangeValidationErrorFormat,
			tipOverAngleDegrees,
			diagnostics)
		& TryParseNumberInput(
			NearMissDistanceInput.Get(),
			NearMissDistanceLabel.Get(),
			NumberValidationErrorFormat,
			RangeValidationErrorFormat,
			nearMissDistanceMeters,
			diagnostics)
		& TryParseNumberInput(
			GoalAcceptanceRadiusInput.Get(),
			GoalAcceptanceRadiusLabel.Get(),
			NumberValidationErrorFormat,
			RangeValidationErrorFormat,
			goalAcceptanceRadiusMeters,
			diagnostics);
	if (!bInputsValid)
	{
		configViewModel->SetDiagnosticsText(FString::Join(diagnostics, TEXT("\n")));
		return false;
	}

	configViewModel->SetMapId(FExperimentConfigSettings().MapId);
	configViewModel->SetFixedFps(fixedFps);
	configViewModel->SetTimeScale(timeScale);
	configViewModel->SetMaxDurationSeconds(maxDurationSeconds);
	configViewModel->SetEpisodeCount(episodeCount);
	configViewModel->SetBaseSeed(baseSeed);
	configViewModel->SetTipOverAngleDegrees(tipOverAngleDegrees);
	configViewModel->SetNearMissDistanceMeters(nearMissDistanceMeters);
	configViewModel->SetGoalAcceptanceRadiusMeters(goalAcceptanceRadiusMeters);
	const bool bSaved = configViewModel->SaveExperimentSettings();
	return bSaved;
}

void URunListScreenWidget::RebuildRunRows(const bool bForceRebuild)
{
	UProjectWorkspaceViewModel* workspaceViewModel = ResolveWorkspaceViewModel();
	UExperimentConfigViewModel* configViewModel = ResolveExperimentConfigViewModel();
	const UPlatformUiSubsystem* platformUiSubsystem = UPlatformUiSubsystem::ResolveForWorldContext(this);
	const TSubclassOf<UProjectExperimentRunRowWidget> rowClass = ResolveRunRowWidgetClass();
	if (!RunRowListBox || !workspaceViewModel || !rowClass)
	{
		// Editor widget captures keep authored preview rows; runtime screens must not show stale data.
		if (UWorld* world = GetWorld(); !world || world->WorldType != EWorldType::Editor)
		{
			ClearRunRows();
		}
		return;
	}

	const TArray<FRunListRenderedRowData> renderedRows = BuildRunListRenderedRows(
		workspaceViewModel->GetRunItems(),
		configViewModel,
		platformUiSubsystem,
		FRunListTextOptions{
			EmptyRunMetricText,
			SuccessRateFormat,
			SuccessRateDisplayDecimals,
			TotalDurationFormat,
			TotalDurationDisplayDecimals,
			ProgressCountFormat,
			ProgressErrorText,
			ProgressStartingText,
			ProgressCanceledText,
			ProgressCompletedText,
			ProgressFailedText,
			ProgressPendingText
		});
	const FString newSignature = BuildRunListResultsSignature(renderedRows);
	if (!bForceRebuild && newSignature == RenderedRunResultsSignature && RunRows.Num() == renderedRows.Num())
	{
		return;
	}

	ClearRunRows();
	RenderedRunResultsSignature = newSignature;

	for (const FRunListRenderedRowData& rowData : renderedRows)
	{
		UOdiroListItemViewModel* item = rowData.Item;
		if (!item)
		{
			continue;
		}

		UProjectExperimentRunRowWidget* rowWidget =
			CreateWidget<UProjectExperimentRunRowWidget>(this, rowClass);
		if (!rowWidget)
		{
			continue;
		}

		rowWidget->InitializeFromItemViewModel(
			item,
			rowData.RunState,
			rowData.RunState == ESimulationRunState::Completed,
			rowData.ProgressTotalCount,
			rowData.DashboardLabels.SuccessRateLabel,
			rowData.DashboardLabels.TotalDurationLabel,
			rowData.RunState == ESimulationRunState::Completed);
		rowWidget->SetProgressStatus(
			rowData.ProgressPercent,
			rowData.ProgressLabel,
			rowData.ProgressState);
		rowWidget->OnAnalyzeRequested.RemoveAll(this);
		rowWidget->OnAnalyzeRequested.AddUObject(this, &URunListScreenWidget::HandleRunRowAnalyzeRequested);
		RunRowListBox->AddChild(rowWidget);
		RunRows.Add(rowWidget);
	}
}

TSubclassOf<UProjectExperimentRunRowWidget> URunListScreenWidget::ResolveRunRowWidgetClass() const
{
	return RunRowWidgetClass;
}

ESimulationRunState URunListScreenWidget::ResolveRunState(const FString& runDirectory)
{
	return UPlatformUiSubsystem::ResolveProjectRunState(NormalizeRunListPath(runDirectory));
}

void URunListScreenWidget::ClearRunRows()
{
	for (UProjectExperimentRunRowWidget* rowWidget : RunRows)
	{
		if (rowWidget)
		{
			rowWidget->OnAnalyzeRequested.RemoveAll(this);
			rowWidget->RemoveFromParent();
		}
	}
	RunRows.Reset();
	RenderedRunResultsSignature.Reset();
	if (RunRowListBox)
	{
		RunRowListBox->ClearChildren();
	}
}

void URunListScreenWidget::HandleRunClicked(UBaseButtonWidget* button)
{
	if (IsValid(button) && button == StartRunButton)
	{
		StartNewRun();
	}
}

void URunListScreenWidget::HandleRunRowAnalyzeRequested(UProjectExperimentRunRowWidget* rowWidget)
{
	UProjectWorkspaceViewModel* workspaceViewModel = ResolveWorkspaceViewModel();
	if (!workspaceViewModel || !rowWidget)
	{
		return;
	}

	const FString runId = FPaths::GetCleanFilename(NormalizeRunListPath(rowWidget->GetRunDirectory()));
	if (!runId.IsEmpty() && workspaceViewModel->SelectRun(runId))
	{
		OnRunDetailRequested.Broadcast(this, runId);
	}
}
