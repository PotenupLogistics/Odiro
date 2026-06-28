
#include "Platform/Widget/MainMenuWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Platform/PlatformUiDeveloperSettings.h"
#include "Platform/PlatformUiSubsystem.h"
#include "Platform/ProjectRunResultDashboard.h"
#include "Platform/PlatformAnalysisAiSubsystem.h"
#include "Platform/SimulatorLaunchSubsystem.h"
#include "Platform/ViewModel/ExperimentConfigViewModel.h"
#include "Platform/ViewModel/ExperimentResultItemViewModels.h"
#include "Platform/ViewModel/ExperimentResultViewModel.h"
#include "Platform/ViewModel/OdiroListItemViewModel.h"
#include "Platform/ViewModel/ProjectWorkspaceViewModel.h"
#include "Platform/Widget/ExperimentResultIterationSelectorWidget.h"
#include "Platform/Widget/FileListItemWidget.h"
#include "Platform/Widget/ProjectEpisodeReplayCardWidget.h"
#include "Platform/Widget/ProjectEpisodeReplayViewerWidget.h"
#include "Platform/Widget/ProjectExperimentRunRowWidget.h"
#include "Platform/Widget/ProjectWorkspaceTabWidget.h"
#include "Scenario/Editor/ScenarioEditorController.h"
#include "Scenario/Widget/ScenarioEditorRootWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogMainMenuWidget, Log, All);

namespace
{
	const int32 ReportPreviewCharacterLimit = 4000;
	const int32 LogPreviewEdgeLineCount = 5;
	const TCHAR* DefaultSimulationSetupPath = TEXT("Json/Input/SimulationSetupNew.json");
	const TCHAR* MainMenuDefaultSimulationMapId = TEXT("ScenarioSimulationMap");
	const TCHAR* DefaultMeasurementOutputDirectory = TEXT("Saved/AnalysisLogs");
	const TCHAR* DefaultMeasurementFilePrefix = TEXT("MeasurementLog");
	const TCHAR* DefaultReportOutputDirectory = TEXT("Json/Output");
	const TCHAR* DefaultStatusOutputPath = TEXT("Saved/SimulationRuns/latest_status.json");
	const TCHAR* MainMenuDefaultPolicySpecJsonPath = TEXT("Json/Input/PolicySpecs/PolicySpec_DefaultDelivery.json");
	const int32 DefaultFlushIntervalTicks = 60;
	const TCHAR* ScenarioSetupTemplatePath = TEXT("Json/Input/ScenarioSetupSample_0.json");
	const TCHAR* DeliveryBotTemplatePath = TEXT("Json/Input/DeliveryBotSetupSample_0.json");
	const FName MetricLabelTextName(TEXT("MetricLabelText"));
	const FName MetricValueTextName(TEXT("MetricValueText"));
	const FName MetricUnitTextName(TEXT("MetricUnitText"));
	const FName EpisodeLabelTextName(TEXT("EpisodeLabelText"));
	const FName EpisodeDurationTextName(TEXT("EpisodeDurationText"));
	const FName EpisodeSuccessStateBoxName(TEXT("EpisodeSuccessStateBox"));
	const FName EpisodeFailureStateBoxName(TEXT("EpisodeFailureStateBox"));
	const FName EpisodePreviewImageBoxName(TEXT("EpisodePreviewImageBox"));
	const FName EpisodePreviewPlaceholderBoxName(TEXT("EpisodePreviewPlaceholderBox"));
	const FName SuggestionSeverityTextName(TEXT("SuggestionSeverityText"));
	const FName SuggestionTitleRowName(TEXT("SuggestionTitleRow"));
	const FName SuggestionTitleTextName(TEXT("SuggestionTitleText"));
	const FName SuggestionMessageRowName(TEXT("SuggestionMessageRow"));
	const FName SuggestionMessageTextName(TEXT("SuggestionMessageText"));
	const FName SuggestionReasonRowName(TEXT("SuggestionReasonRow"));
	const FName SuggestionReasonTextName(TEXT("SuggestionReasonText"));
	const FName SuggestionRecommendationRowName(TEXT("SuggestionRecommendationRow"));
	const FName SuggestionRecommendationTextName(TEXT("SuggestionRecommendationText"));
	const FName SuggestionValueRowName(TEXT("SuggestionValueRow"));
	const FName SuggestionParamTextName(TEXT("SuggestionParamText"));
	const FName SuggestionCurrentTextName(TEXT("SuggestionCurrentText"));
	const FName SuggestionSuggestedTextName(TEXT("SuggestionSuggestedText"));
	const FName SuggestionHighIndicatorName(TEXT("SuggestionHighIndicator"));
	const FName SuggestionMediumIndicatorName(TEXT("SuggestionMediumIndicator"));
	const FName SuggestionLowIndicatorName(TEXT("SuggestionLowIndicator"));
	const FName SuggestionInfoIndicatorName(TEXT("SuggestionInfoIndicator"));
	const TCHAR* MainMenuUserProjectScenarioFileName = TEXT("scenario.json");
	const FName ProjectScenarioEditTabId(TEXT("ScenarioEdit"));
	const FName ProjectRobotConfigTabId(TEXT("RobotConfig"));
	const FName ProjectExperimentStatusTabId(TEXT("ExperimentStatus"));
	const FName ProjectExperimentConfigTabId(TEXT("ExperimentConfig"));
	const FName ProjectExperimentResultDetailTabId(TEXT("ExperimentResultDetail"));

	enum class EMainMenuSection : int32
	{
		Scenario = 0,
		Policy,
		ExperimentConfig,
		RunStatus,
		ExperimentResult,
	};

	// Keeps fullscreen overlays full-screen even when their WBP Canvas slot was authored with fixed offsets.
	void ApplyMainMenuFullscreenCanvasSlot(UWidget* widget)
	{
		if (UCanvasPanelSlot* canvasSlot = Cast<UCanvasPanelSlot>(widget ? widget->Slot : nullptr))
		{
			canvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			canvasSlot->SetAlignment(FVector2D::ZeroVector);
			canvasSlot->SetOffsets(FMargin(0.0f));
			canvasSlot->SetZOrder(100);
		}
	}

	FLinearColor MakeSrgbColor(const uint8 red, const uint8 green, const uint8 blue, const float alpha = 1.0f)
	{
		const uint8 alphaByte = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(alpha * 255.0f), 0, 255));
		return FLinearColor::FromSRGBColor(FColor(red, green, blue, alphaByte));
	}

	FString NormalizeMainMenuPath(FString path)
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

	FString ExtractProjectRunIdFromDirectory(const FString& runDirectory)
	{
		return FPaths::GetCleanFilename(NormalizeMainMenuPath(runDirectory));
	}

	// Numeric project run ids keep zero-padded directory names but UI labels show the human run number.
	FString FormatProjectRunDisplayId(const FString& runId)
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

	FString BuildProjectRunDirectory(const FString& projectPath, const FString& runId)
	{
		if (projectPath.TrimStartAndEnd().IsEmpty() || runId.TrimStartAndEnd().IsEmpty())
		{
			return FString();
		}
		return NormalizeMainMenuPath(FPaths::Combine(projectPath, TEXT("runs"), runId));
	}

	FString BuildProjectRunStatusPath(const FString& runDirectory)
	{
		return NormalizeMainMenuPath(FPaths::Combine(runDirectory, TEXT("status.json")));
	}

	FString BuildProjectRunSummaryPath(const FString& runDirectory)
	{
		return NormalizeMainMenuPath(FPaths::Combine(runDirectory, TEXT("summary.json")));
	}

	FString FormatProjectRunTotalDuration(const double durationSeconds)
	{
		const int32 RoundedSeconds = FMath::Max(0, FMath::RoundToInt(durationSeconds));
		const int32 Hours = RoundedSeconds / 3600;
		const int32 Minutes = (RoundedSeconds % 3600) / 60;
		const int32 Seconds = RoundedSeconds % 60;
		return Hours > 0
			? FString::Printf(TEXT("%d:%02d:%02d"), Hours, Minutes, Seconds)
			: FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
	}

	FString FormatProjectRunEpisodeDuration(const double durationSeconds)
	{
		const int32 RoundedSeconds = FMath::Max(0, FMath::RoundToInt(durationSeconds));
		return FString::Printf(TEXT("%d:%02d"), RoundedSeconds / 60, RoundedSeconds % 60);
	}

	FString FormatProjectRunEpisodeLabel(const FString& episodeId)
	{
		const int32 EpisodeNumber = FCString::Atoi(*episodeId);
		return EpisodeNumber > 0
			? FString::Printf(TEXT("에피소드 %02d"), EpisodeNumber)
			: TEXT("에피소드 ?");
	}

	FString FormatProjectRunSuccessRate(const int32 successCount, const int32 episodeCount)
	{
		if (episodeCount <= 0)
		{
			return TEXT("0");
		}

		const int32 Percent = FMath::RoundToInt(static_cast<double>(successCount) * 100.0 / static_cast<double>(episodeCount));
		return FString::FromInt(FMath::Clamp(Percent, 0, 100));
	}

	void SetWidgetVisible(UWidget* widget, const bool bVisible)
	{
		if (widget)
		{
			widget->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
	}

	UWidget* FindDashboardChildWidget(UUserWidget* ownerWidget, const FName childName)
	{
		return ownerWidget ? ownerWidget->GetWidgetFromName(childName) : nullptr;
	}

	void SetDashboardChildText(UUserWidget* ownerWidget, const FName childName, const FString& text)
	{
		if (UTextBlock* textBlock = Cast<UTextBlock>(FindDashboardChildWidget(ownerWidget, childName)))
		{
			textBlock->SetText(FText::FromString(text));
		}
	}

	void SetDashboardChildVisibility(UUserWidget* ownerWidget, const FName childName, const bool bVisible)
	{
		SetWidgetVisible(FindDashboardChildWidget(ownerWidget, childName), bVisible);
	}

	bool TryParsePositiveIntText(const FString& text, const FString& label, int32& outValue, TArray<FString>& outDiagnostics)
	{
		const FString trimmedText = text.TrimStartAndEnd();
		if (!LexTryParseString(outValue, *trimmedText) || outValue <= 0)
		{
			outDiagnostics.Add(FString::Printf(TEXT("%s은 1 이상의 정수여야 합니다."), *label));
			return false;
		}

		return true;
	}

	bool TryParseInt64Text(const FString& text, const FString& label, int64& outValue, TArray<FString>& outDiagnostics)
	{
		const FString trimmedText = text.TrimStartAndEnd();
		if (!LexTryParseString(outValue, *trimmedText))
		{
			outDiagnostics.Add(FString::Printf(TEXT("%s은 정수여야 합니다."), *label));
			return false;
		}

		return true;
	}

	FString ToRunStateString(ESimulationRunState state)
	{
		if (const UEnum* stateEnum = StaticEnum<ESimulationRunState>())
		{
			return stateEnum->GetNameStringByValue(static_cast<int64>(state));
		}

		return TEXT("Unknown");
	}

	float ResolveProjectRunProgressPercent(const ESimulationRunState state)
	{
		return state == ESimulationRunState::Completed ? 100.0f : 0.0f;
	}

	bool IsActiveProjectRunDirectory(
		const FString& resultDirectory,
		const FSimulatorRunInfo& runInfo,
		const FString& selectedProjectPath)
	{
		const FString runId = ExtractProjectRunIdFromDirectory(resultDirectory);
		return runInfo.bProjectRun
			&& !runId.IsEmpty()
			&& runInfo.ProjectPath.Equals(selectedProjectPath, ESearchCase::IgnoreCase)
			&& runInfo.RunId.Equals(runId, ESearchCase::CaseSensitive);
	}

	bool TryReadBridgeRunStatusState(
		const FString& statusPath,
		ESimulationRunState& outState)
	{
		return UPlatformUiSubsystem::TryReadBridgeRunStatusState(statusPath, outState);
	}

	bool TryReadProjectRunStatusState(
		const FString& resultDirectory,
		ESimulationRunState& outState)
	{
		outState = ESimulationRunState::Pending;
		const FString statusPath = BuildProjectRunStatusPath(resultDirectory);
		if (!UPlatformUiSubsystem::DoesResolvedFileExist(statusPath))
		{
			return false;
		}

		FSimulationRunStatus status;
		TArray<FString> diagnostics;
		if (FSimulationRunStatusJson::ParseFromFile(statusPath, status, diagnostics))
		{
			outState = status.State;
			return true;
		}

		return TryReadBridgeRunStatusState(statusPath, outState);
	}

	bool IsVisibleProjectRunDirectory(
		const FString& resultDirectory,
		const FSimulatorRunInfo& runInfo,
		const FString& selectedProjectPath)
	{
		const FString runId = ExtractProjectRunIdFromDirectory(resultDirectory);
		if (IsActiveProjectRunDirectory(resultDirectory, runInfo, selectedProjectPath))
		{
			return true;
		}

		if (!FUserProjectRunSnapshot::IsValidRunId(runId))
		{
			return false;
		}

		const FString expectedRunDirectory = BuildProjectRunDirectory(selectedProjectPath, runId);
		return !expectedRunDirectory.IsEmpty()
			&& NormalizeMainMenuPath(resultDirectory).Equals(expectedRunDirectory, ESearchCase::IgnoreCase);
	}

	ESimulationRunState ResolveProjectRunDisplayState(
		const FString& resultDirectory,
		const FSimulatorRunInfo& runInfo,
		const FString& selectedProjectPath)
	{
		if (IsActiveProjectRunDirectory(resultDirectory, runInfo, selectedProjectPath))
		{
			if (runInfo.bProcessRunning && !USimulatorLaunchSubsystem::IsTerminalRunState(runInfo.Status.State))
			{
				return ESimulationRunState::Running;
			}

			return runInfo.Status.State;
		}

		if (UPlatformUiSubsystem::DoesResolvedFileExist(BuildProjectRunSummaryPath(resultDirectory)))
		{
			return ESimulationRunState::Completed;
		}

		ESimulationRunState statusState = ESimulationRunState::Pending;
		return TryReadProjectRunStatusState(resultDirectory, statusState)
			? statusState
			: ESimulationRunState::Pending;
	}

	FString TruncatePreview(const FString& text, const int32 characterLimit)
	{
		if (text.Len() <= characterLimit)
		{
			return text;
		}

		return text.Left(characterLimit) + TEXT("\n...");
	}

	FString BuildLogPreview(const FString& logPath)
	{
		return UPlatformUiSubsystem::BuildLogPreview(logPath, LogPreviewEdgeLineCount);
	}

	FString JoinStringLines(const TArray<FString>& lines)
	{
		return FString::Join(lines, TEXT("\n"));
	}

	// AI 분석 실패를 retry CTA와 함께 표시할 compact message로 변환한다.
	FString BuildAnalysisFailureDisplayText(const FPlatformAnalysisAiResponse& response)
	{
		TArray<FString> lines;
		lines.Add(TEXT("AI analysis failed"));
		if (response.ResponseCode != 0)
		{
			lines.Add(FString::Printf(TEXT("Response: %d"), response.ResponseCode));
		}
		if (!response.ErrorMessage.IsEmpty())
		{
			lines.Add(response.ErrorMessage);
		}
		if (!response.ResponseBody.IsEmpty())
		{
			lines.Add(TEXT(""));
			lines.Add(TruncatePreview(response.ResponseBody, ReportPreviewCharacterLimit));
		}
		return JoinStringLines(lines);
	}

	bool TryReadExperimentResultReportItem(const FString& reportPath, FExperimentResultReportItem& outItem)
	{
		return UPlatformUiSubsystem::TryReadExperimentResultReportItem(reportPath, outItem);
	}

	TArray<FExperimentResultReportItem> BuildExperimentResultReportItems(const TArray<FString>& reportPaths)
	{
		return UPlatformUiSubsystem::BuildExperimentResultReportItems(reportPaths);
	}

	bool IsReferenceSampleJsonPath(const FString& jsonPath)
	{
		return FPaths::GetBaseFilename(jsonPath).Contains(TEXT("Sample"), ESearchCase::IgnoreCase);
	}

	FString NormalizeJsonPathInDirectory(FString rawPath, const TCHAR* defaultDirectory)
	{
		rawPath = rawPath.TrimStartAndEnd();
		rawPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (rawPath.IsEmpty())
		{
			return FString();
		}

		if (FPaths::GetExtension(rawPath).IsEmpty())
		{
			rawPath += TEXT(".json");
		}

		if (!rawPath.Contains(TEXT("/")))
		{
			rawPath = FPaths::Combine(defaultDirectory, rawPath);
		}

		rawPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		return rawPath;
	}

	FString NormalizeInputJsonPath(const FString& rawPath)
	{
		return NormalizeJsonPathInDirectory(rawPath, TEXT("Json/Input"));
	}

	FString NormalizeOutputJsonPath(const FString& rawPath)
	{
		return NormalizeJsonPathInDirectory(rawPath, TEXT("Json/Output"));
	}

	bool IsEditableInputJsonPath(const FString& jsonPath)
	{
		return jsonPath.StartsWith(TEXT("Json/Input/"), ESearchCase::IgnoreCase)
			&& FPaths::GetExtension(jsonPath).Equals(TEXT("json"), ESearchCase::IgnoreCase)
			&& !IsReferenceSampleJsonPath(jsonPath);
	}

	bool IsEditableOutputJsonPath(const FString& jsonPath)
	{
		return jsonPath.StartsWith(TEXT("Json/Output/"), ESearchCase::IgnoreCase)
			&& FPaths::GetExtension(jsonPath).Equals(TEXT("json"), ESearchCase::IgnoreCase)
			&& !IsReferenceSampleJsonPath(jsonPath);
	}

	bool MoveProjectRelativeFile(
		const FString& sourcePath,
		const FString& targetPath,
		const FString& itemLabel,
		FString& outError)
	{
		return UPlatformUiSubsystem::MoveProjectRelativeFile(sourcePath, targetPath, itemLabel, outError);
	}

	FString MakeUniqueInputJsonPath(const FString& baseFileName)
	{
		return UPlatformUiSubsystem::MakeUniqueInputJsonPath(baseFileName);
	}

	FString MakeGeneratedRunQueuePathForSetup(const FString& setupPath)
	{
		FString baseName = FPaths::GetBaseFilename(setupPath);
		if (baseName.IsEmpty())
		{
			baseName = TEXT("SimulationSetup");
		}

		FString runQueuePath = FPaths::Combine(TEXT("Json/Input"), FString::Printf(TEXT("%s_RunQueue.json"), *baseName));
		runQueuePath.ReplaceInline(TEXT("\\"), TEXT("/"));
		return runQueuePath;
	}

	bool OpenTextFileInExternalEditor(const FString& resolvedFilePath, FString& outError)
	{
		outError.Reset();
		const FString cmdArguments = FString::Printf(
			TEXT("/d /c start \"\" %s"),
			*USimulatorLaunchSubsystem::QuoteCommandLineArgument(resolvedFilePath));
		uint32 processId = 0;
		FProcHandle handle = FPlatformProcess::CreateProc(
			TEXT("cmd.exe"),
			*cmdArguments,
			true,
			true,
			true,
			&processId,
			0,
			nullptr,
			nullptr);
		if (handle.IsValid())
		{
			FPlatformProcess::CloseProc(handle);
			return true;
		}

		outError = FString::Printf(TEXT("기본 JSON 편집기 실행 실패: %s"), *resolvedFilePath);
		return false;
	}

	void SetComboBoxOptions(UComboBoxString* comboBox, const TArray<FString>& options, const FString& preferredOption)
	{
		if (!comboBox) return;

		comboBox->ClearOptions();
		for (const FString& option : options)
		{
			comboBox->AddOption(option);
		}

		const FString selectedOption = options.Contains(preferredOption)
			? preferredOption
			: (options.IsEmpty() ? FString() : options[0]);
		if (!selectedOption.IsEmpty())
		{
			comboBox->SetSelectedOption(selectedOption);
		}
	}

}

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);
	if (UPlatformUiSubsystem* platformUiSubsystem = GetPlatformUiSubsystem())
	{
		ProjectWorkspaceViewModel = platformUiSubsystem->GetProjectWorkspaceViewModel();
		ExperimentConfigViewModel = platformUiSubsystem->GetExperimentConfigViewModel();
		ExperimentResultViewModel = platformUiSubsystem->GetExperimentResultViewModel();
		platformUiSubsystem->RefreshFromProjectSession();
	}
	if (!ValidateRequiredBindings()) return;

	if (AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer()))
	{
		if (ScenarioEditorRootWidget)
		{
			editorController->RegisterEditorRootWidget(ScenarioEditorRootWidget.Get());
		}
		RequestEditorWidgetInputMode();
	}

	BindControls();
	ShowProjectExperimentConfigPanel(false);
	SetExperimentConfigDetailVisible(false);
	SetExperimentResultDetailVisible(false);
	ShowMainMenuSection(static_cast<int32>(EMainMenuSection::Scenario));
	if (IsProjectOpened())
	{
		RefreshProjectRunSelection();
		RefreshExperimentResultList();
		ShowProjectWorkspaceScreen();
		ShowProjectWorkspaceTab(EProjectWorkspaceTabType::ScenarioEdit);
	}
	else
	{
		ShowProjectWorkspaceScreen();
		SetDiagnosticsText(TEXT("Active project가 없습니다. StartupMap에서 프로젝트를 선택하세요."));
	}

	if (UPlatformUiSubsystem* platformUiSubsystem = GetPlatformUiSubsystem())
	{
		platformUiSubsystem->OnRunInfoChanged.RemoveAll(this);
		platformUiSubsystem->OnRunInfoChanged.AddUObject(this, &UMainMenuWidget::HandleRunInfoChanged);
		platformUiSubsystem->OnAnalysisCompleted.RemoveAll(this);
		platformUiSubsystem->OnAnalysisCompleted.AddUObject(this, &UMainMenuWidget::HandleAnalysisCompleted);
	}

	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().SetTimer(
			RefreshTimerHandle,
			this,
			&UMainMenuWidget::RefreshFromSubsystem,
			1.0f,
			true);
	}

	UpdateStatusText();
	UpdateReportAndLogText();
}

