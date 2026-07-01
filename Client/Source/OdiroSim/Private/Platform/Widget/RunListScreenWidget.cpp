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
#include "UI/BaseSliderComboWidget.h"
#include "UI/BaseTextInputWidget.h"
#include "UI/BaseTextWidget.h"

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

	void SetNumberInputValue(UBaseTextInputWidget* input, const int32 value)
	{
		if (!input)
		{
			return;
		}

		input->SetText(FText::AsNumber(value));
		input->SetNumericValue(static_cast<float>(value));
	}

	void SetNumberInputValue(UBaseTextInputWidget* input, const int64 value)
	{
		if (!input)
		{
			return;
		}

		input->SetText(FText::AsNumber(value));
		input->SetNumericValue(static_cast<float>(value));
	}

	void SetSliderComboValue(UBaseSliderComboWidget* input, const int64 value)
	{
		if (input)
		{
			input->SetValue(static_cast<float>(value));
		}
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

	bool TryReadPositiveIntInput(
		const UBaseSliderComboWidget* sliderCombo,
		const UBaseTextInputWidget* textInput,
		const FString& label,
		int32& outValue,
		TArray<FString>& outDiagnostics)
	{
		if (sliderCombo)
		{
			outValue = FMath::RoundToInt(sliderCombo->GetValue());
			if (outValue <= 0)
			{
				outDiagnostics.Add(FString::Printf(TEXT("%s은 1 이상의 정수여야 합니다."), *label));
				return false;
			}
			return true;
		}

		return TryParsePositiveIntInput(textInput, label, outValue, outDiagnostics);
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

	struct FRunListDashboardLabels
	{
		FString SuccessRateLabel = TEXT("-");
		FString TotalDurationLabel = TEXT("-");
	};

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
			labels.SuccessRateLabel = FString::Printf(
				TEXT("%.0f%%"),
				100.0 * static_cast<double>(dashboardData.SuccessCount) / dashboardData.EpisodeCount);
		}
		labels.TotalDurationLabel = FString::Printf(TEXT("%.1f s"), dashboardData.TotalDurationSeconds);
		return labels;
	}
}

void URunListScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (RefreshButton)
	{
		RefreshButton->OnBaseClicked.RemoveDynamic(this, &URunListScreenWidget::HandleRefreshClicked);
		RefreshButton->OnBaseClicked.AddDynamic(this, &URunListScreenWidget::HandleRefreshClicked);
	}
	if (StartRunButton)
	{
		StartRunButton->OnBaseClicked.RemoveDynamic(this, &URunListScreenWidget::HandleRunClicked);
		StartRunButton->OnBaseClicked.AddDynamic(this, &URunListScreenWidget::HandleRunClicked);
	}
	if (AnalyzeRunButton)
	{
		AnalyzeRunButton->OnBaseClicked.RemoveDynamic(this, &URunListScreenWidget::HandleAnalyzeClicked);
		AnalyzeRunButton->OnBaseClicked.AddDynamic(this, &URunListScreenWidget::HandleAnalyzeClicked);
	}
	if (OpenRunDetailButton)
	{
		OpenRunDetailButton->OnBaseClicked.RemoveDynamic(this, &URunListScreenWidget::HandleOpenDetailClicked);
		OpenRunDetailButton->OnBaseClicked.AddDynamic(this, &URunListScreenWidget::HandleOpenDetailClicked);
	}

	RefreshFromViewModels();
}

