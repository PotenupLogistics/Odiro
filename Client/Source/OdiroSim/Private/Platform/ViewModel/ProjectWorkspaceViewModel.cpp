#include "Platform/ViewModel/ProjectWorkspaceViewModel.h"

#include "Engine/GameInstance.h"
#include "Misc/Paths.h"
#include "Platform/PlatformAnalysisAiSubsystem.h"
#include "Platform/PlatformUiSubsystem.h"
#include "Platform/ProjectSessionSubsystem.h"
#include "Platform/ScenarioEditorLaunchSubsystem.h"
#include "Platform/ViewModel/OdiroListItemViewModel.h"

namespace
{
	const FName WorkspaceVmDefaultTabId(TEXT("ScenarioEdit"));

	FString NormalizeWorkspaceVmPath(FString path)
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

	TArray<UOdiroListItemViewModel*> CopyWorkspaceVmItems(const TArray<TObjectPtr<UOdiroListItemViewModel>>& sourceItems)
	{
		TArray<UOdiroListItemViewModel*> result;
		result.Reserve(sourceItems.Num());
		for (UOdiroListItemViewModel* item : sourceItems)
		{
			result.Add(item);
		}
		return result;
	}
}

void UProjectWorkspaceViewModel::InitializeForGameInstance(UGameInstance* gameInstance)
{
	GameInstance = gameInstance;
	if (USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem())
	{
		subsystem->OnRunInfoChanged.RemoveAll(this);
		subsystem->OnRunInfoChanged.AddUObject(this, &UProjectWorkspaceViewModel::HandleRunInfoChanged);
	}

	RefreshFromProjectSession();
}

void UProjectWorkspaceViewModel::SetSubsystemOverrides(
	USimulatorLaunchSubsystem* simulatorLaunchSubsystem,
	UProjectSessionSubsystem* projectSessionSubsystem,
	UScenarioEditorLaunchSubsystem* scenarioEditorLaunchSubsystem,
	UPlatformAnalysisAiSubsystem* analysisAiSubsystem)
{
	if (USimulatorLaunchSubsystem* oldSubsystem = ResolveSimulatorLaunchSubsystem())
	{
		oldSubsystem->OnRunInfoChanged.RemoveAll(this);
	}

	SimulatorLaunchOverride = simulatorLaunchSubsystem;
	ProjectSessionOverride = projectSessionSubsystem;
	ScenarioEditorLaunchOverride = scenarioEditorLaunchSubsystem;
	AnalysisAiOverride = analysisAiSubsystem;

	if (USimulatorLaunchSubsystem* newSubsystem = ResolveSimulatorLaunchSubsystem())
	{
		newSubsystem->OnRunInfoChanged.RemoveAll(this);
		newSubsystem->OnRunInfoChanged.AddUObject(this, &UProjectWorkspaceViewModel::HandleRunInfoChanged);
	}
}

void UProjectWorkspaceViewModel::RefreshFromProjectSession()
{
	UProjectSessionSubsystem* projectSession = ResolveProjectSessionSubsystem();
	if (!projectSession || !projectSession->HasActiveProject())
	{
		SetActiveProjectPath(FString());
		SetActiveScenarioPath(FString());
		SetSelectedRunState(FString(), FString());
		RunItems.Reset();
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RunItems);
		SetStatusText(TEXT("Active project가 없습니다."));
		return;
	}

	SetActiveProjectPath(projectSession->GetActiveProjectPath());
	SetActiveScenarioPath(projectSession->GetActiveProjectScenarioPath());
	SelectWorkspaceTab(SelectedWorkspaceTabId.IsNone() ? WorkspaceVmDefaultTabId : SelectedWorkspaceTabId);
	RefreshProjectRuns();
}