void UMainMenuWidget::NativeDestruct()
{
	ReleaseEditorWidgetInputMode();

	if (ProjectEpisodeReplayViewerWidget)
	{
		ProjectEpisodeReplayViewerWidget->OnReplayFullscreenChanged.RemoveAll(this);
	}

	if (AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer()))
	{
		if (ScenarioEditorRootWidget && editorController->GetEditorRootWidget() == ScenarioEditorRootWidget.Get())
		{
			editorController->ClearRegisteredEditorRootWidget(ScenarioEditorRootWidget.Get());
		}
	}

	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}

	if (UPlatformUiSubsystem* platformUiSubsystem = GetPlatformUiSubsystem())
	{
		platformUiSubsystem->OnRunInfoChanged.RemoveAll(this);
		platformUiSubsystem->OnAnalysisCompleted.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UMainMenuWidget::RefreshSetupOptions()
{
	UPlatformUiSubsystem* platformUiSubsystem = GetPlatformUiSubsystem();
	if (!platformUiSubsystem) return;

	const FString currentPath = GetSelectedSetupPath();
	const TArray<FString> setupFiles = platformUiSubsystem->ListLegacySimulationSetupFiles();
	SetComboBoxOptions(SetupComboBox, setupFiles, currentPath);

	const FString selectedPath = setupFiles.Contains(currentPath)
		? currentPath
		: (setupFiles.IsEmpty() ? FString() : setupFiles[0]);
	if (!selectedPath.IsEmpty())
	{
		SetSelectedSetupPath(selectedPath);
		LoadSelectedSetup();
	}
	else
	{
		ApplyNewSetupDefaults(DefaultSimulationSetupPath);
		SetDiagnosticsText(TEXT("편집 가능한 SimulationSetup JSON이 없습니다. 저장하면 새 파일을 생성합니다."));
	}
	RefreshExperimentConfigList();

	const TArray<FString> scenarioSetupFiles = platformUiSubsystem->ListLegacyScenarioSetupFiles();
	const FString currentScenarioSetupPath = GetSelectedScenarioSetupPath();
	const FString selectedScenarioSetupPath = scenarioSetupFiles.Contains(currentScenarioSetupPath)
		? currentScenarioSetupPath
		: (scenarioSetupFiles.IsEmpty() ? FString() : scenarioSetupFiles[0]);
	SetComboBoxOptions(ExperimentScenarioSetupComboBox, scenarioSetupFiles, selectedScenarioSetupPath);
	SetComboBoxOptions(ScenarioSetupComboBox, scenarioSetupFiles, selectedScenarioSetupPath);
	SetSelectedScenarioSetupPath(selectedScenarioSetupPath);
	RefreshScenarioList();

	const TArray<FString> deliveryBotSetupFiles = platformUiSubsystem->ListLegacyDeliveryBotSetupFiles();
	const FString currentDeliveryBotSetupPath = GetSelectedDeliveryBotSetupPath();
	const FString selectedDeliveryBotSetupPath = deliveryBotSetupFiles.Contains(currentDeliveryBotSetupPath)
		? currentDeliveryBotSetupPath
		: (deliveryBotSetupFiles.IsEmpty() ? FString() : deliveryBotSetupFiles[0]);
	SetComboBoxOptions(DeliveryBotSetupComboBox, deliveryBotSetupFiles, selectedDeliveryBotSetupPath);
	SetComboBoxOptions(PolicyDeliveryBotSetupComboBox, deliveryBotSetupFiles, selectedDeliveryBotSetupPath);
	SetSelectedDeliveryBotSetupPath(selectedDeliveryBotSetupPath);
	RefreshPolicyList();

	const TArray<FString> policySpecFiles = platformUiSubsystem->ListLegacyPolicySpecFiles();
	const FString currentPolicySpecPath = GetSelectedPolicySpecPath();
	const FString defaultPolicySpecPath = MainMenuDefaultPolicySpecJsonPath;
	const FString selectedPolicySpecPath = policySpecFiles.Contains(currentPolicySpecPath)
		? currentPolicySpecPath
		: (policySpecFiles.Contains(defaultPolicySpecPath)
			? defaultPolicySpecPath
			: (policySpecFiles.IsEmpty() ? defaultPolicySpecPath : policySpecFiles[0]));
	SetComboBoxOptions(PolicySpecComboBox, policySpecFiles, selectedPolicySpecPath);
	SetSelectedPolicySpecPath(selectedPolicySpecPath);

	RefreshProjectRunSelection();
	RefreshExperimentResultList();
}

void UMainMenuWidget::RefreshProjectRunSelection()
{
	if (!IsProjectModeSelected() || !ProjectWorkspaceViewModel)
	{
		SetSelectedProjectRunId(FString());
		return;
	}

	ProjectWorkspaceViewModel->RefreshProjectRuns();

	TArray<FString> runIds;
	for (const UOdiroListItemViewModel* runItem : ProjectWorkspaceViewModel->GetRunItems())
	{
		const FString runId = runItem ? runItem->GetItemId() : FString();
		if (FUserProjectRunSnapshot::IsValidRunId(runId))
		{
			runIds.Add(runId);
		}
	}
	runIds.Sort();

	const FString currentRunId = GetSelectedProjectRunId();
	const FString selectedRunId = runIds.Contains(currentRunId)
		? currentRunId
		: (runIds.IsEmpty() ? FString() : runIds.Last());
	SetSelectedProjectRunId(selectedRunId);
	ProjectWorkspaceViewModel->SelectRun(selectedRunId);
}

void UMainMenuWidget::ShowProjectExperimentConfigPanel(const bool bVisible)
{
	if (ProjectExperimentConfigPanel)
	{
		ProjectExperimentConfigPanel->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (!bVisible)
	{
		SetProjectExperimentConfigWarningText(FString());
	}
}

bool UMainMenuWidget::StartProjectExperimentRun(TArray<FString>& outDiagnostics, FString& outRunId)
{
	outDiagnostics.Reset();
	outRunId.Reset();

	if (!ProjectWorkspaceViewModel)
	{
		outDiagnostics.Add(TEXT("ProjectWorkspaceViewModel이 없습니다."));
		return false;
	}

	if (!ProjectWorkspaceViewModel->StartNewRun(outRunId))
	{
		const FString diagnostics = ProjectWorkspaceViewModel->GetDiagnosticsText();
		outDiagnostics.Add(diagnostics.IsEmpty() ? TEXT("Project run 시작 실패.") : diagnostics);
		return false;
	}

	SetSelectedProjectRunId(outRunId);
	return true;
}

bool UMainMenuWidget::LoadProjectExperimentSettingIntoPanel(TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	if (!ExperimentConfigViewModel)
	{
		outDiagnostics.Add(TEXT("ExperimentConfigViewModel이 없습니다."));
		return false;
	}

	if (!ExperimentConfigViewModel->LoadFromProject(GetSelectedProjectPath()))
	{
		outDiagnostics.Add(ExperimentConfigViewModel->GetDiagnosticsText());
		return false;
	}

	if (ProjectExperimentMapIdTextBox)
	{
		ProjectExperimentMapIdTextBox->SetText(FText::FromString(ExperimentConfigViewModel->GetMapId()));
	}
	if (ProjectExperimentFixedFpsTextBox)
	{
		ProjectExperimentFixedFpsTextBox->SetText(FText::AsNumber(ExperimentConfigViewModel->GetFixedFps()));
	}
	if (ProjectExperimentEpisodeCountTextBox)
	{
		ProjectExperimentEpisodeCountTextBox->SetText(FText::AsNumber(ExperimentConfigViewModel->GetEpisodeCount()));
	}
	if (ProjectExperimentBaseSeedTextBox)
	{
		ProjectExperimentBaseSeedTextBox->SetText(FText::AsNumber(ExperimentConfigViewModel->GetBaseSeed()));
	}

	SetProjectExperimentConfigWarningText(FString());
	return true;
}

bool UMainMenuWidget::SaveProjectExperimentSettingFromPanel(TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();
	if (!ExperimentConfigViewModel)
	{
		outDiagnostics.Add(TEXT("ExperimentConfigViewModel이 없습니다."));
		return false;
	}

	int32 fixedFps = 0;
	int32 episodeCount = 0;
	int64 baseSeed = 0;
	TryParsePositiveIntText(
		ProjectExperimentFixedFpsTextBox ? ProjectExperimentFixedFpsTextBox->GetText().ToString() : FString(),
		TEXT("Fixed FPS"),
		fixedFps,
		outDiagnostics);
	TryParsePositiveIntText(
		ProjectExperimentEpisodeCountTextBox ? ProjectExperimentEpisodeCountTextBox->GetText().ToString() : FString(),
		TEXT("Episode Count"),
		episodeCount,
		outDiagnostics);
	TryParseInt64Text(
		ProjectExperimentBaseSeedTextBox ? ProjectExperimentBaseSeedTextBox->GetText().ToString() : FString(),
		TEXT("Base Seed"),
		baseSeed,
		outDiagnostics);
	if (!outDiagnostics.IsEmpty())
	{
		return false;
	}

	const FString mapId = ProjectExperimentMapIdTextBox
		? ProjectExperimentMapIdTextBox->GetText().ToString().TrimStartAndEnd()
		: ExperimentConfigViewModel->GetMapId();
	if (mapId.IsEmpty())
	{
		outDiagnostics.Add(TEXT("Map ID를 입력하세요."));
		return false;
	}

	ExperimentConfigViewModel->SetMapId(mapId);
	ExperimentConfigViewModel->SetFixedFps(fixedFps);
	ExperimentConfigViewModel->SetEpisodeCount(episodeCount);
	ExperimentConfigViewModel->SetBaseSeed(baseSeed);
	if (!ExperimentConfigViewModel->SaveToProject(GetSelectedProjectPath()))
	{
		outDiagnostics.Add(ExperimentConfigViewModel->GetDiagnosticsText());
		return false;
	}

	return true;
}

void UMainMenuWidget::SetProjectExperimentConfigWarningText(const FString& message)
{
	if (!ProjectExperimentConfigWarningText)
	{
		return;
	}

	ProjectExperimentConfigWarningText->SetText(FText::FromString(message));
	ProjectExperimentConfigWarningText->SetVisibility(message.TrimStartAndEnd().IsEmpty()
		? ESlateVisibility::Collapsed
		: ESlateVisibility::HitTestInvisible);
}

void UMainMenuWidget::RefreshFromSubsystem()
{
	if (UPlatformUiSubsystem* platformUiSubsystem = GetPlatformUiSubsystem())
	{
		platformUiSubsystem->RefreshActiveRunStatus();
	}

	UpdateStatusText();
	UpdateReportAndLogText();
}

void UMainMenuWidget::HandleSetupSelectionChanged(FString selectedItem, ESelectInfo::Type selectionType)
{
	(void)selectionType;

	SetSelectedSetupPath(selectedItem);
	LoadSelectedSetup();
}

void UMainMenuWidget::HandleLoadClicked()
{
	if (bExperimentConfigDetailVisible)
	{
		SetExperimentConfigDetailVisible(false);
		RefreshExperimentConfigList();
		SetDiagnosticsText(TEXT("SimulationSetup 목록으로 돌아왔습니다."));
		return;
	}

	if (SetupComboBox && SetupComboBox->GetVisibility() != ESlateVisibility::Visible)
	{
		SetupComboBox->SetVisibility(ESlateVisibility::Visible);
		if (GetSelectedSetupPath().TrimStartAndEnd().IsEmpty()
			|| !UPlatformUiSubsystem::DoesResolvedFileExist(GetSelectedSetupPath()))
		{
			SetDiagnosticsText(TEXT("SimulationSetup을 선택하거나 새 구성을 만든 뒤 불러오세요."));
			return;
		}
	}

	LoadSelectedSetup();
}

void UMainMenuWidget::HandleNewSetupClicked()
{
	const FString newSetupPath = MakeUniqueInputJsonPath(TEXT("SimulationSetupNew"));
	SetExperimentConfigDetailVisible(true);
	ApplyNewSetupDefaults(newSetupPath);
	if (SetupComboBox)
	{
		SetupComboBox->SetVisibility(ESlateVisibility::Collapsed);
	}

	SetDiagnosticsText(TEXT("새 SimulationSetup 초안입니다. 시나리오와 행동 정책을 고른 뒤 저장하세요."));
}

void UMainMenuWidget::HandleSaveFpsClicked()
{
	UPlatformUiSubsystem* platformUiSubsystem = GetPlatformUiSubsystem();
	if (!platformUiSubsystem || !FixedStepFpsTextBox)
	{
		return;
	}

	const int32 fps = FCString::Atoi(*FixedStepFpsTextBox->GetText().ToString());
	if (IsReferenceSampleJsonPath(GetSelectedSetupPath()))
	{
		SetDiagnosticsText(TEXT("샘플 JSON은 읽기 전용입니다. 저장하기 전에 새 SimulationSetup을 만드세요."));
		return;
	}

	TArray<FString> diagnostics;
	if (platformUiSubsystem->SaveLegacyFixedStepFpsToSetupFile(GetSelectedSetupPath(), fps, diagnostics))
	{
		UpdateStatusText(FString::Printf(TEXT("fixed_step.fps 저장됨: %d"), fps));
		return;
	}

	SetDiagnosticsText(JoinStringLines(diagnostics));
}

bool UMainMenuWidget::BuildSimulationSetupFromControls(
	const FSimulationSetup& baseSetup,
	const FString& runQueuePath,
	FSimulationSetup& outSetup,
	TArray<FString>& outDiagnostics) const
{
	outDiagnostics.Reset();
	outSetup = baseSetup;

	outSetup.Schema = TEXT("simulation_setup");
	outSetup.Version = FMath::Max(1, outSetup.Version);
	outSetup.RunQueueJsonPath = runQueuePath.TrimStartAndEnd();

	if (MapIdTextBox)
	{
		outSetup.MapId = MapIdTextBox->GetText().ToString().TrimStartAndEnd();
	}

	if (FixedStepFpsTextBox)
	{
		outSetup.FixedStep.Fps = FCString::Atoi(*FixedStepFpsTextBox->GetText().ToString().TrimStartAndEnd());
	}

	if (MeasurementLogEnabledCheckBox)
	{
		outSetup.MeasurementLog.bEnabled = MeasurementLogEnabledCheckBox->IsChecked();
	}
	if (MeasurementOutputDirectoryTextBox)
	{
		outSetup.MeasurementLog.OutputDirectory = MeasurementOutputDirectoryTextBox->GetText().ToString().TrimStartAndEnd();
	}
	if (MeasurementFilePrefixTextBox)
	{
		outSetup.MeasurementLog.FilePrefix = MeasurementFilePrefixTextBox->GetText().ToString().TrimStartAndEnd();
	}
	if (FlushIntervalTicksTextBox)
	{
		outSetup.MeasurementLog.FlushIntervalTicks =
			FCString::Atoi(*FlushIntervalTicksTextBox->GetText().ToString().TrimStartAndEnd());
	}
	if (StatusOutputPathTextBox)
	{
		outSetup.Status.OutputPath = StatusOutputPathTextBox->GetText().ToString().TrimStartAndEnd();
	}

	if (outSetup.RunQueueJsonPath.IsEmpty())
	{
		outDiagnostics.Add(TEXT("SimulationSetup run_queue는 비어 있을 수 없습니다."));
		return false;
	}

	return true;
}

void UMainMenuWidget::HandleSaveSetupClicked()
{
	UPlatformUiSubsystem* platformUiSubsystem = GetPlatformUiSubsystem();
	if (!platformUiSubsystem)
	{
		return;
	}

	const FString setupPath = GetSelectedSetupPath();
	if (IsReferenceSampleJsonPath(setupPath))
	{
		SetDiagnosticsText(TEXT("샘플 JSON은 읽기 전용입니다. 저장하기 전에 새 SimulationSetup을 만드세요."));
		return;
	}

	const FString scenarioSetupPath = GetSelectedScenarioSetupPath();
	const FString deliveryBotSetupPath = GetSelectedDeliveryBotSetupPath();
	const FString policySpecPath = GetSelectedPolicySpecPath();
	if (scenarioSetupPath.TrimStartAndEnd().IsEmpty() || deliveryBotSetupPath.TrimStartAndEnd().IsEmpty())
	{
		SetDiagnosticsText(TEXT("ScenarioSetup과 DeliveryBotSetup을 선택해야 합니다."));
		return;
	}
	if (policySpecPath.TrimStartAndEnd().IsEmpty())
	{
		SetDiagnosticsText(TEXT("PolicySpec JSON을 선택해야 합니다."));
		return;
	}
	if (IsReferenceSampleJsonPath(scenarioSetupPath) || IsReferenceSampleJsonPath(deliveryBotSetupPath))
	{
		SetDiagnosticsText(TEXT("샘플 JSON은 편집 가능한 SimulationSetup에 사용할 수 없습니다. 먼저 편집 가능한 시나리오/행동 정책 파일을 만드세요."));
		return;
	}

	FSimulationSetup setupBase;
	const FSimulationSetupParseResult parseResult = platformUiSubsystem->LoadLegacySimulationSetupFile(setupPath);
	if (parseResult.bSuccess)
	{
		setupBase = parseResult.Setup;
	}

	FString runQueuePath = setupBase.RunQueueJsonPath.TrimStartAndEnd();
	runQueuePath.ReplaceInline(TEXT("\\"), TEXT("/"));
	if (runQueuePath.IsEmpty())
	{
		runQueuePath = MakeGeneratedRunQueuePathForSetup(setupPath);
	}

	const int32 runCount = FMath::Max(
		1,
		RunCountTextBox ? FCString::Atoi(*RunCountTextBox->GetText().ToString()) : 1);

	// MainMenu edits a single scenario/policy pair plus repeat count into the SimulationSetup-owned RunQueue.
	TArray<FScenarioRunInput> runInputs;
	runInputs.Reserve(runCount);
	const FString pairIdBase = FPaths::GetBaseFilename(setupPath).IsEmpty()
		? FString(TEXT("run"))
		: FPaths::GetBaseFilename(setupPath);
	for (int32 runIndex = 0; runIndex < runCount; ++runIndex)
	{
		FScenarioRunInput runInput;
		runInput.PairId = FString::Printf(TEXT("%s_%03d"), *pairIdBase, runIndex);
		runInput.EpisodeScenarioJsonPath = scenarioSetupPath;
		runInput.ProfileJsonPath = deliveryBotSetupPath;
		runInput.PolicySpecJsonPath = policySpecPath;
		runInputs.Add(runInput);
	}

	TArray<FString> diagnostics;
	if (!platformUiSubsystem->SaveLegacyScenarioRunQueueFile(runQueuePath, runInputs, diagnostics))
	{
		SetDiagnosticsText(JoinStringLines(diagnostics));
		return;
	}

	FSimulationSetup setup;
	diagnostics.Reset();
	if (!BuildSimulationSetupFromControls(setupBase, runQueuePath, setup, diagnostics))
	{
		SetDiagnosticsText(JoinStringLines(diagnostics));
		return;
	}

	if (platformUiSubsystem->SaveLegacySimulationSetupFile(setupPath, setup, diagnostics))
	{
		RefreshSetupOptions();
		SetExperimentConfigDetailVisible(false);
		ShowMainMenuSection(static_cast<int32>(EMainMenuSection::ExperimentConfig));
		SetDiagnosticsText(FString::Printf(TEXT("SimulationSetup saved: %s\nRunQueue saved: %s"), *setupPath, *runQueuePath));
		return;
	}

	SetDiagnosticsText(JoinStringLines(diagnostics));
}

void UMainMenuWidget::HandleOpenEditorClicked()
{
	OpenScenarioInEditor(IsProjectModeSelected() ? GetSelectedProjectScenarioPath() : GetSelectedScenarioSetupPath());
}

void UMainMenuWidget::HandleNewScenarioClicked()
{
	const FString newScenarioPath = MakeUniqueInputJsonPath(TEXT("ScenarioSetupNew"));
	if (!CreateScenarioFileFromTemplate(newScenarioPath))
	{
		return;
	}

	SetSelectedScenarioSetupPath(newScenarioPath);
	RefreshSetupOptions();
	SetDiagnosticsText(FString::Printf(TEXT("시나리오 생성됨: %s"), *newScenarioPath));
}

void UMainMenuWidget::HandleScenarioRenameRequested(UFileListItemWidget* itemWidget, const FString& requestedPath)
{
	if (!IsValid(itemWidget)) return;

	const FString sourcePath = NormalizeInputJsonPath(itemWidget->GetOriginalPath());
	const FString targetPath = NormalizeInputJsonPath(requestedPath);
	if (!IsEditableInputJsonPath(sourcePath))
	{
		SetDiagnosticsText(TEXT("Json/Input 아래의 편집 가능한 ScenarioSetup JSON만 이름을 변경할 수 있습니다."));
		return;
	}
	if (!IsEditableInputJsonPath(targetPath))
	{
		SetDiagnosticsText(TEXT("시나리오 파일 이름은 편집 가능한 Json/Input/*.json 경로여야 합니다."));
		return;
	}
	if (sourcePath.Equals(targetPath, ESearchCase::IgnoreCase))
	{
		SetSelectedScenarioSetupPath(sourcePath);
		RefreshScenarioList();
		return;
	}

	UPlatformUiSubsystem* platformUiSubsystem = GetPlatformUiSubsystem();
	if (!platformUiSubsystem)
	{
		SetDiagnosticsText(TEXT("PlatformUiSubsystem을 사용할 수 없습니다."));
		return;
	}

	FString moveError;
	if (!MoveProjectRelativeFile(sourcePath, targetPath, TEXT("Scenario"), moveError))
	{
		SetDiagnosticsText(moveError);
		return;
	}

	TArray<FString> diagnostics;
	if (!platformUiSubsystem->ReplaceLegacyScenarioSetupReferencesInRunQueues(sourcePath, targetPath, diagnostics))
	{
		FString rollbackError;
		if (MoveProjectRelativeFile(targetPath, sourcePath, TEXT("Scenario rollback"), rollbackError))
		{
			diagnostics.Add(TEXT("시나리오 이름 변경을 롤백했습니다."));
		}
		else
		{
			diagnostics.Add(rollbackError);
		}

		SetDiagnosticsText(JoinStringLines(diagnostics));
		return;
	}

	SetSelectedScenarioSetupPath(targetPath);
	RefreshSetupOptions();
	diagnostics.Insert(FString::Printf(TEXT("시나리오 이름 변경됨: %s -> %s"), *sourcePath, *targetPath), 0);
	SetDiagnosticsText(JoinStringLines(diagnostics));
}

void UMainMenuWidget::HandleScenarioEditRequested(UFileListItemWidget* itemWidget)
{
	if (!IsValid(itemWidget)) return;

	const FString scenarioSetupPath = itemWidget->GetOriginalPath();
	SetSelectedScenarioSetupPath(scenarioSetupPath);
	OpenScenarioInEditor(scenarioSetupPath);
}

void UMainMenuWidget::HandlePolicyRenameRequested(UFileListItemWidget* itemWidget, const FString& requestedPath)
{
	if (!IsValid(itemWidget)) return;

	const FString sourcePath = NormalizeInputJsonPath(itemWidget->GetOriginalPath());
	const FString targetPath = NormalizeInputJsonPath(requestedPath);
	if (!IsEditableInputJsonPath(sourcePath) || !IsEditableInputJsonPath(targetPath))
	{
		SetDiagnosticsText(TEXT("행동 정책 파일 이름은 편집 가능한 Json/Input/*.json 경로여야 합니다."));
		return;
	}

	UPlatformUiSubsystem* platformUiSubsystem = GetPlatformUiSubsystem();
	if (!platformUiSubsystem)
	{
		SetDiagnosticsText(TEXT("PlatformUiSubsystem을 사용할 수 없습니다."));
		return;
	}

	FString moveError;
	if (!MoveProjectRelativeFile(sourcePath, targetPath, TEXT("Policy"), moveError))
	{
		SetDiagnosticsText(moveError);
		return;
	}

	TArray<FString> diagnostics;
	if (!platformUiSubsystem->ReplaceLegacyDeliveryBotSetupReferencesInRunQueues(sourcePath, targetPath, diagnostics))
	{
		FString rollbackError;
		if (MoveProjectRelativeFile(targetPath, sourcePath, TEXT("Policy rollback"), rollbackError))
		{
			diagnostics.Add(TEXT("행동 정책 이름 변경을 롤백했습니다."));
		}
		else
		{
			diagnostics.Add(rollbackError);
		}

		SetDiagnosticsText(JoinStringLines(diagnostics));
		return;
	}

	SetSelectedDeliveryBotSetupPath(targetPath);
	RefreshSetupOptions();
	diagnostics.Insert(FString::Printf(TEXT("행동 정책 이름 변경됨: %s -> %s"), *sourcePath, *targetPath), 0);
	SetDiagnosticsText(JoinStringLines(diagnostics));
}

void UMainMenuWidget::HandlePolicyEditRequested(UFileListItemWidget* itemWidget)
{
	if (!IsValid(itemWidget)) return;

	SetSelectedDeliveryBotSetupPath(itemWidget->GetOriginalPath());
	HandleOpenPolicyTextEditorClicked();
}

void UMainMenuWidget::HandleExperimentConfigRenameRequested(
	UFileListItemWidget* itemWidget,
	const FString& requestedPath)
{
	if (!IsValid(itemWidget)) return;

	const FString sourcePath = NormalizeInputJsonPath(itemWidget->GetOriginalPath());
	const FString targetPath = NormalizeInputJsonPath(requestedPath);
	if (!IsEditableInputJsonPath(sourcePath) || !IsEditableInputJsonPath(targetPath))
	{
		SetDiagnosticsText(TEXT("SimulationSetup 파일 이름은 편집 가능한 Json/Input/*.json 경로여야 합니다."));
		return;
	}

	FString moveError;
	if (!MoveProjectRelativeFile(sourcePath, targetPath, TEXT("SimulationSetup"), moveError))
	{
		SetDiagnosticsText(moveError);
		return;
	}

	SetSelectedSetupPath(targetPath);
	RefreshSetupOptions();
	SetDiagnosticsText(FString::Printf(TEXT("SimulationSetup 이름 변경됨: %s -> %s"), *sourcePath, *targetPath));
}

void UMainMenuWidget::HandleExperimentConfigEditRequested(UFileListItemWidget* itemWidget)
{
	if (!IsValid(itemWidget)) return;

	SetExperimentConfigDetailVisible(true);
	const FString setupPath = NormalizeInputJsonPath(itemWidget->GetOriginalPath());
	SetSelectedSetupPath(setupPath);
	LoadSelectedSetup();
}

void UMainMenuWidget::HandleExperimentConfigPlayRequested(UFileListItemWidget* itemWidget)
{
	if (!IsValid(itemWidget)) return;

	const FString setupPath = NormalizeInputJsonPath(itemWidget->GetOriginalPath());
	SetSelectedSetupPath(setupPath);
	LoadSelectedSetup();
	HandleStartClicked();
}

void UMainMenuWidget::HandleExperimentResultDetailsRequested(UFileListItemWidget* itemWidget)
{
	if (!IsValid(itemWidget)) return;

	SetSelectedExperimentResultRunDirectory(itemWidget->GetOriginalPath());
	if (IsProjectRunDirectory(SelectedExperimentResultRunDirectory))
	{
		SetSelectedProjectRunId(ExtractProjectRunIdFromDirectory(SelectedExperimentResultRunDirectory));
		if (ExperimentResultViewModel)
		{
			ExperimentResultViewModel->LoadRunDirectory(SelectedExperimentResultRunDirectory);
		}
		RefreshExperimentResultDetailPanel();
		SetExperimentResultDetailVisible(true);
		return;
	}
	RefreshExperimentResultIterationList();
	UpdateReportAndLogText();
	SetExperimentResultDetailVisible(true);
}

void UMainMenuWidget::HandleProjectExperimentRunAnalyzeRequested(UProjectExperimentRunRowWidget* rowWidget)
{
	if (!IsValid(rowWidget)) return;

	SetSelectedExperimentResultRunDirectory(rowWidget->GetRunDirectory());
	SetSelectedProjectRunId(ExtractProjectRunIdFromDirectory(SelectedExperimentResultRunDirectory));
	if (ExperimentResultViewModel)
	{
		ExperimentResultViewModel->LoadRunDirectory(SelectedExperimentResultRunDirectory);
	}
	RefreshExperimentResultDetailPanel();
	SetExperimentResultDetailVisible(true);
}

void UMainMenuWidget::HandleExperimentResultIterationSelectorClicked(
	UExperimentResultIterationSelectorWidget* selectorWidget)
{
	if (!IsValid(selectorWidget)) return;

	SetSelectedExperimentResultPath(selectorWidget->GetResultPath());
	RefreshExperimentResultIterationList();
	if (IsProjectRunDirectory(SelectedExperimentResultRunDirectory))
	{
		RefreshExperimentResultDetailPanel();
	}
	else
	{
		UpdateReportAndLogText();
	}
}

void UMainMenuWidget::HandleOpenPolicyTextEditorClicked()
{
	const FString policyPath = GetSelectedDeliveryBotSetupPath();
	if (policyPath.TrimStartAndEnd().IsEmpty())
	{
		SetDiagnosticsText(TEXT("DeliveryBotSetup 파일이 선택되지 않았습니다."));
		return;
	}
	if (IsReferenceSampleJsonPath(policyPath))
	{
		SetDiagnosticsText(TEXT("샘플 JSON은 읽기 전용입니다. + 버튼으로 새 행동 정책을 만든 뒤 편집하세요."));
		return;
	}

	const FString resolvedPolicyPath = FSimulationSetupJson::ResolveProjectPath(policyPath);
	if (!UPlatformUiSubsystem::DoesResolvedFileExist(resolvedPolicyPath))
	{
		SetDiagnosticsText(FString::Printf(TEXT("행동 정책 파일을 찾을 수 없습니다: %s"), *resolvedPolicyPath));
		return;
	}

	FString openError;
	if (!OpenTextFileInExternalEditor(resolvedPolicyPath, openError))
	{
		SetDiagnosticsText(openError);
		return;
	}

	SetDiagnosticsText(FString::Printf(TEXT("행동 정책 파일을 열었습니다: %s"), *policyPath));
}

void UMainMenuWidget::HandleNewPolicyClicked()
{
	const FString newPolicyPath = MakeUniqueInputJsonPath(TEXT("DeliveryBotSetupNew"));
	FString errorText;
	if (!UPlatformUiSubsystem::CreateTextFileFromTemplate(DeliveryBotTemplatePath, newPolicyPath, errorText))
	{
		SetDiagnosticsText(errorText.IsEmpty()
			? FString::Printf(TEXT("행동 정책 파일 생성 실패: %s"), *newPolicyPath)
			: errorText);
		return;
	}

	SetSelectedDeliveryBotSetupPath(newPolicyPath);
	RefreshSetupOptions();

	SetDiagnosticsText(FString::Printf(TEXT("행동 정책 생성됨: %s"), *newPolicyPath));
}

void UMainMenuWidget::HandleStartClicked()
{
	if (IsProjectModeSelected())
	{
		if (!ProjectWorkspaceViewModel)
		{
			SetDiagnosticsText(TEXT("ProjectWorkspaceViewModel이 없습니다."));
			UpdateStatusText(TEXT("Project run creation failed."));
			return;
		}

		if (ProjectWorkspaceViewModel->StartRun())
		{
			SetSelectedProjectRunId(ProjectWorkspaceViewModel->GetSelectedRunId());
			RefreshProjectRunSelection();
			RefreshExperimentResultList();
			UpdateStatusText(TEXT("Project simulator launch requested."));
			return;
		}

		UpdateStatusText(ProjectWorkspaceViewModel->GetDiagnosticsText());
		return;
	}

	UPlatformUiSubsystem* platformUiSubsystem = GetPlatformUiSubsystem();
	if (!platformUiSubsystem)
	{
		UpdateStatusText(TEXT("PlatformUiSubsystem을 사용할 수 없습니다."));
		return;
	}

	const FString requestedRunId = RunIdTextBox ? RunIdTextBox->GetText().ToString().TrimStartAndEnd() : FString();
	FString errorText;
	if (platformUiSubsystem->StartLegacySimulationRun(GetSelectedSetupPath(), requestedRunId, errorText))
	{
		UpdateStatusText(TEXT("Simulator launch requested."));
		return;
	}

	UpdateStatusText(errorText);
}

void UMainMenuWidget::HandleRefreshClicked()
{
	RefreshSetupOptions();
	RefreshFromSubsystem();
}

void UMainMenuWidget::HandleSendToAiClicked()
{
	if (IsProjectModeSelected())
	{
		const FString runId = GetSelectedProjectRunId();
		if (runId.IsEmpty())
		{
			if (AiAnalysisTextBlock)
			{
				AiAnalysisTextBlock->SetText(FText::FromString(TEXT("AI analysis unavailable: no project run selected.")));
			}
			return;
		}

		if (!ExperimentResultViewModel && !ProjectWorkspaceViewModel)
		{
			if (AiAnalysisTextBlock)
			{
				AiAnalysisTextBlock->SetText(FText::FromString(TEXT("AI analysis unavailable: ViewModel not found.")));
			}
			return;
		}

		if (AiAnalysisTextBlock)
		{
			AiAnalysisTextBlock->SetText(FText::FromString(TEXT("Analyzing project run...")));
		}
		PendingProjectRunAnalysisRunId = runId;
		LastProjectRunAnalysisFailureRunId.Reset();
		LastProjectRunAnalysisFailureText.Reset();
		if (SendToAiButton)
		{
			SendToAiButton->SetIsEnabled(false);
		}

		const bool bRequested = ExperimentResultViewModel
			? ExperimentResultViewModel->RequestAiAnalysis(GetSelectedProjectPath(), runId)
			: ProjectWorkspaceViewModel->RequestAiAnalysis();
		if (!bRequested)
		{
			const FString diagnostics = ExperimentResultViewModel
				? ExperimentResultViewModel->GetDiagnosticsText()
				: ProjectWorkspaceViewModel->GetDiagnosticsText();
			if (AiAnalysisTextBlock)
			{
				AiAnalysisTextBlock->SetText(FText::FromString(diagnostics.IsEmpty()
					? TEXT("AI analysis request failed.")
					: diagnostics));
			}
			if (SendToAiButton)
			{
				SendToAiButton->SetIsEnabled(true);
			}
		}
		return;
	}

	if (AiAnalysisTextBlock)
	{
		AiAnalysisTextBlock->SetText(FText::FromString(
			TEXT("AI analysis unavailable: legacy evaluation report analysis is no longer supported. Select a project run.")));
	}
}

void UMainMenuWidget::HandleShowProjectScenarioTabClicked()
{
	ShowProjectWorkspaceTab(EProjectWorkspaceTabType::ScenarioEdit);
}

void UMainMenuWidget::HandleShowProjectExperimentStatusTabClicked()
{
	RefreshExperimentResultList();
	if (IsProjectRunDirectory(SelectedExperimentResultRunDirectory))
	{
		RefreshExperimentResultDetailPanel();
	}
	else
	{
		UpdateReportAndLogText();
	}
	ShowProjectWorkspaceTab(EProjectWorkspaceTabType::ExperimentStatus);
}

void UMainMenuWidget::HandleProjectHomeClicked()
{
	if (ProjectWorkspaceViewModel && ProjectWorkspaceViewModel->ReturnToStartup())
	{
		return;
	}

	SetDiagnosticsText(TEXT("ProjectWorkspaceViewModel을 통해 Startup으로 돌아갈 수 없습니다."));
}

void UMainMenuWidget::HandleConfigureExperimentClicked()
{
	TArray<FString> diagnostics;
	if (!LoadProjectExperimentSettingIntoPanel(diagnostics))
	{
		SetDiagnosticsText(JoinStringLines(diagnostics));
		UpdateStatusText(TEXT("Experiment creation failed."));
		return;
	}
	if (ExperimentConfigViewModel)
	{
		ExperimentConfigViewModel->LoadFromActiveProject();
	}

	ShowProjectExperimentConfigPanel(true);
	OpenTransientProjectTab(EProjectWorkspaceTabType::ExperimentConfig, FText::FromString(TEXT("실험 설정")));
	SetDiagnosticsText(TEXT("새 실험 설정을 수정하세요."));
	UpdateStatusText();
}

void UMainMenuWidget::HandleRunExperimentClicked()
{
	TArray<FString> diagnostics;
	FString runId;
	if (!StartProjectExperimentRun(diagnostics, runId))
	{
		SetDiagnosticsText(JoinStringLines(diagnostics));
		UpdateStatusText(TEXT("Experiment launch failed."));
		return;
	}

	RefreshProjectRunSelection();
	RefreshExperimentResultList();
	ShowProjectWorkspaceTab(EProjectWorkspaceTabType::ExperimentStatus);
	SetDiagnosticsText(FString::Printf(TEXT("실험 실행을 시작했습니다: %s"), *FormatProjectRunDisplayId(runId)));
	UpdateStatusText(TEXT("Project simulator launch requested."));
}

void UMainMenuWidget::HandleCreateExperimentFromConfigClicked()
{
	TArray<FString> diagnostics;
	if (!SaveProjectExperimentSettingFromPanel(diagnostics))
	{
		const FString message = JoinStringLines(diagnostics);
		SetProjectExperimentConfigWarningText(message);
		SetDiagnosticsText(message);
		UpdateStatusText(TEXT("Experiment setting save failed."));
		return;
	}

	FString runId;
	if (!StartProjectExperimentRun(diagnostics, runId))
	{
		const FString message = JoinStringLines(diagnostics);
		SetProjectExperimentConfigWarningText(message);
		SetDiagnosticsText(message);
		UpdateStatusText(TEXT("Experiment launch failed."));
		return;
	}

	SetProjectExperimentConfigWarningText(FString());
	CloseTransientProjectTab(EProjectWorkspaceTabType::ExperimentConfig);
	RefreshProjectRunSelection();
	RefreshExperimentResultList();
	ShowProjectWorkspaceTab(EProjectWorkspaceTabType::ExperimentStatus);
	SetDiagnosticsText(FString::Printf(TEXT("실험 실행을 시작했습니다: %s"), *FormatProjectRunDisplayId(runId)));
	UpdateStatusText(TEXT("Project simulator launch requested."));
}

void UMainMenuWidget::HandleSaveExperimentConfigClicked()
{
	TArray<FString> diagnostics;
	if (!SaveProjectExperimentSettingFromPanel(diagnostics))
	{
		const FString message = JoinStringLines(diagnostics);
		SetProjectExperimentConfigWarningText(message);
		SetDiagnosticsText(message);
		UpdateStatusText(TEXT("Experiment setting save failed."));
		return;
	}

	SetProjectExperimentConfigWarningText(FString());
	CloseTransientProjectTab(EProjectWorkspaceTabType::ExperimentConfig);
	RefreshProjectRunSelection();
	RefreshExperimentResultList();
	SetDiagnosticsText(TEXT("실험 설정을 저장했습니다."));
	UpdateStatusText(TEXT("Experiment setting saved."));
}

void UMainMenuWidget::HandleCancelExperimentConfigClicked()
{
	SetProjectExperimentConfigWarningText(FString());
	CloseTransientProjectTab(EProjectWorkspaceTabType::ExperimentConfig);
	SetDiagnosticsText(TEXT("실험 설정 수정을 취소했습니다."));
}

void UMainMenuWidget::HandleShowScenarioClicked()
{
	ShowMainMenuSection(static_cast<int32>(EMainMenuSection::Scenario));
}

void UMainMenuWidget::HandleShowPolicyClicked()
{
	ShowMainMenuSection(static_cast<int32>(EMainMenuSection::Policy));
}

void UMainMenuWidget::HandleShowExperimentConfigClicked()
{
	SetExperimentConfigDetailVisible(false);
	ShowMainMenuSection(static_cast<int32>(EMainMenuSection::ExperimentConfig));
}

void UMainMenuWidget::HandleShowRunStatusClicked()
{
	ShowMainMenuSection(static_cast<int32>(EMainMenuSection::RunStatus));
}

void UMainMenuWidget::HandleShowExperimentResultClicked()
{
	ClearExperimentResultIterationWidgets();
	SetExperimentResultDetailVisible(false);
	if (IsProjectModeSelected())
	{
		RefreshExperimentResultList();
		UpdateReportAndLogText();
		ShowProjectWorkspaceTab(EProjectWorkspaceTabType::ExperimentStatus);
		return;
	}

	ShowMainMenuSection(static_cast<int32>(EMainMenuSection::ExperimentResult));
}

void UMainMenuWidget::HandleExperimentResultBackClicked()
{
	ClearExperimentResultIterationWidgets();
	SetExperimentResultDetailVisible(false);
	RefreshExperimentResultList();
	SetDiagnosticsText(TEXT("실험 결과 목록으로 돌아왔습니다."));
}

void UMainMenuWidget::HandleExperimentConfigBackClicked()
{
	SetExperimentConfigDetailVisible(false);
	RefreshExperimentConfigList();
	SetDiagnosticsText(TEXT("SimulationSetup 목록으로 돌아왔습니다."));
}

void UMainMenuWidget::HandleScenarioSetupSelectionChanged(FString selectedItem, ESelectInfo::Type selectionType)
{
	(void)selectionType;
	SetSelectedScenarioSetupPath(selectedItem);
}

void UMainMenuWidget::HandlePolicyDeliveryBotSelectionChanged(FString selectedItem, ESelectInfo::Type selectionType)
{
	(void)selectionType;
	SetSelectedDeliveryBotSetupPath(selectedItem);
}

void UMainMenuWidget::HandleExperimentScenarioSetupSelectionChanged(FString selectedItem, ESelectInfo::Type selectionType)
{
	(void)selectionType;
	SetSelectedScenarioSetupPath(selectedItem);
}

void UMainMenuWidget::HandleExperimentDeliveryBotSelectionChanged(FString selectedItem, ESelectInfo::Type selectionType)
{
	(void)selectionType;
	SetSelectedDeliveryBotSetupPath(selectedItem);
}

void UMainMenuWidget::HandlePolicySpecSelectionChanged(FString selectedItem, ESelectInfo::Type selectionType)
{
	(void)selectionType;
	SetSelectedPolicySpecPath(selectedItem);
}

void UMainMenuWidget::ShowMainMenuSection(const int32 sectionIndex)
{
	if (MainContentSwitcher)
	{
		const int32 widgetCount = MainContentSwitcher->GetChildrenCount();
		if (sectionIndex >= 0 && sectionIndex < widgetCount)
		{
			MainContentSwitcher->SetActiveWidgetIndex(sectionIndex);
		}
	}

	if (ActiveSectionTextBlock)
	{
		ActiveSectionTextBlock->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMainMenuWidget::ShowProjectWorkspaceScreen()
{
	ApplyProjectWorkspaceTabState(ActiveProjectWorkspaceTab);
}

void UMainMenuWidget::ShowProjectWorkspaceTab(const EProjectWorkspaceTabType tabType)
{
	ActiveProjectWorkspaceTab = tabType;
	if (ProjectWorkspaceViewModel)
	{
		FName tabId = ProjectScenarioEditTabId;
		switch (tabType)
		{
		case EProjectWorkspaceTabType::ScenarioEdit:
			tabId = ProjectScenarioEditTabId;
			break;
		case EProjectWorkspaceTabType::RobotConfig:
			tabId = ProjectRobotConfigTabId;
			break;
		case EProjectWorkspaceTabType::ExperimentStatus:
			tabId = ProjectExperimentStatusTabId;
			break;
		case EProjectWorkspaceTabType::ExperimentConfig:
			tabId = ProjectExperimentConfigTabId;
			break;
		case EProjectWorkspaceTabType::ExperimentResultDetail:
			tabId = ProjectExperimentResultDetailTabId;
			break;
		}
		ProjectWorkspaceViewModel->SelectWorkspaceTab(tabId);
	}
	if (!ProjectWorkspaceSwitcher)
	{
		ApplyProjectWorkspaceTabState(tabType);
		return;
	}

	auto setSwitcherIndex = [this](const int32 tabIndex)
	{
		const int32 widgetCount = ProjectWorkspaceSwitcher->GetChildrenCount();
		if (tabIndex >= 0 && tabIndex < widgetCount)
		{
			ProjectWorkspaceSwitcher->SetActiveWidgetIndex(tabIndex);
		}
	};

	auto setSwitcherWidget = [this](UWidget* targetPage)
	{
		if (IsValid(targetPage) && ProjectWorkspaceSwitcher->GetChildIndex(targetPage) != INDEX_NONE)
		{
			ProjectWorkspaceSwitcher->SetActiveWidget(targetPage);
			return true;
		}
		return false;
	};

	switch (tabType)
	{
	case EProjectWorkspaceTabType::ScenarioEdit:
		ShowProjectExperimentConfigPanel(false);
		setSwitcherIndex(0);
		break;
	case EProjectWorkspaceTabType::RobotConfig:
		ShowProjectExperimentConfigPanel(false);
		if (!setSwitcherWidget(ProjectRobotConfigPanel.Get()))
		{
			setSwitcherIndex(1);
		}
		break;
	case EProjectWorkspaceTabType::ExperimentStatus:
		ShowProjectExperimentConfigPanel(false);
		if (!setSwitcherWidget(ProjectExperimentStatusPanel.Get()))
		{
			setSwitcherIndex(2);
		}
		break;
	case EProjectWorkspaceTabType::ExperimentConfig:
		ShowProjectExperimentConfigPanel(true);
		if (!setSwitcherWidget(ProjectExperimentConfigPanel.Get()))
		{
			setSwitcherIndex(3);
		}
		break;
	case EProjectWorkspaceTabType::ExperimentResultDetail:
		ShowProjectExperimentConfigPanel(false);
		if (!setSwitcherWidget(ProjectExperimentResultDetailPanel.Get()))
		{
			UE_LOG(
				LogMainMenuWidget,
				Warning,
				TEXT("Project experiment result detail page is not a child of ProjectWorkspaceSwitcher."));
		}
		break;
	}

	ApplyProjectWorkspaceTabState(tabType);
}

void UMainMenuWidget::OpenTransientProjectTab(const EProjectWorkspaceTabType tabType, const FText& label)
{
	UProjectWorkspaceTabWidget* targetTab = nullptr;
	if (tabType == EProjectWorkspaceTabType::ExperimentConfig)
	{
		bProjectExperimentConfigTabOpen = true;
		targetTab = ExperimentConfigTab.Get();
	}
	else if (tabType == EProjectWorkspaceTabType::ExperimentResultDetail)
	{
		bProjectExperimentResultDetailTabOpen = true;
		targetTab = ExperimentResultDetailTab.Get();
	}

	if (targetTab)
	{
		targetTab->SetTabLabel(label);
		targetTab->SetTabClosable(true);
		targetTab->SetTabVisible(true);
	}

	ShowProjectWorkspaceTab(tabType);
}

void UMainMenuWidget::CloseTransientProjectTab(const EProjectWorkspaceTabType tabType)
{
	if (tabType == EProjectWorkspaceTabType::ExperimentConfig)
	{
		bProjectExperimentConfigTabOpen = false;
		ShowProjectExperimentConfigPanel(false);
		if (ExperimentConfigTab)
		{
			ExperimentConfigTab->SetTabVisible(false);
		}
	}
	else if (tabType == EProjectWorkspaceTabType::ExperimentResultDetail)
	{
		bProjectExperimentResultDetailTabOpen = false;
		ClearExperimentResultIterationWidgets();
		if (ExperimentResultDetailTab)
		{
			ExperimentResultDetailTab->SetTabVisible(false);
		}
	}

	if (ActiveProjectWorkspaceTab == tabType)
	{
		ShowProjectWorkspaceTab(EProjectWorkspaceTabType::ExperimentStatus);
	}
	else
	{
		ApplyProjectWorkspaceTabState(ActiveProjectWorkspaceTab);
	}
}

void UMainMenuWidget::ApplyProjectWorkspaceTabState(const EProjectWorkspaceTabType activeTabType)
{
	if (ScenarioEditTab)
	{
		ScenarioEditTab->SetTabVisible(true);
		ScenarioEditTab->SetTabActive(activeTabType == EProjectWorkspaceTabType::ScenarioEdit);
	}
	if (RobotConfigTab)
	{
		RobotConfigTab->SetTabVisible(true);
		RobotConfigTab->SetTabActive(activeTabType == EProjectWorkspaceTabType::RobotConfig);
	}
	if (ExperimentStatusTab)
	{
		ExperimentStatusTab->SetTabVisible(true);
		ExperimentStatusTab->SetTabActive(activeTabType == EProjectWorkspaceTabType::ExperimentStatus);
	}
	if (ExperimentConfigTab)
	{
		ExperimentConfigTab->SetTabVisible(bProjectExperimentConfigTabOpen);
		ExperimentConfigTab->SetTabActive(activeTabType == EProjectWorkspaceTabType::ExperimentConfig);
	}
	if (ExperimentResultDetailTab)
	{
		ExperimentResultDetailTab->SetTabVisible(bProjectExperimentResultDetailTabOpen);
		ExperimentResultDetailTab->SetTabActive(activeTabType == EProjectWorkspaceTabType::ExperimentResultDetail);
	}
}

void UMainMenuWidget::HandleProjectWorkspaceTabSelected(UProjectWorkspaceTabWidget* tabWidget)
{
	if (!IsValid(tabWidget))
	{
		return;
	}

	if (tabWidget == ScenarioEditTab.Get() || tabWidget->GetTabId() == ProjectScenarioEditTabId)
	{
		HandleShowProjectScenarioTabClicked();
	}
	else if (tabWidget == RobotConfigTab.Get() || tabWidget->GetTabId() == ProjectRobotConfigTabId)
	{
		ShowProjectWorkspaceTab(EProjectWorkspaceTabType::RobotConfig);
	}
	else if (tabWidget == ExperimentStatusTab.Get() || tabWidget->GetTabId() == ProjectExperimentStatusTabId)
	{
		HandleShowProjectExperimentStatusTabClicked();
	}
	else if (bProjectExperimentConfigTabOpen
		&& (tabWidget == ExperimentConfigTab.Get() || tabWidget->GetTabId() == ProjectExperimentConfigTabId))
	{
		ShowProjectWorkspaceTab(EProjectWorkspaceTabType::ExperimentConfig);
	}
	else if (bProjectExperimentResultDetailTabOpen
		&& (tabWidget == ExperimentResultDetailTab.Get() || tabWidget->GetTabId() == ProjectExperimentResultDetailTabId))
	{
		ShowProjectWorkspaceTab(EProjectWorkspaceTabType::ExperimentResultDetail);
	}
}

void UMainMenuWidget::HandleProjectWorkspaceTabCloseRequested(UProjectWorkspaceTabWidget* tabWidget)
{
	if (!IsValid(tabWidget))
	{
		return;
	}

	if (tabWidget == ExperimentConfigTab.Get() || tabWidget->GetTabId() == ProjectExperimentConfigTabId)
	{
		SetProjectExperimentConfigWarningText(FString());
		CloseTransientProjectTab(EProjectWorkspaceTabType::ExperimentConfig);
		SetDiagnosticsText(TEXT("실험 설정 수정을 취소했습니다."));
	}
	else if (tabWidget == ExperimentResultDetailTab.Get() || tabWidget->GetTabId() == ProjectExperimentResultDetailTabId)
	{
		CloseTransientProjectTab(EProjectWorkspaceTabType::ExperimentResultDetail);
		RefreshExperimentResultList();
		UpdateReportAndLogText();
		SetDiagnosticsText(TEXT("실험 결과 목록으로 돌아왔습니다."));
	}
}

void UMainMenuWidget::SyncComboBoxSelection(UComboBoxString* targetComboBox, const FString& selectedItem)
{
	if (!targetComboBox || selectedItem.IsEmpty() || targetComboBox->GetSelectedOption() == selectedItem)
	{
		return;
	}

	targetComboBox->SetSelectedOption(selectedItem);
}

bool UMainMenuWidget::ValidateRequiredBindings() const
{
	TArray<FString> missingWidgetNames;
	auto requireWidget = [&missingWidgetNames](const UObject* widget, const TCHAR* widgetName)
	{
		if (!IsValid(widget))
		{
			missingWidgetNames.Add(widgetName);
		}
	};

	requireWidget(ProjectWorkspaceScreen, TEXT("ProjectWorkspaceScreen"));
	requireWidget(ScenarioEditorRootWidget, TEXT("ScenarioEditorRootWidget"));
	requireWidget(ProjectWorkspaceSwitcher, TEXT("ProjectWorkspaceSwitcher"));
	requireWidget(ScenarioEditTab, TEXT("ScenarioEditTab"));
	requireWidget(RobotConfigTab, TEXT("RobotConfigTab"));
	requireWidget(ExperimentStatusTab, TEXT("ExperimentStatusTab"));
	requireWidget(ConfigureExperimentButton, TEXT("ConfigureExperimentButton"));
	requireWidget(RunExperimentButton, TEXT("RunExperimentButton"));
	requireWidget(ProjectRobotConfigPanel, TEXT("ProjectRobotConfigPanel"));
	requireWidget(ProjectExperimentConfigPanel, TEXT("ProjectExperimentConfigPanel"));
	requireWidget(ProjectExperimentResultDetailPanel, TEXT("ProjectExperimentResultDetailPanel"));
	requireWidget(CreateExperimentConfigButton, TEXT("CreateExperimentConfigButton"));
	requireWidget(SaveExperimentConfigButton, TEXT("SaveExperimentConfigButton"));
	requireWidget(CancelExperimentConfigButton, TEXT("CancelExperimentConfigButton"));
	requireWidget(ExperimentResultDetailSectionBoxScrollBox, TEXT("ExperimentResultDetailSectionBoxScrollBox"));
	requireWidget(ExperimentResultListScrollBox, TEXT("ExperimentResultListScrollBox"));
	requireWidget(ExperimentResultDetailTitleText, TEXT("ExperimentResultDetailTitleText"));
	requireWidget(TotalPlayTimeMetricCard, TEXT("TotalPlayTimeMetricCard"));
	requireWidget(SuccessRateMetricCard, TEXT("SuccessRateMetricCard"));
	requireWidget(CollisionCountMetricCard, TEXT("CollisionCountMetricCard"));
	requireWidget(EpisodeReplayCountText, TEXT("EpisodeReplayCountText"));
	requireWidget(EpisodeReplayCardWrapBox, TEXT("EpisodeReplayCardWrapBox"));
	requireWidget(ProjectEpisodeReplayViewerWidget, TEXT("ProjectEpisodeReplayViewerWidget"));
	requireWidget(ProjectEpisodeReplayNormalHost, TEXT("ProjectEpisodeReplayNormalHost"));
	requireWidget(ProjectEpisodeReplayFullscreenHost, TEXT("ProjectEpisodeReplayFullscreenHost"));
	requireWidget(AiAnalysisActionBox, TEXT("AiAnalysisActionBox"));
	requireWidget(AiSuggestionPanel, TEXT("AiSuggestionPanel"));
	requireWidget(AiSuggestionSummaryText, TEXT("AiSuggestionSummaryText"));
	requireWidget(AiSuggestionListBox, TEXT("AiSuggestionListBox"));
	requireWidget(AiSuggestionEmptyText, TEXT("AiSuggestionEmptyText"));
	requireWidget(DiagnosticsTextBlock, TEXT("DiagnosticsTextBlock"));

	if (missingWidgetNames.IsEmpty())
	{
		return true;
	}

	const FString missingWidgetSummary = FString::Join(missingWidgetNames, TEXT(", "));
	UE_LOG(
		LogMainMenuWidget,
		Error,
		TEXT("WBP_MainMenu binding is invalid. Missing widgets: %s"),
		*missingWidgetSummary);
	ensureMsgf(false, TEXT("WBP_MainMenu binding is invalid. Missing widgets: %s"), *missingWidgetSummary);
	return false;
}

void UMainMenuWidget::BindControls()
{
	if (ScenarioNavButton)
	{
		ScenarioNavButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleShowScenarioClicked);
		ScenarioNavButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleShowScenarioClicked);
	}

	if (PolicyNavButton)
	{
		PolicyNavButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleShowPolicyClicked);
		PolicyNavButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleShowPolicyClicked);
	}

	if (ExperimentConfigNavButton)
	{
		ExperimentConfigNavButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleShowExperimentConfigClicked);
		ExperimentConfigNavButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleShowExperimentConfigClicked);
	}

	if (RunStatusNavButton)
	{
		RunStatusNavButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleShowRunStatusClicked);
		RunStatusNavButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleShowRunStatusClicked);
	}

	if (ExperimentResultNavButton)
	{
		ExperimentResultNavButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleShowExperimentResultClicked);
		ExperimentResultNavButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleShowExperimentResultClicked);
	}

	if (ExperimentResultBackButton)
	{
		ExperimentResultBackButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleExperimentResultBackClicked);
		ExperimentResultBackButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleExperimentResultBackClicked);
	}

	if (ExperimentConfigBackButton)
	{
		ExperimentConfigBackButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleExperimentConfigBackClicked);
		ExperimentConfigBackButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleExperimentConfigBackClicked);
	}

	if (SetupComboBox)
	{
		SetupComboBox->OnSelectionChanged.RemoveDynamic(this, &UMainMenuWidget::HandleSetupSelectionChanged);
		SetupComboBox->OnSelectionChanged.AddDynamic(this, &UMainMenuWidget::HandleSetupSelectionChanged);
	}

	if (ScenarioSetupComboBox)
	{
		ScenarioSetupComboBox->OnSelectionChanged.RemoveDynamic(this, &UMainMenuWidget::HandleScenarioSetupSelectionChanged);
		ScenarioSetupComboBox->OnSelectionChanged.AddDynamic(this, &UMainMenuWidget::HandleScenarioSetupSelectionChanged);
	}

	if (PolicyDeliveryBotSetupComboBox)
	{
		PolicyDeliveryBotSetupComboBox->OnSelectionChanged.RemoveDynamic(this, &UMainMenuWidget::HandlePolicyDeliveryBotSelectionChanged);
		PolicyDeliveryBotSetupComboBox->OnSelectionChanged.AddDynamic(this, &UMainMenuWidget::HandlePolicyDeliveryBotSelectionChanged);
	}

	if (ExperimentScenarioSetupComboBox)
	{
		ExperimentScenarioSetupComboBox->OnSelectionChanged.RemoveDynamic(this, &UMainMenuWidget::HandleExperimentScenarioSetupSelectionChanged);
		ExperimentScenarioSetupComboBox->OnSelectionChanged.AddDynamic(this, &UMainMenuWidget::HandleExperimentScenarioSetupSelectionChanged);
	}

	if (DeliveryBotSetupComboBox)
	{
		DeliveryBotSetupComboBox->OnSelectionChanged.RemoveDynamic(this, &UMainMenuWidget::HandleExperimentDeliveryBotSelectionChanged);
		DeliveryBotSetupComboBox->OnSelectionChanged.AddDynamic(this, &UMainMenuWidget::HandleExperimentDeliveryBotSelectionChanged);
	}

	if (PolicySpecComboBox)
	{
		PolicySpecComboBox->OnSelectionChanged.RemoveDynamic(this, &UMainMenuWidget::HandlePolicySpecSelectionChanged);
		PolicySpecComboBox->OnSelectionChanged.AddDynamic(this, &UMainMenuWidget::HandlePolicySpecSelectionChanged);
	}

	if (LoadButton)
	{
		LoadButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleLoadClicked);
		LoadButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleLoadClicked);
	}

	if (NewSetupButton)
	{
		NewSetupButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleNewSetupClicked);
		NewSetupButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleNewSetupClicked);
	}

	if (SaveFpsButton)
	{
		SaveFpsButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleSaveFpsClicked);
		SaveFpsButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleSaveFpsClicked);
	}

	if (SaveSetupButton)
	{
		SaveSetupButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleSaveSetupClicked);
		SaveSetupButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleSaveSetupClicked);
	}

	if (OpenEditorButton)
	{
		OpenEditorButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleOpenEditorClicked);
		OpenEditorButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleOpenEditorClicked);
	}

	if (NewScenarioButton)
	{
		NewScenarioButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleNewScenarioClicked);
		NewScenarioButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleNewScenarioClicked);
	}

	if (OpenPolicyTextEditorButton)
	{
		OpenPolicyTextEditorButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleOpenPolicyTextEditorClicked);
		OpenPolicyTextEditorButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleOpenPolicyTextEditorClicked);
	}

	if (NewPolicyButton)
	{
		NewPolicyButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleNewPolicyClicked);
		NewPolicyButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleNewPolicyClicked);
	}

	if (StartButton)
	{
		StartButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleStartClicked);
		StartButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleStartClicked);
	}

	if (RefreshButton)
	{
		RefreshButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleRefreshClicked);
		RefreshButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleRefreshClicked);
	}

	if (SendToAiButton)
	{
		SendToAiButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleSendToAiClicked);
		SendToAiButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleSendToAiClicked);
	}

	BindProjectModeControls();
}

