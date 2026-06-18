
#include "Scenario/Llm/ScenarioLlmPromptWidget.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/TextBlock.h"
#include "Misc/Paths.h"
#include "Platform/SimulatorLaunchSubsystem.h"
#include "Scenario/Editor/ScenarioEditorController.h"
#include "Scenario/Data/WidgetTextStyleCatalog.h"

namespace
{
	const TCHAR* ProjectScenarioFileName = TEXT("scenario.json");

	FString ResolveProjectScenarioJsonPath(FString rawPath)
	{
		rawPath.TrimStartAndEndInline();
		rawPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (rawPath.IsEmpty())
		{
			return FString();
		}

		if (FPaths::GetExtension(rawPath).IsEmpty())
		{
			rawPath = FPaths::Combine(rawPath, ProjectScenarioFileName);
		}
		if (FPaths::IsRelative(rawPath))
		{
			rawPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), rawPath);
		}
		FPaths::NormalizeFilename(rawPath);
		return rawPath;
	}

	bool IsProjectScenarioJsonPath(const FString& scenarioJsonPath)
	{
		return FPaths::GetCleanFilename(scenarioJsonPath).Equals(ProjectScenarioFileName, ESearchCase::IgnoreCase);
	}
}

void UScenarioLlmPromptWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BindControls();
}

void UScenarioLlmPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindLlmSubsystem();
	ConfigureStatusTextBlock();
	UWidgetTextStyleCatalog::ApplyMultiLineEditableTextBoxStyle(PromptTextBox.Get(), EWidgetTextStyleRole::Value);
	UWidgetTextStyleCatalog::ApplyEditableTextBoxStyle(ScenarioCountTextBox.Get(), EWidgetTextStyleRole::Value);
	UWidgetTextStyleCatalog::ApplyTextBlockStyle(StatusTextBlock.Get(), EWidgetTextStyleRole::Value);
	RequestEditorWidgetInputMode();
	SetStatusText(TEXT("대기 중."));
}
	
void UScenarioLlmPromptWidget::NativeDestruct()
{
	ReleaseEditorWidgetInputMode();
	UnbindLlmSubsystem();
	Super::NativeDestruct();
}

bool UScenarioLlmPromptWidget::GenerateFromPromptTextBox()
{
	UScenarioLlmAuthoringSubsystem* llmSubsystem = GetLlmAuthoringSubsystem();
	if (!llmSubsystem)
	{
		SetStatusText(TEXT("LLM authoring subsystem is unavailable."));
		return false;
	}

	FString prompt;
	if (!TryGetPrompt(prompt)) return false;

	int32 episodeCount = 0;
	if (!TryGetEpisodeCount(episodeCount)) return false;

	FString scenarioJsonPath;
	FString projectPath;
	if (!TryResolveCurrentProjectScenarioPath(scenarioJsonPath, projectPath)) return false;

	SetStatusText(TEXT("Requesting project scenario generation."));
	return llmSubsystem->GenerateProjectScenarioFromPrompt(prompt, scenarioJsonPath, episodeCount);
}

bool UScenarioLlmPromptWidget::LoadGeneratedScenario()
{
	AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		SetStatusText(TEXT("Owning player is not ScenarioEditorController."));
		return false;
	}

	FString scenarioJsonPath;
	FString projectPath;
	if (const UScenarioLlmAuthoringSubsystem* llmSubsystem = GetLlmAuthoringSubsystem())
	{
		const FScenarioLlmGenerationResult result = llmSubsystem->GetLatestResult();
		if (result.bSuccess && !result.ProjectScenarioJsonPath.IsEmpty())
		{
			scenarioJsonPath = ResolveProjectScenarioJsonPath(result.ProjectScenarioJsonPath);
		}
	}
	if (scenarioJsonPath.IsEmpty())
	{
		if (!TryResolveCurrentProjectScenarioPath(scenarioJsonPath, projectPath)) return false;
	}

	FString resolvedJsonFilePath;
	TArray<FString> diagnostics;
	if (!editorController->LoadProjectScenarioJsonFile(
			scenarioJsonPath,
			resolvedJsonFilePath,
			diagnostics))
	{
		SetStatusText(diagnostics.IsEmpty()
			? FString::Printf(TEXT("scenario.json load failed: %s"), *scenarioJsonPath)
			: FString::Printf(TEXT("scenario.json load failed:\n%s"), *FString::Join(diagnostics, TEXT("\n"))));
		return false;
	}

	SetStatusText(FString::Printf(TEXT("Loaded project scenario: %s"), *resolvedJsonFilePath));
	return true;
}