void URunListScreenWidget::NativeDestruct()
{
	if (RefreshButton)
	{
		RefreshButton->OnBaseClicked.RemoveDynamic(this, &URunListScreenWidget::HandleRefreshClicked);
	}
	if (StartRunButton)
	{
		StartRunButton->OnBaseClicked.RemoveDynamic(this, &URunListScreenWidget::HandleRunClicked);
	}
	if (AnalyzeRunButton)
	{
		AnalyzeRunButton->OnBaseClicked.RemoveDynamic(this, &URunListScreenWidget::HandleAnalyzeClicked);
	}
	if (OpenRunDetailButton)
	{
		OpenRunDetailButton->OnBaseClicked.RemoveDynamic(this, &URunListScreenWidget::HandleOpenDetailClicked);
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
		if (ProjectPathText)
		{
			ProjectPathText->SetText(FText::FromString(workspaceViewModel->GetActiveProjectPath()));
		}
		if (SelectedRunText)
		{
			SelectedRunText->SetText(FText::FromString(workspaceViewModel->GetSelectedRunId()));
		}
		if (StatusText)
		{
			StatusText->SetText(FText::FromString(workspaceViewModel->GetStatusText()));
		}
	}

	if (configViewModel && configViewModel->LoadFromActiveProject())
	{
		if (FixedFpsInput)
		{
			SetNumberInputValue(FixedFpsInput.Get(), configViewModel->GetFixedFps());
		}
		if (FixedFpsSliderCombo)
		{
			SetSliderComboValue(FixedFpsSliderCombo.Get(), configViewModel->GetFixedFps());
		}
		if (EpisodeCountInput)
		{
			SetNumberInputValue(EpisodeCountInput.Get(), configViewModel->GetEpisodeCount());
		}
		if (BaseSeedInput)
		{
			SetNumberInputValue(BaseSeedInput.Get(), configViewModel->GetBaseSeed());
		}
	}

	RebuildRunRows();
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

bool URunListScreenWidget::OpenSelectedRunDetail()
{
	UProjectWorkspaceViewModel* workspaceViewModel = ResolveWorkspaceViewModel();
	if (!workspaceViewModel || workspaceViewModel->GetSelectedRunId().IsEmpty())
	{
		return false;
	}

	OnRunDetailRequested.Broadcast(this, workspaceViewModel->GetSelectedRunId());
	return true;
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

bool URunListScreenWidget::SaveExperimentSettings()
{
	UExperimentConfigViewModel* configViewModel = ResolveExperimentConfigViewModel();
	if (!configViewModel)
	{
		if (StatusText)
		{
			StatusText->SetText(NSLOCTEXT("OdiroPlatform", "RunListConfigVmMissing", "ExperimentConfig ViewModel 없음"));
		}
		return false;
	}

	TArray<FString> diagnostics;
	int32 fixedFps = 0;
	int32 episodeCount = 0;
	int64 baseSeed = 0;
	const bool bInputsValid =
		TryReadPositiveIntInput(FixedFpsSliderCombo.Get(), FixedFpsInput.Get(), TEXT("Fixed FPS"), fixedFps, diagnostics)
		& TryParsePositiveIntInput(EpisodeCountInput.Get(), TEXT("Episode Count"), episodeCount, diagnostics)
		& TryParseInt64Input(BaseSeedInput.Get(), TEXT("Base Seed"), baseSeed, diagnostics);
	if (!bInputsValid)
	{
		if (StatusText)
		{
			StatusText->SetText(FText::FromString(FString::Join(diagnostics, TEXT("\n"))));
		}
		return false;
	}

	configViewModel->SetMapId(FExperimentConfigSettings().MapId);
	configViewModel->SetFixedFps(fixedFps);
	configViewModel->SetEpisodeCount(episodeCount);
	configViewModel->SetBaseSeed(baseSeed);
	const bool bSaved = configViewModel->SaveExperimentSettings();
	if (StatusText && !bSaved)
	{
		StatusText->SetText(FText::FromString(configViewModel->GetDiagnosticsText()));
	}
	return bSaved;
}

void URunListScreenWidget::RebuildRunRows()
{
	UProjectWorkspaceViewModel* workspaceViewModel = ResolveWorkspaceViewModel();
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

	ClearRunRows();

	for (UOdiroListItemViewModel* item : workspaceViewModel->GetRunItems())
	{
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

		const ESimulationRunState runState = ResolveRunState(item->GetPayloadPath());
		const FRunListDashboardLabels dashboardLabels = MakeRunListDashboardLabels(item->GetPayloadPath());
		rowWidget->InitializeFromItemViewModel(
			item,
			runState,
			runState == ESimulationRunState::Completed,
			dashboardLabels.SuccessRateLabel,
			dashboardLabels.TotalDurationLabel,
			runState == ESimulationRunState::Completed);
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
	if (RunRowListBox)
	{
		RunRowListBox->ClearChildren();
	}
}

void URunListScreenWidget::HandleRefreshClicked(UBaseButtonWidget* button)
{
	if (IsValid(button) && button == RefreshButton)
	{
		RefreshFromViewModels();
	}
}

void URunListScreenWidget::HandleRunClicked(UBaseButtonWidget* button)
{
	if (IsValid(button) && button == StartRunButton)
	{
		StartNewRun();
	}
}

void URunListScreenWidget::HandleAnalyzeClicked(UBaseButtonWidget* button)
{
	if (IsValid(button) && button == AnalyzeRunButton)
	{
		RequestAnalysisForSelectedRun();
	}
}

void URunListScreenWidget::HandleOpenDetailClicked(UBaseButtonWidget* button)
{
	if (IsValid(button) && button == OpenRunDetailButton)
	{
		OpenSelectedRunDetail();
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