void UMainMenuWidget::BindProjectModeControls()
{
	UE_LOG(
		LogMainMenuWidget,
		Log,
		TEXT("Project mode controls bound: %s"),
		ScenarioEditTab && RobotConfigTab && ExperimentStatusTab && ConfigureExperimentButton && RunExperimentButton
			? TEXT("true")
			: TEXT("false"));

	auto bindWorkspaceTab = [this](
		UProjectWorkspaceTabWidget* tabWidget,
		const FName& tabId,
		const FText& label,
		const bool bClosable,
		const bool bVisible)
	{
		if (!tabWidget)
		{
			return;
		}

		tabWidget->SetTabId(tabId);
		tabWidget->SetTabLabel(label);
		tabWidget->SetTabClosable(bClosable);
		tabWidget->SetTabVisible(bVisible);
		tabWidget->OnSelectedRequested.RemoveAll(this);
		tabWidget->OnSelectedRequested.AddUObject(this, &UMainMenuWidget::HandleProjectWorkspaceTabSelected);
		tabWidget->OnCloseRequested.RemoveAll(this);
		tabWidget->OnCloseRequested.AddUObject(this, &UMainMenuWidget::HandleProjectWorkspaceTabCloseRequested);
	};

	bindWorkspaceTab(
		ScenarioEditTab.Get(),
		ProjectScenarioEditTabId,
		FText::FromString(TEXT("시나리오")),
		false,
		true);
	bindWorkspaceTab(
		RobotConfigTab.Get(),
		ProjectRobotConfigTabId,
		FText::FromString(TEXT("로봇 구성")),
		false,
		true);
	bindWorkspaceTab(
		ExperimentStatusTab.Get(),
		ProjectExperimentStatusTabId,
		FText::FromString(TEXT("실험")),
		false,
		true);
	bindWorkspaceTab(
		ExperimentConfigTab.Get(),
		ProjectExperimentConfigTabId,
		FText::FromString(TEXT("실험 설정")),
		true,
		bProjectExperimentConfigTabOpen);
	bindWorkspaceTab(
		ExperimentResultDetailTab.Get(),
		ProjectExperimentResultDetailTabId,
		FText::FromString(TEXT("분석")),
		true,
		bProjectExperimentResultDetailTabOpen);

	if (HomeButton)
	{
		HomeButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleProjectHomeClicked);
		HomeButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleProjectHomeClicked);
	}

	if (ConfigureExperimentButton)
	{
		ConfigureExperimentButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleConfigureExperimentClicked);
		ConfigureExperimentButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleConfigureExperimentClicked);
	}

	if (RunExperimentButton)
	{
		RunExperimentButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleRunExperimentClicked);
		RunExperimentButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleRunExperimentClicked);
	}

	if (CreateExperimentConfigButton)
	{
		CreateExperimentConfigButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleCreateExperimentFromConfigClicked);
		CreateExperimentConfigButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleCreateExperimentFromConfigClicked);
	}

	if (SaveExperimentConfigButton)
	{
		SaveExperimentConfigButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleSaveExperimentConfigClicked);
		SaveExperimentConfigButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleSaveExperimentConfigClicked);
	}

	if (CancelExperimentConfigButton)
	{
		CancelExperimentConfigButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleCancelExperimentConfigClicked);
		CancelExperimentConfigButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleCancelExperimentConfigClicked);
	}

	if (ProjectEpisodeReplayViewerWidget)
	{
		ProjectEpisodeReplayViewerWidget->OnReplayFullscreenChanged.RemoveAll(this);
		ProjectEpisodeReplayViewerWidget->OnReplayFullscreenChanged.AddUObject(
			this,
			&UMainMenuWidget::HandleProjectEpisodeReplayFullscreenChanged);
		RestoreProjectEpisodeReplayViewerToNormalHost();
	}

	ApplyProjectWorkspaceTabState(ActiveProjectWorkspaceTab);
}

