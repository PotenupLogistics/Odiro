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

namespace
{
	// RunList 화면이 열려 있는 동안 결과 목록을 다시 읽는 간격.
	constexpr float RunListResultsPollingIntervalSeconds = 2.5f;

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

	void ConfigureNumberInput(
		UBaseTextInputWidget* input,
		const float minValue,
		const float maxValue,
		const int32 displayDecimals)
	{
		if (!input)
		{
			return;
		}

		input->SetInputMode(EBaseTextInputMode::Number);
		input->SetValueRange(minValue, maxValue);
		input->SetDisplayDecimals(displayDecimals);
	}

	void SetNumberInputValue(
		UBaseTextInputWidget* input,
		const float value,
		const float minValue,
		const float maxValue,
		const int32 displayDecimals)
	{
		if (!input)
		{
			return;
		}

		ConfigureNumberInput(input, minValue, maxValue, displayDecimals);
		input->SetNumericValue(value);
	}

	void SetNumberInputValue(
		UBaseTextInputWidget* input,
		const int32 value,
		const float minValue,
		const float maxValue)
	{
		SetNumberInputValue(input, static_cast<float>(value), minValue, maxValue, 0);
	}

	void SetNumberInputValue(
		UBaseTextInputWidget* input,
		const int64 value,
		const float minValue,
		const float maxValue)
	{
		SetNumberInputValue(input, static_cast<float>(value), minValue, maxValue, 0);
	}

	bool TryParsePositiveIntInput(
		const UBaseTextInputWidget* input,
		const FString& label,
		int32& outValue,
		TArray<FString>& outDiagnostics)
	{
		const FString text = GetInputText(input);
		if (!LexTryParseString(outValue, *text) || outValue <= 0)
		{
			outDiagnostics.Add(FString::Printf(TEXT("%s은 1 이상의 정수여야 합니다."), *label));
			return false;
		}
		return true;
	}

	bool TryParsePositiveNumberInput(
		const UBaseTextInputWidget* input,
		const FString& label,
		float& outValue,
		TArray<FString>& outDiagnostics)
	{
		const FString text = GetInputText(input);
		if (!LexTryParseString(outValue, *text) || outValue <= 0.0f)
		{
			outDiagnostics.Add(FString::Printf(TEXT("%s은 0보다 큰 숫자여야 합니다."), *label));
			return false;
		}
		return true;
	}

	bool TryParseNonNegativeNumberInput(
		const UBaseTextInputWidget* input,
		const FString& label,
		float& outValue,
		TArray<FString>& outDiagnostics)
	{
		const FString text = GetInputText(input);
		if (!LexTryParseString(outValue, *text) || outValue < 0.0f)
		{
			outDiagnostics.Add(FString::Printf(TEXT("%s은 0 이상의 숫자여야 합니다."), *label));
			return false;
		}
		return true;
	}

	bool TryParseRangedNumberInput(
		const UBaseTextInputWidget* input,
		const FString& label,
		const float minValue,
		const float maxValue,
		float& outValue,
		TArray<FString>& outDiagnostics)
	{
		const FString text = GetInputText(input);
		if (!LexTryParseString(outValue, *text))
		{
			outDiagnostics.Add(FString::Printf(TEXT("%s은 숫자여야 합니다."), *label));
			return false;
		}

		if (outValue < minValue || outValue > maxValue)
		{
			outDiagnostics.Add(FString::Printf(TEXT("%s은 %.0f~%.0f 범위여야 합니다."), *label, minValue, maxValue));
			return false;
		}
		return true;
	}

	bool TryParseInt64Input(
		const UBaseTextInputWidget* input,
		const FString& label,
		int64& outValue,
		TArray<FString>& outDiagnostics)
	{
		const FString text = GetInputText(input);
		if (!LexTryParseString(outValue, *text))
		{
			outDiagnostics.Add(FString::Printf(TEXT("%s은 정수여야 합니다."), *label));
			return false;
		}
		return true;
	}

	// run summary dashboard에서 row에 표시할 문자열 묶음.
	struct FRunListDashboardLabels
	{
		FString SuccessRateLabel = TEXT("-");
		FString TotalDurationLabel = TEXT("-");
		int32 EpisodeCount = 0;
	};

	// 한 row를 그릴 때 필요한 파일 기반 결과 snapshot.
	struct FRunListRenderedRowData
	{
		UOdiroListItemViewModel* Item = nullptr;
		ESimulationRunState RunState = ESimulationRunState::Pending;
		FRunListDashboardLabels DashboardLabels;
		int32 ProgressTotalCount = 0;
	};

	// status.json과 summary.json을 기준으로 run state를 계산한다.
	ESimulationRunState ResolveRunListState(const FString& runDirectory)
	{
		const FString normalizedRunDirectory = NormalizeRunListPath(runDirectory);
		const FString statusPath = NormalizeRunListPath(FPaths::Combine(normalizedRunDirectory, TEXT("status.json")));
		ESimulationRunState runState = ESimulationRunState::Pending;
		if (UPlatformUiSubsystem::TryReadBridgeRunStatusState(statusPath, runState))
		{
			return runState;
		}

		const FString summaryPath = NormalizeRunListPath(FPaths::Combine(normalizedRunDirectory, TEXT("summary.json")));
		return UPlatformUiSubsystem::DoesResolvedFileExist(summaryPath)
			? ESimulationRunState::Completed
			: ESimulationRunState::Pending;
	}

