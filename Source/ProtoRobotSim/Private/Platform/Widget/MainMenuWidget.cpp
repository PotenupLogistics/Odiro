#include "Platform/Widget/MainMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Misc/FileHelper.h"
#include "Platform/SimulatorLaunchSubsystem.h"
#include "Widgets/SWidget.h"

namespace
{
	const int32 ReportPreviewCharacterLimit = 4000;
	const int32 LogPreviewEdgeLineCount = 5;

	UTextBlock* MakeTextBlock(UWidgetTree* widgetTree, const FName name, const FString& text, const int32 fontSize = 16)
	{
		UTextBlock* textBlock = widgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), name);
		textBlock->SetText(FText::FromString(text));
		textBlock->SetAutoWrapText(true);

		FSlateFontInfo fontInfo = textBlock->GetFont();
		fontInfo.Size = fontSize;
		textBlock->SetFont(fontInfo);
		return textBlock;
	}

	UButton* MakeButton(UWidgetTree* widgetTree, const FName name, const FString& label)
	{
		UButton* button = widgetTree->ConstructWidget<UButton>(UButton::StaticClass(), name);
		UTextBlock* buttonLabel = MakeTextBlock(widgetTree, FName(*(name.ToString() + TEXT("Label"))), label, 14);
		button->AddChild(buttonLabel);
		return button;
	}

	void AddRootChild(UVerticalBox* rootBox, UWidget* widget, const float padding = 6.0f)
	{
		if (UVerticalBoxSlot* slot = rootBox->AddChildToVerticalBox(widget))
		{
			slot->SetPadding(FMargin(padding));
		}
	}

	void AddRowChild(UHorizontalBox* rowBox, UWidget* widget, const float padding = 4.0f)
	{
		if (UHorizontalBoxSlot* slot = rowBox->AddChildToHorizontalBox(widget))
		{
			slot->SetPadding(FMargin(padding));
		}
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

	FString JoinLines(const TArray<FString>& lines)
	{
		return FString::Join(lines, TEXT("\n"));
	}
}

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildWidgetTreeIfNeeded();
	BindControls();
	RefreshSetupOptions();

	if (USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem())
	{
		subsystem->OnRunInfoChanged.RemoveAll(this);
		subsystem->OnRunInfoChanged.AddUObject(this, &UMainMenuWidget::HandleRunInfoChanged);
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

TSharedRef<SWidget> UMainMenuWidget::RebuildWidget()
{
	BuildWidgetTreeIfNeeded();
	SetIsFocusable(true);
	return Super::RebuildWidget();
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

	Super::NativeDestruct();
}

void UMainMenuWidget::RefreshSetupOptions()
{
	USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem();
	if (!subsystem || !SetupComboBox || !SetupPathTextBox)
	{
		return;
	}

	const FString currentPath = GetSelectedSetupPath();
	SetupComboBox->ClearOptions();

	const TArray<FString> setupFiles = subsystem->ListSimulationSetupFiles();
	for (const FString& setupFile : setupFiles)
	{
		SetupComboBox->AddOption(setupFile);
	}

	const FString selectedPath = setupFiles.Contains(currentPath)
		? currentPath
		: (setupFiles.IsEmpty() ? FString() : setupFiles[0]);
	if (!selectedPath.IsEmpty())
	{
		SetupComboBox->SetSelectedOption(selectedPath);
		SetupPathTextBox->SetText(FText::FromString(selectedPath));
		LoadSelectedSetup();
	}
	else
	{
		SetDiagnosticsText(TEXT("No SimulationSetup JSON found in Json/Input."));
	}
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

	if (SetupPathTextBox)
	{
		SetupPathTextBox->SetText(FText::FromString(selectedItem));
	}

	LoadSelectedSetup();
}

void UMainMenuWidget::HandleLoadClicked()
{
	LoadSelectedSetup();
}

void UMainMenuWidget::HandleSaveFpsClicked()
{
	USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem();
	if (!subsystem || !FixedStepFpsTextBox)
	{
		return;
	}

	const int32 fps = FCString::Atoi(*FixedStepFpsTextBox->GetText().ToString());
	TArray<FString> diagnostics;
	if (subsystem->SaveFixedStepFpsToSetupFile(GetSelectedSetupPath(), fps, diagnostics))
	{
		UpdateStatusText(FString::Printf(TEXT("Saved fixed_step.fps=%d."), fps));
		return;
	}

	SetDiagnosticsText(JoinLines(diagnostics));
}

void UMainMenuWidget::HandleStartClicked()
{
	USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem();
	if (!subsystem || !RunIdTextBox)
	{
		return;
	}

	const FString requestedRunId = RunIdTextBox->GetText().ToString().TrimStartAndEnd();
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

void UMainMenuWidget::BuildWidgetTreeIfNeeded()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	// T05 범위에서는 asset/Blueprint 수동 작업 없이 검증 가능한 최소 C++ UI를 만든다.
	UBorder* rootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MainMenuRootBorder"));
	rootBorder->SetBrushColor(FLinearColor(0.03f, 0.03f, 0.04f, 0.92f));
	rootBorder->SetPadding(FMargin(18.0f));

	UScrollBox* scrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("MainMenuScrollBox"));
	RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainMenuRootBox"));
	rootBorder->AddChild(scrollBox);
	scrollBox->AddChild(RootBox);
	WidgetTree->RootWidget = rootBorder;

	AddRootChild(RootBox, MakeTextBlock(WidgetTree, TEXT("TitleText"), TEXT("Simulation"), 24), 10.0f);

	UHorizontalBox* setupRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SetupRow"));
	AddRowChild(setupRow, MakeTextBlock(WidgetTree, TEXT("SetupLabel"), TEXT("Setup"), 14));
	SetupComboBox = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("SetupComboBox"));
	AddRowChild(setupRow, SetupComboBox);
	SetupPathTextBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("SetupPathTextBox"));
	AddRowChild(setupRow, SetupPathTextBox);
	LoadButton = MakeButton(WidgetTree, TEXT("LoadButton"), TEXT("Load"));
	AddRowChild(setupRow, LoadButton);
	AddRootChild(RootBox, setupRow);

	UHorizontalBox* runRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RunRow"));
	AddRowChild(runRow, MakeTextBlock(WidgetTree, TEXT("RunIdLabel"), TEXT("Run Id"), 14));
	RunIdTextBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("RunIdTextBox"));
	RunIdTextBox->SetHintText(FText::FromString(TEXT("auto")));
	AddRowChild(runRow, RunIdTextBox);
	AddRowChild(runRow, MakeTextBlock(WidgetTree, TEXT("FixedStepLabel"), TEXT("Fixed-Step FPS"), 14));
	FixedStepFpsTextBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("FixedStepFpsTextBox"));
	AddRowChild(runRow, FixedStepFpsTextBox);
	SaveFpsButton = MakeButton(WidgetTree, TEXT("SaveFpsButton"), TEXT("Save FPS"));
	AddRowChild(runRow, SaveFpsButton);
	StartButton = MakeButton(WidgetTree, TEXT("StartButton"), TEXT("Start Run"));
	AddRowChild(runRow, StartButton);
	RefreshButton = MakeButton(WidgetTree, TEXT("RefreshButton"), TEXT("Refresh"));
	AddRowChild(runRow, RefreshButton);
	AddRootChild(RootBox, runRow);

	StatusTextBlock = MakeTextBlock(WidgetTree, TEXT("StatusTextBlock"), TEXT("Status"), 14);
	AddRootChild(RootBox, StatusTextBlock);
	ReportTextBlock = MakeTextBlock(WidgetTree, TEXT("ReportTextBlock"), TEXT("Reports"), 14);
	AddRootChild(RootBox, ReportTextBlock);
	LogPreviewTextBlock = MakeTextBlock(WidgetTree, TEXT("LogPreviewTextBlock"), TEXT("Logs"), 14);
	AddRootChild(RootBox, LogPreviewTextBlock);
}