void UMainMenuWidget::LoadSelectedSetup()
{
	UPlatformUiSubsystem* platformUiSubsystem = GetPlatformUiSubsystem();
	if (!platformUiSubsystem) return;

	const FSimulationSetupParseResult parseResult =
		platformUiSubsystem->LoadLegacySimulationSetupFile(GetSelectedSetupPath());
	if (!parseResult.bSuccess)
	{
		if (GetSelectedSetupPath().TrimStartAndEnd().IsEmpty())
		{
			SetDiagnosticsText(TEXT("SimulationSetup이 선택되지 않았습니다."));
			return;
		}

		if (!UPlatformUiSubsystem::DoesResolvedFileExist(GetSelectedSetupPath()))
		{
			SetDiagnosticsText(FString::Printf(TEXT("SimulationSetup 파일이 아직 없습니다: %s\n저장 버튼으로 생성하세요."), *GetSelectedSetupPath()));
			return;
		}

		// Start Run 전에 setup 계약 위반을 보여줘 simulator process를 불필요하게 띄우지 않는다.
		TArray<FString> diagnostics;
		for (const FScenarioCompileDiagnostic& diagnostic : parseResult.Diagnostics)
		{
			diagnostics.Add(FString::Printf(TEXT("%s: %s"), *diagnostic.Code, *diagnostic.Message));
		}
		SetDiagnosticsText(JoinStringLines(diagnostics));
		return;
	}

	if (FixedStepFpsTextBox)
	{
		FixedStepFpsTextBox->SetText(FText::AsNumber(parseResult.Setup.FixedStep.Fps));
	}

	if (MapIdTextBox)
	{
		MapIdTextBox->SetText(FText::FromString(parseResult.Setup.MapId));
	}

	// SimulationSetup stores a generated ScenarioRunQueue path; the detail page exposes the resolved pair and run count.
	TArray<FScenarioRunInput> loadedRunInputs;
	TArray<FString> runQueueDiagnostics;
	if (platformUiSubsystem->LoadLegacyScenarioRunQueueFile(
			parseResult.Setup.RunQueueJsonPath,
			loadedRunInputs,
			runQueueDiagnostics)
		&& !loadedRunInputs.IsEmpty())
	{
		const FScenarioRunInput& firstRunInput = loadedRunInputs[0];
		SetSelectedScenarioSetupPath(firstRunInput.EpisodeScenarioJsonPath);
		if (DeliveryBotSetupComboBox)
		{
			DeliveryBotSetupComboBox->SetSelectedOption(firstRunInput.ProfileJsonPath);
		}
		if (PolicyDeliveryBotSetupComboBox)
		{
			PolicyDeliveryBotSetupComboBox->SetSelectedOption(firstRunInput.ProfileJsonPath);
		}
		SetSelectedDeliveryBotSetupPath(firstRunInput.ProfileJsonPath);
		SetSelectedPolicySpecPath(firstRunInput.PolicySpecJsonPath.IsEmpty()
			? FString(MainMenuDefaultPolicySpecJsonPath)
			: firstRunInput.PolicySpecJsonPath);
		if (RunCountTextBox)
		{
			RunCountTextBox->SetText(FText::AsNumber(loadedRunInputs.Num()));
		}
	}
	if (MeasurementLogEnabledCheckBox)
	{
		MeasurementLogEnabledCheckBox->SetIsChecked(parseResult.Setup.MeasurementLog.bEnabled);
	}
	if (MeasurementOutputDirectoryTextBox)
	{
		MeasurementOutputDirectoryTextBox->SetText(FText::FromString(parseResult.Setup.MeasurementLog.OutputDirectory));
	}
	if (MeasurementFilePrefixTextBox)
	{
		MeasurementFilePrefixTextBox->SetText(FText::FromString(parseResult.Setup.MeasurementLog.FilePrefix));
	}
	if (FlushIntervalTicksTextBox)
	{
		FlushIntervalTicksTextBox->SetText(FText::AsNumber(parseResult.Setup.MeasurementLog.FlushIntervalTicks));
	}
	if (ReportOutputDirectoryTextBox)
	{
		ReportOutputDirectoryTextBox->SetText(FText::FromString(DefaultReportOutputDirectory));
	}
	if (StatusOutputPathTextBox)
	{
		StatusOutputPathTextBox->SetText(FText::FromString(parseResult.Setup.Status.OutputPath));
	}

	TArray<FString> lines;
	lines.Add(FString::Printf(TEXT("로드한 SimulationSetup: %s"), *GetSelectedSetupPath()));
	lines.Add(FString::Printf(TEXT("RunQueue: %s"), *parseResult.Setup.RunQueueJsonPath));
	if (!loadedRunInputs.IsEmpty())
	{
		lines.Add(FString::Printf(TEXT("시나리오(ScenarioSetup): %s"), *loadedRunInputs[0].EpisodeScenarioJsonPath));
		lines.Add(FString::Printf(TEXT("행동 정책(DeliveryBotSetup): %s"), *loadedRunInputs[0].ProfileJsonPath));
		lines.Add(FString::Printf(
			TEXT("PolicySpec: %s"),
			loadedRunInputs[0].PolicySpecJsonPath.IsEmpty() ? MainMenuDefaultPolicySpecJsonPath : *loadedRunInputs[0].PolicySpecJsonPath));
		lines.Add(FString::Printf(TEXT("실행 수: %d"), loadedRunInputs.Num()));
	}
	lines.Add(FString::Printf(TEXT("고정 스텝 FPS: %d"), parseResult.Setup.FixedStep.Fps));
	SetDiagnosticsText(JoinStringLines(lines));
}

