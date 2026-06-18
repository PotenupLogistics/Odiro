
#include "Platform/Widget/MainMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Platform/ScenarioEditorLaunchSubsystem.h"
#include "Platform/PlatformAnalysisAiSubsystem.h"
#include "Platform/SimulatorLaunchSubsystem.h"
#include "Platform/Widget/ExperimentResultIterationButton.h"
#include "Platform/Widget/FileListItemWidget.h"
#include "Shared/UserProjectDataTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogMainMenuWidget, Log, All);

namespace
{
	const int32 ReportPreviewCharacterLimit = 4000;
	const int32 LogPreviewEdgeLineCount = 5;
	const TCHAR* DefaultSimulationSetupPath = TEXT("Json/Input/SimulationSetupNew.json");
	const TCHAR* MainMenuDefaultSimulationMapId = TEXT("ScenarioSimulationMap");
	const TCHAR* DefaultMeasurementOutputDirectory = TEXT("Saved/AnalysisLogs");
	const TCHAR* DefaultMeasurementFilePrefix = TEXT("MeasurementLog");
	const TCHAR* DefaultStatusOutputPath = TEXT("Saved/SimulationRuns/latest_status.json");
	const TCHAR* MainMenuDefaultPolicySpecJsonPath = TEXT("Json/Input/PolicySpecs/PolicySpec_DefaultDelivery.json");
	const int32 DefaultFlushIntervalTicks = 60;
	const TCHAR* UserProjectDirectory = TEXT("Saved/UserProjects");
	const TCHAR* UserProjectScenarioFileName = TEXT("scenario.json");
	const TCHAR* UserProjectSettingFileName = TEXT("setting.json");
	const TCHAR* UserProjectProfileFileName = TEXT("profile.json");
	const TCHAR* UserProjectPolicyDirectoryName = TEXT("policy");
	const TCHAR* BlankProjectTemplateDirectory = TEXT("../static/project-templates/blank");
	const TCHAR* PackagedBlankProjectTemplateDirectory = TEXT("resources/project-templates/blank");
	const TCHAR* DefaultScenarioEditorProjectName = TEXT("ScenarioEditor");
	const TCHAR* DeliveryBotTemplatePath = TEXT("Json/Input/DeliveryBotSetupSample_0.json");
	const TCHAR* FileListItemWidgetBlueprintClassPath =
		TEXT("/Game/Widgets/MainMenu/WBP_FileListItem.WBP_FileListItem_C");

	enum class EMainMenuSection : int32
	{
		Scenario = 0,
		Policy,
		ExperimentConfig,
		RunStatus,
		ExperimentResult,
	};

	struct FProjectRunResultItem
	{
		FString ProjectPath;
		FString RunId;
		FString RunPath;
	};

	struct FProjectRunEpisodeResultItem
	{
		FString EpisodeId;
		FString ResultPath;
		FString EventsPath;
		FString ScenarioPath;
	};

	FName MakeUniqueWidgetName(UWidgetTree* widgetTree, UClass* widgetClass, const FName name)
	{
		return MakeUniqueObjectName(widgetTree, widgetClass, name);
	}

	template <typename WidgetT>
	WidgetT* MakeWidget(UWidgetTree* widgetTree, const FName name)
	{
		return widgetTree->ConstructWidget<WidgetT>(
			WidgetT::StaticClass(),
			MakeUniqueWidgetName(widgetTree, WidgetT::StaticClass(), name));
	}

