
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
#include "Shared/ExperimentSettingTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogMainMenuWidget, Log, All);

namespace
{
	const int32 ResultPreviewCharacterLimit = 4000;
	const int32 LogPreviewEdgeLineCount = 5;
	const TCHAR* DefaultExperimentRef = TEXT("Json/Experiments/FeatureProbeNoPedestrians");
	const TCHAR* MainMenuDefaultSimulationMapId = TEXT("ScenarioSimulationMap");
	const TCHAR* DefaultMeasurementOutputDirectory = TEXT("Saved/AnalysisLogs");
	const TCHAR* DefaultMeasurementFilePrefix = TEXT("MeasurementLog");
	const TCHAR* DefaultReportOutputDirectory = TEXT("Json/Output");
	const TCHAR* DefaultStatusOutputPath = TEXT("Saved/SimulationRuns/latest_status.json");
	const TCHAR* MainMenuDefaultPolicySpecJsonPath = TEXT("Json/Input/PolicySpecs/PolicySpec_DefaultDelivery.json");
	const int32 DefaultFlushIntervalTicks = 60;
	const TCHAR* ScenarioSetupTemplatePath = TEXT("Json/Input/ScenarioTemplates/FeatureProbeNoPedestrians.template.json");
	const TCHAR* DeliveryBotTemplatePath = TEXT("Json/Input/ScenarioTemplates/TemplateProfileForTest.json");
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

	struct FExperimentEpisodeResultItem
	{
		FString ResultPath;
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
		if (!FFileHelper::LoadFileToStringArray(lines, *FExperimentSettingJson::ResolveProjectPath(logPath)))
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

	bool TryReadExperimentEpisodeResultItem(const FString& resultPath, FExperimentEpisodeResultItem& outItem)
	{
		outItem = FExperimentEpisodeResultItem{};
		outItem.ResultPath = resultPath;

		FString resultJson;
		if (!FFileHelper::LoadFileToString(resultJson, *FExperimentSettingJson::ResolveProjectPath(resultPath)))
		{
			return false;
		}

		TSharedPtr<FJsonObject> rootObject;
		const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(resultJson);
		if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
		{
			return false;
		}

		FString schema;
		if (!rootObject->TryGetStringField(TEXT("schema"), schema))
		{
			return false;
		}

		if (!schema.Equals(TEXT("episode_result"), ESearchCase::CaseSensitive))
		{
			return false;
		}

		const TSharedPtr<FJsonValue> sampleValue = rootObject->TryGetField(TEXT("sample"));
		if (sampleValue.IsValid() && sampleValue->Type == EJson::Object)
		{
			FString sampleId;
			if (sampleValue->AsObject()->TryGetStringField(TEXT("sample_id"), sampleId))
			{
				outItem.RunIndex = FCString::Atoi(*sampleId);
			}
		}
		return true;
	}