void UMainMenuWidget::ApplyNewSetupDefaults(const FString& setupPath)
{
	SetSelectedSetupPath(setupPath);
	if (MapIdTextBox)
	{
		MapIdTextBox->SetText(FText::FromString(MainMenuDefaultSimulationMapId));
	}
	if (FixedStepFpsTextBox)
	{
		FixedStepFpsTextBox->SetText(FText::AsNumber(60));
	}
	if (RunCountTextBox)
	{
		RunCountTextBox->SetText(FText::AsNumber(1));
	}
	SetSelectedPolicySpecPath(MainMenuDefaultPolicySpecJsonPath);
	if (MeasurementLogEnabledCheckBox)
	{
		MeasurementLogEnabledCheckBox->SetIsChecked(true);
	}
	if (MeasurementOutputDirectoryTextBox)
	{
		MeasurementOutputDirectoryTextBox->SetText(FText::FromString(DefaultMeasurementOutputDirectory));
	}
	if (MeasurementFilePrefixTextBox)
	{
		MeasurementFilePrefixTextBox->SetText(FText::FromString(DefaultMeasurementFilePrefix));
	}
	if (FlushIntervalTicksTextBox)
	{
		FlushIntervalTicksTextBox->SetText(FText::AsNumber(DefaultFlushIntervalTicks));
	}
	if (ReportOutputDirectoryTextBox)
	{
		ReportOutputDirectoryTextBox->SetText(FText::FromString(DefaultReportOutputDirectory));
	}
	if (StatusOutputPathTextBox)
	{
		StatusOutputPathTextBox->SetText(FText::FromString(DefaultStatusOutputPath));
	}
}

void UMainMenuWidget::SetExperimentConfigDetailVisible(const bool bVisible)
{
	bExperimentConfigDetailVisible = bVisible;

	if (MainContentSwitcher)
	{
		// Config list/detail are separate switcher pages so old row widgets never leak into the edit surface.
		UWidget* targetPage = bVisible
			? ExperimentConfigDetailSectionBoxScrollBox.Get()
			: ExperimentConfigSectionBoxScrollBox.Get();
		if (IsValid(targetPage))
		{
			MainContentSwitcher->SetActiveWidget(targetPage);
		}
	}
}

void UMainMenuWidget::SetExperimentResultDetailVisible(const bool bVisible)
{
	if (IsProjectModeSelected() && ProjectWorkspaceSwitcher)
	{
		ShowProjectWorkspaceScreen();
		if (bVisible)
		{
			FString runId = ExtractProjectRunIdFromDirectory(SelectedExperimentResultRunDirectory);
			if (!FUserProjectRunSnapshot::IsValidRunId(runId))
			{
				runId = GetSelectedProjectRunId();
			}

			const FText tabLabel = runId.IsEmpty()
				? FText::FromString(TEXT("분석"))
				: FText::FromString(FString::Printf(TEXT("분석 #%s"), *FormatProjectRunDisplayId(runId)));
			OpenTransientProjectTab(EProjectWorkspaceTabType::ExperimentResultDetail, tabLabel);
		}
		else
		{
			ClearExperimentResultDashboardWidgets();
			CloseTransientProjectTab(EProjectWorkspaceTabType::ExperimentResultDetail);
		}
		return;
	}

	if (MainContentSwitcher)
	{
		// Result details are read-only and live on their own page, matching the config list/detail navigation model.
		UWidget* targetPage = bVisible
			? ExperimentResultDetailSectionBoxScrollBox.Get()
			: ExperimentResultSectionBoxScrollBox.Get();
		if (IsValid(targetPage))
		{
			MainContentSwitcher->SetActiveWidget(targetPage);
		}
	}
}

void UMainMenuWidget::RefreshScenarioList()
{
	if (!ScenarioListScrollBox) return;

	UPlatformUiSubsystem* platformUiSubsystem = GetPlatformUiSubsystem();
	if (!platformUiSubsystem) return;

	const TSubclassOf<UFileListItemWidget> itemWidgetClass = ResolveFileListItemWidgetClass();
	if (!itemWidgetClass)
	{
		SetDiagnosticsText(TEXT("WBP_FileListItem 클래스를 사용할 수 없습니다."));
		return;
	}

	const TArray<FString> scenarioSetupFiles = platformUiSubsystem->ListLegacyScenarioSetupFiles();
	if (!scenarioSetupFiles.Contains(SelectedScenarioSetupPath))
	{
		SetSelectedScenarioSetupPath(scenarioSetupFiles.IsEmpty() ? FString() : scenarioSetupFiles[0]);
	}

	ScenarioListScrollBox->ClearChildren();
	ScenarioListItems.Reset();
	ScenarioListItems.Reserve(scenarioSetupFiles.Num());

	const TArray<UOdiroListItemViewModel*> scenarioItems =
		platformUiSubsystem->CreatePathItemViewModels(scenarioSetupFiles, false);
	for (UOdiroListItemViewModel* scenarioItem : scenarioItems)
	{
		UFileListItemWidget* itemWidget = CreateWidget<UFileListItemWidget>(this, itemWidgetClass);
		if (!itemWidget) continue;

		itemWidget->InitializeFromItemViewModel(scenarioItem, TEXT("편집"), TEXT("실행"), true, true, false);
		itemWidget->OnRenameRequested.AddUObject(this, &UMainMenuWidget::HandleScenarioRenameRequested);
		itemWidget->OnPrimaryActionRequested.AddUObject(this, &UMainMenuWidget::HandleScenarioEditRequested);
		ScenarioListScrollBox->AddChild(itemWidget);
		ScenarioListItems.Add(itemWidget);
	}
}

void UMainMenuWidget::RefreshPolicyList()
{
	if (!PolicyListScrollBox) return;

	UPlatformUiSubsystem* platformUiSubsystem = GetPlatformUiSubsystem();
	if (!platformUiSubsystem) return;

	const TSubclassOf<UFileListItemWidget> itemWidgetClass = ResolveFileListItemWidgetClass();
	if (!itemWidgetClass) return;

	const TArray<FString> deliveryBotSetupFiles = platformUiSubsystem->ListLegacyDeliveryBotSetupFiles();
	const FString currentPath = GetSelectedDeliveryBotSetupPath();
	const FString selectedPath = deliveryBotSetupFiles.Contains(currentPath)
		? currentPath
		: (deliveryBotSetupFiles.IsEmpty() ? FString() : deliveryBotSetupFiles[0]);
	SetSelectedDeliveryBotSetupPath(selectedPath);

	PolicyListScrollBox->ClearChildren();
	PolicyListItems.Reset();
	PolicyListItems.Reserve(deliveryBotSetupFiles.Num());

	if (deliveryBotSetupFiles.IsEmpty())
	{
		SetDiagnosticsText(TEXT("편집 가능한 DeliveryBotSetup JSON이 없습니다."));
		return;
	}

	const TArray<UOdiroListItemViewModel*> deliveryBotItems =
		platformUiSubsystem->CreatePathItemViewModels(deliveryBotSetupFiles, false);
	for (UOdiroListItemViewModel* deliveryBotItem : deliveryBotItems)
	{
		UFileListItemWidget* itemWidget = CreateWidget<UFileListItemWidget>(this, itemWidgetClass);
		if (!itemWidget) continue;

		itemWidget->InitializeFromItemViewModel(deliveryBotItem, TEXT("편집"), TEXT("실행"), true, true, false);
		itemWidget->OnRenameRequested.AddUObject(this, &UMainMenuWidget::HandlePolicyRenameRequested);
		itemWidget->OnPrimaryActionRequested.AddUObject(this, &UMainMenuWidget::HandlePolicyEditRequested);
		PolicyListScrollBox->AddChild(itemWidget);
		PolicyListItems.Add(itemWidget);
	}
}