void UMainMenuWidget::BindControls()
{
	if (SetupComboBox)
	{
		SetupComboBox->OnSelectionChanged.RemoveDynamic(this, &UMainMenuWidget::HandleSetupSelectionChanged);
		SetupComboBox->OnSelectionChanged.AddDynamic(this, &UMainMenuWidget::HandleSetupSelectionChanged);
	}

	if (LoadButton)
	{
		LoadButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleLoadClicked);
		LoadButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleLoadClicked);
	}

	if (SaveFpsButton)
	{
		SaveFpsButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleSaveFpsClicked);
		SaveFpsButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleSaveFpsClicked);
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
}

void UMainMenuWidget::LoadSelectedSetup()
{
	USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		return;
	}

	const FSimulationSetupParseResult parseResult = subsystem->LoadSimulationSetupFile(GetSelectedSetupPath());
	if (!parseResult.bSuccess)
	{
		// Start Run 전에 setup 계약 위반을 보여줘 simulator process를 불필요하게 띄우지 않는다.
		TArray<FString> diagnostics;
		for (const FEpisodeCompileDiagnostic& diagnostic : parseResult.Diagnostics)
		{
			diagnostics.Add(FString::Printf(TEXT("%s: %s"), *diagnostic.Code, *diagnostic.Message));
		}
		SetDiagnosticsText(JoinLines(diagnostics));
		return;
	}

	if (FixedStepFpsTextBox)
	{
		FixedStepFpsTextBox->SetText(FText::AsNumber(parseResult.Setup.FixedStep.Fps));
	}

	TArray<FString> lines;
	lines.Add(FString::Printf(TEXT("Loaded setup: %s"), *GetSelectedSetupPath()));
	lines.Add(FString::Printf(TEXT("Map: %s"), *parseResult.Setup.MapId));
	lines.Add(FString::Printf(TEXT("Run queue: %s"), *parseResult.Setup.RunQueueJsonPath));
	lines.Add(FString::Printf(TEXT("Status: %s"), *parseResult.Setup.Status.OutputPath));
	lines.Add(FString::Printf(TEXT("Report directory: %s"), *parseResult.Setup.Report.OutputDirectory));
	SetDiagnosticsText(JoinLines(lines));
}