	TArray<FExperimentEpisodeResultItem> BuildExperimentEpisodeResultItems(const TArray<FString>& resultPaths)
	{
		TArray<FExperimentEpisodeResultItem> items;
		items.Reserve(resultPaths.Num());
		for (const FString& resultPath : resultPaths)
		{
			FExperimentEpisodeResultItem item;
			if (TryReadExperimentEpisodeResultItem(resultPath, item))
			{
				items.Add(item);
			}
		}

		items.Sort([](const FExperimentEpisodeResultItem& left, const FExperimentEpisodeResultItem& right)
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

			return left.ResultPath < right.ResultPath;
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

		const FString resolvedSourcePath = FExperimentSettingJson::ResolveProjectPath(sourcePath);
		const FString resolvedTargetPath = FExperimentSettingJson::ResolveProjectPath(targetPath);
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
			if (!FPaths::FileExists(FExperimentSettingJson::ResolveProjectPath(relativePath)))
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
	UpdateResultAndLogText();
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
	const TArray<FString> setupFiles = subsystem->ListExperimentRefs();
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
		ApplyNewSetupDefaults(DefaultExperimentRef);
		SetDiagnosticsText(TEXT("실행 가능한 experiment folder가 없습니다."));
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

	RefreshExperimentResultList();
}

void UMainMenuWidget::RefreshFromSubsystem()
{
	if (USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem())
	{
		subsystem->RefreshActiveRunStatus();
	}

	UpdateStatusText();
	UpdateResultAndLogText();
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
		SetDiagnosticsText(TEXT("Experiment 목록으로 돌아왔습니다."));
		return;
	}

	if (SetupComboBox && SetupComboBox->GetVisibility() != ESlateVisibility::Visible)
	{
		SetupComboBox->SetVisibility(ESlateVisibility::Visible);
		if (GetSelectedSetupPath().TrimStartAndEnd().IsEmpty()
			|| !FPaths::FileExists(FExperimentSettingJson::ResolveProjectPath(GetSelectedSetupPath())))
		{
			SetDiagnosticsText(TEXT("Experiment folder를 선택한 뒤 불러오세요."));
			return;
		}
	}

	LoadSelectedSetup();
}

void UMainMenuWidget::HandleNewSetupClicked()
{
	SetDiagnosticsText(TEXT("Experiment 생성/편집 UI는 다음 slice에서 setting.json/profile override 흐름으로 교체합니다."));
}

void UMainMenuWidget::HandleSaveFpsClicked()
{
	SetDiagnosticsText(TEXT("fixed_fps는 experiment setting.json의 runtime.fixed_fps에서 관리합니다."));
}

void UMainMenuWidget::HandleSaveSetupClicked()
{
	SetDiagnosticsText(TEXT("Experiment setting 편집 UI로 실행 설정 저장 흐름을 대체 예정입니다."));
}

void UMainMenuWidget::HandleOpenEditorClicked()
{
	OpenScenarioInEditor(GetSelectedScenarioSetupPath());
}

void UMainMenuWidget::HandleNewScenarioClicked()
{
	const FString newScenarioPath = MakeUniqueInputJsonPath(TEXT("ScenarioTemplateNew"));
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

	FString moveError;
	if (!MoveProjectRelativeFile(sourcePath, targetPath, TEXT("Scenario"), moveError))
	{
		SetDiagnosticsText(moveError);
		return;
	}

	SetSelectedScenarioSetupPath(targetPath);
	RefreshSetupOptions();
	SetDiagnosticsText(FString::Printf(TEXT("시나리오 이름 변경됨: %s -> %s"), *sourcePath, *targetPath));
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

	FString moveError;
	if (!MoveProjectRelativeFile(sourcePath, targetPath, TEXT("Policy"), moveError))
	{
		SetDiagnosticsText(moveError);
		return;
	}

	SetSelectedDeliveryBotSetupPath(targetPath);
	RefreshSetupOptions();
	SetDiagnosticsText(FString::Printf(TEXT("행동 정책 이름 변경됨: %s -> %s"), *sourcePath, *targetPath));
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

	(void)requestedPath;
	SetDiagnosticsText(TEXT("Experiment folder rename은 setting/profile override UI slice에서 별도 처리합니다."));
}

void UMainMenuWidget::HandleExperimentConfigEditRequested(UFileListItemWidget* itemWidget)
{
	if (!IsValid(itemWidget)) return;

	SetExperimentConfigDetailVisible(true);
	const FString setupPath = itemWidget->GetOriginalPath();
	SetSelectedSetupPath(setupPath);
	LoadSelectedSetup();
}

void UMainMenuWidget::HandleExperimentConfigPlayRequested(UFileListItemWidget* itemWidget)
{
	if (!IsValid(itemWidget)) return;

	const FString setupPath = itemWidget->GetOriginalPath();
	SetSelectedSetupPath(setupPath);
	LoadSelectedSetup();
	HandleStartClicked();
}

void UMainMenuWidget::HandleExperimentResultDetailsRequested(UFileListItemWidget* itemWidget)
{
	if (!IsValid(itemWidget)) return;

	SetSelectedExperimentResultRunDirectory(itemWidget->GetOriginalPath());
	RefreshExperimentResultIterationList();
	UpdateResultAndLogText();
	SetExperimentResultDetailVisible(true);
}

void UMainMenuWidget::HandleExperimentResultIterationButtonClicked(UExperimentResultIterationButton* buttonWidget)
{
	if (!IsValid(buttonWidget)) return;

	SetSelectedExperimentResultPath(buttonWidget->GetResultPath());
	RefreshExperimentResultIterationList();
	UpdateResultAndLogText();
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

	const FString resolvedPolicyPath = FExperimentSettingJson::ResolveProjectPath(policyPath);
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
	const FString templatePath = FExperimentSettingJson::ResolveProjectPath(DeliveryBotTemplatePath);
	FString templateJson;
	if (!FFileHelper::LoadFileToString(templateJson, *templatePath))
	{
		SetDiagnosticsText(FString::Printf(TEXT("행동 정책 템플릿 읽기 실패: %s"), DeliveryBotTemplatePath));
		return;
	}

	const FString newPolicyPath = MakeUniqueInputJsonPath(TEXT("SimulationProfileNew"));
	const FString resolvedNewPolicyPath = FExperimentSettingJson::ResolveProjectPath(newPolicyPath);
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

	const FString requestedRunId = RunIdTextBox ? RunIdTextBox->GetText().ToString().TrimStartAndEnd() : FString();
	if (subsystem->StartExperimentRun(GetSelectedSetupPath(), requestedRunId))
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
	if (CurrentPreviewResultPath.IsEmpty())
	{
		if (AiAnalysisTextBlock)
		{
			AiAnalysisTextBlock->SetText(FText::FromString(TEXT("AI analysis unavailable: no episode result selected.")));
		}
		return;
	}

	if (AiAnalysisTextBlock)
	{
		AiAnalysisTextBlock->SetText(FText::FromString(TEXT("AI analysis still requires a legacy evaluation report. episode_result support is pending.")));
	}
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
	SetDiagnosticsText(TEXT("Experiment 목록으로 돌아왔습니다."));
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

	const FExperimentSettingParseResult experimentResult = subsystem->LoadExperimentSettingFile(GetSelectedSetupPath());
	if (!experimentResult.bSuccess)
	{
		TArray<FString> diagnostics;
		for (const FScenarioSchemaDiagnostic& diagnostic : experimentResult.Diagnostics)
		{
			diagnostics.Add(FString::Printf(TEXT("%s: %s"), *diagnostic.Code, *diagnostic.Message));
		}
		SetDiagnosticsText(diagnostics.IsEmpty() ? TEXT("experiment_setting을 읽을 수 없습니다.") : JoinStringLines(diagnostics));
		return;
	}

	if (FixedStepFpsTextBox)
	{
		FixedStepFpsTextBox->SetText(FText::AsNumber(experimentResult.Document.Runtime.FixedFps));
	}
	if (MapIdTextBox)
	{
		MapIdTextBox->SetText(FText::FromString(experimentResult.Document.Runtime.MapId));
	}
	if (RunCountTextBox)
	{
		RunCountTextBox->SetText(FText::AsNumber(experimentResult.Document.Sampling.SampleCount));
	}

	SetSelectedScenarioSetupPath(experimentResult.Document.Sampling.ScenarioTemplateRef);
	SetSelectedDeliveryBotSetupPath(experimentResult.Document.Sampling.ProfileTemplateRef);

	TArray<FString> experimentLines;
	experimentLines.Add(FString::Printf(TEXT("Experiment: %s"), *GetSelectedSetupPath()));
	experimentLines.Add(FString::Printf(TEXT("setting.json: %s"), *FExperimentSettingJson::BuildExperimentSettingPath(GetSelectedSetupPath())));
	experimentLines.Add(FString::Printf(TEXT("Scenario Template: %s"), *experimentResult.Document.Sampling.ScenarioTemplateRef));
	experimentLines.Add(FString::Printf(TEXT("Profile Template: %s"), *experimentResult.Document.Sampling.ProfileTemplateRef));
	experimentLines.Add(FString::Printf(TEXT("Samples: %d"), experimentResult.Document.Sampling.SampleCount));
	experimentLines.Add(FString::Printf(TEXT("Map: %s"), *experimentResult.Document.Runtime.MapId));
	experimentLines.Add(FString::Printf(TEXT("Fixed FPS: %d"), experimentResult.Document.Runtime.FixedFps));
	SetDiagnosticsText(JoinStringLines(experimentLines));
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

	const TArray<FString> setupFiles = subsystem->ListExperimentRefs();
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
		AddEmptyListMessage(WidgetTree, ExperimentConfigListScrollBox, TEXT("실행 가능한 experiment folder가 없습니다."));
		return;
	}