void UMainMenuWidget::RefreshExperimentConfigList()
{
	if (!ExperimentConfigListScrollBox) return;

	UPlatformUiSubsystem* platformUiSubsystem = GetPlatformUiSubsystem();
	if (!platformUiSubsystem) return;

	const TSubclassOf<UFileListItemWidget> itemWidgetClass = ResolveFileListItemWidgetClass();
	if (!itemWidgetClass) return;

	const TArray<FString> setupFiles = platformUiSubsystem->ListLegacySimulationSetupFiles();
	const FString currentPath = GetSelectedSetupPath();
	const FString selectedPath = setupFiles.Contains(currentPath)
		? currentPath
		: (setupFiles.IsEmpty() ? FString() : setupFiles[0]);
	if (!selectedPath.IsEmpty())
	{
		SetSelectedSetupPath(selectedPath);
	}

	ExperimentConfigListScrollBox->ClearChildren();
	ExperimentConfigListItems.Reset();
	ExperimentConfigListItems.Reserve(setupFiles.Num());

	if (setupFiles.IsEmpty())
	{
		SetDiagnosticsText(TEXT("편집 가능한 SimulationSetup JSON이 없습니다."));
		return;
	}

	const TArray<UOdiroListItemViewModel*> setupItems =
		platformUiSubsystem->CreatePathItemViewModels(setupFiles, false);
	for (UOdiroListItemViewModel* setupItem : setupItems)
	{
		UFileListItemWidget* itemWidget = CreateWidget<UFileListItemWidget>(this, itemWidgetClass);
		if (!itemWidget) continue;

		itemWidget->InitializeFromItemViewModel(setupItem, TEXT("편집"), TEXT("실행"), true, true, true);
		itemWidget->OnRenameRequested.AddUObject(this, &UMainMenuWidget::HandleExperimentConfigRenameRequested);
		itemWidget->OnPrimaryActionRequested.AddUObject(this, &UMainMenuWidget::HandleExperimentConfigEditRequested);
		itemWidget->OnSecondaryActionRequested.AddUObject(this, &UMainMenuWidget::HandleExperimentConfigPlayRequested);
		ExperimentConfigListScrollBox->AddChild(itemWidget);
		ExperimentConfigListItems.Add(itemWidget);
	}
}

void UMainMenuWidget::RefreshExperimentResultList()
{
	if (!ExperimentResultListScrollBox) return;

	UPlatformUiSubsystem* platformUiSubsystem = GetPlatformUiSubsystem();

	if (IsProjectModeSelected())
	{
		const TSubclassOf<UProjectExperimentRunRowWidget> runRowWidgetClass = ResolveProjectExperimentRunRowWidgetClass();
		if (!runRowWidgetClass) return;
		if (!ProjectWorkspaceViewModel) return;

		TArray<UOdiroListItemViewModel*> visibleRunItems;
		const FSimulatorRunInfo runInfo = platformUiSubsystem
			? platformUiSubsystem->GetActiveRunInfo()
			: FSimulatorRunInfo();
		ProjectWorkspaceViewModel->RefreshProjectRuns();
		for (UOdiroListItemViewModel* runItem : ProjectWorkspaceViewModel->GetRunItems())
		{
			if (!runItem || runItem->GetPayloadPath().IsEmpty())
			{
				continue;
			}
			if (IsVisibleProjectRunDirectory(runItem->GetPayloadPath(), runInfo, GetSelectedProjectPath()))
			{
				visibleRunItems.AddUnique(runItem);
			}
		}
		visibleRunItems.Sort([](const UOdiroListItemViewModel& left, const UOdiroListItemViewModel& right)
		{
			return left.GetPayloadPath() < right.GetPayloadPath();
		});

		const bool bSelectedRunStillVisible = visibleRunItems.ContainsByPredicate(
			[this](const UOdiroListItemViewModel* runItem)
			{
				return runItem
					&& runItem->GetPayloadPath().Equals(SelectedExperimentResultRunDirectory, ESearchCase::IgnoreCase);
			});
		if (!bSelectedRunStillVisible)
		{
			SetSelectedExperimentResultRunDirectory(visibleRunItems.IsEmpty()
				? FString()
				: visibleRunItems.Last()->GetPayloadPath());
		}
		SetSelectedProjectRunId(ExtractProjectRunIdFromDirectory(SelectedExperimentResultRunDirectory));

		ExperimentResultListScrollBox->ClearChildren();
		ExperimentResultListItems.Reset();
		ProjectExperimentRunRows.Reset();
		ProjectExperimentRunRows.Reserve(visibleRunItems.Num());

		if (visibleRunItems.IsEmpty())
		{
			SetDiagnosticsText(TEXT("실행된 실험이 없습니다."));
			return;
		}

		for (UOdiroListItemViewModel* runItem : visibleRunItems)
		{
			UProjectExperimentRunRowWidget* rowWidget =
				CreateWidget<UProjectExperimentRunRowWidget>(this, runRowWidgetClass);
			if (!rowWidget) continue;

			const FString resultDirectory = runItem ? runItem->GetPayloadPath() : FString();
			const ESimulationRunState displayState =
				ResolveProjectRunDisplayState(resultDirectory, runInfo, GetSelectedProjectPath());
			const bool bCompleted = displayState == ESimulationRunState::Completed;
			rowWidget->InitializeFromItemViewModel(
				runItem,
				displayState,
				ResolveProjectRunProgressPercent(displayState),
				bCompleted);
			if (bCompleted)
			{
				rowWidget->OnAnalyzeRequested.AddUObject(
					this,
					&UMainMenuWidget::HandleProjectExperimentRunAnalyzeRequested);
			}
			if (UScrollBoxSlot* scrollBoxSlot =
				Cast<UScrollBoxSlot>(ExperimentResultListScrollBox->AddChild(rowWidget)))
			{
				scrollBoxSlot->SetHorizontalAlignment(HAlign_Fill);
			}
			ProjectExperimentRunRows.Add(rowWidget);
		}
		return;
	}

	if (!platformUiSubsystem) return;

	const TSubclassOf<UFileListItemWidget> itemWidgetClass = ResolveFileListItemWidgetClass();
	if (!itemWidgetClass) return;

	TArray<FString> resultDirectories = platformUiSubsystem->ListLegacySimulationRunResultDirectories();
	const FSimulatorRunInfo runInfo = platformUiSubsystem->GetActiveRunInfo();
	if (!runInfo.StatusPath.IsEmpty())
	{
		const FString activeRunDirectory = FPaths::GetPath(runInfo.StatusPath);
		if (!activeRunDirectory.Equals(TEXT("Saved/SimulationRuns"), ESearchCase::IgnoreCase))
		{
			resultDirectories.AddUnique(activeRunDirectory);
		}
	}
	resultDirectories.Sort();

	if (!resultDirectories.Contains(SelectedExperimentResultRunDirectory))
	{
		SetSelectedExperimentResultRunDirectory(resultDirectories.IsEmpty() ? FString() : resultDirectories.Last());
	}

	ExperimentResultListScrollBox->ClearChildren();
	ExperimentResultListItems.Reset();
	ExperimentResultListItems.Reserve(resultDirectories.Num());
	ProjectExperimentRunRows.Reset();

	if (resultDirectories.IsEmpty())
	{
		SetDiagnosticsText(TEXT("실험 결과가 없습니다."));
		return;
	}

	const TArray<UOdiroListItemViewModel*> resultItems =
		platformUiSubsystem->CreatePathItemViewModels(resultDirectories, true);
	for (UOdiroListItemViewModel* resultItem : resultItems)
	{
		UFileListItemWidget* itemWidget = CreateWidget<UFileListItemWidget>(this, itemWidgetClass);
		if (!itemWidget) continue;

		itemWidget->InitializeFromItemViewModel(
			resultItem,
			TEXT("상세 보기"),
			TEXT("실행"),
			false,
			true,
			false);
		itemWidget->OnPrimaryActionRequested.AddUObject(this, &UMainMenuWidget::HandleExperimentResultDetailsRequested);
		if (UScrollBoxSlot* scrollBoxSlot =
			Cast<UScrollBoxSlot>(ExperimentResultListScrollBox->AddChild(itemWidget)))
		{
			scrollBoxSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		ExperimentResultListItems.Add(itemWidget);
	}
}

void UMainMenuWidget::RefreshExperimentResultIterationList()
{
	ClearExperimentResultIterationWidgets();

	UPlatformUiSubsystem* platformUiSubsystem = GetPlatformUiSubsystem();
	if (SelectedExperimentResultRunDirectory.IsEmpty())
	{
		return;
	}

	if (!ensureMsgf(IsValid(ExperimentResultIterationScrollBox), TEXT("Missing required WBP binding: ExperimentResultIterationScrollBox")))
	{
		return;
	}

	if (IsProjectRunDirectory(SelectedExperimentResultRunDirectory))
	{
		TArray<FString> resultPaths = platformUiSubsystem
			? platformUiSubsystem->ListProjectEpisodeResultFiles(SelectedExperimentResultRunDirectory)
			: TArray<FString>();
		if (resultPaths.IsEmpty())
		{
			SetSelectedExperimentResultPath(FString());
			return;
		}

		if (!resultPaths.Contains(SelectedExperimentResultPath))
		{
			SetSelectedExperimentResultPath(resultPaths[0]);
		}

		for (const FString& resultPath : resultPaths)
		{
			const FString episodeId = FPaths::GetCleanFilename(FPaths::GetPath(resultPath));
			const int32 episodeIndex = FUserProjectRunSnapshot::IsValidRunId(episodeId)
				? FCString::Atoi(*episodeId)
				: INDEX_NONE;
			const FText labelText = FText::FromString(
				episodeIndex == INDEX_NONE ? FString(TEXT("?")) : FString::FromInt(episodeIndex));

			const TSubclassOf<UExperimentResultIterationSelectorWidget> selectorClass =
				ResolveExperimentResultIterationSelectorWidgetClass();
			if (!selectorClass) return;

			const bool bSelected = resultPath.Equals(SelectedExperimentResultPath, ESearchCase::IgnoreCase);
			UExperimentResultIterationSelectorWidget* selectorWidget =
				CreateWidget<UExperimentResultIterationSelectorWidget>(this, selectorClass);
			if (!selectorWidget) continue;

			UOdiroListItemViewModel* selectorItem = NewObject<UOdiroListItemViewModel>(selectorWidget);
			if (!selectorItem) continue;
			selectorItem->InitializeItem(episodeId, labelText.ToString(), FString(), resultPath);
			selectorItem->SetSelected(bSelected);
			selectorWidget->InitializeFromItemViewModel(selectorItem);
			selectorWidget->OnSelectorClicked.AddUObject(
				this,
				&UMainMenuWidget::HandleExperimentResultIterationSelectorClicked);
			if (UScrollBoxSlot* scrollBoxSlot =
				Cast<UScrollBoxSlot>(ExperimentResultIterationScrollBox->AddChild(selectorWidget)))
			{
				scrollBoxSlot->SetPadding(ExperimentResultIterationSelectorPadding);
			}
			ExperimentResultIterationSelectors.Add(selectorWidget);
		}
		return;
	}

	if (!platformUiSubsystem)
	{
		return;
	}

	TArray<FString> reportPaths =
		platformUiSubsystem->ListLegacyEvaluationReportFilesInDirectory(SelectedExperimentResultRunDirectory);
	const FSimulatorRunInfo runInfo = platformUiSubsystem->GetActiveRunInfo();
	if (FPaths::GetPath(runInfo.StatusPath).Equals(SelectedExperimentResultRunDirectory, ESearchCase::IgnoreCase))
	{
		for (const FString& reportPath : runInfo.Status.ResultPaths)
		{
			reportPaths.AddUnique(reportPath);
		}
	}

	const TArray<FExperimentResultReportItem> reportItems = BuildExperimentResultReportItems(reportPaths);
	if (reportItems.IsEmpty())
	{
		SetSelectedExperimentResultPath(FString());
		return;
	}

	bool bSelectedReportStillExists = false;
	for (const FExperimentResultReportItem& reportItem : reportItems)
	{
		if (reportItem.ReportPath.Equals(SelectedExperimentResultPath, ESearchCase::IgnoreCase))
		{
			bSelectedReportStillExists = true;
			break;
		}
	}
	if (!bSelectedReportStillExists)
	{
		SetSelectedExperimentResultPath(reportItems[0].ReportPath);
	}

	for (const FExperimentResultReportItem& reportItem : reportItems)
	{
		const FString iterationLabel =
			reportItem.RunIndex == INDEX_NONE ? FString(TEXT("?")) : FString::FromInt(reportItem.RunIndex);
		const TSubclassOf<UExperimentResultIterationSelectorWidget> selectorClass =
			ResolveExperimentResultIterationSelectorWidgetClass();
		if (!selectorClass) return;

		const bool bSelected = reportItem.ReportPath.Equals(SelectedExperimentResultPath, ESearchCase::IgnoreCase);
		UExperimentResultIterationSelectorWidget* selectorWidget =
			CreateWidget<UExperimentResultIterationSelectorWidget>(this, selectorClass);
		if (!selectorWidget) continue;

		UOdiroListItemViewModel* selectorItem = NewObject<UOdiroListItemViewModel>(selectorWidget);
		if (!selectorItem) continue;
		selectorItem->InitializeItem(iterationLabel, iterationLabel, FString(), reportItem.ReportPath);
		selectorItem->SetSelected(bSelected);
		selectorWidget->InitializeFromItemViewModel(selectorItem);
		selectorWidget->OnSelectorClicked.AddUObject(
			this,
			&UMainMenuWidget::HandleExperimentResultIterationSelectorClicked);
		if (UScrollBoxSlot* scrollBoxSlot =
			Cast<UScrollBoxSlot>(ExperimentResultIterationScrollBox->AddChild(selectorWidget)))
		{
			scrollBoxSlot->SetPadding(ExperimentResultIterationSelectorPadding);
		}
		ExperimentResultIterationSelectors.Add(selectorWidget);
	}
}

void UMainMenuWidget::RefreshExperimentResultDetailPanel()
{
	ClearExperimentResultDashboardWidgets();

	if (!IsProjectRunDirectory(SelectedExperimentResultRunDirectory))
	{
		return;
	}

	if (!ExperimentResultViewModel)
	{
		SetDiagnosticsText(TEXT("ExperimentResultViewModel이 없습니다."));
		return;
	}

	const bool bDashboardLoaded =
		ExperimentResultViewModel->LoadRunDirectory(SelectedExperimentResultRunDirectory);
	const FProjectRunResultDashboardData DashboardData = ExperimentResultViewModel->GetDashboardData();
	if (!bDashboardLoaded && !ExperimentResultViewModel->GetDiagnosticsText().IsEmpty())
	{
		SetDiagnosticsText(ExperimentResultViewModel->GetDiagnosticsText());
	}

	if (ExperimentResultDetailTitleText)
	{
		const FString Title = DashboardData.RunId.IsEmpty()
			? TEXT("분석 상세")
			: FString::Printf(TEXT("분석 상세   #%s"), *FormatProjectRunDisplayId(DashboardData.RunId));
		ExperimentResultDetailTitleText->SetText(FText::FromString(Title));
	}

	if (TotalPlayTimeMetricCard)
	{
		SetProjectRunMetricCardText(
			TotalPlayTimeMetricCard,
			TEXT("총 플레이 시간"),
			FormatProjectRunTotalDuration(DashboardData.TotalDurationSeconds),
			FString());
	}
	if (SuccessRateMetricCard)
	{
		SetProjectRunMetricCardText(
			SuccessRateMetricCard,
			TEXT("성공률"),
			FormatProjectRunSuccessRate(DashboardData.SuccessCount, DashboardData.EpisodeCount),
			TEXT("%"));
	}
	if (CollisionCountMetricCard)
	{
		SetProjectRunMetricCardText(
			CollisionCountMetricCard,
			TEXT("충돌 횟수"),
			FString::FromInt(DashboardData.CollisionCount),
			TEXT("회"));
	}
	if (EpisodeReplayCountText)
	{
		EpisodeReplayCountText->SetText(FText::FromString(
			FString::Printf(TEXT("%d개"), DashboardData.EpisodeCount)));
	}

	const TSubclassOf<UUserWidget> EpisodeCardClass =
		ResolveProjectEpisodeReplayCardWidgetClass();
	if (EpisodeReplayCardWrapBox && EpisodeCardClass)
	{
		for (const UExperimentResultEpisodeViewModel* EpisodeItem : ExperimentResultViewModel->GetEpisodeItems())
		{
			UUserWidget* CardWidget = CreateWidget<UUserWidget>(this, EpisodeCardClass);
			if (!CardWidget)
			{
				continue;
			}

			ConfigureProjectEpisodeReplayCard(CardWidget, EpisodeItem);
			if (UProjectEpisodeReplayCardWidget* ReplayCardWidget = Cast<UProjectEpisodeReplayCardWidget>(CardWidget))
			{
				ReplayCardWidget->InitializeFromEpisodeViewModel(EpisodeItem);
				ReplayCardWidget->OnReplayRequested.RemoveAll(this);
				ReplayCardWidget->OnReplayRequested.AddUObject(
					this,
					&UMainMenuWidget::HandleProjectEpisodeReplayRequested);
			}

			if (UWrapBoxSlot* WrapBoxSlot = EpisodeReplayCardWrapBox->AddChildToWrapBox(CardWidget))
			{
				WrapBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 12.0f));
			}
			ProjectEpisodeReplayCards.Add(CardWidget);
		}
	}

	const UPlatformUiSubsystem* platformUiSubsystem = GetPlatformUiSubsystem();
	const bool bAnalysisPending = platformUiSubsystem && platformUiSubsystem->IsAnalysisRequestPending();
	const bool bHasAnalysisFailure =
		!LastProjectRunAnalysisFailureText.IsEmpty()
		&& LastProjectRunAnalysisFailureRunId.Equals(DashboardData.RunId, ESearchCase::CaseSensitive);
	const bool bShowAiAction = bAnalysisPending || bHasAnalysisFailure || !DashboardData.bAiLoaded;
	SetWidgetVisible(AiAnalysisActionBox, bShowAiAction);
	SetWidgetVisible(AiSuggestionPanel, DashboardData.bAiLoaded && !bAnalysisPending && !bHasAnalysisFailure);
	if (SendToAiButton)
	{
		SendToAiButton->SetIsEnabled(!bAnalysisPending);
	}
	if (AiAnalysisTextBlock && bShowAiAction)
	{
		const FString ActionText = bAnalysisPending
			? FString(TEXT("AI 분석 중..."))
			: (bHasAnalysisFailure
				? LastProjectRunAnalysisFailureText
				: FString(TEXT("에피소드 데이터를 기반으로 개선점을 제안합니다.")));
		AiAnalysisTextBlock->SetText(FText::FromString(ActionText));
	}

	if (!DashboardData.bAiLoaded || bAnalysisPending || bHasAnalysisFailure)
	{
		return;
	}

	if (AiSuggestionSummaryText)
	{
		AiSuggestionSummaryText->SetText(FText::FromString(DashboardData.AiSummary.IsEmpty()
			? TEXT("AI 분석 결과, 표시할 요약 문장이 없습니다.")
			: DashboardData.AiSummary));
	}

	const bool bHasSuggestions = !DashboardData.Suggestions.IsEmpty();
	SetWidgetVisible(AiSuggestionEmptyText, !bHasSuggestions);

	const TSubclassOf<UUserWidget> SuggestionRowClass =
		ResolveProjectAiSuggestionRowWidgetClass();
	if (AiSuggestionListBox && SuggestionRowClass)
	{
		for (const UExperimentResultSuggestionViewModel* SuggestionItem : ExperimentResultViewModel->GetSuggestionItems())
		{
			UUserWidget* RowWidget = CreateWidget<UUserWidget>(this, SuggestionRowClass);
			if (!RowWidget)
			{
				continue;
			}

			ConfigureProjectAiSuggestionRow(RowWidget, SuggestionItem);
			AiSuggestionListBox->AddChild(RowWidget);
			ProjectAiSuggestionRows.Add(RowWidget);
		}
	}
}