bool UScenarioLlmPromptWidget::RunGeneratedSimulation()
{
	USimulatorLaunchSubsystem* launchSubsystem = GetSimulatorLaunchSubsystem();
	if (!launchSubsystem)
	{
		SetStatusText(TEXT("SimulatorLaunchSubsystem is unavailable."));
		return false;
	}

	FString scenarioJsonPath;
	FString projectPath;
	if (!TrySaveCurrentProjectScenario(scenarioJsonPath, projectPath)) return false;

	FString runId;
	TArray<FString> diagnostics;
	if (!launchSubsystem->PrepareProjectRunSnapshot(projectPath, FString(), runId, diagnostics))
	{
		SetStatusText(diagnostics.IsEmpty()
			? FString::Printf(TEXT("Project run snapshot preparation failed: %s"), *projectPath)
			: FString::Printf(TEXT("Project run snapshot preparation failed:\n%s"), *FString::Join(diagnostics, TEXT("\n"))));
		return false;
	}

	if (!launchSubsystem->StartProjectRun(projectPath, runId))
	{
		SetStatusText(launchSubsystem->GetLastError());
		return false;
	}

	SetStatusText(FString::Printf(TEXT("Project run launch requested: %s / %s"), *projectPath, *runId));
	return true;
}

void UScenarioLlmPromptWidget::SetStatusText(const FString& message)
{
	if (StatusTextBlock)
	{
		StatusTextBlock->SetText(FText::FromString(message));
	}
}

void UScenarioLlmPromptWidget::HandleGenerateButtonClicked()
{
	GenerateFromPromptTextBox();
}

void UScenarioLlmPromptWidget::HandleLoadGeneratedScenarioButtonClicked()
{
	LoadGeneratedScenario();
}

void UScenarioLlmPromptWidget::HandleRunGeneratedSimulationButtonClicked()
{
	RunGeneratedSimulation();
}

void UScenarioLlmPromptWidget::HandleGenerationCompleted(const FScenarioLlmGenerationResult& result)
{
	if (!result.bSuccess)
	{
		SetStatusText(result.Diagnostics.IsEmpty()
			? FString::Printf(TEXT("LLM generation failed: %s"), *result.Message)
			: FString::Printf(TEXT("LLM generation failed:\n%s"), *FString::Join(result.Diagnostics, TEXT("\n"))));
		return;
	}

	SetStatusText(FString::Printf(
		TEXT("LLM generation completed: %s\n%s"),
		*result.Message,
		*result.ProjectScenarioJsonPath));

	if (bLoadProjectScenarioAfterGenerate)
	{
		LoadGeneratedScenario();
	}
}

void UScenarioLlmPromptWidget::BindControls()
{
	if (GenerateButton)
	{
		GenerateButton->OnClicked.RemoveDynamic(this, &UScenarioLlmPromptWidget::HandleGenerateButtonClicked);
		GenerateButton->OnClicked.AddDynamic(this, &UScenarioLlmPromptWidget::HandleGenerateButtonClicked);
	}

	if (LoadGeneratedScenarioButton)
	{
		LoadGeneratedScenarioButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioLlmPromptWidget::HandleLoadGeneratedScenarioButtonClicked);
		LoadGeneratedScenarioButton->OnClicked.AddDynamic(
			this,
			&UScenarioLlmPromptWidget::HandleLoadGeneratedScenarioButtonClicked);
	}

	if (RunGeneratedSimulationButton)
	{
		RunGeneratedSimulationButton->OnClicked.RemoveDynamic(
			this,
			&UScenarioLlmPromptWidget::HandleRunGeneratedSimulationButtonClicked);
		RunGeneratedSimulationButton->OnClicked.AddDynamic(
			this,
			&UScenarioLlmPromptWidget::HandleRunGeneratedSimulationButtonClicked);
	}
}

void UScenarioLlmPromptWidget::BindLlmSubsystem()
{
	if (UScenarioLlmAuthoringSubsystem* llmSubsystem = GetLlmAuthoringSubsystem())
	{
		llmSubsystem->OnGenerationCompleted.RemoveDynamic(
			this,
			&UScenarioLlmPromptWidget::HandleGenerationCompleted);
		llmSubsystem->OnGenerationCompleted.AddDynamic(
			this,
			&UScenarioLlmPromptWidget::HandleGenerationCompleted);
	}
}

void UScenarioLlmPromptWidget::UnbindLlmSubsystem()
{
	if (UScenarioLlmAuthoringSubsystem* llmSubsystem = GetLlmAuthoringSubsystem())
	{
		llmSubsystem->OnGenerationCompleted.RemoveDynamic(
			this,
			&UScenarioLlmPromptWidget::HandleGenerationCompleted);
	}
}

void UScenarioLlmPromptWidget::ConfigureStatusTextBlock()
{
	if (!StatusTextBlock)
	{
		return;
	}

	StatusTextBlock->SetAutoWrapText(true);
	StatusTextBlock->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
}

void UScenarioLlmPromptWidget::RequestEditorWidgetInputMode()
{
	if (AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer()))
	{
		UWidget* focusWidget = ResolveInputModeFocusWidget();
		RequestedInputModeFocusWidget = focusWidget;
		editorController->RequestEditorWidgetInputMode(focusWidget);
	}
}