void UProjectWorkspaceViewModel::RefreshProjectRuns()
{
	RunItems.Reset();

	if (ActiveProjectPath.IsEmpty())
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RunItems);
		SetStatusText(TEXT("Active project가 없습니다."));
		return;
	}

	USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RunItems);
		SetStatusText(TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다."));
		return;
	}

	TArray<FString> diagnostics;
	if (!subsystem->ValidateUserProject(ActiveProjectPath, diagnostics))
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RunItems);
		const FString message = diagnostics.IsEmpty() ? TEXT("Project 검증 실패.") : FString::Join(diagnostics, TEXT("\n"));
		SetDiagnosticsText(message);
		SetStatusText(message);
		return;
	}

	TArray<FString> runDirectories = subsystem->ListProjectRunDirectories(ActiveProjectPath);
	const FSimulatorRunInfo activeRunInfo = subsystem->GetActiveRunInfo();
	const FString activeRunProjectPath = NormalizeWorkspaceVmPath(activeRunInfo.ProjectPath);
	if (activeRunInfo.bProjectRun
		&& activeRunProjectPath.Equals(ActiveProjectPath, ESearchCase::IgnoreCase)
		&& !activeRunInfo.RunId.IsEmpty())
	{
		runDirectories.AddUnique(BuildRunDirectory(activeRunInfo.RunId));
	}
	RunItems.Reserve(runDirectories.Num());
	for (const FString& runDirectory : runDirectories)
	{
		const FString normalizedRunDirectory = NormalizeWorkspaceVmPath(runDirectory);
		const FString runId = ExtractRunId(normalizedRunDirectory);
		UOdiroListItemViewModel* item = NewObject<UOdiroListItemViewModel>(this);
		item->InitializeItem(runId, FString::Printf(TEXT("Run %s"), *runId), normalizedRunDirectory, normalizedRunDirectory);
		item->SetSelected(runId.Equals(SelectedRunId, ESearchCase::IgnoreCase));
		RunItems.Add(item);
	}

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RunItems);
	if (SelectedRunId.IsEmpty() && !runDirectories.IsEmpty())
	{
		SelectRun(ExtractRunId(runDirectories[0]));
	}
	SetStatusText(FString::Printf(TEXT("Project: %s\nRuns: %d"), *ActiveProjectPath, runDirectories.Num()));
}

void UProjectWorkspaceViewModel::SelectWorkspaceTab(const FName tabId)
{
	UE_MVVM_SET_PROPERTY_VALUE(SelectedWorkspaceTabId, tabId.IsNone() ? WorkspaceVmDefaultTabId : tabId);
}

bool UProjectWorkspaceViewModel::SelectRun(const FString& runId)
{
	const FString normalizedRunId = runId.TrimStartAndEnd();
	if (normalizedRunId.IsEmpty())
	{
		SetSelectedRunState(FString(), FString());
		return false;
	}

	const FString runDirectory = BuildRunDirectory(normalizedRunId);
	SetSelectedRunState(normalizedRunId, runDirectory);
	for (UOdiroListItemViewModel* item : RunItems)
	{
		if (item)
		{
			item->SetSelected(item->GetItemId().Equals(normalizedRunId, ESearchCase::IgnoreCase));
		}
	}
	return true;
}

bool UProjectWorkspaceViewModel::CreateRun(FString& outRunId)
{
	outRunId.Reset();

	USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		SetDiagnosticsText(TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}
	if (ActiveProjectPath.IsEmpty())
	{
		SetDiagnosticsText(TEXT("Active project가 없습니다."));
		return false;
	}

	TArray<FString> diagnostics;
	if (!subsystem->CreateProjectRun(ActiveProjectPath, outRunId, diagnostics))
	{
		const FString message = diagnostics.IsEmpty() ? TEXT("Project run 생성 실패.") : FString::Join(diagnostics, TEXT("\n"));
		SetDiagnosticsText(message);
		return false;
	}

	SelectRun(outRunId);
	RefreshProjectRuns();
	SetDiagnosticsText(FString::Printf(TEXT("Project run created: %s"), *outRunId));
	return true;
}