void UMainMenuWidget::SetProjectRunMetricCardText(
	UUserWidget* cardWidget,
	const FString& label,
	const FString& value,
	const FString& unit) const
{
	SetDashboardChildText(cardWidget, MetricLabelTextName, label);
	SetDashboardChildText(cardWidget, MetricValueTextName, value);

	const FString VisibleUnit = unit.TrimStartAndEnd();
	SetDashboardChildText(cardWidget, MetricUnitTextName, VisibleUnit);
	SetDashboardChildVisibility(cardWidget, MetricUnitTextName, !VisibleUnit.IsEmpty());
}

// Episode preview PNG를 WBP placeholder border background로 적용한다.
bool UMainMenuWidget::ApplyProjectEpisodePreviewImage(
	UUserWidget* cardWidget,
	const FString& imagePath)
{
	const FString normalizedImagePath = imagePath.TrimStartAndEnd();
	if (!cardWidget || normalizedImagePath.IsEmpty() || !FPaths::FileExists(normalizedImagePath))
	{
		return false;
	}

	UBorder* previewBorder = Cast<UBorder>(FindDashboardChildWidget(cardWidget, EpisodePreviewPlaceholderBoxName));
	if (!previewBorder)
	{
		return false;
	}

	UTexture2D* previewTexture = FImageUtils::ImportFileAsTexture2D(normalizedImagePath);
	if (!previewTexture)
	{
		return false;
	}

	ProjectEpisodePreviewTextures.Add(previewTexture);

	previewBorder->SetBrushFromTexture(previewTexture);
	previewBorder->SetBrushColor(FLinearColor::White);
	previewBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
	return true;
}

// Episode replay card WBP의 텍스트, 상태, preview image를 갱신한다.
void UMainMenuWidget::ConfigureProjectEpisodeReplayCard(
	UUserWidget* cardWidget,
	const UExperimentResultEpisodeViewModel* episodeItem)
{
	if (!episodeItem)
	{
		return;
	}

	SetDashboardChildText(cardWidget, EpisodeLabelTextName, episodeItem->GetTitle());
	SetDashboardChildText(cardWidget, EpisodeDurationTextName, episodeItem->GetDurationLabel());
	SetDashboardChildVisibility(cardWidget, EpisodeSuccessStateBoxName, episodeItem->IsSuccess());
	SetDashboardChildVisibility(cardWidget, EpisodeFailureStateBoxName, !episodeItem->IsSuccess());
	if (episodeItem->HasPreviewImage())
	{
		ApplyProjectEpisodePreviewImage(cardWidget, episodeItem->GetPayloadPath());
	}
	SetDashboardChildVisibility(cardWidget, EpisodePreviewPlaceholderBoxName, true);
	SetDashboardChildVisibility(cardWidget, EpisodePreviewImageBoxName, false);
}

void UMainMenuWidget::HandleProjectEpisodeReplayRequested(UProjectEpisodeReplayCardWidget* cardWidget)
{
	if (!IsValid(cardWidget))
	{
		return;
	}

	const FString EpisodeDirectory = cardWidget->GetEpisodeDirectory();
	if (EpisodeDirectory.IsEmpty())
	{
		SetDiagnosticsText(TEXT("Replay episode 경로가 없습니다."));
		return;
	}

	if (!cardWidget->IsReplayAvailable())
	{
		SetDiagnosticsText(FString::Printf(TEXT("Replay 파일이 없습니다: %s"), *EpisodeDirectory));
		return;
	}

	if (!ProjectEpisodeReplayViewerWidget)
	{
		SetDiagnosticsText(TEXT("ProjectEpisodeReplayViewerWidget is not bound in WBP_MainMenu."));
		return;
	}

	RestoreProjectEpisodeReplayViewerToNormalHost();

	if (!ProjectEpisodeReplayViewerWidget->OpenEpisodeReplay(EpisodeDirectory))
	{
		SetDiagnosticsText(ProjectEpisodeReplayViewerWidget->GetLastDiagnosticsText());
		return;
	}

	SetDiagnosticsText(ProjectEpisodeReplayViewerWidget->GetLastDiagnosticsText());
}