	for (const FString& setupFile : setupFiles)
	{
		UFileListItemWidget* itemWidget = CreateWidget<UFileListItemWidget>(this, itemWidgetClass);
		if (!itemWidget) continue;

		itemWidget->InitializeDisplayItem(setupFile, FPaths::GetBaseFilename(setupFile), TEXT("열기"), TEXT("실행"), false, true, true);
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

	TArray<FString> resultPaths = subsystem->ListEpisodeResultFilesInDirectory(SelectedExperimentResultRunDirectory);
	const FSimulatorRunInfo runInfo = subsystem->GetActiveRunInfo();
	if (FPaths::GetPath(runInfo.StatusPath).Equals(SelectedExperimentResultRunDirectory, ESearchCase::IgnoreCase))
	{
		for (const FString& resultPath : runInfo.Status.ResultPaths)
		{
			resultPaths.AddUnique(resultPath);
		}
	}

	const TArray<FExperimentEpisodeResultItem> resultItems = BuildExperimentEpisodeResultItems(resultPaths);
	if (resultItems.IsEmpty())
	{
		SetSelectedExperimentResultPath(FString());
		return;
	}

	bool bSelectedResultStillExists = false;
	for (const FExperimentEpisodeResultItem& resultItem : resultItems)
	{
		if (resultItem.ResultPath.Equals(SelectedExperimentResultPath, ESearchCase::IgnoreCase))
		{
			bSelectedResultStillExists = true;
			break;
		}
	}
	if (!bSelectedResultStillExists)
	{
		SetSelectedExperimentResultPath(resultItems[0].ResultPath);
	}

	for (const FExperimentEpisodeResultItem& resultItem : resultItems)
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
			resultItem.RunIndex == INDEX_NONE ? FString(TEXT("?")) : FString::FromInt(resultItem.RunIndex),
			18);