void UScenarioLlmPromptWidget::ReleaseEditorWidgetInputMode()
{
	if (AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer()))
	{
		UWidget* focusWidget = RequestedInputModeFocusWidget.Get();
		if (!focusWidget)
		{
			focusWidget = ResolveInputModeFocusWidget();
		}
		editorController->ReleaseEditorWidgetInputMode(focusWidget);
		RequestedInputModeFocusWidget.Reset();
	}
}

bool UScenarioLlmPromptWidget::TryGetPrompt(FString& outPrompt)
{
	outPrompt.Reset();
	if (!PromptTextBox) return false;

	outPrompt = PromptTextBox->GetText().ToString().TrimStartAndEnd();
	if (outPrompt.IsEmpty())
	{
		SetStatusText(TEXT("프롬프트를 입력하세요."));
		return false;
	}

	return true;
}

bool UScenarioLlmPromptWidget::TryGetEpisodeCount(int32& outEpisodeCount)
{
	outEpisodeCount = 0;
	if (!ScenarioCountTextBox)
	{
		if (const UScenarioLlmAuthoringSubsystem* llmSubsystem = GetLlmAuthoringSubsystem())
		{
			outEpisodeCount = llmSubsystem->DefaultEpisodeCount;
			return true;
		}

		outEpisodeCount = 1;
		return true;
	}

	const FString text = ScenarioCountTextBox->GetText().ToString().TrimStartAndEnd();
	if (text.IsEmpty())
	{
		if (const UScenarioLlmAuthoringSubsystem* llmSubsystem = GetLlmAuthoringSubsystem())
		{
			outEpisodeCount = llmSubsystem->DefaultEpisodeCount;
			return true;
		}

		outEpisodeCount = 1;
		return true;
	}

	if (!text.IsNumeric())
	{
		SetStatusText(TEXT("생성 횟수는 정수여야 합니다."));
		return false;
	}

	outEpisodeCount = FCString::Atoi(*text);
	if (outEpisodeCount <= 0)
	{
		SetStatusText(TEXT("생성 횟수는 1 이상이어야 합니다."));
		return false;
	}

	return true;
}

bool UScenarioLlmPromptWidget::TryResolveCurrentProjectScenarioPath(FString& outScenarioJsonPath, FString& outProjectPath)
{
	outScenarioJsonPath.Reset();
	outProjectPath.Reset();

	const AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		SetStatusText(TEXT("Owning player is not ScenarioEditorController."));
		return false;
	}

	outScenarioJsonPath = ResolveProjectScenarioJsonPath(editorController->GetSourceProjectScenarioJsonPath());
	if (!IsProjectScenarioJsonPath(outScenarioJsonPath))
	{
		SetStatusText(TEXT("LLM generate/load/run requires the editor source to be <UserProject>/scenario.json."));
		return false;
	}

	outProjectPath = FPaths::GetPath(outScenarioJsonPath);
	if (outProjectPath.IsEmpty())
	{
		SetStatusText(TEXT("Project path could not be resolved from scenario.json."));
		return false;
	}

	return true;
}

bool UScenarioLlmPromptWidget::TrySaveCurrentProjectScenario(FString& outScenarioJsonPath, FString& outProjectPath)
{
	outScenarioJsonPath.Reset();
	outProjectPath.Reset();

	AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		SetStatusText(TEXT("Owning player is not ScenarioEditorController."));
		return false;
	}

	if (!TryResolveCurrentProjectScenarioPath(outScenarioJsonPath, outProjectPath))
	{
		return false;
	}

	FString resolvedJsonFilePath;
	TArray<FString> diagnostics;
	if (!editorController->SaveProjectScenarioJsonFile(outScenarioJsonPath, resolvedJsonFilePath, diagnostics))
	{
		SetStatusText(diagnostics.IsEmpty()
			? FString::Printf(TEXT("scenario.json save failed: %s"), *outScenarioJsonPath)
			: FString::Printf(TEXT("scenario.json save failed:\n%s"), *FString::Join(diagnostics, TEXT("\n"))));
		return false;
	}

	outScenarioJsonPath = ResolveProjectScenarioJsonPath(resolvedJsonFilePath);
	outProjectPath = FPaths::GetPath(outScenarioJsonPath);
	return true;
}

UWidget* UScenarioLlmPromptWidget::ResolveInputModeFocusWidget()
{
	if (LlmInputModeFocus)
	{
		return LlmInputModeFocus;
	}

	if (PromptTextBox)
	{
		return PromptTextBox;
	}

	return this;
}

UScenarioLlmAuthoringSubsystem* UScenarioLlmPromptWidget::GetLlmAuthoringSubsystem() const
{
	UGameInstance* gameInstance = GetGameInstance();
	return gameInstance ? gameInstance->GetSubsystem<UScenarioLlmAuthoringSubsystem>() : nullptr;
}

USimulatorLaunchSubsystem* UScenarioLlmPromptWidget::GetSimulatorLaunchSubsystem() const
{
	UGameInstance* gameInstance = GetGameInstance();
	return gameInstance ? gameInstance->GetSubsystem<USimulatorLaunchSubsystem>() : nullptr;
}