bool UProjectWorkspaceViewModel::StartRun()
{
	USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem();
	if (!subsystem)
	{
		SetDiagnosticsText(TEXT("SimulatorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}

	FString runId = SelectedRunId;
	if (runId.IsEmpty() && !CreateRun(runId))
	{
		return false;
	}

	if (!subsystem->StartProjectRun(ActiveProjectPath, runId))
	{
		SetDiagnosticsText(subsystem->GetLastError().IsEmpty() ? TEXT("Project run 시작 실패.") : subsystem->GetLastError());
		return false;
	}

	SetStatusText(FString::Printf(TEXT("Run started: %s"), *runId));
	return true;
}

bool UProjectWorkspaceViewModel::StartNewRun(FString& outRunId)
{
	outRunId.Reset();

	FString newRunId;
	if (!CreateRun(newRunId))
	{
		return false;
	}

	outRunId = newRunId;
	if (!StartRun())
	{
		return false;
	}

	return true;
}

void UProjectWorkspaceViewModel::StopRun()
{
	if (USimulatorLaunchSubsystem* subsystem = ResolveSimulatorLaunchSubsystem())
	{
		subsystem->StopActiveRun();
		SetStatusText(TEXT("Run stop requested."));
	}
}

bool UProjectWorkspaceViewModel::RequestAiAnalysis()
{
	UPlatformAnalysisAiSubsystem* subsystem = ResolvePlatformAnalysisAiSubsystem();
	if (!subsystem)
	{
		SetDiagnosticsText(TEXT("PlatformAnalysisAiSubsystem을 사용할 수 없습니다."));
		return false;
	}
	if (ActiveProjectPath.IsEmpty() || SelectedRunId.IsEmpty())
	{
		SetDiagnosticsText(TEXT("AI 분석을 요청할 project/run 선택이 필요합니다."));
		return false;
	}

	if (!subsystem->RequestAnalysisForProjectRun(ActiveProjectPath, SelectedRunId))
	{
		SetDiagnosticsText(TEXT("AI 분석 요청 실패."));
		return false;
	}

	SetStatusText(FString::Printf(TEXT("AI analysis requested: %s"), *SelectedRunId));
	return true;
}

bool UProjectWorkspaceViewModel::OpenScenarioEditor()
{
	UScenarioEditorLaunchSubsystem* subsystem = ResolveScenarioEditorLaunchSubsystem();
	if (!subsystem)
	{
		SetDiagnosticsText(TEXT("ScenarioEditorLaunchSubsystem을 사용할 수 없습니다."));
		return false;
	}

	const FString scenarioPath = ActiveScenarioPath.IsEmpty()
		? (ResolveProjectSessionSubsystem() ? ResolveProjectSessionSubsystem()->GetActiveProjectScenarioPath() : FString())
		: ActiveScenarioPath;
	if (scenarioPath.IsEmpty())
	{
		SetDiagnosticsText(TEXT("Active project scenario.json이 없습니다."));
		return false;
	}

	return subsystem->OpenScenarioEditor(scenarioPath);
}

bool UProjectWorkspaceViewModel::ReturnToStartup()
{
	UPlatformUiSubsystem* platformUiSubsystem = ResolvePlatformUiSubsystem();
	if (!platformUiSubsystem)
	{
		SetDiagnosticsText(TEXT("PlatformUiSubsystem을 사용할 수 없습니다."));
		return false;
	}

	FString errorText;
	if (!platformUiSubsystem->ReturnToStartupMap(errorText))
	{
		SetDiagnosticsText(errorText.IsEmpty() ? TEXT("Startup screen 복귀 실패.") : errorText);
		return false;
	}

	return true;
}

TArray<UOdiroListItemViewModel*> UProjectWorkspaceViewModel::GetRunItems() const
{
	return CopyWorkspaceVmItems(RunItems);
}

USimulatorLaunchSubsystem* UProjectWorkspaceViewModel::ResolveSimulatorLaunchSubsystem() const
{
	if (SimulatorLaunchOverride)
	{
		return SimulatorLaunchOverride;
	}
	return GameInstance ? GameInstance->GetSubsystem<USimulatorLaunchSubsystem>() : nullptr;
}

UProjectSessionSubsystem* UProjectWorkspaceViewModel::ResolveProjectSessionSubsystem() const
{
	if (ProjectSessionOverride)
	{
		return ProjectSessionOverride;
	}
	return GameInstance ? GameInstance->GetSubsystem<UProjectSessionSubsystem>() : nullptr;
}

UScenarioEditorLaunchSubsystem* UProjectWorkspaceViewModel::ResolveScenarioEditorLaunchSubsystem() const
{
	if (ScenarioEditorLaunchOverride)
	{
		return ScenarioEditorLaunchOverride;
	}
	return GameInstance ? GameInstance->GetSubsystem<UScenarioEditorLaunchSubsystem>() : nullptr;
}

UPlatformUiSubsystem* UProjectWorkspaceViewModel::ResolvePlatformUiSubsystem() const
{
	return GameInstance ? GameInstance->GetSubsystem<UPlatformUiSubsystem>() : nullptr;
}

UPlatformAnalysisAiSubsystem* UProjectWorkspaceViewModel::ResolvePlatformAnalysisAiSubsystem() const
{
	if (AnalysisAiOverride)
	{
		return AnalysisAiOverride;
	}
	return GameInstance ? GameInstance->GetSubsystem<UPlatformAnalysisAiSubsystem>() : nullptr;
}

void UProjectWorkspaceViewModel::HandleRunInfoChanged(const FSimulatorRunInfo& runInfo)
{
	const FString normalizedRunProjectPath = NormalizeWorkspaceVmPath(runInfo.ProjectPath);
	const bool bMatchesActiveProject =
		!normalizedRunProjectPath.IsEmpty()
		&& !ActiveProjectPath.IsEmpty()
		&& normalizedRunProjectPath.Equals(ActiveProjectPath, ESearchCase::IgnoreCase);
	if (runInfo.bProjectRun && !runInfo.RunId.IsEmpty() && bMatchesActiveProject)
	{
		RefreshProjectRuns();
		SelectRun(runInfo.RunId);
	}
	SetStatusText(runInfo.LastError.IsEmpty()
		? FString::Printf(TEXT("Run: %s"), *runInfo.RunId)
		: runInfo.LastError);
}

void UProjectWorkspaceViewModel::SetActiveProjectPath(const FString& projectPath)
{
	UE_MVVM_SET_PROPERTY_VALUE(ActiveProjectPath, NormalizeWorkspaceVmPath(projectPath));
}

void UProjectWorkspaceViewModel::SetActiveScenarioPath(const FString& scenarioPath)
{
	UE_MVVM_SET_PROPERTY_VALUE(ActiveScenarioPath, NormalizeWorkspaceVmPath(scenarioPath));
}

void UProjectWorkspaceViewModel::SetSelectedRunState(const FString& runId, const FString& runDirectory)
{
	UE_MVVM_SET_PROPERTY_VALUE(SelectedRunId, runId.TrimStartAndEnd());
	UE_MVVM_SET_PROPERTY_VALUE(SelectedRunDirectory, NormalizeWorkspaceVmPath(runDirectory));
}

void UProjectWorkspaceViewModel::SetStatusText(const FString& statusText)
{
	UE_MVVM_SET_PROPERTY_VALUE(StatusText, statusText);
}

FString UProjectWorkspaceViewModel::BuildRunDirectory(const FString& runId) const
{
	if (ActiveProjectPath.IsEmpty() || runId.TrimStartAndEnd().IsEmpty())
	{
		return FString();
	}
	return NormalizeWorkspaceVmPath(FPaths::Combine(ActiveProjectPath, TEXT("runs"), runId.TrimStartAndEnd()));
}

FString UProjectWorkspaceViewModel::ExtractRunId(const FString& runDirectory) const
{
	return FPaths::GetCleanFilename(NormalizeWorkspaceVmPath(runDirectory));
}