		if (!sizeBox || !button || !label) continue;

		const bool bSelected = resultItem.ResultPath.Equals(SelectedExperimentResultPath, ESearchCase::IgnoreCase);
		button->Configure(resultItem.ResultPath, resultItem.RunIndex);
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

void UMainMenuWidget::SetSelectedExperimentResultPath(const FString& resultPath)
{
	SelectedExperimentResultPath = resultPath.TrimStartAndEnd();
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
	const FString resolvedTemplatePath = FExperimentSettingJson::ResolveProjectPath(ScenarioSetupTemplatePath);
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

	const FString resolvedScenarioSetupPath = FExperimentSettingJson::ResolveProjectPath(normalizedScenarioSetupPath);
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

	const FString normalizedScenarioSetupPath = NormalizeInputJsonPath(scenarioSetupPath);
	if (normalizedScenarioSetupPath.TrimStartAndEnd().IsEmpty())
	{
		SetDiagnosticsText(TEXT("ScenarioSetup 파일이 선택되지 않았습니다."));
		return false;
	}
	if (!IsEditableInputJsonPath(normalizedScenarioSetupPath))
	{
		SetDiagnosticsText(TEXT("Json/Input 아래의 편집 가능한 ScenarioSetup JSON을 선택하세요."));
		return false;
	}

	if (!subsystem->OpenScenarioEditor(normalizedScenarioSetupPath))
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
	UpdateResultAndLogText();
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
		lines.Add(TruncatePreview(response.ResponseBody, ResultPreviewCharacterLimit));
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
		const FExperimentSettingParseResult setupParseResult = subsystem->LoadExperimentSettingFile(GetSelectedSetupPath());
		if (setupParseResult.bSuccess)
		{
			lines.Add(FString::Printf(TEXT("Experiment: %s"), *GetSelectedSetupPath()));
			lines.Add(FString::Printf(TEXT("Fixed-Step FPS: %d"), setupParseResult.Document.Runtime.FixedFps));
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

void UMainMenuWidget::UpdateResultAndLogText()
{
	USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		return;
	}

	const FSimulatorRunInfo runInfo = subsystem->GetActiveRunInfo();

	CurrentPreviewResultPath = SelectedExperimentResultPath;
	CurrentPreviewLogPath.Reset();

	if (ReportTextBlock)
	{
		TArray<FString> resultLines;
		resultLines.Add(SelectedExperimentResultRunDirectory.IsEmpty()
			? TEXT("Experiment Run: <none>")
			: FString::Printf(TEXT("Experiment Run: %s"), *SelectedExperimentResultRunDirectory));

		if (!SelectedExperimentResultPath.IsEmpty())
		{
			FString resultJson;
			if (FFileHelper::LoadFileToString(resultJson, *FExperimentSettingJson::ResolveProjectPath(SelectedExperimentResultPath)))
			{
				resultLines.Add(TEXT(""));
				resultLines.Add(FString::Printf(TEXT("Episode Result: %s"), *SelectedExperimentResultPath));
				resultLines.Add(TruncatePreview(resultJson, ResultPreviewCharacterLimit));
			}
		}

		ReportTextBlock->SetText(FText::FromString(JoinStringLines(resultLines)));
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