void UMainMenuWidget::HandleProjectEpisodeReplayFullscreenChanged(
	UProjectEpisodeReplayViewerWidget* viewerWidget,
	bool bFullscreen)
{
	if (!IsValid(viewerWidget) || viewerWidget != ProjectEpisodeReplayViewerWidget)
	{
		return;
	}

	UOverlay* targetHost = bFullscreen
		? ProjectEpisodeReplayFullscreenHost.Get()
		: ProjectEpisodeReplayNormalHost.Get();
	if (!targetHost)
	{
		SetDiagnosticsText(
			bFullscreen
				? TEXT("ProjectEpisodeReplayFullscreenHost is not bound in WBP_MainMenu.")
				: TEXT("ProjectEpisodeReplayNormalHost is not bound in WBP_MainMenu."));
		return;
	}

	if (ProjectEpisodeReplayFullscreenHost)
	{
		if (bFullscreen)
		{
			ApplyMainMenuFullscreenCanvasSlot(ProjectEpisodeReplayFullscreenHost.Get());
		}

		ProjectEpisodeReplayFullscreenHost->SetVisibility(
			bFullscreen
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}

	viewerWidget->RemoveFromParent();
	if (UOverlaySlot* overlaySlot = targetHost->AddChildToOverlay(viewerWidget))
	{
		overlaySlot->SetHorizontalAlignment(HAlign_Fill);
		overlaySlot->SetVerticalAlignment(VAlign_Fill);
		overlaySlot->SetPadding(FMargin(0.0f));
	}

	targetHost->InvalidateLayoutAndVolatility();
	targetHost->ForceLayoutPrepass();
	viewerWidget->InvalidateLayoutAndVolatility();
	viewerWidget->ForceLayoutPrepass();
}

void UMainMenuWidget::RestoreProjectEpisodeReplayViewerToNormalHost()
{
	if (!ProjectEpisodeReplayViewerWidget || !ProjectEpisodeReplayNormalHost)
	{
		return;
	}

	if (ProjectEpisodeReplayFullscreenHost)
	{
		ApplyMainMenuFullscreenCanvasSlot(ProjectEpisodeReplayFullscreenHost.Get());
		ProjectEpisodeReplayFullscreenHost->SetVisibility(ESlateVisibility::Collapsed);
	}

	ProjectEpisodeReplayViewerWidget->RemoveFromParent();
	if (UOverlaySlot* overlaySlot =
		ProjectEpisodeReplayNormalHost->AddChildToOverlay(ProjectEpisodeReplayViewerWidget))
	{
		overlaySlot->SetHorizontalAlignment(HAlign_Fill);
		overlaySlot->SetVerticalAlignment(VAlign_Fill);
		overlaySlot->SetPadding(FMargin(0.0f));
	}

	ProjectEpisodeReplayNormalHost->InvalidateLayoutAndVolatility();
	ProjectEpisodeReplayNormalHost->ForceLayoutPrepass();
	ProjectEpisodeReplayViewerWidget->InvalidateLayoutAndVolatility();
	ProjectEpisodeReplayViewerWidget->ForceLayoutPrepass();
}

void UMainMenuWidget::ConfigureProjectAiSuggestionRow(
	UUserWidget* rowWidget,
	const UExperimentResultSuggestionViewModel* suggestionItem) const
{
	if (!suggestionItem)
	{
		return;
	}

	auto setTextAndVisibility = [rowWidget](
		const FName textName,
		const FName rowName,
		const FString& value)
	{
		const FString visibleValue = value.TrimStartAndEnd();
		SetDashboardChildText(rowWidget, textName, visibleValue);
		SetDashboardChildVisibility(rowWidget, textName, !visibleValue.IsEmpty());
		SetDashboardChildVisibility(rowWidget, rowName, !visibleValue.IsEmpty());
	};

	SetDashboardChildText(rowWidget, SuggestionSeverityTextName, suggestionItem->GetSeverityLabel());
	setTextAndVisibility(SuggestionTitleTextName, SuggestionTitleRowName, suggestionItem->GetTitle());
	setTextAndVisibility(SuggestionMessageTextName, SuggestionMessageRowName, suggestionItem->GetSubtitle());
	setTextAndVisibility(SuggestionReasonTextName, SuggestionReasonRowName, suggestionItem->GetReason());
	setTextAndVisibility(
		SuggestionRecommendationTextName,
		SuggestionRecommendationRowName,
		suggestionItem->GetRecommendation());

	const bool bHasValueRow = !suggestionItem->GetParameterName().TrimStartAndEnd().IsEmpty()
		|| !suggestionItem->GetCurrentValue().TrimStartAndEnd().IsEmpty()
		|| !suggestionItem->GetSuggestedValue().TrimStartAndEnd().IsEmpty();
	SetDashboardChildText(rowWidget, SuggestionParamTextName, suggestionItem->GetParameterName().TrimStartAndEnd());
	SetDashboardChildText(rowWidget, SuggestionCurrentTextName, suggestionItem->GetCurrentValue().TrimStartAndEnd());
	SetDashboardChildText(rowWidget, SuggestionSuggestedTextName, suggestionItem->GetSuggestedValue().TrimStartAndEnd());
	SetDashboardChildVisibility(rowWidget, SuggestionValueRowName, bHasValueRow);

	SetDashboardChildVisibility(rowWidget, SuggestionHighIndicatorName, suggestionItem->GetSeverity() == EProjectRunAiSuggestionSeverity::High);
	SetDashboardChildVisibility(rowWidget, SuggestionMediumIndicatorName, suggestionItem->GetSeverity() == EProjectRunAiSuggestionSeverity::Medium);
	SetDashboardChildVisibility(rowWidget, SuggestionLowIndicatorName, suggestionItem->GetSeverity() == EProjectRunAiSuggestionSeverity::Low);
	SetDashboardChildVisibility(rowWidget, SuggestionInfoIndicatorName, suggestionItem->GetSeverity() == EProjectRunAiSuggestionSeverity::Info);
}

void UMainMenuWidget::SetSelectedScenarioSetupPath(const FString& scenarioSetupPath)
{
	SelectedScenarioSetupPath = scenarioSetupPath.TrimStartAndEnd();
	SelectedScenarioSetupPath.ReplaceInline(TEXT("\\"), TEXT("/"));

	if (ScenarioSetupPathTextBox)
	{
		ScenarioSetupPathTextBox->SetText(FText::FromString(SelectedScenarioSetupPath));
	}
	SyncComboBoxSelection(ExperimentScenarioSetupComboBox, SelectedScenarioSetupPath);
	SyncComboBoxSelection(ScenarioSetupComboBox, SelectedScenarioSetupPath);
}

void UMainMenuWidget::SetSelectedSetupPath(const FString& setupPath)
{
	SelectedSetupPath = setupPath.TrimStartAndEnd();
	SelectedSetupPath.ReplaceInline(TEXT("\\"), TEXT("/"));

	SyncComboBoxSelection(SetupComboBox, SelectedSetupPath);
}

void UMainMenuWidget::SetSelectedDeliveryBotSetupPath(const FString& deliveryBotSetupPath)
{
	SelectedDeliveryBotSetupPath = deliveryBotSetupPath.TrimStartAndEnd();
	SelectedDeliveryBotSetupPath.ReplaceInline(TEXT("\\"), TEXT("/"));

	SyncComboBoxSelection(PolicyDeliveryBotSetupComboBox, SelectedDeliveryBotSetupPath);
	SyncComboBoxSelection(DeliveryBotSetupComboBox, SelectedDeliveryBotSetupPath);
}

void UMainMenuWidget::SetSelectedPolicySpecPath(const FString& policySpecPath)
{
	SelectedPolicySpecJsonPath = policySpecPath.TrimStartAndEnd();
	SelectedPolicySpecJsonPath.ReplaceInline(TEXT("\\"), TEXT("/"));

	SyncComboBoxSelection(PolicySpecComboBox, SelectedPolicySpecJsonPath);
}

void UMainMenuWidget::SetSelectedExperimentResultRunDirectory(const FString& runDirectory)
{
	const FString previousRunDirectory = SelectedExperimentResultRunDirectory;
	SelectedExperimentResultRunDirectory = runDirectory.TrimStartAndEnd();
	SelectedExperimentResultRunDirectory.ReplaceInline(TEXT("\\"), TEXT("/"));

	if (!previousRunDirectory.Equals(SelectedExperimentResultRunDirectory, ESearchCase::IgnoreCase))
	{
		SetSelectedExperimentResultPath(FString());
	}
}

void UMainMenuWidget::SetSelectedExperimentResultPath(const FString& reportPath)
{
	SelectedExperimentResultPath = reportPath.TrimStartAndEnd();
	SelectedExperimentResultPath.ReplaceInline(TEXT("\\"), TEXT("/"));
}

void UMainMenuWidget::ClearExperimentResultIterationWidgets()
{
	for (UExperimentResultIterationSelectorWidget* selectorWidget : ExperimentResultIterationSelectors)
	{
		if (IsValid(selectorWidget))
		{
			selectorWidget->OnSelectorClicked.RemoveAll(this);
		}
	}

	ExperimentResultIterationSelectors.Reset();
	if (ExperimentResultIterationScrollBox)
	{
		ExperimentResultIterationScrollBox->ClearChildren();
	}
}

void UMainMenuWidget::ClearExperimentResultDashboardWidgets()
{
	for (UUserWidget* CardWidget : ProjectEpisodeReplayCards)
	{
		if (UProjectEpisodeReplayCardWidget* ReplayCardWidget = Cast<UProjectEpisodeReplayCardWidget>(CardWidget))
		{
			ReplayCardWidget->OnReplayRequested.RemoveAll(this);
		}
	}

	ProjectEpisodeReplayCards.Reset();
	ProjectEpisodePreviewTextures.Reset();
	ProjectAiSuggestionRows.Reset();

	if (EpisodeReplayCardWrapBox)
	{
		EpisodeReplayCardWrapBox->ClearChildren();
	}
	if (AiSuggestionListBox)
	{
		AiSuggestionListBox->ClearChildren();
	}
}

bool UMainMenuWidget::CreateScenarioFileFromTemplate(const FString& scenarioSetupPath)
{
	const FString normalizedScenarioSetupPath = NormalizeInputJsonPath(scenarioSetupPath);
	if (!IsEditableInputJsonPath(normalizedScenarioSetupPath))
	{
		SetDiagnosticsText(TEXT("새 시나리오 경로는 편집 가능한 Json/Input/*.json 경로여야 합니다."));
		return false;
	}

	const FString resolvedScenarioSetupPath = FSimulationSetupJson::ResolveProjectPath(normalizedScenarioSetupPath);
	if (UPlatformUiSubsystem::DoesResolvedFileExist(resolvedScenarioSetupPath))
	{
		SetDiagnosticsText(FString::Printf(TEXT("시나리오 파일이 이미 존재합니다: %s"), *normalizedScenarioSetupPath));
		return false;
	}

	FString errorText;
	if (!UPlatformUiSubsystem::CreateTextFileFromTemplate(
		ScenarioSetupTemplatePath,
		resolvedScenarioSetupPath,
		errorText))
	{
		SetDiagnosticsText(errorText.IsEmpty()
			? FString::Printf(TEXT("시나리오 파일 생성 실패: %s"), *resolvedScenarioSetupPath)
			: errorText);
		return false;
	}

	return true;
}

bool UMainMenuWidget::OpenScenarioInEditor(const FString& scenarioSetupPath)
{
	const FString trimmedScenarioPath = scenarioSetupPath.TrimStartAndEnd();
	const FString normalizedInputScenarioPath = NormalizeInputJsonPath(trimmedScenarioPath);
	const bool bLegacyInputScenario = IsEditableInputJsonPath(normalizedInputScenarioPath);
	const FString normalizedScenarioPath = bLegacyInputScenario
		? normalizedInputScenarioPath
		: NormalizeMainMenuPath(trimmedScenarioPath);
	if (normalizedScenarioPath.TrimStartAndEnd().IsEmpty())
	{
		SetDiagnosticsText(TEXT("scenario 파일이 선택되지 않았습니다."));
		return false;
	}
	if (!bLegacyInputScenario && !FPaths::GetCleanFilename(normalizedScenarioPath).Equals(MainMenuUserProjectScenarioFileName, ESearchCase::IgnoreCase))
	{
		SetDiagnosticsText(TEXT("project scenario는 <UserProject>/scenario.json 경로여야 합니다."));
		return false;
	}
	if (!bLegacyInputScenario && !UPlatformUiSubsystem::DoesResolvedFileExist(normalizedScenarioPath))
	{
		SetDiagnosticsText(FString::Printf(TEXT("project scenario 파일이 없습니다: %s"), *normalizedScenarioPath));
		return false;
	}

	if (!bLegacyInputScenario && ProjectWorkspaceViewModel)
	{
		if (!ProjectWorkspaceViewModel->OpenScenarioEditor())
		{
			const FString diagnostics = ProjectWorkspaceViewModel->GetDiagnosticsText();
			SetDiagnosticsText(diagnostics.IsEmpty() ? TEXT("ScenarioEditorMap 열기 실패.") : diagnostics);
			return false;
		}

		return true;
	}

	UPlatformUiSubsystem* platformUiSubsystem = GetPlatformUiSubsystem();
	if (!platformUiSubsystem)
	{
		SetDiagnosticsText(TEXT("PlatformUiSubsystem을 사용할 수 없습니다."));
		return false;
	}

	FString errorText;
	if (!platformUiSubsystem->OpenScenarioEditorPath(normalizedScenarioPath, errorText))
	{
		SetDiagnosticsText(errorText);
		return false;
	}

	return true;
}

TSubclassOf<UFileListItemWidget> UMainMenuWidget::ResolveFileListItemWidgetClass() const
{
	if (!FileListItemWidgetClass)
	{
		UE_LOG(
			LogMainMenuWidget,
			Error,
			TEXT("FileListItemWidgetClass is not configured on the MainMenu widget asset."));
		ensureMsgf(false, TEXT("File list item widget class is missing."));
		return nullptr;
	}

	return FileListItemWidgetClass;
}

TSubclassOf<UProjectExperimentRunRowWidget> UMainMenuWidget::ResolveProjectExperimentRunRowWidgetClass() const
{
	if (ProjectExperimentRunRowWidgetClass)
	{
		return ProjectExperimentRunRowWidgetClass;
	}

	const UPlatformUiDeveloperSettings* platformUiSettings = GetDefault<UPlatformUiDeveloperSettings>();
	const TSubclassOf<UProjectExperimentRunRowWidget> configuredClass = platformUiSettings
		? platformUiSettings->ProjectExperimentRunRowWidgetClass.LoadSynchronous()
		: nullptr;
	if (!configuredClass)
	{
		UE_LOG(
			LogMainMenuWidget,
			Error,
			TEXT("ProjectExperimentRunRowWidgetClass is not configured on the MainMenu widget asset or Platform UI project settings."));
		ensureMsgf(false, TEXT("Project experiment run row widget class is missing."));
		return nullptr;
	}

	return configuredClass;
}

TSubclassOf<UUserWidget> UMainMenuWidget::ResolveProjectEpisodeReplayCardWidgetClass() const
{
	if (ProjectEpisodeReplayCardWidgetClass)
	{
		return ProjectEpisodeReplayCardWidgetClass;
	}

	const UPlatformUiDeveloperSettings* platformUiSettings = GetDefault<UPlatformUiDeveloperSettings>();
	const TSubclassOf<UUserWidget> configuredClass = platformUiSettings
		? platformUiSettings->ProjectEpisodeReplayCardWidgetClass.LoadSynchronous()
		: nullptr;
	if (!configuredClass)
	{
		UE_LOG(
			LogMainMenuWidget,
			Error,
			TEXT("ProjectEpisodeReplayCardWidgetClass is not configured on the MainMenu widget asset or Platform UI project settings."));
		ensureMsgf(false, TEXT("Project episode replay card widget class is missing."));
		return nullptr;
	}

	return configuredClass;
}

TSubclassOf<UUserWidget> UMainMenuWidget::ResolveProjectAiSuggestionRowWidgetClass() const
{
	if (ProjectAiSuggestionRowWidgetClass)
	{
		return ProjectAiSuggestionRowWidgetClass;
	}

	const UPlatformUiDeveloperSettings* platformUiSettings = GetDefault<UPlatformUiDeveloperSettings>();
	const TSubclassOf<UUserWidget> configuredClass = platformUiSettings
		? platformUiSettings->ProjectAiSuggestionRowWidgetClass.LoadSynchronous()
		: nullptr;
	if (!configuredClass)
	{
		UE_LOG(
			LogMainMenuWidget,
			Error,
			TEXT("ProjectAiSuggestionRowWidgetClass is not configured on the MainMenu widget asset or Platform UI project settings."));
		ensureMsgf(false, TEXT("Project AI suggestion row widget class is missing."));
		return nullptr;
	}

	return configuredClass;
}

TSubclassOf<UExperimentResultIterationSelectorWidget>
UMainMenuWidget::ResolveExperimentResultIterationSelectorWidgetClass() const
{
	if (ExperimentResultIterationSelectorWidgetClass)
	{
		return ExperimentResultIterationSelectorWidgetClass;
	}

	const UPlatformUiDeveloperSettings* platformUiSettings = GetDefault<UPlatformUiDeveloperSettings>();
	const TSubclassOf<UExperimentResultIterationSelectorWidget> configuredClass = platformUiSettings
		? platformUiSettings->ExperimentResultIterationSelectorWidgetClass.LoadSynchronous()
		: nullptr;
	if (configuredClass)
	{
		return configuredClass;
	}

	UE_LOG(
		LogMainMenuWidget,
		Warning,
		TEXT("ExperimentResultIterationSelectorWidgetClass is not configured on the MainMenu widget asset or Platform UI project settings."));
	return nullptr;
}

void UMainMenuWidget::HandleRunInfoChanged(const FSimulatorRunInfo& runInfo)
{
	(void)runInfo;
	UpdateStatusText();
	if (ActiveProjectWorkspaceTab == EProjectWorkspaceTabType::ExperimentResultDetail)
	{
		if (IsProjectRunDirectory(SelectedExperimentResultRunDirectory))
		{
			RefreshExperimentResultDetailPanel();
			return;
		}
		RefreshExperimentResultIterationList();
	}
	else
	{
		RefreshExperimentResultList();
	}
	UpdateReportAndLogText();
}

void UMainMenuWidget::HandleAnalysisCompleted(const FPlatformAnalysisAiResponse& response)
{
	if (SendToAiButton)
	{
		SendToAiButton->SetIsEnabled(true);
	}

	const FString CompletedRunId = PendingProjectRunAnalysisRunId.IsEmpty()
		? GetSelectedProjectRunId()
		: PendingProjectRunAnalysisRunId;
	PendingProjectRunAnalysisRunId.Reset();

	if (response.bSuccess)
	{
		LastProjectRunAnalysisFailureRunId.Reset();
		LastProjectRunAnalysisFailureText.Reset();
		RefreshExperimentResultDetailPanel();
		return;
	}

	LastProjectRunAnalysisFailureRunId = CompletedRunId;
	LastProjectRunAnalysisFailureText = BuildAnalysisFailureDisplayText(response);

	if (IsProjectRunDirectory(SelectedExperimentResultRunDirectory)
		&& ActiveProjectWorkspaceTab == EProjectWorkspaceTabType::ExperimentResultDetail)
	{
		RefreshExperimentResultDetailPanel();
		return;
	}

	if (AiAnalysisTextBlock)
	{
		AiAnalysisTextBlock->SetText(FText::FromString(LastProjectRunAnalysisFailureText));
	}
}

void UMainMenuWidget::UpdateStatusText(const FString& extraMessage)
{
	if (!StatusTextBlock) return;
	
	TArray<FString> lines;
	if (!extraMessage.IsEmpty())
	{
		lines.Add(extraMessage);
	}

	if (UPlatformUiSubsystem* platformUiSubsystem = GetPlatformUiSubsystem())
	{
		if (IsProjectModeSelected())
		{
			const FString projectPath = ProjectWorkspaceViewModel && !ProjectWorkspaceViewModel->GetActiveProjectPath().IsEmpty()
				? ProjectWorkspaceViewModel->GetActiveProjectPath()
				: GetSelectedProjectPath();
			lines.Add(FString::Printf(TEXT("Project: %s"), *projectPath));

			if (ProjectWorkspaceViewModel)
			{
				ProjectWorkspaceViewModel->RefreshProjectRuns();
				const TArray<UOdiroListItemViewModel*> runItems = ProjectWorkspaceViewModel->GetRunItems();
				const FString workspaceStatus = ProjectWorkspaceViewModel->GetStatusText();
				if (!workspaceStatus.IsEmpty())
				{
					lines.Add(workspaceStatus);
				}
				if (!runItems.IsEmpty())
				{
					lines.Add(FString::Printf(TEXT("Runs: %d"), runItems.Num()));
				}
				lines.Add(FString::Printf(TEXT("Selected Run: %s"), *FormatProjectRunDisplayId(GetSelectedProjectRunId())));
			}
			else
			{
				lines.Add(TEXT("ProjectWorkspaceViewModel unavailable."));
			}

			const FSimulatorRunInfo runInfo = platformUiSubsystem->GetActiveRunInfo();
			if (!runInfo.RunId.IsEmpty())
			{
				lines.Add(TEXT(""));
				lines.Add(TEXT("Current Simulator Process"));
				lines.Add(FString::Printf(TEXT("Mode: %s"), runInfo.bProjectRun ? TEXT("Project") : TEXT("Legacy")));
				lines.Add(FString::Printf(TEXT("Run Id: %s"), *FormatProjectRunDisplayId(runInfo.RunId)));
				lines.Add(FString::Printf(TEXT("State: %s"), *ToRunStateString(runInfo.Status.State)));
				lines.Add(FString::Printf(TEXT("Process: %s"), runInfo.bProcessRunning ? TEXT("Running") : TEXT("Stopped")));
				lines.Add(FString::Printf(TEXT("Launcher: %s"), runInfo.bUsedPreviewLauncher ? TEXT("RunPreview.ps1") : TEXT("Executable")));
				if (runInfo.ProcessReturnCode != INDEX_NONE)
				{
					lines.Add(FString::Printf(TEXT("Exit code: %d"), runInfo.ProcessReturnCode));
				}
				if (!runInfo.LastError.IsEmpty())
				{
					lines.Add(FString::Printf(TEXT("Error: %s"), *runInfo.LastError));
				}
				for (const FString& diagnostic : runInfo.Diagnostics)
				{
					lines.Add(FString::Printf(TEXT("Diagnostic: %s"), *diagnostic));
				}
			}

			StatusTextBlock->SetText(FText::FromString(JoinStringLines(lines)));
			return;
		}

		const FSimulationSetupParseResult setupParseResult =
			platformUiSubsystem->LoadLegacySimulationSetupFile(GetSelectedSetupPath());
		if (setupParseResult.bSuccess)
		{
			lines.Add(FString::Printf(TEXT("Setup: %s"), *GetSelectedSetupPath()));
			lines.Add(FString::Printf(TEXT("Fixed-Step FPS: %d"), setupParseResult.Setup.FixedStep.Fps));
		}

		const FSimulatorRunInfo runInfo = platformUiSubsystem->GetActiveRunInfo();
		if (!runInfo.RunId.IsEmpty())
		{
			lines.Add(TEXT(""));
			lines.Add(TEXT("Current Simulator Process"));
			lines.Add(FString::Printf(TEXT("Run Id: %s"), *FormatProjectRunDisplayId(runInfo.RunId)));
			lines.Add(FString::Printf(TEXT("State: %s"), *ToRunStateString(runInfo.Status.State)));
			lines.Add(FString::Printf(TEXT("Progress: %d / %d"), runInfo.Status.CompletedRuns, runInfo.Status.TotalRuns));
			lines.Add(FString::Printf(TEXT("Process: %s"), runInfo.bProcessRunning ? TEXT("Running") : TEXT("Stopped")));
			lines.Add(FString::Printf(TEXT("Launcher: %s"), runInfo.bUsedPreviewLauncher ? TEXT("RunPreview.ps1") : TEXT("Executable")));
			if (runInfo.ProcessReturnCode != INDEX_NONE)
			{
				lines.Add(FString::Printf(TEXT("Exit code: %d"), runInfo.ProcessReturnCode));
			}
			if (!runInfo.LastError.IsEmpty())
			{
				lines.Add(FString::Printf(TEXT("Error: %s"), *runInfo.LastError));
			}
			for (const FString& diagnostic : runInfo.Diagnostics)
			{
				lines.Add(FString::Printf(TEXT("Diagnostic: %s"), *diagnostic));
			}
		}

		const TArray<FString> statusFiles = platformUiSubsystem->ListLegacySimulationRunStatusFiles();
		if (!statusFiles.IsEmpty())
		{
			lines.Add(TEXT(""));
			lines.Add(TEXT("Previous Simulator Runs"));

			const int32 firstIndex = FMath::Max(0, statusFiles.Num() - 8);
			for (int32 statusIndex = statusFiles.Num() - 1; statusIndex >= firstIndex; --statusIndex)
			{
				FSimulationRunStatus status;
				TArray<FString> diagnostics;
				if (!FSimulationRunStatusJson::ParseFromFile(statusFiles[statusIndex], status, diagnostics))
				{
					continue;
				}

				lines.Add(FString::Printf(
					TEXT("- %s | %s | %d/%d | %s"),
					*status.RunId,
					*ToRunStateString(status.State),
					status.CompletedRuns,
					status.TotalRuns,
					*statusFiles[statusIndex]));
			}
		}
	}

	StatusTextBlock->SetText(FText::FromString(JoinStringLines(lines)));
}

void UMainMenuWidget::UpdateReportAndLogText()
{
	UPlatformUiSubsystem* platformUiSubsystem = GetPlatformUiSubsystem();
	if (!platformUiSubsystem)
	{
		return;
	}

	const FSimulatorRunInfo runInfo = platformUiSubsystem->GetActiveRunInfo();

	CurrentPreviewReportPath = SelectedExperimentResultPath;
	CurrentPreviewLogPath.Reset();

	if (IsProjectRunDirectory(SelectedExperimentResultRunDirectory))
	{
		RefreshExperimentResultDetailPanel();
		return;
	}

	if (ReportTextBlock)
	{
		TArray<FString> reportLines;
		reportLines.Add(SelectedExperimentResultRunDirectory.IsEmpty()
			? TEXT("Experiment Run: <none>")
			: FString::Printf(TEXT("Experiment Run: %s"), *SelectedExperimentResultRunDirectory));

		if (!SelectedExperimentResultPath.IsEmpty())
		{
			FString reportJson;
			if (UPlatformUiSubsystem::ReadResolvedTextFile(SelectedExperimentResultPath, reportJson))
			{
				reportLines.Add(TEXT(""));
				reportLines.Add(FString::Printf(TEXT("Details: %s"), *SelectedExperimentResultPath));
				reportLines.Add(TruncatePreview(reportJson, ReportPreviewCharacterLimit));
			}
		}

		ReportTextBlock->SetText(FText::FromString(JoinStringLines(reportLines)));
	}

	if (LogPreviewTextBlock)
	{
		TArray<FString> logLines;
		logLines.Add(TEXT("Measurement Logs"));
		TArray<FString> logPaths;
		if (!SelectedExperimentResultRunDirectory.IsEmpty())
		{
			logPaths = platformUiSubsystem->ListLegacyMeasurementLogFilesInDirectory(SelectedExperimentResultRunDirectory);
			if (FPaths::GetPath(runInfo.StatusPath).Equals(SelectedExperimentResultRunDirectory, ESearchCase::IgnoreCase))
			{
				for (const FString& logPath : runInfo.Status.LogPaths)
				{
					logPaths.AddUnique(logPath);
				}
			}
		}
		logPaths.Sort();
		CurrentPreviewLogPath = logPaths.IsEmpty() ? FString() : logPaths.Last();

		for (const FString& logPath : logPaths)
		{
			logLines.Add(FString::Printf(TEXT("- %s"), *logPath));
		}

		if (!logPaths.IsEmpty())
		{
			const FString previewLogPath = CurrentPreviewLogPath;
			logLines.Add(TEXT(""));
			logLines.Add(FString::Printf(TEXT("Preview: %s"), *previewLogPath));
			logLines.Add(BuildLogPreview(previewLogPath));
		}

		LogPreviewTextBlock->SetText(FText::FromString(JoinStringLines(logLines)));
	}
}

void UMainMenuWidget::SetDiagnosticsText(const FString& message)
{
	if (DiagnosticsTextBlock)
	{
		DiagnosticsTextBlock->SetText(FText::FromString(message));
	}

	// Status mirrors diagnostics for immediate feedback; periodic run refresh can replace it with live process state.
	if (StatusTextBlock)
	{
		StatusTextBlock->SetText(FText::FromString(message));
	}
}

bool UMainMenuWidget::IsProjectModeSelected() const
{
	return IsProjectOpened();
}

bool UMainMenuWidget::IsProjectRunDirectory(const FString& runDirectory) const
{
	const FString runId = ExtractProjectRunIdFromDirectory(runDirectory);
	if (!IsProjectModeSelected() || !FUserProjectRunSnapshot::IsValidRunId(runId))
	{
		return false;
	}

	return NormalizeMainMenuPath(runDirectory).Equals(
		BuildProjectRunDirectory(GetSelectedProjectPath(), runId),
		ESearchCase::IgnoreCase);
}

FString UMainMenuWidget::GetProjectRunIdForPrototype() const
{
	return GetSelectedProjectRunId();
}

bool UMainMenuWidget::IsProjectOpened() const
{
	if (ProjectWorkspaceViewModel)
	{
		return !ProjectWorkspaceViewModel->GetActiveProjectPath().IsEmpty();
	}

	const UPlatformUiSubsystem* platformUiSubsystem = GetPlatformUiSubsystem();
	return platformUiSubsystem && platformUiSubsystem->HasActiveProject();
}

FString UMainMenuWidget::GetSelectedSetupPath() const
{
	if (!SelectedSetupPath.TrimStartAndEnd().IsEmpty())
	{
		return SelectedSetupPath;
	}

	return SetupComboBox ? SetupComboBox->GetSelectedOption() : FString();
}

FString UMainMenuWidget::GetSelectedScenarioSetupPath() const
{
	if (!SelectedScenarioSetupPath.TrimStartAndEnd().IsEmpty())
	{
		return SelectedScenarioSetupPath;
	}

	if (ScenarioSetupPathTextBox)
	{
		const FString scenarioSetupPath = ScenarioSetupPathTextBox->GetText().ToString().TrimStartAndEnd();
		if (!scenarioSetupPath.IsEmpty())
		{
			return scenarioSetupPath;
		}
	}

	return ExperimentScenarioSetupComboBox ? ExperimentScenarioSetupComboBox->GetSelectedOption() : FString();
}

FString UMainMenuWidget::GetSelectedDeliveryBotSetupPath() const
{
	if (!SelectedDeliveryBotSetupPath.TrimStartAndEnd().IsEmpty())
	{
		return SelectedDeliveryBotSetupPath;
	}

	return DeliveryBotSetupComboBox ? DeliveryBotSetupComboBox->GetSelectedOption() : FString();
}

FString UMainMenuWidget::GetSelectedPolicySpecPath() const
{
	if (!SelectedPolicySpecJsonPath.TrimStartAndEnd().IsEmpty())
	{
		return SelectedPolicySpecJsonPath;
	}

	if (PolicySpecComboBox && !PolicySpecComboBox->GetSelectedOption().TrimStartAndEnd().IsEmpty())
	{
		return PolicySpecComboBox->GetSelectedOption();
	}

	return MainMenuDefaultPolicySpecJsonPath;
}

FString UMainMenuWidget::GetSelectedProjectPath() const
{
	if (ProjectWorkspaceViewModel && !ProjectWorkspaceViewModel->GetActiveProjectPath().IsEmpty())
	{
		return ProjectWorkspaceViewModel->GetActiveProjectPath();
	}

	if (const UPlatformUiSubsystem* platformUiSubsystem = GetPlatformUiSubsystem())
	{
		return platformUiSubsystem->GetActiveProjectPath();
	}

	return FString();
}

FString UMainMenuWidget::GetSelectedProjectScenarioPath() const
{
	if (ProjectWorkspaceViewModel && !ProjectWorkspaceViewModel->GetActiveScenarioPath().IsEmpty())
	{
		return ProjectWorkspaceViewModel->GetActiveScenarioPath();
	}

	if (const UPlatformUiSubsystem* platformUiSubsystem = GetPlatformUiSubsystem())
	{
		return platformUiSubsystem->GetActiveProjectScenarioPath();
	}

	return FString();
}

FString UMainMenuWidget::GetSelectedProjectRunId() const
{
	if (!SelectedProjectRunId.TrimStartAndEnd().IsEmpty())
	{
		return SelectedProjectRunId;
	}

	return FString();
}

FString UMainMenuWidget::GetSelectedProjectRunDirectory() const
{
	return BuildProjectRunDirectory(GetSelectedProjectPath(), GetSelectedProjectRunId());
}

void UMainMenuWidget::SetSelectedProjectRunId(const FString& runId)
{
	SelectedProjectRunId = runId.TrimStartAndEnd();
	if (ProjectWorkspaceViewModel)
	{
		ProjectWorkspaceViewModel->SelectRun(SelectedProjectRunId);
	}

	if (IsProjectModeSelected() && FUserProjectRunSnapshot::IsValidRunId(SelectedProjectRunId))
	{
		SetSelectedExperimentResultRunDirectory(GetSelectedProjectRunDirectory());
		if (ExperimentResultViewModel)
		{
			ExperimentResultViewModel->LoadRunDirectory(SelectedExperimentResultRunDirectory);
		}
	}
	else if (SelectedProjectRunId.IsEmpty())
	{
		SetSelectedExperimentResultRunDirectory(FString());
	}
}

UPlatformUiSubsystem* UMainMenuWidget::GetPlatformUiSubsystem() const
{
	return UPlatformUiSubsystem::ResolveForWorldContext(this);
}

void UMainMenuWidget::RequestEditorWidgetInputMode()
{
	if (AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer()))
	{
		UWidget* focusWidget = ResolveInputModeFocusWidget();
		if (!focusWidget)
		{
			return;
		}

		RequestedInputModeFocusWidget = focusWidget;
		editorController->RequestEditorWidgetInputMode(focusWidget);
	}
}

void UMainMenuWidget::ReleaseEditorWidgetInputMode()
{
	if (AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer()))
	{
		UWidget* focusWidget = RequestedInputModeFocusWidget.Get();
		if (!focusWidget)
		{
			focusWidget = ResolveInputModeFocusWidget();
		}
		if (!focusWidget)
		{
			return;
		}

		editorController->ReleaseEditorWidgetInputMode(focusWidget);
		RequestedInputModeFocusWidget.Reset();
	}
}

UWidget* UMainMenuWidget::ResolveInputModeFocusWidget() const
{
	return MainMenuInputModePanel ? MainMenuInputModePanel.Get() : nullptr;
}
