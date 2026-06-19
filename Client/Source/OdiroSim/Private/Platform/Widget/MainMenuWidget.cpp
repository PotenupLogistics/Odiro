
#include "Platform/Widget/MainMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Platform/ProjectSessionSubsystem.h"
#include "Platform/ScenarioEditorLaunchSubsystem.h"
#include "Platform/PlatformAnalysisAiSubsystem.h"
#include "Platform/SimulatorLaunchSubsystem.h"
#include "Platform/Widget/ExperimentResultIterationButton.h"
#include "Platform/Widget/FileListItemWidget.h"
#include "Scenario/Editor/ScenarioEditorController.h"
#include "Scenario/Widget/ScenarioEditorRootWidget.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

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
	const TCHAR* FileListItemWidgetBlueprintClassPath =
		TEXT("/Game/Widgets/MainMenu/WBP_FileListItem.WBP_FileListItem_C");
	const TCHAR* MainMenuUserProjectSettingFileName = TEXT("setting.json");
	const TCHAR* MainMenuUserProjectScenarioFileName = TEXT("scenario.json");
	const TCHAR* MainMenuRegularFontPath =
		TEXT("/Game/Fonts/Freesentation/Freesentation-4Regular_Font.Freesentation-4Regular_Font");
	const TCHAR* MainMenuBoldFontPath =
		TEXT("/Game/Fonts/Freesentation/Freesentation-7Bold_Font.Freesentation-7Bold_Font");

	enum class EMainMenuSection : int32
	{
		Scenario = 0,
		Policy,
		ExperimentConfig,
		RunStatus,
		ExperimentResult,
	};

	enum class EProjectWorkspaceTab : int32
	{
		ScenarioEdit = 0,
		ExperimentStatus,
		ExperimentResultDetail,
	};

	struct FExperimentResultReportItem
	{
		FString ReportPath;
		int32 RunIndex = INDEX_NONE;
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

	FLinearColor MakeSrgbColor(const uint8 red, const uint8 green, const uint8 blue, const float alpha = 1.0f)
	{
		const uint8 alphaByte = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(alpha * 255.0f), 0, 255));
		return FLinearColor::FromSRGBColor(FColor(red, green, blue, alphaByte));
	}

	UObject* ResolveMainMenuFontObject(const bool bBold)
	{
		return LoadObject<UObject>(nullptr, bBold ? MainMenuBoldFontPath : MainMenuRegularFontPath);
	}

	FSlateFontInfo MakeMainMenuFont(const int32 fontSize, const bool bBold = false)
	{
		FSlateFontInfo fontInfo;
		fontInfo.FontObject = ResolveMainMenuFontObject(bBold);
		fontInfo.Size = fontSize;
		return fontInfo;
	}

	void ApplyTextBlockStyle(UTextBlock* textBlock, const int32 fontSize, const bool bBold, const FLinearColor& color)
	{
		if (!textBlock) return;

		textBlock->SetFont(MakeMainMenuFont(fontSize, bBold));
		textBlock->SetColorAndOpacity(FSlateColor(color));
	}

	UTextBlock* MakeTextBlock(UWidgetTree* widgetTree, const FName name, const FString& text, const int32 fontSize = 16)
	{
		UTextBlock* textBlock = MakeWidget<UTextBlock>(widgetTree, name);
		textBlock->SetText(FText::FromString(text));
		textBlock->SetAutoWrapText(true);
		ApplyTextBlockStyle(textBlock, fontSize, false, MakeSrgbColor(0xc0, 0xc0, 0xc0));
		return textBlock;
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

	FString BuildProjectSettingPath(const FString& projectPath)
	{
		return NormalizeMainMenuPath(FPaths::Combine(projectPath, MainMenuUserProjectSettingFileName));
	}

	bool TryLoadJsonRootObject(const FString& jsonFilePath, TSharedPtr<FJsonObject>& outRootObject, FString& outError)
	{
		outRootObject.Reset();
		outError.Reset();

		FString jsonString;
		if (!FFileHelper::LoadFileToString(jsonString, *jsonFilePath))
		{
			outError = FString::Printf(TEXT("setting.json 읽기 실패: %s"), *jsonFilePath);
			return false;
		}

		const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(jsonString);
		if (!FJsonSerializer::Deserialize(reader, outRootObject) || !outRootObject.IsValid())
		{
			outError = FString::Printf(TEXT("setting.json 파싱 실패: %s"), *jsonFilePath);
			return false;
		}

		return true;
	}

	TSharedRef<FJsonObject> FindOrCreateObjectField(const TSharedRef<FJsonObject>& rootObject, const FString& fieldName)
	{
		const TSharedPtr<FJsonObject>* existingObject = nullptr;
		if (rootObject->TryGetObjectField(fieldName, existingObject) && existingObject && existingObject->IsValid())
		{
			return existingObject->ToSharedRef();
		}

		TSharedRef<FJsonObject> newObject = MakeShared<FJsonObject>();
		rootObject->SetObjectField(fieldName, newObject);
		return newObject;
	}

	FString ReadJsonStringFieldOrDefault(const FJsonObject& object, const FString& fieldName, const FString& defaultValue)
	{
		FString value;
		return object.TryGetStringField(fieldName, value) ? value : defaultValue;
	}

	int32 ReadJsonIntFieldOrDefault(const FJsonObject& object, const FString& fieldName, const int32 defaultValue)
	{
		double value = 0.0;
		return object.TryGetNumberField(fieldName, value) ? FMath::RoundToInt(value) : defaultValue;
	}

	int64 ReadJsonInt64FieldOrDefault(const FJsonObject& object, const FString& fieldName, const int64 defaultValue)
	{
		double value = 0.0;
		return object.TryGetNumberField(fieldName, value) ? static_cast<int64>(value) : defaultValue;
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

	FString ToProjectRunStatusLabel(const ESimulationRunState state)
	{
		switch (state)
		{
		case ESimulationRunState::Running:
			return TEXT("실행중");
		case ESimulationRunState::Completed:
			return TEXT("완료");
		case ESimulationRunState::Failed:
			return TEXT("실패");
		case ESimulationRunState::Canceled:
			return TEXT("취소");
		case ESimulationRunState::Pending:
		default:
			return TEXT("대기");
		}
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
		outState = ESimulationRunState::Pending;
		if (!FPaths::FileExists(statusPath))
		{
			return false;
		}

		FString statusJson;
		if (!FFileHelper::LoadFileToString(statusJson, *statusPath))
		{
			return false;
		}

		TSharedPtr<FJsonObject> rootObject;
		const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(statusJson);
		if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
		{
			return false;
		}

		FString schema;
		if (!rootObject->TryGetStringField(TEXT("schema"), schema)
			|| !schema.Equals(TEXT("run_status"), ESearchCase::CaseSensitive))
		{
			return false;
		}

		FString stateText;
		if (!rootObject->TryGetStringField(TEXT("state"), stateText))
		{
			return false;
		}

		stateText = stateText.TrimStartAndEnd().ToLower();
		if (stateText == TEXT("starting")
			|| stateText == TEXT("running")
			|| stateText == TEXT("stopping"))
		{
			outState = ESimulationRunState::Running;
			return true;
		}

		if (stateText == TEXT("exited") || stateText == TEXT("completed"))
		{
			outState = ESimulationRunState::Completed;
			return true;
		}

		if (stateText == TEXT("failed"))
		{
			outState = ESimulationRunState::Failed;
			return true;
		}

		if (stateText == TEXT("canceled") || stateText == TEXT("cancelled"))
		{
			outState = ESimulationRunState::Canceled;
			return true;
		}

		return false;
	}

	bool TryReadProjectRunStatusState(
		const FString& resultDirectory,
		ESimulationRunState& outState)
	{
		outState = ESimulationRunState::Pending;
		const FString statusPath = BuildProjectRunStatusPath(resultDirectory);
		if (!FPaths::FileExists(statusPath))
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
		if (IsActiveProjectRunDirectory(resultDirectory, runInfo, selectedProjectPath))
		{
			return true;
		}

		return FPaths::FileExists(BuildProjectRunStatusPath(resultDirectory))
			|| FPaths::FileExists(BuildProjectRunSummaryPath(resultDirectory));
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

		if (FPaths::FileExists(BuildProjectRunSummaryPath(resultDirectory)))
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

	bool TryReadExperimentResultReportItem(const FString& reportPath, FExperimentResultReportItem& outItem)
	{
		outItem = FExperimentResultReportItem{};
		outItem.ReportPath = reportPath;

		FString reportJson;
		if (!FFileHelper::LoadFileToString(reportJson, *FSimulationSetupJson::ResolveProjectPath(reportPath)))
		{
			return false;
		}

		TSharedPtr<FJsonObject> rootObject;
		const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(reportJson);
		if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
		{
			return false;
		}

		FString schema;
		if (!rootObject->TryGetStringField(TEXT("schema"), schema)
			|| !schema.Equals(TEXT("episode_evaluation_report"), ESearchCase::CaseSensitive))
		{
			return false;
		}

		const TSharedPtr<FJsonValue> runValue = rootObject->TryGetField(TEXT("run"));
		if (!runValue.IsValid() || runValue->Type != EJson::Object)
		{
			return false;
		}

		const TSharedPtr<FJsonObject> runObject = runValue->AsObject();
		if (!runObject.IsValid())
		{
			return false;
		}

		double runIndex = 0.0;
		if (runObject->TryGetNumberField(TEXT("run_index"), runIndex))
		{
			outItem.RunIndex = FMath::RoundToInt(runIndex);
		}
		return true;
	}

	TArray<FExperimentResultReportItem> BuildExperimentResultReportItems(const TArray<FString>& reportPaths)
	{
		TArray<FExperimentResultReportItem> items;
		items.Reserve(reportPaths.Num());
		for (const FString& reportPath : reportPaths)
		{
			FExperimentResultReportItem item;
			if (TryReadExperimentResultReportItem(reportPath, item))
			{
				items.Add(item);
			}
		}

		items.Sort([](const FExperimentResultReportItem& left, const FExperimentResultReportItem& right)
		{
			if (left.RunIndex != right.RunIndex)
			{
				if (left.RunIndex == INDEX_NONE)
				{
					return false;
				}
				if (right.RunIndex == INDEX_NONE)
				{
					return true;
				}
				return left.RunIndex < right.RunIndex;
			}

			return left.ReportPath < right.ReportPath;
		});
		return items;
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
		bProjectWorkspaceOpen = true;
		RefreshProjectRunSelection();
		RefreshExperimentResultList();
		ShowProjectWorkspaceScreen();
		ShowProjectWorkspaceTab(static_cast<int32>(EProjectWorkspaceTab::ScenarioEdit));
	}
	else
	{
		bProjectWorkspaceOpen = false;
		ShowProjectWorkspaceScreen();
		SetDiagnosticsText(TEXT("Active project가 없습니다. StartupMap에서 프로젝트를 선택하세요."));
	}

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
	ReleaseEditorWidgetInputMode();

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
	SetComboBoxOptions(ScenarioSetupComboBox, scenarioSetupFiles, selectedScenarioSetupPath);
	SetSelectedScenarioSetupPath(selectedScenarioSetupPath);
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

	RefreshProjectRunSelection();
	RefreshExperimentResultList();
}

void UMainMenuWidget::RefreshProjectRunSelection()
{
	USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem();
	if (!subsystem || !IsProjectModeSelected())
	{
		SetSelectedProjectRunId(FString());
		return;
	}

	TArray<FString> diagnostics;
	if (!subsystem->ValidateUserProject(GetSelectedProjectPath(), diagnostics))
	{
		SetSelectedProjectRunId(FString());
		return;
	}

	TArray<FString> runIds;
	for (const FString& runDirectory : subsystem->ListProjectRunDirectories(GetSelectedProjectPath()))
	{
		const FString runId = ExtractProjectRunIdFromDirectory(runDirectory);
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

bool UMainMenuWidget::LoadProjectExperimentSettingIntoPanel(TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();

	const FString settingPath = BuildProjectSettingPath(GetSelectedProjectPath());
	TSharedPtr<FJsonObject> rootObject;
	FString error;
	if (!TryLoadJsonRootObject(settingPath, rootObject, error))
	{
		outDiagnostics.Add(error);
		return false;
	}

	const TSharedPtr<FJsonObject>* runtimeObject = nullptr;
	const TSharedPtr<FJsonObject>* samplingObject = nullptr;
	if (!rootObject->TryGetObjectField(TEXT("runtime"), runtimeObject) || !runtimeObject || !runtimeObject->IsValid())
	{
		outDiagnostics.Add(TEXT("setting.json에 runtime object가 없습니다."));
		return false;
	}
	if (!rootObject->TryGetObjectField(TEXT("sampling"), samplingObject) || !samplingObject || !samplingObject->IsValid())
	{
		outDiagnostics.Add(TEXT("setting.json에 sampling object가 없습니다."));
		return false;
	}

	if (ProjectExperimentMapIdTextBox)
	{
		ProjectExperimentMapIdTextBox->SetText(FText::FromString(
			ReadJsonStringFieldOrDefault(*runtimeObject->Get(), TEXT("map_id"), MainMenuDefaultSimulationMapId)));
	}
	if (ProjectExperimentFixedFpsTextBox)
	{
		ProjectExperimentFixedFpsTextBox->SetText(FText::AsNumber(
			ReadJsonIntFieldOrDefault(*runtimeObject->Get(), TEXT("fixed_fps"), 60)));
	}
	if (ProjectExperimentEpisodeCountTextBox)
	{
		ProjectExperimentEpisodeCountTextBox->SetText(FText::AsNumber(
			ReadJsonIntFieldOrDefault(*samplingObject->Get(), TEXT("episode_count"), 1)));
	}
	if (ProjectExperimentBaseSeedTextBox)
	{
		ProjectExperimentBaseSeedTextBox->SetText(FText::AsNumber(
			ReadJsonInt64FieldOrDefault(*samplingObject->Get(), TEXT("base_seed"), 0)));
	}

	SetProjectExperimentConfigWarningText(FString());
	return true;
}

bool UMainMenuWidget::SaveProjectExperimentSettingFromPanel(TArray<FString>& outDiagnostics)
{
	outDiagnostics.Reset();

	const FString mapId = ProjectExperimentMapIdTextBox
		? ProjectExperimentMapIdTextBox->GetText().ToString().TrimStartAndEnd()
		: FString();
	if (mapId.IsEmpty())
	{
		outDiagnostics.Add(TEXT("Map ID를 입력하세요."));
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

	const FString settingPath = BuildProjectSettingPath(GetSelectedProjectPath());
	TSharedPtr<FJsonObject> rootObject;
	FString error;
	if (!TryLoadJsonRootObject(settingPath, rootObject, error))
	{
		outDiagnostics.Add(error);
		return false;
	}

	TSharedRef<FJsonObject> runtimeObject = FindOrCreateObjectField(rootObject.ToSharedRef(), TEXT("runtime"));
	TSharedRef<FJsonObject> samplingObject = FindOrCreateObjectField(rootObject.ToSharedRef(), TEXT("sampling"));
	runtimeObject->SetStringField(TEXT("map_id"), mapId);
	runtimeObject->SetNumberField(TEXT("fixed_fps"), fixedFps);
	samplingObject->SetNumberField(TEXT("episode_count"), episodeCount);
	samplingObject->SetNumberField(TEXT("base_seed"), static_cast<double>(baseSeed));

	FString updatedJson;
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&updatedJson);
	if (!FJsonSerializer::Serialize(rootObject.ToSharedRef(), writer))
	{
		outDiagnostics.Add(TEXT("setting.json 직렬화 실패."));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(
		updatedJson,
		*settingPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		outDiagnostics.Add(FString::Printf(TEXT("setting.json 저장 실패: %s"), *settingPath));
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
		runInput.EpisodeScenarioJsonPath = scenarioSetupPath;
		runInput.ProfileJsonPath = deliveryBotSetupPath;
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
	if (IsProjectRunDirectory(SelectedExperimentResultRunDirectory))
	{
		SetSelectedProjectRunId(ExtractProjectRunIdFromDirectory(SelectedExperimentResultRunDirectory));
	}
	RefreshExperimentResultIterationList();
	UpdateReportAndLogText();
	SetExperimentResultDetailVisible(true);
}

void UMainMenuWidget::HandleExperimentResultIterationButtonClicked(UExperimentResultIterationButton* buttonWidget)
{
	if (!IsValid(buttonWidget)) return;

	SetSelectedExperimentResultPath(buttonWidget->GetResultPath());
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
	USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem();
	if (!subsystem) return;

	if (IsProjectModeSelected())
	{
		FString runId = GetSelectedProjectRunId();
		TArray<FString> diagnostics;
		if (runId.IsEmpty() && !CreateProjectRunForPrototype(runId, diagnostics))
		{
			SetDiagnosticsText(JoinStringLines(diagnostics));
			UpdateStatusText(TEXT("Project run creation failed."));
			return;
		}

		if (subsystem->StartProjectRun(GetSelectedProjectPath(), runId))
		{
			RefreshProjectRunSelection();
			RefreshExperimentResultList();
			UpdateStatusText(TEXT("Project simulator launch requested."));
			return;
		}

		UpdateStatusText(subsystem->GetLastError());
		return;
	}

	const FString requestedRunId = RunIdTextBox ? RunIdTextBox->GetText().ToString().TrimStartAndEnd() : FString();
	if (subsystem->StartSimulationRun(GetSelectedSetupPath(), requestedRunId))
	{
		UpdateStatusText(TEXT("Simulator launch requested."));
		return;
	}

	UpdateStatusText(subsystem->GetLastError());
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
		const FString projectPath = GetSelectedProjectPath();
		const FString runId = GetSelectedProjectRunId();
		if (runId.IsEmpty())
		{
			if (AiAnalysisTextBlock)
			{
				AiAnalysisTextBlock->SetText(FText::FromString(TEXT("AI analysis unavailable: no project run selected.")));
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
			AiAnalysisTextBlock->SetText(FText::FromString(TEXT("Analyzing project run...")));
		}

		analysisSubsystem->RequestAnalysisForProjectRun(projectPath, runId);
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
	ShowProjectWorkspaceTab(static_cast<int32>(EProjectWorkspaceTab::ScenarioEdit));
}

void UMainMenuWidget::HandleShowProjectExperimentStatusTabClicked()
{
	ClearExperimentResultIterationWidgets();
	SetExperimentResultDetailVisible(false);
	RefreshExperimentResultList();
	UpdateReportAndLogText();
	ShowProjectWorkspaceTab(static_cast<int32>(EProjectWorkspaceTab::ExperimentStatus));
}

void UMainMenuWidget::HandleAddExperimentClicked()
{
	TArray<FString> diagnostics;
	if (!LoadProjectExperimentSettingIntoPanel(diagnostics))
	{
		SetDiagnosticsText(JoinStringLines(diagnostics));
		UpdateStatusText(TEXT("Experiment creation failed."));
		return;
	}

	ShowProjectExperimentConfigPanel(true);
	ShowProjectWorkspaceTab(static_cast<int32>(EProjectWorkspaceTab::ExperimentStatus));
	SetDiagnosticsText(TEXT("새 실험 설정을 수정하세요."));
	UpdateStatusText();
}

void UMainMenuWidget::HandleCreateExperimentFromConfigClicked()
{
	USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		const FString message = TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다.");
		SetProjectExperimentConfigWarningText(message);
		SetDiagnosticsText(message);
		return;
	}

	const FSimulatorRunInfo activeRunInfo = subsystem->GetActiveRunInfo();
	if (activeRunInfo.bProcessRunning && !USimulatorLaunchSubsystem::IsTerminalRunState(activeRunInfo.Status.State))
	{
		const FString message = TEXT("이미 실행 중인 실험이 있습니다.");
		SetProjectExperimentConfigWarningText(message);
		SetDiagnosticsText(message);
		UpdateStatusText(TEXT("Project simulator is already running."));
		return;
	}

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
	if (!CreateProjectRunForPrototype(runId, diagnostics, subsystem))
	{
		const FString message = JoinStringLines(diagnostics);
		SetProjectExperimentConfigWarningText(message);
		SetDiagnosticsText(message);
		UpdateStatusText(TEXT("Experiment creation failed."));
		return;
	}

	if (!subsystem->StartProjectRun(GetSelectedProjectPath(), runId))
	{
		diagnostics.Add(subsystem->GetLastError());
		const FString message = JoinStringLines(diagnostics);
		SetProjectExperimentConfigWarningText(message);
		SetDiagnosticsText(message);
		RefreshProjectRunSelection();
		RefreshExperimentResultList();
		UpdateStatusText(TEXT("Experiment launch failed."));
		return;
	}

	ShowProjectExperimentConfigPanel(false);
	RefreshProjectRunSelection();
	RefreshExperimentResultList();
	ShowProjectWorkspaceTab(static_cast<int32>(EProjectWorkspaceTab::ExperimentStatus));
	SetDiagnosticsText(FString::Printf(TEXT("실험 실행을 시작했습니다: %s"), *runId));
	UpdateStatusText(TEXT("Project simulator launch requested."));
}

void UMainMenuWidget::HandleCancelExperimentConfigClicked()
{
	SetProjectExperimentConfigWarningText(FString());
	ShowProjectExperimentConfigPanel(false);
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

void UMainMenuWidget::ShowProjectWorkspaceScreen()
{
	if (ProjectWorkspaceSwitcher)
	{
		ApplyProjectWorkspaceTabStyle(ProjectWorkspaceSwitcher->GetActiveWidgetIndex());
	}
}

void UMainMenuWidget::ShowProjectWorkspaceTab(const int32 tabIndex)
{
	if (!ProjectWorkspaceSwitcher)
	{
		return;
	}

	const int32 widgetCount = ProjectWorkspaceSwitcher->GetChildrenCount();
	if (tabIndex >= 0 && tabIndex < widgetCount)
	{
		ProjectWorkspaceSwitcher->SetActiveWidgetIndex(tabIndex);
		ApplyProjectWorkspaceTabStyle(tabIndex);
	}
}

void UMainMenuWidget::ApplyProjectWorkspaceTabStyle(const int32 activeTabIndex)
{
	const FLinearColor transparentTab = MakeSrgbColor(0x15, 0x15, 0x15, 0.0f);
	const FLinearColor hoverTab = MakeSrgbColor(0x20, 0x20, 0x20);
	const FLinearColor activeTab = MakeSrgbColor(0x24, 0x24, 0x24);
	const FLinearColor textColor = MakeSrgbColor(0xc0, 0xc0, 0xc0);
	const FLinearColor activeTextColor = MakeSrgbColor(0xff, 0xff, 0xff);

	auto applyStyle = [&transparentTab, &hoverTab, &activeTab, &textColor, &activeTextColor](UButton* button, const bool bActive)
	{
		if (!button)
		{
			return;
		}

		FButtonStyle buttonStyle = button->GetStyle();
		buttonStyle.Normal.TintColor = FSlateColor(bActive ? activeTab : transparentTab);
		buttonStyle.Hovered.TintColor = FSlateColor(bActive ? activeTab : hoverTab);
		buttonStyle.Pressed.TintColor = FSlateColor(activeTab);
		buttonStyle.Disabled.TintColor = FSlateColor(transparentTab);
		buttonStyle.SetNormalForeground(FSlateColor(bActive ? activeTextColor : textColor));
		buttonStyle.SetHoveredForeground(FSlateColor(activeTextColor));
		buttonStyle.SetPressedForeground(FSlateColor(activeTextColor));
		buttonStyle.SetDisabledForeground(FSlateColor(textColor));
		button->SetStyle(buttonStyle);
		button->SetBackgroundColor(FLinearColor::White);
		button->SetColorAndOpacity(FLinearColor::White);
	};

	const int32 scenarioTabIndex = static_cast<int32>(EProjectWorkspaceTab::ScenarioEdit);
	const int32 experimentStatusIndex = static_cast<int32>(EProjectWorkspaceTab::ExperimentStatus);
	const int32 experimentDetailIndex = static_cast<int32>(EProjectWorkspaceTab::ExperimentResultDetail);
	applyStyle(ScenarioEditTabButton, activeTabIndex == scenarioTabIndex);
	applyStyle(ExperimentStatusTabButton, activeTabIndex == experimentStatusIndex || activeTabIndex == experimentDetailIndex);
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
	requireWidget(ScenarioEditTabButton, TEXT("ScenarioEditTabButton"));
	requireWidget(ExperimentStatusTabButton, TEXT("ExperimentStatusTabButton"));
	requireWidget(AddExperimentButton, TEXT("AddExperimentButton"));
	requireWidget(ExperimentResultDetailSectionBoxScrollBox, TEXT("ExperimentResultDetailSectionBoxScrollBox"));
	requireWidget(ExperimentResultListScrollBox, TEXT("ExperimentResultListScrollBox"));
	requireWidget(ExperimentResultIterationScrollBox, TEXT("ExperimentResultIterationScrollBox"));
	requireWidget(ExperimentResultBackButton, TEXT("ExperimentResultBackButton"));
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

	BindProjectModeControls();
}

void UMainMenuWidget::BindProjectModeControls()
{
	UE_LOG(
		LogMainMenuWidget,
		Log,
		TEXT("Project mode controls bound: %s"),
		ScenarioEditTabButton && ExperimentStatusTabButton && AddExperimentButton
			? TEXT("true")
			: TEXT("false"));

	if (ScenarioEditTabButton)
	{
		ScenarioEditTabButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleShowProjectScenarioTabClicked);
		ScenarioEditTabButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleShowProjectScenarioTabClicked);
	}

	if (ExperimentStatusTabButton)
	{
		ExperimentStatusTabButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleShowProjectExperimentStatusTabClicked);
		ExperimentStatusTabButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleShowProjectExperimentStatusTabClicked);
	}

	if (AddExperimentButton)
	{
		AddExperimentButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleAddExperimentClicked);
		AddExperimentButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleAddExperimentClicked);
	}

	if (CreateExperimentConfigButton)
	{
		CreateExperimentConfigButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleCreateExperimentFromConfigClicked);
		CreateExperimentConfigButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleCreateExperimentFromConfigClicked);
	}

	if (CancelExperimentConfigButton)
	{
		CancelExperimentConfigButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleCancelExperimentConfigClicked);
		CancelExperimentConfigButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleCancelExperimentConfigClicked);
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
	bExperimentResultDetailVisible = bVisible;

	if (IsProjectModeSelected() && ProjectWorkspaceSwitcher)
	{
		ShowProjectWorkspaceScreen();
		ShowProjectWorkspaceTab(static_cast<int32>(
			bVisible ? EProjectWorkspaceTab::ExperimentResultDetail : EProjectWorkspaceTab::ExperimentStatus));
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

	USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem();
	if (!subsystem) return;

	const TSubclassOf<UFileListItemWidget> itemWidgetClass = ResolveFileListItemWidgetClass();
	if (!itemWidgetClass)
	{
		SetDiagnosticsText(TEXT("WBP_FileListItem 클래스를 사용할 수 없습니다."));
		return;
	}

	const TArray<FString> scenarioSetupFiles = subsystem->ListScenarioSetupFiles();
	if (!scenarioSetupFiles.Contains(SelectedScenarioSetupPath))
	{
		SetSelectedScenarioSetupPath(scenarioSetupFiles.IsEmpty() ? FString() : scenarioSetupFiles[0]);
	}

	ScenarioListScrollBox->ClearChildren();
	ScenarioListItems.Reset();
	ScenarioListItems.Reserve(scenarioSetupFiles.Num());

	for (const FString& scenarioSetupFile : scenarioSetupFiles)
	{
		UFileListItemWidget* itemWidget = CreateWidget<UFileListItemWidget>(this, itemWidgetClass);
		if (!itemWidget) continue;

		itemWidget->InitializeItem(scenarioSetupFile, TEXT("편집"), TEXT("실행"), true, true, false);
		itemWidget->OnRenameRequested.AddUObject(this, &UMainMenuWidget::HandleScenarioRenameRequested);
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

	USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem();
	if (!subsystem) return;

	const TSubclassOf<UFileListItemWidget> itemWidgetClass = ResolveFileListItemWidgetClass();
	if (!itemWidgetClass) return;

	if (IsProjectModeSelected())
	{
		TArray<FString> diagnostics;
		TArray<FString> resultDirectories;
		const FSimulatorRunInfo runInfo = subsystem->GetActiveRunInfo();
		if (subsystem->ValidateUserProject(GetSelectedProjectPath(), diagnostics))
		{
			resultDirectories = subsystem->ListProjectRunDirectories(GetSelectedProjectPath());
			if (runInfo.bProjectRun && runInfo.ProjectPath.Equals(GetSelectedProjectPath(), ESearchCase::IgnoreCase))
			{
				resultDirectories.AddUnique(BuildProjectRunDirectory(runInfo.ProjectPath, runInfo.RunId));
			}
		}
		resultDirectories = resultDirectories.FilterByPredicate(
			[&runInfo, this](const FString& resultDirectory)
			{
				return IsVisibleProjectRunDirectory(resultDirectory, runInfo, GetSelectedProjectPath());
			});
		resultDirectories.Sort();

		if (!resultDirectories.Contains(SelectedExperimentResultRunDirectory))
		{
			SetSelectedExperimentResultRunDirectory(resultDirectories.IsEmpty() ? FString() : resultDirectories.Last());
		}
		SetSelectedProjectRunId(ExtractProjectRunIdFromDirectory(SelectedExperimentResultRunDirectory));

		ExperimentResultListScrollBox->ClearChildren();
		ExperimentResultListItems.Reset();
		ExperimentResultListItems.Reserve(resultDirectories.Num());

		if (resultDirectories.IsEmpty())
		{
			AddEmptyListMessage(WidgetTree, ExperimentResultListScrollBox, TEXT("실행된 실험이 없습니다."));
			return;
		}

		for (const FString& resultDirectory : resultDirectories)
		{
			UFileListItemWidget* itemWidget = CreateWidget<UFileListItemWidget>(this, itemWidgetClass);
			if (!itemWidget) continue;

			const FString runId = ExtractProjectRunIdFromDirectory(resultDirectory);
			const ESimulationRunState displayState =
				ResolveProjectRunDisplayState(resultDirectory, runInfo, GetSelectedProjectPath());
			const FString statusLabel = ToProjectRunStatusLabel(displayState);
			const bool bCompleted = displayState == ESimulationRunState::Completed;
			itemWidget->InitializeDisplayItem(
				resultDirectory,
				runId.IsEmpty() ? FPaths::GetBaseFilename(resultDirectory) : runId,
				TEXT("분석"),
				TEXT("실행"),
				false,
				bCompleted,
				false);
			itemWidget->SetSecondaryActionDisplayOnly(statusLabel);
			if (bCompleted)
			{
				itemWidget->OnPrimaryActionRequested.AddUObject(this, &UMainMenuWidget::HandleExperimentResultDetailsRequested);
			}
			ExperimentResultListScrollBox->AddChild(itemWidget);
			ExperimentResultListItems.Add(itemWidget);
		}
		return;
	}

	TArray<FString> resultDirectories = subsystem->ListSimulationRunResultDirectories();
	const FSimulatorRunInfo runInfo = subsystem->GetActiveRunInfo();
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

	if (resultDirectories.IsEmpty())
	{
		AddEmptyListMessage(WidgetTree, ExperimentResultListScrollBox, TEXT("실험 결과가 없습니다."));
		return;
	}

	for (const FString& resultDirectory : resultDirectories)
	{
		UFileListItemWidget* itemWidget = CreateWidget<UFileListItemWidget>(this, itemWidgetClass);
		if (!itemWidget) continue;
		
		itemWidget->InitializeDisplayItem(
			resultDirectory,
			FPaths::GetBaseFilename(resultDirectory),
			TEXT("상세 보기"),
			TEXT("실행"),
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

	USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem();
	if (!subsystem || SelectedExperimentResultRunDirectory.IsEmpty())
	{
		return;
	}

	if (!ensureMsgf(IsValid(ExperimentResultIterationScrollBox), TEXT("Missing required WBP binding: ExperimentResultIterationScrollBox"))
		|| !ensureMsgf(IsValid(WidgetTree), TEXT("MainMenuWidget requires a valid WidgetTree.")))
	{
		return;
	}

	if (IsProjectRunDirectory(SelectedExperimentResultRunDirectory))
	{
		TArray<FString> resultPaths = subsystem->ListProjectEpisodeResultFiles(SelectedExperimentResultRunDirectory);
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

			USizeBox* sizeBox = WidgetTree->ConstructWidget<USizeBox>(
				USizeBox::StaticClass(),
				MakeUniqueWidgetName(WidgetTree, USizeBox::StaticClass(), TEXT("ProjectResultEpisodeButtonSlot")));
			UExperimentResultIterationButton* button = WidgetTree->ConstructWidget<UExperimentResultIterationButton>(
				UExperimentResultIterationButton::StaticClass(),
				MakeUniqueWidgetName(WidgetTree, UExperimentResultIterationButton::StaticClass(), TEXT("ProjectResultEpisodeButton")));
			UTextBlock* label = MakeTextBlock(
				WidgetTree,
				TEXT("ProjectResultEpisodeButtonLabel"),
				episodeIndex == INDEX_NONE ? FString(TEXT("?")) : FString::FromInt(episodeIndex),
				18);

			if (!sizeBox || !button || !label) continue;

			const bool bSelected = resultPath.Equals(SelectedExperimentResultPath, ESearchCase::IgnoreCase);
			button->Configure(resultPath, episodeId);
			button->SetBackgroundColor(bSelected
				? FLinearColor(0.16f, 0.42f, 0.78f, 1.0f)
				: FLinearColor(0.10f, 0.11f, 0.14f, 1.0f));
			button->SetColorAndOpacity(FLinearColor::White);
			button->OnIterationClicked.AddUObject(this, &UMainMenuWidget::HandleExperimentResultIterationButtonClicked);

			label->SetJustification(ETextJustify::Center);
			label->SetColorAndOpacity(FSlateColor(FLinearColor::White));

			button->SetContent(label);
			sizeBox->SetWidthOverride(44.0f);
			sizeBox->SetHeightOverride(36.0f);
			sizeBox->SetContent(button);

			if (UScrollBoxSlot* scrollBoxSlot = Cast<UScrollBoxSlot>(ExperimentResultIterationScrollBox->AddChild(sizeBox)))
			{
				scrollBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
			}
			ExperimentResultIterationButtons.Add(button);
		}
		return;
	}

	TArray<FString> reportPaths = subsystem->ListEvaluationReportFilesInDirectory(SelectedExperimentResultRunDirectory);
	const FSimulatorRunInfo runInfo = subsystem->GetActiveRunInfo();
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
		USizeBox* sizeBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			MakeUniqueWidgetName(WidgetTree, USizeBox::StaticClass(), TEXT("ExperimentResultIterationButtonSlot")));
		UExperimentResultIterationButton* button = WidgetTree->ConstructWidget<UExperimentResultIterationButton>(
			UExperimentResultIterationButton::StaticClass(),
			MakeUniqueWidgetName(WidgetTree, UExperimentResultIterationButton::StaticClass(), TEXT("ExperimentResultIterationButton")));
		UTextBlock* label = MakeTextBlock(
			WidgetTree,
			TEXT("ExperimentResultIterationButtonLabel"),
			reportItem.RunIndex == INDEX_NONE ? FString(TEXT("?")) : FString::FromInt(reportItem.RunIndex),
			18);

		if (!sizeBox || !button || !label) continue;

		const bool bSelected = reportItem.ReportPath.Equals(SelectedExperimentResultPath, ESearchCase::IgnoreCase);
		button->Configure(
			reportItem.ReportPath,
			reportItem.RunIndex == INDEX_NONE ? FString(TEXT("?")) : FString::FromInt(reportItem.RunIndex));
		button->SetBackgroundColor(bSelected
			? FLinearColor(0.16f, 0.42f, 0.78f, 1.0f)
			: FLinearColor(0.10f, 0.11f, 0.14f, 1.0f));
		button->SetColorAndOpacity(FLinearColor::White);
		button->OnIterationClicked.AddUObject(this, &UMainMenuWidget::HandleExperimentResultIterationButtonClicked);

		label->SetJustification(ETextJustify::Center);
		label->SetColorAndOpacity(FSlateColor(FLinearColor::White));

		button->SetContent(label);
		sizeBox->SetWidthOverride(44.0f);
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

bool UMainMenuWidget::CreateScenarioFileFromTemplate(const FString& scenarioSetupPath)
{
	const FString resolvedTemplatePath = FSimulationSetupJson::ResolveProjectPath(ScenarioSetupTemplatePath);
	FString templateJson;
	if (!FFileHelper::LoadFileToString(templateJson, *resolvedTemplatePath))
	{
		SetDiagnosticsText(FString::Printf(TEXT("시나리오 템플릿 읽기 실패: %s"), ScenarioSetupTemplatePath));
		return false;
	}

	const FString normalizedScenarioSetupPath = NormalizeInputJsonPath(scenarioSetupPath);
	if (!IsEditableInputJsonPath(normalizedScenarioSetupPath))
	{
		SetDiagnosticsText(TEXT("새 시나리오 경로는 편집 가능한 Json/Input/*.json 경로여야 합니다."));
		return false;
	}

	const FString resolvedScenarioSetupPath = FSimulationSetupJson::ResolveProjectPath(normalizedScenarioSetupPath);
	if (FPaths::FileExists(resolvedScenarioSetupPath))
	{
		SetDiagnosticsText(FString::Printf(TEXT("시나리오 파일이 이미 존재합니다: %s"), *normalizedScenarioSetupPath));
		return false;
	}

	const FString outputDirectory = FPaths::GetPath(resolvedScenarioSetupPath);
	if (!IFileManager::Get().MakeDirectory(*outputDirectory, true)
		|| !FFileHelper::SaveStringToFile(
			templateJson,
			*resolvedScenarioSetupPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		SetDiagnosticsText(FString::Printf(TEXT("시나리오 파일 생성 실패: %s"), *resolvedScenarioSetupPath));
		return false;
	}

	return true;
}

bool UMainMenuWidget::OpenScenarioInEditor(const FString& scenarioSetupPath)
{
	UScenarioEditorLaunchSubsystem* subsystem = GetScenarioEditorLaunchSubsystem();
	if (!subsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioEditorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}

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
	if (!bLegacyInputScenario && !FPaths::FileExists(normalizedScenarioPath))
	{
		SetDiagnosticsText(FString::Printf(TEXT("project scenario 파일이 없습니다: %s"), *normalizedScenarioPath));
		return false;
	}

	if (!subsystem->OpenScenarioEditor(normalizedScenarioPath))
	{
		SetDiagnosticsText(TEXT("ScenarioEditorMap 열기 실패."));
		return false;
	}

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
		if (IsProjectModeSelected())
		{
			const FString projectPath = GetSelectedProjectPath();
			lines.Add(FString::Printf(TEXT("Project: %s"), *projectPath));

			TArray<FString> diagnostics;
			if (subsystem->ValidateUserProject(projectPath, diagnostics))
			{
				lines.Add(TEXT("Project Status: Valid"));
				const TArray<FString> runDirectories = subsystem->ListProjectRunDirectories(projectPath);
				if (!runDirectories.IsEmpty())
				{
					lines.Add(FString::Printf(TEXT("Runs: %d"), runDirectories.Num()));
					lines.Add(FString::Printf(TEXT("Selected Run: %s"), *GetSelectedProjectRunId()));
				}
			}
			else
			{
				lines.Add(TEXT("Project Status: Invalid"));
				for (const FString& diagnostic : diagnostics)
				{
					lines.Add(FString::Printf(TEXT("Diagnostic: %s"), *diagnostic));
				}
			}

			const FSimulatorRunInfo runInfo = subsystem->GetActiveRunInfo();
			if (!runInfo.RunId.IsEmpty())
			{
				lines.Add(TEXT(""));
				lines.Add(TEXT("Current Simulator Process"));
				lines.Add(FString::Printf(TEXT("Mode: %s"), runInfo.bProjectRun ? TEXT("Project") : TEXT("Legacy")));
				lines.Add(FString::Printf(TEXT("Run Id: %s"), *runInfo.RunId));
				lines.Add(FString::Printf(TEXT("State: %s"), *ToRunStateString(runInfo.Status.State)));
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

			StatusTextBlock->SetText(FText::FromString(JoinStringLines(lines)));
			return;
		}

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
	USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		return;
	}

	const FSimulatorRunInfo runInfo = subsystem->GetActiveRunInfo();

	CurrentPreviewReportPath = SelectedExperimentResultPath;
	CurrentPreviewLogPath.Reset();

	if (IsProjectRunDirectory(SelectedExperimentResultRunDirectory))
	{
		if (ReportTextBlock)
		{
			TArray<FString> reportLines;
			reportLines.Add(FString::Printf(TEXT("Project Run: %s"), *SelectedExperimentResultRunDirectory));

			const FString summaryPath = NormalizeMainMenuPath(FPaths::Combine(SelectedExperimentResultRunDirectory, TEXT("summary.json")));
			if (FPaths::FileExists(summaryPath))
			{
				FString summaryJson;
				if (FFileHelper::LoadFileToString(summaryJson, *summaryPath))
				{
					reportLines.Add(TEXT(""));
					reportLines.Add(FString::Printf(TEXT("Summary: %s"), *summaryPath));
					reportLines.Add(TruncatePreview(summaryJson, ReportPreviewCharacterLimit));
				}
			}

			if (!SelectedExperimentResultPath.IsEmpty())
			{
				FString resultJson;
				if (FFileHelper::LoadFileToString(resultJson, *SelectedExperimentResultPath))
				{
					reportLines.Add(TEXT(""));
					reportLines.Add(FString::Printf(TEXT("Episode Result: %s"), *SelectedExperimentResultPath));
					reportLines.Add(TruncatePreview(resultJson, ReportPreviewCharacterLimit));
				}
			}

			ReportTextBlock->SetText(FText::FromString(JoinStringLines(reportLines)));
		}

		if (LogPreviewTextBlock)
		{
			TArray<FString> logLines;
			logLines.Add(TEXT("Project Run Logs"));
			TArray<FString> logPaths = subsystem->ListProjectRunLogFiles(SelectedExperimentResultRunDirectory);
			logPaths.Sort();
			CurrentPreviewLogPath = logPaths.IsEmpty() ? FString() : logPaths.Last();

			for (const FString& logPath : logPaths)
			{
				logLines.Add(FString::Printf(TEXT("- %s"), *logPath));
			}

			if (!CurrentPreviewLogPath.IsEmpty())
			{
				logLines.Add(TEXT(""));
				logLines.Add(FString::Printf(TEXT("Preview: %s"), *CurrentPreviewLogPath));
				logLines.Add(BuildLogPreview(CurrentPreviewLogPath));
			}

			LogPreviewTextBlock->SetText(FText::FromString(JoinStringLines(logLines)));
		}
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
			if (FFileHelper::LoadFileToString(reportJson, *FSimulationSetupJson::ResolveProjectPath(SelectedExperimentResultPath)))
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
			logPaths = subsystem->ListMeasurementLogFilesInDirectory(SelectedExperimentResultRunDirectory);
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

bool UMainMenuWidget::CreateProjectRunForPrototype(
	FString& outRunId,
	TArray<FString>& outDiagnostics,
	USimulatorLaunchSubsystem* simulatorLaunchSubsystem)
{
	outRunId.Reset();
	outDiagnostics.Reset();

	USimulatorLaunchSubsystem* subsystem = simulatorLaunchSubsystem ? simulatorLaunchSubsystem : GetSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		outDiagnostics.Add(TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}

	if (!subsystem->CreateProjectRun(GetSelectedProjectPath(), outRunId, outDiagnostics))
	{
		return false;
	}

	SetSelectedProjectRunId(outRunId);
	if (!simulatorLaunchSubsystem)
	{
		RefreshProjectRunSelection();
		RefreshExperimentResultIterationList();
		UpdateReportAndLogText();
	}
	return true;
}

bool UMainMenuWidget::IsProjectOpened() const
{
	const UProjectSessionSubsystem* projectSession = GetProjectSessionSubsystem();
	return bProjectWorkspaceOpen && projectSession && projectSession->HasActiveProject();
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
	if (const UProjectSessionSubsystem* projectSession = GetProjectSessionSubsystem();
		projectSession && projectSession->HasActiveProject())
	{
		return projectSession->GetActiveProjectPath();
	}

	return FString();
}

FString UMainMenuWidget::GetSelectedProjectScenarioPath() const
{
	if (const UProjectSessionSubsystem* projectSession = GetProjectSessionSubsystem();
		projectSession && projectSession->HasActiveProject())
	{
		return projectSession->GetActiveProjectScenarioPath();
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

	if (IsProjectModeSelected() && FUserProjectRunSnapshot::IsValidRunId(SelectedProjectRunId))
	{
		SetSelectedExperimentResultRunDirectory(GetSelectedProjectRunDirectory());
	}
	else if (SelectedProjectRunId.IsEmpty())
	{
		SetSelectedExperimentResultRunDirectory(FString());
	}
}

UProjectSessionSubsystem* UMainMenuWidget::GetProjectSessionSubsystem() const
{
	const UGameInstance* gameInstance = GetGameInstance();
	return gameInstance ? gameInstance->GetSubsystem<UProjectSessionSubsystem>() : nullptr;
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