void UMainMenuWidget::HandleRunInfoChanged(const FSimulatorRunInfo& runInfo)
{
	(void)runInfo;
	UpdateStatusText();
	UpdateReportAndLogText();
}

void UMainMenuWidget::UpdateStatusText(const FString& extraMessage)
{
	if (!StatusTextBlock)
	{
		return;
	}

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
			lines.Add(FString::Printf(TEXT("Run queue: %s"), *setupParseResult.Setup.RunQueueJsonPath));
			lines.Add(FString::Printf(TEXT("Fixed-Step FPS: %d"), setupParseResult.Setup.FixedStep.Fps));
			lines.Add(FString::Printf(TEXT("Status file: %s"), *setupParseResult.Setup.Status.OutputPath));
		}

		const FSimulatorRunInfo runInfo = subsystem->GetActiveRunInfo();
		if (!runInfo.RunId.IsEmpty())
		{
			lines.Add(TEXT(""));
			lines.Add(FString::Printf(TEXT("Run Id: %s"), *runInfo.RunId));
			lines.Add(FString::Printf(TEXT("State: %s"), *ToRunStateString(runInfo.Status.State)));
			lines.Add(FString::Printf(TEXT("Progress: %d / %d"), runInfo.Status.CompletedRuns, runInfo.Status.TotalRuns));
			lines.Add(FString::Printf(TEXT("Process: %s"), runInfo.bProcessRunning ? TEXT("Running") : TEXT("Stopped")));
			lines.Add(FString::Printf(TEXT("Launcher: %s"), runInfo.bUsedPreviewLauncher ? TEXT("RunPreview.bat") : TEXT("Executable")));
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
	}

	StatusTextBlock->SetText(FText::FromString(JoinLines(lines)));
}

void UMainMenuWidget::UpdateReportAndLogText()
{
	USimulatorLaunchSubsystem* subsystem = GetSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		return;
	}

	const FSimulatorRunInfo runInfo = subsystem->GetActiveRunInfo();

	TArray<FString> reportPaths = runInfo.Status.ReportPaths;
	if (reportPaths.IsEmpty())
	{
		// Run status가 아직 report path를 주지 못한 초기 상태에서도 기존 output을 빠르게 확인한다.
		reportPaths = subsystem->ListEvaluationReportFiles();
	}

	if (ReportTextBlock)
	{
		TArray<FString> reportLines;
		reportLines.Add(TEXT("Reports"));
		for (const FString& reportPath : reportPaths)
		{
			reportLines.Add(FString::Printf(TEXT("- %s"), *reportPath));
		}

		if (!reportPaths.IsEmpty())
		{
			FString reportJson;
			const FString previewReportPath = reportPaths.Last();
			if (FFileHelper::LoadFileToString(reportJson, *FSimulationSetupJson::ResolveProjectPath(previewReportPath)))
			{
				reportLines.Add(TEXT(""));
				reportLines.Add(FString::Printf(TEXT("Preview: %s"), *previewReportPath));
				reportLines.Add(TruncatePreview(reportJson, ReportPreviewCharacterLimit));
			}
		}

		ReportTextBlock->SetText(FText::FromString(JoinLines(reportLines)));
	}

	if (LogPreviewTextBlock)
	{
		TArray<FString> logLines;
		logLines.Add(TEXT("Measurement Logs"));
		for (const FString& logPath : runInfo.Status.LogPaths)
		{
			logLines.Add(FString::Printf(TEXT("- %s"), *logPath));
		}

		if (!runInfo.Status.LogPaths.IsEmpty())
		{
			const FString previewLogPath = runInfo.Status.LogPaths.Last();
			logLines.Add(TEXT(""));
			logLines.Add(FString::Printf(TEXT("Preview: %s"), *previewLogPath));
			logLines.Add(BuildLogPreview(previewLogPath));
		}

		LogPreviewTextBlock->SetText(FText::FromString(JoinLines(logLines)));
	}
}

void UMainMenuWidget::SetDiagnosticsText(const FString& message)
{
	if (StatusTextBlock)
	{
		StatusTextBlock->SetText(FText::FromString(message));
	}
}

FString UMainMenuWidget::GetSelectedSetupPath() const
{
	if (SetupPathTextBox)
	{
		const FString setupPath = SetupPathTextBox->GetText().ToString().TrimStartAndEnd();
		if (!setupPath.IsEmpty())
		{
			return setupPath;
		}
	}

	return SetupComboBox ? SetupComboBox->GetSelectedOption() : FString();
}

USimulatorLaunchSubsystem* UMainMenuWidget::GetSimulatorLaunchSubsystem() const
{
	const UGameInstance* gameInstance = GetGameInstance();
	return gameInstance ? gameInstance->GetSubsystem<USimulatorLaunchSubsystem>() : nullptr;
}