	UTextBlock* MakeTextBlock(UWidgetTree* widgetTree, const FName name, const FString& text, const int32 fontSize = 16)
	{
		UTextBlock* textBlock = MakeWidget<UTextBlock>(widgetTree, name);
		textBlock->SetText(FText::FromString(text));
		textBlock->SetAutoWrapText(true);
		textBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.92f, 0.94f, 1.0f)));

		FSlateFontInfo fontInfo = textBlock->GetFont();
		fontInfo.Size = fontSize;
		textBlock->SetFont(fontInfo);
		return textBlock;
	}

	FString ToRunStateString(ESimulationRunState state)
	{
		if (const UEnum* stateEnum = StaticEnum<ESimulationRunState>())
		{
			return stateEnum->GetNameStringByValue(static_cast<int64>(state));
		}

		return TEXT("Unknown");
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
		TArray<FString> lines;
		if (!FFileHelper::LoadFileToStringArray(lines, *FSimulationSetupJson::ResolveProjectPath(logPath)))
		{
			return FString::Printf(TEXT("Log read failed: %s"), *logPath);
		}

		TArray<FString> previewLines;
		for (int32 lineIndex = 0; lineIndex < FMath::Min(LogPreviewEdgeLineCount, lines.Num()); ++lineIndex)
		{
			previewLines.Add(lines[lineIndex]);
		}

		// Measurement JSONL은 tick record가 커질 수 있어 전체 내용을 UI에 펼치지 않는다.
		if (lines.Num() > LogPreviewEdgeLineCount * 2)
		{
			previewLines.Add(TEXT("..."));
		}

		const int32 tailStartIndex = FMath::Max(LogPreviewEdgeLineCount, lines.Num() - LogPreviewEdgeLineCount);
		for (int32 lineIndex = tailStartIndex; lineIndex < lines.Num(); ++lineIndex)
		{
			previewLines.Add(lines[lineIndex]);
		}

		return FString::Join(previewLines, TEXT("\n"));
	}

	FString JoinStringLines(const TArray<FString>& lines)
	{
		return FString::Join(lines, TEXT("\n"));
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

	FString ToMainMenuProjectRelativePath(FString filePath)
	{
		filePath.TrimStartAndEndInline();
		filePath.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (filePath.IsEmpty() || FPaths::IsRelative(filePath))
		{
			return filePath;
		}

		const FString projectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		if (FPaths::MakePathRelativeTo(filePath, *projectDir))
		{
			filePath.ReplaceInline(TEXT("\\"), TEXT("/"));
		}
		return filePath;
	}

	FString NormalizeUserProjectScenarioPath(FString rawPath)
	{
		rawPath.TrimStartAndEndInline();
		rawPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (rawPath.IsEmpty())
		{
			return FString();
		}

		if (FPaths::GetExtension(rawPath).Equals(TEXT("json"), ESearchCase::IgnoreCase))
		{
			return rawPath;
		}

		if (FPaths::IsRelative(rawPath) && !rawPath.Contains(TEXT("/")))
		{
			rawPath = FPaths::Combine(UserProjectDirectory, rawPath);
		}

		rawPath = FPaths::Combine(rawPath, UserProjectScenarioFileName);
		rawPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		return rawPath;
	}

	bool IsUserProjectScenarioPath(const FString& scenarioPath)
	{
		return FPaths::GetCleanFilename(scenarioPath).Equals(UserProjectScenarioFileName, ESearchCase::IgnoreCase);
	}

	FString ResolveUserProjectRootFromScenarioPath(const FString& scenarioJsonPath)
	{
		const FString normalizedScenarioJsonPath = NormalizeUserProjectScenarioPath(scenarioJsonPath);
		if (normalizedScenarioJsonPath.IsEmpty() || !IsUserProjectScenarioPath(normalizedScenarioJsonPath))
		{
			return FString();
		}

		return FSimulationSetupJson::ResolveProjectPath(FPaths::GetPath(normalizedScenarioJsonPath));
	}

	FString ResolveBlankProjectTemplateDirectory()
	{
		TArray<FString> candidates;
		candidates.Add(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), BlankProjectTemplateDirectory)));
		candidates.Add(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), PackagedBlankProjectTemplateDirectory)));

		for (FString candidate : candidates)
		{
			candidate.ReplaceInline(TEXT("\\"), TEXT("/"));
			if (FPaths::DirectoryExists(candidate))
			{
				return candidate;
			}
		}

		return FString();
	}

	bool CopyDefaultFileIfMissing(
		const FString& sourceFile,
		const FString& destinationFile,
		const TCHAR* label,
		TArray<FString>& outDiagnostics)
	{
		if (FPaths::FileExists(destinationFile))
		{
			return true;
		}
		if (!FPaths::FileExists(sourceFile))
		{
			outDiagnostics.Add(FString::Printf(TEXT("%s template file is missing: %s"), label, *sourceFile));
			return false;
		}

		const FString destinationDirectory = FPaths::GetPath(destinationFile);
		if (!IFileManager::Get().MakeDirectory(*destinationDirectory, true))
		{
			outDiagnostics.Add(FString::Printf(TEXT("%s directory create failed: %s"), label, *destinationDirectory));
			return false;
		}

		TArray<uint8> fileBytes;
		if (!FFileHelper::LoadFileToArray(fileBytes, *sourceFile))
		{
			outDiagnostics.Add(FString::Printf(TEXT("%s template read failed: %s"), label, *sourceFile));
			return false;
		}
		if (!FFileHelper::SaveArrayToFile(fileBytes, *destinationFile))
		{
			outDiagnostics.Add(FString::Printf(TEXT("%s project file write failed: %s"), label, *destinationFile));
			return false;
		}

		return true;
	}

	bool CopyDefaultDirectoryIfMissing(
		const FString& sourceDirectory,
		const FString& destinationDirectory,
		const TCHAR* label,
		TArray<FString>& outDiagnostics)
	{
		if (FPaths::DirectoryExists(destinationDirectory))
		{
			return true;
		}
		if (!FPaths::DirectoryExists(sourceDirectory))
		{
			outDiagnostics.Add(FString::Printf(TEXT("%s template directory is missing: %s"), label, *sourceDirectory));
			return false;
		}
		if (!IFileManager::Get().MakeDirectory(*destinationDirectory, true))
		{
			outDiagnostics.Add(FString::Printf(TEXT("%s project directory create failed: %s"), label, *destinationDirectory));
			return false;
		}

		FString sourceDirectoryRoot = sourceDirectory;
		sourceDirectoryRoot.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (!sourceDirectoryRoot.EndsWith(TEXT("/")))
		{
			sourceDirectoryRoot += TEXT("/");
		}

		TArray<FString> sourceFiles;
		IFileManager::Get().FindFilesRecursive(sourceFiles, *sourceDirectory, TEXT("*"), true, false);
		for (FString sourceFile : sourceFiles)
		{
			sourceFile.ReplaceInline(TEXT("\\"), TEXT("/"));
			FString relativeFile = sourceFile;
			FPaths::MakePathRelativeTo(relativeFile, *sourceDirectoryRoot);
			if (!CopyDefaultFileIfMissing(
					sourceFile,
					FPaths::Combine(destinationDirectory, relativeFile),
					label,
					outDiagnostics))
			{
				return false;
			}
		}

		return true;
	}

	FString GetDefaultUserProjectScenarioPath()
	{
		return NormalizeUserProjectScenarioPath(DefaultScenarioEditorProjectName);
	}

	void FindUserProjectScenarioFiles(TArray<FString>& outFiles)
	{
		outFiles.Reset();
		const FString searchRoot = FSimulationSetupJson::ResolveProjectPath(UserProjectDirectory);
		TArray<FString> foundFiles;
		IFileManager::Get().FindFilesRecursive(foundFiles, *searchRoot, UserProjectScenarioFileName, true, false);
		for (FString filePath : foundFiles)
		{
			outFiles.Add(ToMainMenuProjectRelativePath(filePath));
		}
		outFiles.Sort();
	}

	FString NormalizeMainMenuRunPath(FString rawPath)
	{
		rawPath.TrimStartAndEndInline();
		rawPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (rawPath.IsEmpty())
		{
			return FString();
		}

		if (FPaths::IsRelative(rawPath))
		{
			rawPath = FSimulationSetupJson::ResolveProjectPath(rawPath);
		}

		FPaths::NormalizeFilename(rawPath);
		FPaths::CollapseRelativeDirectories(rawPath);
		return rawPath;
	}

	bool TryResolveProjectRunPath(
		const FString& runPath,
		FString& outProjectPath,
		FString& outRunId,
		FString& outRunPath)
	{
		outProjectPath.Reset();
		outRunId.Reset();
		outRunPath.Reset();

		const FString normalizedRunPath = NormalizeMainMenuRunPath(runPath);
		if (normalizedRunPath.IsEmpty())
		{
			return false;
		}

		const FString runId = FPaths::GetCleanFilename(normalizedRunPath);
		const FString runsPath = FPaths::GetPath(normalizedRunPath);
		if (!FUserProjectRunSnapshot::IsValidRunId(runId)
			|| !FPaths::GetCleanFilename(runsPath).Equals(TEXT("runs"), ESearchCase::CaseSensitive))
		{
			return false;
		}

		const FString projectPath = FPaths::GetPath(runsPath);
		const FUserProjectRunSnapshotPaths paths = FUserProjectRunSnapshot::BuildPaths(projectPath, runId);
		outProjectPath = paths.ProjectPath;
		outRunId = paths.RunId;
		outRunPath = paths.RunPath;
		return true;
	}

	void AddProjectRunResultItem(
		const FString& projectPath,
		const FString& runId,
		TArray<FProjectRunResultItem>& outItems,
		TSet<FString>& seenRunPaths)
	{
		if (projectPath.TrimStartAndEnd().IsEmpty() || !FUserProjectRunSnapshot::IsValidRunId(runId))
		{
			return;
		}

		const FUserProjectRunSnapshotPaths paths = FUserProjectRunSnapshot::BuildPaths(projectPath, runId);
		if (!FPaths::DirectoryExists(paths.RunPath) || seenRunPaths.Contains(paths.RunPath))
		{
			return;
		}

		FProjectRunResultItem item;
		item.ProjectPath = paths.ProjectPath;
		item.RunId = paths.RunId;
		item.RunPath = paths.RunPath;
		outItems.Add(MoveTemp(item));
		seenRunPaths.Add(paths.RunPath);
	}

	void FindProjectRunResultItems(TArray<FProjectRunResultItem>& outItems)
	{
		outItems.Reset();
		TSet<FString> seenRunPaths;

		TArray<FString> scenarioFiles;
		FindUserProjectScenarioFiles(scenarioFiles);
		for (const FString& scenarioFile : scenarioFiles)
		{
			const FString projectPath = ResolveUserProjectRootFromScenarioPath(scenarioFile);
			if (projectPath.IsEmpty())
			{
				continue;
			}

			const FString runsPath = FPaths::Combine(projectPath, TEXT("runs"));
			TArray<FString> runDirectoryNames;
			IFileManager::Get().FindFiles(runDirectoryNames, *FPaths::Combine(runsPath, TEXT("*")), false, true);
			for (const FString& runDirectoryName : runDirectoryNames)
			{
				AddProjectRunResultItem(projectPath, runDirectoryName, outItems, seenRunPaths);
			}
		}

		outItems.Sort([](const FProjectRunResultItem& left, const FProjectRunResultItem& right)
		{
			if (!left.ProjectPath.Equals(right.ProjectPath, ESearchCase::IgnoreCase))
			{
				return left.ProjectPath < right.ProjectPath;
			}
			return left.RunId < right.RunId;
		});
	}

	void AddProjectRunEpisodeResultItem(
		const FString& episodeId,
		const FString& episodeDirectory,
		TArray<FProjectRunEpisodeResultItem>& outItems,
		TSet<FString>& seenResultPaths)
	{
		if (!FUserProjectEpisodeScenarioJson::IsValidEpisodeId(episodeId))
		{
			return;
		}

		const FString resultPath = FPaths::Combine(episodeDirectory, TEXT("result.json"));
		if (!FPaths::FileExists(resultPath) || seenResultPaths.Contains(resultPath))
		{
			return;
		}

		FProjectRunEpisodeResultItem item;
		item.EpisodeId = episodeId;
		item.ResultPath = resultPath;
		item.EventsPath = FPaths::Combine(episodeDirectory, TEXT("events.jsonl"));
		item.ScenarioPath = FPaths::Combine(episodeDirectory, TEXT("scenario.json"));
		outItems.Add(MoveTemp(item));
		seenResultPaths.Add(resultPath);
	}

	TArray<FProjectRunEpisodeResultItem> BuildProjectRunEpisodeResultItems(
		const FUserProjectRunSnapshotPaths& paths)
	{
		TArray<FProjectRunEpisodeResultItem> items;
		TSet<FString> seenResultPaths;

		TArray<FString> episodeDirectoryNames;
		IFileManager::Get().FindFiles(episodeDirectoryNames, *FPaths::Combine(paths.EpisodesPath, TEXT("*")), false, true);
		for (const FString& episodeDirectoryName : episodeDirectoryNames)
		{
			AddProjectRunEpisodeResultItem(
				episodeDirectoryName,
				FPaths::Combine(paths.EpisodesPath, episodeDirectoryName),
				items,
				seenResultPaths);
		}

		items.Sort([](const FProjectRunEpisodeResultItem& left, const FProjectRunEpisodeResultItem& right)
		{
			return left.EpisodeId < right.EpisodeId;
		});
		return items;
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
		outError.Reset();
		if (sourcePath.Equals(targetPath, ESearchCase::IgnoreCase))
		{
			return true;
		}

		const FString resolvedSourcePath = FSimulationSetupJson::ResolveProjectPath(sourcePath);
		const FString resolvedTargetPath = FSimulationSetupJson::ResolveProjectPath(targetPath);
		if (!FPaths::FileExists(resolvedSourcePath))
		{
			outError = FString::Printf(TEXT("%s 파일을 찾을 수 없습니다: %s"), *itemLabel, *sourcePath);
			return false;
		}
		if (FPaths::FileExists(resolvedTargetPath))
		{
			outError = FString::Printf(TEXT("%s 파일이 이미 존재합니다: %s"), *itemLabel, *targetPath);
			return false;
		}

		const FString targetDirectory = FPaths::GetPath(resolvedTargetPath);
		if (!IFileManager::Get().MakeDirectory(*targetDirectory, true)
			|| !IFileManager::Get().Move(*resolvedTargetPath, *resolvedSourcePath, false, false))
		{
			outError = FString::Printf(TEXT("%s 이름 변경 실패: %s -> %s"), *itemLabel, *sourcePath, *targetPath);
			return false;
		}

		return true;
	}

	FString MakeUniqueInputJsonPath(const FString& baseFileName)
	{
		for (int32 index = 0; index < 1000; ++index)
		{
			const FString fileName = index == 0
				? FString::Printf(TEXT("%s.json"), *baseFileName)
				: FString::Printf(TEXT("%s_%d.json"), *baseFileName, index);
			FString relativePath = FPaths::Combine(TEXT("Json/Input"), fileName);
			relativePath.ReplaceInline(TEXT("\\"), TEXT("/"));
			if (!FPaths::FileExists(FSimulationSetupJson::ResolveProjectPath(relativePath)))
			{
				return relativePath;
			}
		}

		FString fallbackPath = FPaths::Combine(
			TEXT("Json/Input"),
			FString::Printf(TEXT("%s_%s.json"), *baseFileName, *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8)));
		fallbackPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		return fallbackPath;
	}

	FString MakeUniqueUserProjectScenarioPath(const FString& baseProjectName)
	{
		const FString safeBaseProjectName = baseProjectName.TrimStartAndEnd().IsEmpty()
			? FString(DefaultScenarioEditorProjectName)
			: baseProjectName.TrimStartAndEnd();
		for (int32 index = 0; index < 1000; ++index)
		{
			const FString projectName = index == 0
				? safeBaseProjectName
				: FString::Printf(TEXT("%s_%d"), *safeBaseProjectName, index);
			const FString relativePath = NormalizeUserProjectScenarioPath(projectName);
			if (!FPaths::FileExists(FSimulationSetupJson::ResolveProjectPath(relativePath)))
			{
				return relativePath;
			}
		}

		return NormalizeUserProjectScenarioPath(FString::Printf(
			TEXT("%s_%s"),
			*safeBaseProjectName,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8)));
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

	void AddEmptyListMessage(UWidgetTree* widgetTree, UScrollBox* scrollBox, const FString& message)
	{
		if (!widgetTree || !scrollBox) return;

		scrollBox->AddChild(MakeTextBlock(widgetTree, TEXT("EmptyListMessage"), message, 14));
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
	if (!ValidateRequiredBindings()) return;

	BindControls();
	SetExperimentConfigDetailVisible(false);
	SetExperimentResultDetailVisible(false);
	ShowMainMenuSection(static_cast<int32>(EMainMenuSection::Scenario));
	RefreshSetupOptions();

	if (USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem())
	{
		subsystem->OnRunInfoChanged.RemoveAll(this);
		subsystem->OnRunInfoChanged.AddUObject(this, &UMainMenuWidget::HandleRunInfoChanged);
	}

	if (UPlatformAnalysisAiSubsystem* analysisSubsystem = GetPlatformAnalysisAiSubsystem())
	{
		analysisSubsystem->OnAnalysisCompleted.RemoveAll(this);
		analysisSubsystem->OnAnalysisCompleted.AddUObject(this, &UMainMenuWidget::HandleAnalysisCompleted);
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
	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}

	if (USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem())
	{
		subsystem->OnRunInfoChanged.RemoveAll(this);
	}

	if (UPlatformAnalysisAiSubsystem* analysisSubsystem = GetPlatformAnalysisAiSubsystem())
	{
		analysisSubsystem->OnAnalysisCompleted.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UMainMenuWidget::RefreshSetupOptions()
{
	USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem();
	if (!subsystem) return;

	const FString currentPath = GetSelectedSetupPath();
	const TArray<FString> setupFiles = subsystem->ListSimulationSetupFiles();
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

	const TArray<FString> scenarioSetupFiles = subsystem->ListScenarioSetupFiles();
	const FString currentScenarioSetupPath = GetSelectedScenarioSetupPath();
	const FString selectedScenarioSetupPath = scenarioSetupFiles.Contains(currentScenarioSetupPath)
		? currentScenarioSetupPath
		: (scenarioSetupFiles.IsEmpty() ? FString() : scenarioSetupFiles[0]);
	SetComboBoxOptions(ExperimentScenarioSetupComboBox, scenarioSetupFiles, selectedScenarioSetupPath);
	SetSelectedScenarioSetupPath(selectedScenarioSetupPath);

	TArray<FString> projectScenarioFiles;
	FindUserProjectScenarioFiles(projectScenarioFiles);
	const FString currentProjectScenarioPath = GetSelectedProjectScenarioPath();
	const FString selectedProjectScenarioPath = projectScenarioFiles.Contains(currentProjectScenarioPath)
		? currentProjectScenarioPath
		: (projectScenarioFiles.IsEmpty() ? GetDefaultUserProjectScenarioPath() : projectScenarioFiles[0]);
	SetComboBoxOptions(ScenarioSetupComboBox, projectScenarioFiles, selectedProjectScenarioPath);
	SetSelectedProjectScenarioPath(selectedProjectScenarioPath);
	RefreshScenarioList();

	const TArray<FString> deliveryBotSetupFiles = subsystem->ListDeliveryBotSetupFiles();
	const FString currentDeliveryBotSetupPath = GetSelectedDeliveryBotSetupPath();
	const FString selectedDeliveryBotSetupPath = deliveryBotSetupFiles.Contains(currentDeliveryBotSetupPath)
		? currentDeliveryBotSetupPath
		: (deliveryBotSetupFiles.IsEmpty() ? FString() : deliveryBotSetupFiles[0]);
	SetComboBoxOptions(DeliveryBotSetupComboBox, deliveryBotSetupFiles, selectedDeliveryBotSetupPath);
	SetComboBoxOptions(PolicyDeliveryBotSetupComboBox, deliveryBotSetupFiles, selectedDeliveryBotSetupPath);
	SetSelectedDeliveryBotSetupPath(selectedDeliveryBotSetupPath);
	RefreshPolicyList();

	const TArray<FString> policySpecFiles = subsystem->ListPolicySpecFiles();
	const FString currentPolicySpecPath = GetSelectedPolicySpecPath();
	const FString defaultPolicySpecPath = MainMenuDefaultPolicySpecJsonPath;
	const FString selectedPolicySpecPath = policySpecFiles.Contains(currentPolicySpecPath)
		? currentPolicySpecPath
		: (policySpecFiles.Contains(defaultPolicySpecPath)
			? defaultPolicySpecPath
			: (policySpecFiles.IsEmpty() ? defaultPolicySpecPath : policySpecFiles[0]));
	SetComboBoxOptions(PolicySpecComboBox, policySpecFiles, selectedPolicySpecPath);
	SetSelectedPolicySpecPath(selectedPolicySpecPath);

	RefreshExperimentResultList();
}

void UMainMenuWidget::RefreshFromSubsystem()
{
	if (USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem())
	{
		subsystem->RefreshActiveRunStatus();
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
			|| !FPaths::FileExists(FSimulationSetupJson::ResolveProjectPath(GetSelectedSetupPath())))
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
	USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem();
	if (!subsystem || !FixedStepFpsTextBox)
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
	if (subsystem->SaveFixedStepFpsToSetupFile(GetSelectedSetupPath(), fps, diagnostics))
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
	USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem();
	if (!subsystem)
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
	const FSimulationSetupParseResult parseResult = subsystem->LoadSimulationSetupFile(setupPath);
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
		runInput.ScenarioSetupJsonPath = scenarioSetupPath;
		runInput.DeliveryBotSetupJsonPath = deliveryBotSetupPath;
		runInput.PolicySpecJsonPath = policySpecPath;
		runInputs.Add(runInput);
	}

	TArray<FString> diagnostics;
	if (!subsystem->SaveScenarioRunQueueFile(runQueuePath, runInputs, diagnostics))
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

	if (subsystem->SaveSimulationSetupFile(setupPath, setup, diagnostics))
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
	OpenScenarioInEditor(GetSelectedProjectScenarioPath());
}

void UMainMenuWidget::HandleNewScenarioClicked()
{
	const FString newScenarioPath = MakeUniqueUserProjectScenarioPath(DefaultScenarioEditorProjectName);
	if (!OpenNewProjectScenarioInEditor(newScenarioPath))
	{
		return;
	}

	SetSelectedProjectScenarioPath(newScenarioPath);
	RefreshScenarioList();
	SetDiagnosticsText(FString::Printf(TEXT("project scenario 생성됨: %s"), *newScenarioPath));
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

	USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		SetDiagnosticsText(TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다."));
		return;
	}

	FString moveError;
	if (!MoveProjectRelativeFile(sourcePath, targetPath, TEXT("Scenario"), moveError))
	{
		SetDiagnosticsText(moveError);
		return;
	}

	TArray<FString> diagnostics;
	if (!subsystem->ReplaceScenarioSetupReferencesInRunQueues(sourcePath, targetPath, diagnostics))
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

	const FString scenarioJsonPath = itemWidget->GetOriginalPath();
	SetSelectedProjectScenarioPath(scenarioJsonPath);
	OpenScenarioInEditor(scenarioJsonPath);
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

	USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		SetDiagnosticsText(TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다."));
		return;
	}

	FString moveError;
	if (!MoveProjectRelativeFile(sourcePath, targetPath, TEXT("Policy"), moveError))
	{
		SetDiagnosticsText(moveError);
		return;
	}

	TArray<FString> diagnostics;
	if (!subsystem->ReplaceDeliveryBotSetupReferencesInRunQueues(sourcePath, targetPath, diagnostics))
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
	RefreshExperimentResultIterationList();
	UpdateReportAndLogText();
	SetExperimentResultDetailVisible(true);
}

void UMainMenuWidget::HandleExperimentResultIterationButtonClicked(UExperimentResultIterationButton* buttonWidget)
{
	if (!IsValid(buttonWidget)) return;

	SetSelectedExperimentResultEpisode(buttonWidget->GetResultPath(), buttonWidget->GetEpisodeId());
	RefreshExperimentResultIterationList();
	UpdateReportAndLogText();
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
	if (!FPaths::FileExists(resolvedPolicyPath))
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
	const FString templatePath = FSimulationSetupJson::ResolveProjectPath(DeliveryBotTemplatePath);
	FString templateJson;
	if (!FFileHelper::LoadFileToString(templateJson, *templatePath))
	{
		SetDiagnosticsText(FString::Printf(TEXT("행동 정책 템플릿 읽기 실패: %s"), DeliveryBotTemplatePath));
		return;
	}

	const FString newPolicyPath = MakeUniqueInputJsonPath(TEXT("DeliveryBotSetupNew"));
	const FString resolvedNewPolicyPath = FSimulationSetupJson::ResolveProjectPath(newPolicyPath);
	const FString outputDirectory = FPaths::GetPath(resolvedNewPolicyPath);
	if (!IFileManager::Get().MakeDirectory(*outputDirectory, true)
		|| !FFileHelper::SaveStringToFile(
			templateJson,
			*resolvedNewPolicyPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		SetDiagnosticsText(FString::Printf(TEXT("행동 정책 파일 생성 실패: %s"), *resolvedNewPolicyPath));
		return;
	}

	SetSelectedDeliveryBotSetupPath(newPolicyPath);
	RefreshSetupOptions();

	SetDiagnosticsText(FString::Printf(TEXT("행동 정책 생성됨: %s"), *newPolicyPath));
}

void UMainMenuWidget::HandleStartClicked()
{
	const FString requestedRunId = RunIdTextBox ? RunIdTextBox->GetText().ToString().TrimStartAndEnd() : FString();
	if (StartProjectRunFromScenario(GetSelectedProjectScenarioPath(), requestedRunId))
	{
		UpdateStatusText(TEXT("Project run launch requested."));
		return;
	}
}

void UMainMenuWidget::HandleRefreshClicked()
{
	RefreshSetupOptions();
	RefreshFromSubsystem();
}

void UMainMenuWidget::HandleSendToAiClicked()
{
	if (CurrentPreviewProjectPath.IsEmpty() || CurrentPreviewRunId.IsEmpty())
	{
		if (AiAnalysisTextBlock)
		{
			AiAnalysisTextBlock->SetText(FText::FromString(TEXT("AI analysis unavailable: no project run selected.")));
		}
		return;
	}

	if (CurrentPreviewSummaryPath.IsEmpty() || !FPaths::FileExists(CurrentPreviewSummaryPath))
	{
		if (AiAnalysisTextBlock)
		{
			AiAnalysisTextBlock->SetText(FText::FromString(TEXT("AI analysis unavailable: selected run has no summary.json.")));
		}
		return;
	}

	UPlatformAnalysisAiSubsystem* analysisSubsystem = GetPlatformAnalysisAiSubsystem();
	if (!analysisSubsystem)
	{
		if (AiAnalysisTextBlock)
		{
			AiAnalysisTextBlock->SetText(FText::FromString(TEXT("AI analysis unavailable: subsystem not found.")));
		}
		return;
	}

	if (AiAnalysisTextBlock)
	{
		AiAnalysisTextBlock->SetText(FText::FromString(TEXT("Analyzing...")));
	}

	analysisSubsystem->RequestAnalysisForProjectRun(CurrentPreviewProjectPath, CurrentPreviewRunId);
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
	SetSelectedProjectScenarioPath(selectedItem);
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

UWidget* UMainMenuWidget::HandleGenerateComboBoxItem(FString item)
{
	return WidgetTree ? MakeTextBlock(WidgetTree, TEXT("ComboBoxItemText"), item, 13) : nullptr;
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

	requireWidget(MainContentSwitcher, TEXT("MainContentSwitcher"));
	requireWidget(ScenarioNavButton, TEXT("ScenarioNavButton"));
	requireWidget(PolicyNavButton, TEXT("PolicyNavButton"));
	requireWidget(ExperimentConfigNavButton, TEXT("ExperimentConfigNavButton"));
	requireWidget(RunStatusNavButton, TEXT("RunStatusNavButton"));
	requireWidget(ExperimentResultNavButton, TEXT("ExperimentResultNavButton"));
	requireWidget(ScenarioListScrollBox, TEXT("ScenarioListScrollBox"));
	requireWidget(PolicyListScrollBox, TEXT("PolicyListScrollBox"));
	requireWidget(ExperimentConfigSectionBoxScrollBox, TEXT("ExperimentConfigSectionBoxScrollBox"));
	requireWidget(ExperimentConfigDetailSectionBoxScrollBox, TEXT("ExperimentConfigDetailSectionBoxScrollBox"));
	requireWidget(ExperimentConfigListScrollBox, TEXT("ExperimentConfigListScrollBox"));
	requireWidget(ExperimentResultSectionBoxScrollBox, TEXT("ExperimentResultSectionBoxScrollBox"));
	requireWidget(ExperimentResultDetailSectionBoxScrollBox, TEXT("ExperimentResultDetailSectionBoxScrollBox"));
	requireWidget(ExperimentResultListScrollBox, TEXT("ExperimentResultListScrollBox"));
	requireWidget(ExperimentResultIterationScrollBox, TEXT("ExperimentResultIterationScrollBox"));
	requireWidget(ExperimentConfigBackButton, TEXT("ExperimentConfigBackButton"));
	requireWidget(ExperimentResultBackButton, TEXT("ExperimentResultBackButton"));
	requireWidget(FixedStepFpsTextBox, TEXT("FixedStepFpsTextBox"));
	requireWidget(RunCountTextBox, TEXT("RunCountTextBox"));
	requireWidget(ExperimentScenarioSetupComboBox, TEXT("ExperimentScenarioSetupComboBox"));
	requireWidget(DeliveryBotSetupComboBox, TEXT("DeliveryBotSetupComboBox"));
	requireWidget(NewSetupButton, TEXT("NewSetupButton"));
	requireWidget(SaveSetupButton, TEXT("SaveSetupButton"));
	requireWidget(NewScenarioButton, TEXT("NewScenarioButton"));
	requireWidget(NewPolicyButton, TEXT("NewPolicyButton"));
	requireWidget(StartButton, TEXT("StartButton"));
	requireWidget(RefreshButton, TEXT("RefreshButton"));
	requireWidget(StatusTextBlock, TEXT("StatusTextBlock"));
	requireWidget(DiagnosticsTextBlock, TEXT("DiagnosticsTextBlock"));
	requireWidget(ReportTextBlock, TEXT("ReportTextBlock"));
	requireWidget(LogPreviewTextBlock, TEXT("LogPreviewTextBlock"));

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
		SetupComboBox->OnGenerateWidgetEvent.Unbind();
		SetupComboBox->OnGenerateWidgetEvent.BindDynamic(this, &UMainMenuWidget::HandleGenerateComboBoxItem);
		SetupComboBox->OnSelectionChanged.RemoveDynamic(this, &UMainMenuWidget::HandleSetupSelectionChanged);
		SetupComboBox->OnSelectionChanged.AddDynamic(this, &UMainMenuWidget::HandleSetupSelectionChanged);
	}

	if (ScenarioSetupComboBox)
	{
		ScenarioSetupComboBox->OnGenerateWidgetEvent.Unbind();
		ScenarioSetupComboBox->OnGenerateWidgetEvent.BindDynamic(this, &UMainMenuWidget::HandleGenerateComboBoxItem);
		ScenarioSetupComboBox->OnSelectionChanged.RemoveDynamic(this, &UMainMenuWidget::HandleScenarioSetupSelectionChanged);
		ScenarioSetupComboBox->OnSelectionChanged.AddDynamic(this, &UMainMenuWidget::HandleScenarioSetupSelectionChanged);
	}

	if (PolicyDeliveryBotSetupComboBox)
	{
		PolicyDeliveryBotSetupComboBox->OnGenerateWidgetEvent.Unbind();
		PolicyDeliveryBotSetupComboBox->OnGenerateWidgetEvent.BindDynamic(this, &UMainMenuWidget::HandleGenerateComboBoxItem);
		PolicyDeliveryBotSetupComboBox->OnSelectionChanged.RemoveDynamic(this, &UMainMenuWidget::HandlePolicyDeliveryBotSelectionChanged);
		PolicyDeliveryBotSetupComboBox->OnSelectionChanged.AddDynamic(this, &UMainMenuWidget::HandlePolicyDeliveryBotSelectionChanged);
	}

	if (ExperimentScenarioSetupComboBox)
	{
		ExperimentScenarioSetupComboBox->OnGenerateWidgetEvent.Unbind();
		ExperimentScenarioSetupComboBox->OnGenerateWidgetEvent.BindDynamic(this, &UMainMenuWidget::HandleGenerateComboBoxItem);
		ExperimentScenarioSetupComboBox->OnSelectionChanged.RemoveDynamic(this, &UMainMenuWidget::HandleExperimentScenarioSetupSelectionChanged);
		ExperimentScenarioSetupComboBox->OnSelectionChanged.AddDynamic(this, &UMainMenuWidget::HandleExperimentScenarioSetupSelectionChanged);
	}

	if (DeliveryBotSetupComboBox)
	{
		DeliveryBotSetupComboBox->OnGenerateWidgetEvent.Unbind();
		DeliveryBotSetupComboBox->OnGenerateWidgetEvent.BindDynamic(this, &UMainMenuWidget::HandleGenerateComboBoxItem);
		DeliveryBotSetupComboBox->OnSelectionChanged.RemoveDynamic(this, &UMainMenuWidget::HandleExperimentDeliveryBotSelectionChanged);
		DeliveryBotSetupComboBox->OnSelectionChanged.AddDynamic(this, &UMainMenuWidget::HandleExperimentDeliveryBotSelectionChanged);
	}

	if (PolicySpecComboBox)
	{
		PolicySpecComboBox->OnGenerateWidgetEvent.Unbind();
		PolicySpecComboBox->OnGenerateWidgetEvent.BindDynamic(this, &UMainMenuWidget::HandleGenerateComboBoxItem);
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
}

void UMainMenuWidget::LoadSelectedSetup()
{
	USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem();
	if (!subsystem) return;

	const FSimulationSetupParseResult parseResult = subsystem->LoadSimulationSetupFile(GetSelectedSetupPath());
	if (!parseResult.bSuccess)
	{
		if (GetSelectedSetupPath().TrimStartAndEnd().IsEmpty())
		{
			SetDiagnosticsText(TEXT("SimulationSetup이 선택되지 않았습니다."));
			return;
		}

		if (!FPaths::FileExists(FSimulationSetupJson::ResolveProjectPath(GetSelectedSetupPath())))
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
	if (subsystem->LoadScenarioRunQueueFile(parseResult.Setup.RunQueueJsonPath, loadedRunInputs, runQueueDiagnostics)
		&& !loadedRunInputs.IsEmpty())
	{
		const FScenarioRunInput& firstRunInput = loadedRunInputs[0];
		SetSelectedScenarioSetupPath(firstRunInput.ScenarioSetupJsonPath);
		if (DeliveryBotSetupComboBox)
		{
			DeliveryBotSetupComboBox->SetSelectedOption(firstRunInput.DeliveryBotSetupJsonPath);
		}
		if (PolicyDeliveryBotSetupComboBox)
		{
			PolicyDeliveryBotSetupComboBox->SetSelectedOption(firstRunInput.DeliveryBotSetupJsonPath);
		}
		SetSelectedDeliveryBotSetupPath(firstRunInput.DeliveryBotSetupJsonPath);
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
	if (StatusOutputPathTextBox)
	{
		StatusOutputPathTextBox->SetText(FText::FromString(parseResult.Setup.Status.OutputPath));
	}

	TArray<FString> lines;
	lines.Add(FString::Printf(TEXT("로드한 SimulationSetup: %s"), *GetSelectedSetupPath()));
	lines.Add(FString::Printf(TEXT("RunQueue: %s"), *parseResult.Setup.RunQueueJsonPath));
	if (!loadedRunInputs.IsEmpty())
	{
		lines.Add(FString::Printf(TEXT("시나리오(ScenarioSetup): %s"), *loadedRunInputs[0].ScenarioSetupJsonPath));
		lines.Add(FString::Printf(TEXT("행동 정책(DeliveryBotSetup): %s"), *loadedRunInputs[0].DeliveryBotSetupJsonPath));
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
	bExperimentResultDetailVisible = bVisible;

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

	const TSubclassOf<UFileListItemWidget> itemWidgetClass = ResolveFileListItemWidgetClass();
	if (!itemWidgetClass)
	{
		SetDiagnosticsText(TEXT("WBP_FileListItem 클래스를 사용할 수 없습니다."));
		return;
	}

	TArray<FString> projectScenarioFiles;
	FindUserProjectScenarioFiles(projectScenarioFiles);
	if (!projectScenarioFiles.Contains(SelectedProjectScenarioPath))
	{
		SetSelectedProjectScenarioPath(projectScenarioFiles.IsEmpty() ? GetDefaultUserProjectScenarioPath() : projectScenarioFiles[0]);
	}

	ScenarioListScrollBox->ClearChildren();
	ScenarioListItems.Reset();
	ScenarioListItems.Reserve(projectScenarioFiles.Num());

	if (projectScenarioFiles.IsEmpty())
	{
		AddEmptyListMessage(
			WidgetTree,
			ScenarioListScrollBox,
			FString::Printf(TEXT("%s 아래에 scenario.json이 없습니다. 새 시나리오를 만들거나 project 경로를 입력하세요."), UserProjectDirectory));
	}

	for (const FString& projectScenarioFile : projectScenarioFiles)
	{
		UFileListItemWidget* itemWidget = CreateWidget<UFileListItemWidget>(this, itemWidgetClass);
		if (!itemWidget) continue;

		itemWidget->InitializeItem(projectScenarioFile, TEXT("편집"), TEXT("실행"), false, true, false);
		itemWidget->OnPrimaryActionRequested.AddUObject(this, &UMainMenuWidget::HandleScenarioEditRequested);
		ScenarioListScrollBox->AddChild(itemWidget);
		ScenarioListItems.Add(itemWidget);
	}
}

void UMainMenuWidget::RefreshPolicyList()
{
	if (!PolicyListScrollBox) return;

	USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem();
	if (!subsystem) return;

	const TSubclassOf<UFileListItemWidget> itemWidgetClass = ResolveFileListItemWidgetClass();
	if (!itemWidgetClass) return;

	const TArray<FString> deliveryBotSetupFiles = subsystem->ListDeliveryBotSetupFiles();
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
		AddEmptyListMessage(WidgetTree, PolicyListScrollBox, TEXT("편집 가능한 DeliveryBotSetup JSON이 없습니다."));
		return;
	}

	for (const FString& deliveryBotSetupFile : deliveryBotSetupFiles)
	{
		UFileListItemWidget* itemWidget = CreateWidget<UFileListItemWidget>(this, itemWidgetClass);
		if (!itemWidget) continue;

		itemWidget->InitializeItem(deliveryBotSetupFile, TEXT("편집"), TEXT("실행"), true, true, false);
		itemWidget->OnRenameRequested.AddUObject(this, &UMainMenuWidget::HandlePolicyRenameRequested);
		itemWidget->OnPrimaryActionRequested.AddUObject(this, &UMainMenuWidget::HandlePolicyEditRequested);
		PolicyListScrollBox->AddChild(itemWidget);
		PolicyListItems.Add(itemWidget);
	}
}

void UMainMenuWidget::RefreshExperimentConfigList()
{
	if (!ExperimentConfigListScrollBox) return;

	USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem();
	if (!subsystem) return;

	const TSubclassOf<UFileListItemWidget> itemWidgetClass = ResolveFileListItemWidgetClass();
	if (!itemWidgetClass) return;

	const TArray<FString> setupFiles = subsystem->ListSimulationSetupFiles();
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
		AddEmptyListMessage(WidgetTree, ExperimentConfigListScrollBox, TEXT("편집 가능한 SimulationSetup JSON이 없습니다."));
		return;
	}

	for (const FString& setupFile : setupFiles)
	{
		UFileListItemWidget* itemWidget = CreateWidget<UFileListItemWidget>(this, itemWidgetClass);
		if (!itemWidget) continue;

		itemWidget->InitializeItem(setupFile, TEXT("편집"), TEXT("실행"), true, true, true);
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

	const TSubclassOf<UFileListItemWidget> itemWidgetClass = ResolveFileListItemWidgetClass();
	if (!itemWidgetClass) return;

	TArray<FProjectRunResultItem> resultItems;
	FindProjectRunResultItems(resultItems);
	if (USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem())
	{
		const FSimulatorRunInfo runInfo = subsystem->GetActiveRunInfo();
		if (runInfo.bProjectRun && !runInfo.ProjectPath.IsEmpty() && !runInfo.RunId.IsEmpty())
		{
			TSet<FString> seenRunPaths;
			for (const FProjectRunResultItem& item : resultItems)
			{
				seenRunPaths.Add(item.RunPath);
			}
			AddProjectRunResultItem(runInfo.ProjectPath, runInfo.RunId, resultItems, seenRunPaths);
			resultItems.Sort([](const FProjectRunResultItem& left, const FProjectRunResultItem& right)
			{
				if (!left.ProjectPath.Equals(right.ProjectPath, ESearchCase::IgnoreCase))
				{
					return left.ProjectPath < right.ProjectPath;
				}
				return left.RunId < right.RunId;
			});
		}
	}

	bool bSelectedRunStillExists = false;
	for (const FProjectRunResultItem& item : resultItems)
	{
		if (item.RunPath.Equals(SelectedExperimentResultRunDirectory, ESearchCase::IgnoreCase))
		{
			bSelectedRunStillExists = true;
			break;
		}
	}
	if (!bSelectedRunStillExists)
	{
		SetSelectedExperimentResultRunDirectory(resultItems.IsEmpty() ? FString() : resultItems.Last().RunPath);
	}

	ExperimentResultListScrollBox->ClearChildren();
	ExperimentResultListItems.Reset();
	ExperimentResultListItems.Reserve(resultItems.Num());

	if (resultItems.IsEmpty())
	{
		AddEmptyListMessage(WidgetTree, ExperimentResultListScrollBox, TEXT("No project runs found."));
		return;
	}

	for (const FProjectRunResultItem& resultItem : resultItems)
	{
		UFileListItemWidget* itemWidget = CreateWidget<UFileListItemWidget>(this, itemWidgetClass);
		if (!itemWidget) continue;

		const FString displayText = FString::Printf(
			TEXT("%s / %s"),
			*FPaths::GetCleanFilename(resultItem.ProjectPath),
			*resultItem.RunId);
		itemWidget->InitializeDisplayItem(
			resultItem.RunPath,
			displayText,
			TEXT("Details"),
			TEXT("Run"),
			false,
			true,
			false);
		itemWidget->OnPrimaryActionRequested.AddUObject(this, &UMainMenuWidget::HandleExperimentResultDetailsRequested);
		ExperimentResultListScrollBox->AddChild(itemWidget);
		ExperimentResultListItems.Add(itemWidget);
	}
}

void UMainMenuWidget::RefreshExperimentResultIterationList()
{
	ClearExperimentResultIterationWidgets();

	if (SelectedExperimentResultProjectPath.IsEmpty() || SelectedExperimentResultRunId.IsEmpty())
	{
		return;
	}

	if (!ensureMsgf(IsValid(ExperimentResultIterationScrollBox), TEXT("Missing required WBP binding: ExperimentResultIterationScrollBox"))
		|| !ensureMsgf(IsValid(WidgetTree), TEXT("MainMenuWidget requires a valid WidgetTree.")))
	{
		return;
	}

	const FUserProjectRunSnapshotPaths paths = FUserProjectRunSnapshot::BuildPaths(
		SelectedExperimentResultProjectPath,
		SelectedExperimentResultRunId);
	const TArray<FProjectRunEpisodeResultItem> episodeItems = BuildProjectRunEpisodeResultItems(paths);
	if (episodeItems.IsEmpty())
	{
		SetSelectedExperimentResultEpisode(FString(), FString());
		return;
	}

	bool bSelectedEpisodeStillExists = false;
	for (const FProjectRunEpisodeResultItem& episodeItem : episodeItems)
	{
		if (episodeItem.ResultPath.Equals(SelectedExperimentResultPath, ESearchCase::IgnoreCase))
		{
			bSelectedEpisodeStillExists = true;
			break;
		}
	}
	if (!bSelectedEpisodeStillExists)
	{
		SetSelectedExperimentResultEpisode(episodeItems[0].ResultPath, episodeItems[0].EpisodeId);
	}

	for (const FProjectRunEpisodeResultItem& episodeItem : episodeItems)
	{
		USizeBox* sizeBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			MakeUniqueWidgetName(WidgetTree, USizeBox::StaticClass(), TEXT("ExperimentResultIterationButtonSlot")));
		UExperimentResultIterationButton* button = WidgetTree->ConstructWidget<UExperimentResultIterationButton>(
			UExperimentResultIterationButton::StaticClass(),
			MakeUniqueWidgetName(WidgetTree, UExperimentResultIterationButton::StaticClass(), TEXT("ExperimentResultIterationButton")));
		UTextBlock* label = MakeTextBlock(
			WidgetTree,
			TEXT("ExperimentResultIterationButtonLabel"),
			episodeItem.EpisodeId,
			18);

		if (!sizeBox || !button || !label) continue;

		const bool bSelected = episodeItem.ResultPath.Equals(SelectedExperimentResultPath, ESearchCase::IgnoreCase);
		button->Configure(episodeItem.ResultPath, episodeItem.EpisodeId);
		button->SetBackgroundColor(bSelected
			? FLinearColor(0.16f, 0.42f, 0.78f, 1.0f)
			: FLinearColor(0.10f, 0.11f, 0.14f, 1.0f));
		button->SetColorAndOpacity(FLinearColor::White);
		button->OnIterationClicked.AddUObject(this, &UMainMenuWidget::HandleExperimentResultIterationButtonClicked);

		label->SetJustification(ETextJustify::Center);
		label->SetColorAndOpacity(FSlateColor(FLinearColor::White));

		button->SetContent(label);
		sizeBox->SetWidthOverride(72.0f);
		sizeBox->SetHeightOverride(36.0f);
		sizeBox->SetContent(button);

		if (UScrollBoxSlot* scrollBoxSlot = Cast<UScrollBoxSlot>(ExperimentResultIterationScrollBox->AddChild(sizeBox)))
		{
			scrollBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
		}
		ExperimentResultIterationButtons.Add(button);
	}
}

void UMainMenuWidget::SetSelectedScenarioSetupPath(const FString& scenarioSetupPath)
{
	SelectedScenarioSetupPath = scenarioSetupPath.TrimStartAndEnd();
	SelectedScenarioSetupPath.ReplaceInline(TEXT("\\"), TEXT("/"));

	SyncComboBoxSelection(ExperimentScenarioSetupComboBox, SelectedScenarioSetupPath);
}

void UMainMenuWidget::SetSelectedProjectScenarioPath(const FString& scenarioJsonPath)
{
	SelectedProjectScenarioPath = NormalizeUserProjectScenarioPath(scenarioJsonPath);

	if (ScenarioSetupPathTextBox)
	{
		ScenarioSetupPathTextBox->SetText(FText::FromString(SelectedProjectScenarioPath));
	}
	SyncComboBoxSelection(ScenarioSetupComboBox, SelectedProjectScenarioPath);
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
	FString projectPath;
	FString runId;
	FString normalizedRunPath;
	if (TryResolveProjectRunPath(runDirectory, projectPath, runId, normalizedRunPath))
	{
		SelectedExperimentResultRunDirectory = normalizedRunPath;
		SelectedExperimentResultProjectPath = projectPath;
		SelectedExperimentResultRunId = runId;
	}
	else
	{
		SelectedExperimentResultRunDirectory.Reset();
		SelectedExperimentResultProjectPath.Reset();
		SelectedExperimentResultRunId.Reset();
	}

	if (!previousRunDirectory.Equals(SelectedExperimentResultRunDirectory, ESearchCase::IgnoreCase))
	{
		SetSelectedExperimentResultEpisode(FString(), FString());
	}
}

void UMainMenuWidget::SetSelectedExperimentResultEpisode(const FString& resultPath, const FString& episodeId)
{
	SelectedExperimentResultPath = resultPath.TrimStartAndEnd();
	SelectedExperimentResultPath.ReplaceInline(TEXT("\\"), TEXT("/"));
	SelectedExperimentResultEpisodeId = episodeId.TrimStartAndEnd();
}

void UMainMenuWidget::ClearExperimentResultIterationWidgets()
{
	for (UExperimentResultIterationButton* button : ExperimentResultIterationButtons)
	{
		if (IsValid(button))
		{
			button->OnIterationClicked.RemoveAll(this);
		}
	}

	ExperimentResultIterationButtons.Reset();
	if (ExperimentResultIterationScrollBox)
	{
		ExperimentResultIterationScrollBox->ClearChildren();
	}
}

bool UMainMenuWidget::EnsureProjectDefaultsForScenario(const FString& scenarioJsonPath)
{
	const FString normalizedScenarioJsonPath = NormalizeUserProjectScenarioPath(scenarioJsonPath);
	const FString projectRoot = ResolveUserProjectRootFromScenarioPath(normalizedScenarioJsonPath);
	if (projectRoot.IsEmpty())
	{
		SetDiagnosticsText(TEXT("Project scenario path must be <UserProject>/scenario.json."));
		return false;
	}

	if (!IFileManager::Get().MakeDirectory(*projectRoot, true))
	{
		SetDiagnosticsText(FString::Printf(TEXT("Project directory create failed: %s"), *projectRoot));
		return false;
	}

	const FString templateRoot = ResolveBlankProjectTemplateDirectory();
	if (templateRoot.IsEmpty())
	{
		SetDiagnosticsText(TEXT("Blank project template directory was not found."));
		return false;
	}

	TArray<FString> diagnostics;
	const bool bDefaultsReady =
		CopyDefaultFileIfMissing(
			FPaths::Combine(templateRoot, UserProjectSettingFileName),
			FPaths::Combine(projectRoot, UserProjectSettingFileName),
			TEXT("setting"),
			diagnostics)
		&& CopyDefaultFileIfMissing(
			FPaths::Combine(templateRoot, UserProjectProfileFileName),
			FPaths::Combine(projectRoot, UserProjectProfileFileName),
			TEXT("profile"),
			diagnostics)
		&& CopyDefaultDirectoryIfMissing(
			FPaths::Combine(templateRoot, UserProjectPolicyDirectoryName),
			FPaths::Combine(projectRoot, UserProjectPolicyDirectoryName),
			TEXT("policy"),
			diagnostics);

	if (!bDefaultsReady)
	{
		SetDiagnosticsText(JoinStringLines(diagnostics));
	}
	return bDefaultsReady;
}

bool UMainMenuWidget::OpenNewProjectScenarioInEditor(const FString& scenarioJsonPath)
{
	UScenarioEditorLaunchSubsystem* subsystem = GetScenarioEditorLaunchSubsystem();
	if (!subsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioEditorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}

	const FString normalizedScenarioJsonPath = NormalizeUserProjectScenarioPath(scenarioJsonPath);
	if (normalizedScenarioJsonPath.IsEmpty())
	{
		SetDiagnosticsText(TEXT("새 project scenario 경로가 비어 있습니다."));
		return false;
	}
	if (!IsUserProjectScenarioPath(normalizedScenarioJsonPath))
	{
		SetDiagnosticsText(TEXT("project scenario는 <UserProject>/scenario.json 경로여야 합니다."));
		return false;
	}

	const FString resolvedScenarioJsonPath = FSimulationSetupJson::ResolveProjectPath(normalizedScenarioJsonPath);
	if (FPaths::FileExists(resolvedScenarioJsonPath))
	{
		SetDiagnosticsText(FString::Printf(TEXT("project scenario 파일이 이미 존재합니다: %s"), *normalizedScenarioJsonPath));
		return false;
	}

	if (!EnsureProjectDefaultsForScenario(normalizedScenarioJsonPath))
	{
		return false;
	}

	if (!subsystem->OpenNewScenarioEditorAtPath(normalizedScenarioJsonPath))
	{
		SetDiagnosticsText(TEXT("ScenarioEditorMap 열기 실패."));
		return false;
	}

	return true;
}

bool UMainMenuWidget::OpenScenarioInEditor(const FString& scenarioJsonPath)
{
	UScenarioEditorLaunchSubsystem* subsystem = GetScenarioEditorLaunchSubsystem();
	if (!subsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioEditorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}

	const FString normalizedScenarioJsonPath = NormalizeUserProjectScenarioPath(scenarioJsonPath);
	if (normalizedScenarioJsonPath.TrimStartAndEnd().IsEmpty())
	{
		SetDiagnosticsText(TEXT("project scenario 파일이 선택되지 않았습니다."));
		return false;
	}
	if (!IsUserProjectScenarioPath(normalizedScenarioJsonPath))
	{
		SetDiagnosticsText(TEXT("project scenario는 <UserProject>/scenario.json 경로여야 합니다."));
		return false;
	}

	const FString resolvedScenarioJsonPath = FSimulationSetupJson::ResolveProjectPath(normalizedScenarioJsonPath);
	if (!FPaths::FileExists(resolvedScenarioJsonPath))
	{
		SetDiagnosticsText(FString::Printf(TEXT("project scenario 파일이 없습니다: %s"), *normalizedScenarioJsonPath));
		return false;
	}

	if (!subsystem->OpenScenarioEditor(normalizedScenarioJsonPath))
	{
		SetDiagnosticsText(TEXT("ScenarioEditorMap 열기 실패."));
		return false;
	}

	return true;
}

bool UMainMenuWidget::StartProjectRunFromScenario(const FString& scenarioJsonPath, const FString& requestedRunId)
{
	USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		SetDiagnosticsText(TEXT("SimulatorLaunchSubsystem is unavailable."));
		return false;
	}

	const FString normalizedScenarioJsonPath = NormalizeUserProjectScenarioPath(scenarioJsonPath);
	if (!IsUserProjectScenarioPath(normalizedScenarioJsonPath))
	{
		SetDiagnosticsText(TEXT("Project run requires <UserProject>/scenario.json."));
		return false;
	}

	const FString resolvedScenarioJsonPath = FSimulationSetupJson::ResolveProjectPath(normalizedScenarioJsonPath);
	if (!FPaths::FileExists(resolvedScenarioJsonPath))
	{
		SetDiagnosticsText(FString::Printf(TEXT("Project scenario file is missing: %s"), *normalizedScenarioJsonPath));
		return false;
	}

	if (!EnsureProjectDefaultsForScenario(normalizedScenarioJsonPath))
	{
		return false;
	}

	const FString projectRoot = ResolveUserProjectRootFromScenarioPath(normalizedScenarioJsonPath);
	FString runId;
	TArray<FString> diagnostics;
	if (!subsystem->PrepareProjectRunSnapshot(projectRoot, requestedRunId, runId, diagnostics))
	{
		SetDiagnosticsText(JoinStringLines(diagnostics));
		return false;
	}

	if (!subsystem->StartProjectRun(projectRoot, runId))
	{
		SetDiagnosticsText(subsystem->GetLastError());
		return false;
	}

	if (RunIdTextBox)
	{
		RunIdTextBox->SetText(FText::FromString(runId));
	}
	SetSelectedProjectScenarioPath(normalizedScenarioJsonPath);
	SetDiagnosticsText(FString::Printf(TEXT("Project run launch requested: %s / %s"), *projectRoot, *runId));
	return true;
}

TSubclassOf<UFileListItemWidget> UMainMenuWidget::ResolveFileListItemWidgetClass() const
{
	if (FileListItemWidgetClass)
	{
		return FileListItemWidgetClass;
	}

	UClass* loadedClass = LoadClass<UFileListItemWidget>(nullptr, FileListItemWidgetBlueprintClassPath);
	if (!loadedClass)
	{
		UE_LOG(
			LogMainMenuWidget,
			Error,
			TEXT("File list item widget class is missing: %s"),
			FileListItemWidgetBlueprintClassPath);
		ensureMsgf(false, TEXT("File list item widget class is missing."));
		return nullptr;
	}

	return TSubclassOf<UFileListItemWidget>(loadedClass);
}

void UMainMenuWidget::HandleRunInfoChanged(const FSimulatorRunInfo& runInfo)
{
	(void)runInfo;
	UpdateStatusText();
	if (bExperimentResultDetailVisible)
	{
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
	if (!AiAnalysisTextBlock)
	{
		return;
	}

	if (response.bSuccess)
	{
		AiAnalysisTextBlock->SetText(FText::FromString(response.DisplayText));
		return;
	}

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

	AiAnalysisTextBlock->SetText(FText::FromString(JoinStringLines(lines)));
}

void UMainMenuWidget::UpdateStatusText(const FString& extraMessage)
{
	if (!StatusTextBlock) return;
	
	TArray<FString> lines;
	if (!extraMessage.IsEmpty())
	{
		lines.Add(extraMessage);
	}

	if (USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem())
	{
		const FSimulationSetupParseResult setupParseResult = subsystem->LoadSimulationSetupFile(GetSelectedSetupPath());
		if (setupParseResult.bSuccess)
		{
			lines.Add(FString::Printf(TEXT("Setup: %s"), *GetSelectedSetupPath()));
			lines.Add(FString::Printf(TEXT("Fixed-Step FPS: %d"), setupParseResult.Setup.FixedStep.Fps));
		}

		const FSimulatorRunInfo runInfo = subsystem->GetActiveRunInfo();
		if (!runInfo.RunId.IsEmpty())
		{
			lines.Add(TEXT(""));
			lines.Add(TEXT("Current Simulator Process"));
			lines.Add(FString::Printf(TEXT("Run Id: %s"), *runInfo.RunId));
			lines.Add(FString::Printf(TEXT("State: %s"), *ToRunStateString(runInfo.Status.State)));
			lines.Add(FString::Printf(TEXT("Progress: %d / %d"), runInfo.Status.CompletedRuns, runInfo.Status.TotalRuns));
			lines.Add(FString::Printf(TEXT("Process: %s"), runInfo.bProcessRunning ? TEXT("Running") : TEXT("Stopped")));
			lines.Add(FString::Printf(TEXT("Launcher: %s"), runInfo.bUsedPreviewLauncher ? TEXT("Task-RunPreview.bat") : TEXT("Executable")));
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

		const TArray<FString> statusFiles = subsystem->ListSimulationRunStatusFiles();
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
	CurrentPreviewProjectPath.Reset();
	CurrentPreviewRunId.Reset();
	CurrentPreviewSummaryPath.Reset();
	CurrentPreviewResultPath.Reset();
	CurrentPreviewEventsPath.Reset();

	const bool bHasSelectedRun = !SelectedExperimentResultProjectPath.IsEmpty()
		&& !SelectedExperimentResultRunId.IsEmpty();
	FUserProjectRunSnapshotPaths paths;
	if (bHasSelectedRun)
	{
		paths = FUserProjectRunSnapshot::BuildPaths(
			SelectedExperimentResultProjectPath,
			SelectedExperimentResultRunId);
		CurrentPreviewProjectPath = paths.ProjectPath;
		CurrentPreviewRunId = paths.RunId;
		CurrentPreviewSummaryPath = paths.SummaryPath;
		CurrentPreviewResultPath = SelectedExperimentResultPath;
		if (!SelectedExperimentResultEpisodeId.IsEmpty())
		{
			CurrentPreviewEventsPath = FPaths::Combine(
				FUserProjectRunOutputJson::BuildEpisodeDirectory(paths, SelectedExperimentResultEpisodeId),
				TEXT("events.jsonl"));
		}
	}

	if (ReportTextBlock)
	{
		TArray<FString> reportLines;
		if (!bHasSelectedRun)
		{
			reportLines.Add(TEXT("Project Run: <none>"));
		}
		else
		{
			reportLines.Add(FString::Printf(TEXT("Project: %s"), *paths.ProjectPath));
			reportLines.Add(FString::Printf(TEXT("Run: %s"), *paths.RunId));
			reportLines.Add(FString::Printf(TEXT("Run Directory: %s"), *paths.RunPath));
			reportLines.Add(TEXT(""));

			FString summaryJson;
			if (FFileHelper::LoadFileToString(summaryJson, *paths.SummaryPath))
			{
				reportLines.Add(FString::Printf(TEXT("Summary: %s"), *paths.SummaryPath));
				reportLines.Add(TruncatePreview(summaryJson, ReportPreviewCharacterLimit));
			}
			else
			{
				reportLines.Add(FString::Printf(TEXT("Summary missing: %s"), *paths.SummaryPath));
			}

			if (!SelectedExperimentResultPath.IsEmpty())
			{
				FString resultJson;
				reportLines.Add(TEXT(""));
				if (FFileHelper::LoadFileToString(resultJson, *SelectedExperimentResultPath))
				{
					reportLines.Add(FString::Printf(TEXT("Episode Result: %s"), *SelectedExperimentResultPath));
					reportLines.Add(TruncatePreview(resultJson, ReportPreviewCharacterLimit));
				}
				else
				{
					reportLines.Add(FString::Printf(TEXT("Episode result missing: %s"), *SelectedExperimentResultPath));
				}
			}
			else
			{
				reportLines.Add(TEXT(""));
				reportLines.Add(TEXT("Episode Result: <none>"));
			}
		}

		ReportTextBlock->SetText(FText::FromString(JoinStringLines(reportLines)));
	}

	if (LogPreviewTextBlock)
	{
		TArray<FString> logLines;
		logLines.Add(TEXT("Episode Artifacts"));
		if (!bHasSelectedRun)
		{
			logLines.Add(TEXT("No project run selected."));
		}
		else if (SelectedExperimentResultEpisodeId.IsEmpty())
		{
			logLines.Add(TEXT("No completed episode result selected."));
		}
		else
		{
			const FString episodeDirectory = FUserProjectRunOutputJson::BuildEpisodeDirectory(paths, SelectedExperimentResultEpisodeId);
			const FString scenarioPath = FPaths::Combine(episodeDirectory, TEXT("scenario.json"));
			const FString resultPath = FPaths::Combine(episodeDirectory, TEXT("result.json"));
			const FString actionsPath = FPaths::Combine(episodeDirectory, TEXT("actions.jsonl"));
			const FString eventsPath = FPaths::Combine(episodeDirectory, TEXT("events.jsonl"));
			const FString tracePath = FPaths::Combine(episodeDirectory, TEXT("trace.jsonl"));

			logLines.Add(FString::Printf(TEXT("Episode: %s"), *SelectedExperimentResultEpisodeId));
			logLines.Add(FString::Printf(TEXT("- scenario.json: %s"), *scenarioPath));
			logLines.Add(FString::Printf(TEXT("- result.json: %s"), *resultPath));
			logLines.Add(FString::Printf(TEXT("- actions.jsonl: %s"), *actionsPath));
			logLines.Add(FString::Printf(TEXT("- events.jsonl: %s"), *eventsPath));
			logLines.Add(FString::Printf(TEXT("- trace.jsonl: %s"), *tracePath));

			if (FPaths::FileExists(eventsPath))
			{
				CurrentPreviewEventsPath = eventsPath;
				logLines.Add(TEXT(""));
				logLines.Add(FString::Printf(TEXT("Events Preview: %s"), *eventsPath));
				logLines.Add(BuildLogPreview(eventsPath));
			}
			else
			{
				CurrentPreviewEventsPath.Reset();
				logLines.Add(TEXT(""));
				logLines.Add(FString::Printf(TEXT("events.jsonl missing: %s"), *eventsPath));
			}
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

	return ExperimentScenarioSetupComboBox ? ExperimentScenarioSetupComboBox->GetSelectedOption() : FString();
}

FString UMainMenuWidget::GetSelectedProjectScenarioPath() const
{
	if (!SelectedProjectScenarioPath.TrimStartAndEnd().IsEmpty())
	{
		return SelectedProjectScenarioPath;
	}

	if (ScenarioSetupPathTextBox)
	{
		const FString scenarioJsonPath = NormalizeUserProjectScenarioPath(ScenarioSetupPathTextBox->GetText().ToString());
		if (!scenarioJsonPath.IsEmpty())
		{
			return scenarioJsonPath;
		}
	}

	const FString selectedOption = ScenarioSetupComboBox ? ScenarioSetupComboBox->GetSelectedOption() : FString();
	return selectedOption.IsEmpty() ? GetDefaultUserProjectScenarioPath() : selectedOption;
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

USimulatorLaunchSubsystem* UMainMenuWidget::GetSimulatorLaunchSubsystem() const
{
	const UGameInstance* gameInstance = GetGameInstance();
	return gameInstance ? gameInstance->GetSubsystem<USimulatorLaunchSubsystem>() : nullptr;
}

UScenarioEditorLaunchSubsystem* UMainMenuWidget::GetScenarioEditorLaunchSubsystem() const
{
	const UGameInstance* gameInstance = GetGameInstance();
	return gameInstance ? gameInstance->GetSubsystem<UScenarioEditorLaunchSubsystem>() : nullptr;
}

UPlatformAnalysisAiSubsystem* UMainMenuWidget::GetPlatformAnalysisAiSubsystem() const
{
	const UGameInstance* gameInstance = GetGameInstance();
	return gameInstance ? gameInstance->GetSubsystem<UPlatformAnalysisAiSubsystem>() : nullptr;
}