	// summary dashboard를 RunList row label 값으로 변환한다.
	FRunListDashboardLabels MakeRunListDashboardLabels(const FString& runDirectory)
	{
		FRunListDashboardLabels labels;

		FProjectRunResultDashboardData dashboardData;
		if (!UPlatformUiSubsystem::LoadProjectRunDashboard(
			NormalizeRunListPath(runDirectory),
			dashboardData))
		{
			return labels;
		}

		if (dashboardData.EpisodeCount > 0)
		{
			labels.EpisodeCount = dashboardData.EpisodeCount;
			labels.SuccessRateLabel = FString::Printf(
				TEXT("%.0f%%"),
				100.0 * static_cast<double>(dashboardData.SuccessCount) / dashboardData.EpisodeCount);
		}
		labels.TotalDurationLabel = FString::Printf(TEXT("%.1f s"), dashboardData.TotalDurationSeconds);
		return labels;
	}

	// ViewModel item과 파일 기반 결과 snapshot을 render-ready row data로 결합한다.
	TArray<FRunListRenderedRowData> BuildRunListRenderedRows(
		const TArray<UOdiroListItemViewModel*>& runItems,
		const UExperimentConfigViewModel* configViewModel)
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
			rowData.RunState = ResolveRunListState(item->GetPayloadPath());
			rowData.DashboardLabels = MakeRunListDashboardLabels(item->GetPayloadPath());
			rowData.ProgressTotalCount = rowData.DashboardLabels.EpisodeCount > 0
				? rowData.DashboardLabels.EpisodeCount
				: configuredEpisodeCount;
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
				TEXT("%s|%s|%d|%s|%s|%d|%d|%d"),
				*item->GetItemId(),
				*NormalizeRunListPath(item->GetPayloadPath()),
				static_cast<int32>(rowData.RunState),
				*rowData.DashboardLabels.SuccessRateLabel,
				*rowData.DashboardLabels.TotalDurationLabel,
				rowData.DashboardLabels.EpisodeCount,
				rowData.ProgressTotalCount,
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
			SetNumberInputValue(FixedFpsInput.Get(), configViewModel->GetFixedFps(), 1.0f, 1000.0f);
		}
		if (TimeScaleInput)
		{
			SetNumberInputValue(TimeScaleInput.Get(), configViewModel->GetTimeScale(), UE_SMALL_NUMBER, 100.0f, 2);
		}
		if (MaxDurationInput)
		{
			SetNumberInputValue(MaxDurationInput.Get(), configViewModel->GetMaxDurationSeconds(), 0.0f, 86400.0f, 1);
		}
		if (EpisodeCountInput)
		{
			SetNumberInputValue(EpisodeCountInput.Get(), configViewModel->GetEpisodeCount(), 1.0f, 100000.0f);
		}
		if (BaseSeedInput)
		{
			SetNumberInputValue(BaseSeedInput.Get(), configViewModel->GetBaseSeed(), -1000000000000.0f, 1000000000000.0f);
		}
		if (TipOverAngleInput)
		{
			SetNumberInputValue(TipOverAngleInput.Get(), configViewModel->GetTipOverAngleDegrees(), 10.0f, 120.0f, 0);
		}
		if (NearMissDistanceInput)
		{
			SetNumberInputValue(NearMissDistanceInput.Get(), configViewModel->GetNearMissDistanceMeters(), 0.0f, 1000.0f, 2);
		}
		if (GoalAcceptanceRadiusInput)
		{
			SetNumberInputValue(GoalAcceptanceRadiusInput.Get(), configViewModel->GetGoalAcceptanceRadiusMeters(), UE_SMALL_NUMBER, 1000.0f, 2);
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
	world->GetTimerManager().SetTimer(
		RunResultsPollingTimerHandle,
		this,
		&URunListScreenWidget::HandleRunResultsPollingTick,
		RunListResultsPollingIntervalSeconds,
		true,
		RunListResultsPollingIntervalSeconds);
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
		TryParsePositiveIntInput(FixedFpsInput.Get(), TEXT("FPS"), fixedFps, diagnostics)
		& TryParsePositiveNumberInput(TimeScaleInput.Get(), TEXT("배속"), timeScale, diagnostics)
		& TryParseNonNegativeNumberInput(MaxDurationInput.Get(), TEXT("제한 시간"), maxDurationSeconds, diagnostics)
		& TryParsePositiveIntInput(EpisodeCountInput.Get(), TEXT("반복 횟수"), episodeCount, diagnostics)
		& TryParseInt64Input(BaseSeedInput.Get(), TEXT("랜덤 상수"), baseSeed, diagnostics)
		& TryParseRangedNumberInput(TipOverAngleInput.Get(), TEXT("전복 각도"), 10.0f, 120.0f, tipOverAngleDegrees, diagnostics)
		& TryParseNonNegativeNumberInput(NearMissDistanceInput.Get(), TEXT("근접 허용"), nearMissDistanceMeters, diagnostics)
		& TryParsePositiveNumberInput(GoalAcceptanceRadiusInput.Get(), TEXT("도착 판정 거리"), goalAcceptanceRadiusMeters, diagnostics);
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
		configViewModel);
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
	return ResolveRunListState(runDirectory);
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
