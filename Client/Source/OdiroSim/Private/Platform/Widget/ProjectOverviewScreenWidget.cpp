#include "Platform/Widget/ProjectOverviewScreenWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Platform/PlatformUiSubsystem.h"
#include "Platform/ViewModel/ProjectWorkspaceViewModel.h"
#include "UI/BaseButtonWidget.h"
#include "UI/BaseTextWidget.h"

namespace
{
struct FWorkspaceEditorCandidate
{
	FString Label;
	FString ExecutablePath;
};

// Writes runtime labels across native TextBlock and transitional BaseText widgets.
void SetOverviewWidgetText(UWidget* widget, const FText& text)
{
	if (UTextBlock* textBlock = Cast<UTextBlock>(widget))
	{
		textBlock->SetText(text);
		return;
	}
	if (UBaseTextWidget* baseText = Cast<UBaseTextWidget>(widget))
	{
		baseText->SetText(text);
	}
}

// Quotes a Windows process argument while preserving Unreal-normalized paths.
FString QuoteProcessArgument(const FString& value)
{
	FString escapedValue = value;
	escapedValue.ReplaceInline(TEXT("\""), TEXT("\\\""));
	return FString::Printf(TEXT("\"%s\""), *escapedValue);
}

// Adds an installed workspace editor candidate when the executable exists.
void AddWorkspaceEditorCandidate(
	TArray<FWorkspaceEditorCandidate>& outCandidates,
	const FString& label,
	const FString& executablePath)
{
	if (executablePath.TrimStartAndEnd().IsEmpty())
	{
		return;
	}

	FString normalizedPath = executablePath;
	FPaths::NormalizeFilename(normalizedPath);
	if (!FPaths::FileExists(normalizedPath))
	{
		return;
	}

	if (outCandidates.ContainsByPredicate(
		[&normalizedPath](const FWorkspaceEditorCandidate& candidate)
		{
			return candidate.ExecutablePath.Equals(normalizedPath, ESearchCase::IgnoreCase);
		}))
	{
		return;
	}

	outCandidates.Add(FWorkspaceEditorCandidate{ label, normalizedPath });
}

// Adds common editor installations that support folder and file jump CLI arguments.
void AddKnownWorkspaceEditorCandidates(TArray<FWorkspaceEditorCandidate>& outCandidates)
{
	const FString localAppData = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"));
	const FString programFiles = FPlatformMisc::GetEnvironmentVariable(TEXT("ProgramFiles"));
	const FString programFilesX86 = FPlatformMisc::GetEnvironmentVariable(TEXT("ProgramFiles(x86)"));

	AddWorkspaceEditorCandidate(
		outCandidates,
		TEXT("VS Code"),
		FPaths::Combine(localAppData, TEXT("Programs"), TEXT("Microsoft VS Code"), TEXT("Code.exe")));
	AddWorkspaceEditorCandidate(
		outCandidates,
		TEXT("Cursor"),
		FPaths::Combine(localAppData, TEXT("Programs"), TEXT("Cursor"), TEXT("Cursor.exe")));
	AddWorkspaceEditorCandidate(
		outCandidates,
		TEXT("Windsurf"),
		FPaths::Combine(localAppData, TEXT("Programs"), TEXT("Windsurf"), TEXT("Windsurf.exe")));
	AddWorkspaceEditorCandidate(
		outCandidates,
		TEXT("VSCodium"),
		FPaths::Combine(localAppData, TEXT("Programs"), TEXT("VSCodium"), TEXT("VSCodium.exe")));

	AddWorkspaceEditorCandidate(
		outCandidates,
		TEXT("VS Code"),
		FPaths::Combine(programFiles, TEXT("Microsoft VS Code"), TEXT("Code.exe")));
	AddWorkspaceEditorCandidate(
		outCandidates,
		TEXT("Cursor"),
		FPaths::Combine(programFiles, TEXT("Cursor"), TEXT("Cursor.exe")));
	AddWorkspaceEditorCandidate(
		outCandidates,
		TEXT("Windsurf"),
		FPaths::Combine(programFiles, TEXT("Windsurf"), TEXT("Windsurf.exe")));
	AddWorkspaceEditorCandidate(
		outCandidates,
		TEXT("VSCodium"),
		FPaths::Combine(programFiles, TEXT("VSCodium"), TEXT("VSCodium.exe")));

	AddWorkspaceEditorCandidate(
		outCandidates,
		TEXT("VS Code"),
		FPaths::Combine(programFilesX86, TEXT("Microsoft VS Code"), TEXT("Code.exe")));
}

// Launches a workspace-aware editor when one is installed.
bool TryLaunchPolicyWorkspaceEditor(
	const FString& projectPath,
	const FString& policyEntrypointPath,
	FString& outEditorLabel)
{
	TArray<FWorkspaceEditorCandidate> candidates;
	AddWorkspaceEditorCandidate(
		candidates,
		TEXT("Custom editor"),
		FPlatformMisc::GetEnvironmentVariable(TEXT("ODIRO_POLICY_EDITOR")));
	AddKnownWorkspaceEditorCandidates(candidates);

	const FString arguments = FString::Printf(
		TEXT("-r %s -g %s"),
		*QuoteProcessArgument(projectPath),
		*QuoteProcessArgument(FString::Printf(TEXT("%s:1"), *policyEntrypointPath)));

	for (const FWorkspaceEditorCandidate& candidate : candidates)
	{
		FProcHandle processHandle = FPlatformProcess::CreateProc(
			*candidate.ExecutablePath,
			*arguments,
			true,
			false,
			false,
			nullptr,
			0,
			*projectPath,
			nullptr);
		if (!processHandle.IsValid())
		{
			continue;
		}

		FPlatformProcess::CloseProc(processHandle);
		outEditorLabel = candidate.Label;
		return true;
	}

	return false;
}

}

void UProjectOverviewScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (OpenScenarioButton)
	{
		OpenScenarioButton->OnBaseClicked.RemoveDynamic(
			this,
			&UProjectOverviewScreenWidget::HandleScenarioButtonClicked);
		OpenScenarioButton->OnBaseClicked.AddDynamic(
			this,
			&UProjectOverviewScreenWidget::HandleScenarioButtonClicked);
	}
	if (OpenRobotButton)
	{
		OpenRobotButton->OnBaseClicked.RemoveDynamic(this, &UProjectOverviewScreenWidget::HandleRobotButtonClicked);
		OpenRobotButton->OnBaseClicked.AddDynamic(this, &UProjectOverviewScreenWidget::HandleRobotButtonClicked);
	}
	if (OpenPolicyButton)
	{
		OpenPolicyButton->OnBaseClicked.RemoveDynamic(this, &UProjectOverviewScreenWidget::HandlePolicyButtonClicked);
		OpenPolicyButton->OnBaseClicked.AddDynamic(this, &UProjectOverviewScreenWidget::HandlePolicyButtonClicked);
	}
	if (OpenExperimentButton)
	{
		OpenExperimentButton->OnBaseClicked.RemoveDynamic(
			this,
			&UProjectOverviewScreenWidget::HandleExperimentButtonClicked);
		OpenExperimentButton->OnBaseClicked.AddDynamic(
			this,
			&UProjectOverviewScreenWidget::HandleExperimentButtonClicked);
	}
	if (OpenResultButton)
	{
		OpenResultButton->OnBaseClicked.RemoveDynamic(
			this,
			&UProjectOverviewScreenWidget::HandleExperimentButtonClicked);
		OpenResultButton->OnBaseClicked.AddDynamic(
			this,
			&UProjectOverviewScreenWidget::HandleExperimentButtonClicked);
	}

	RefreshFromViewModel();
}

void UProjectOverviewScreenWidget::NativeDestruct()
{
	if (OpenScenarioButton)
	{
		OpenScenarioButton->OnBaseClicked.RemoveDynamic(
			this,
			&UProjectOverviewScreenWidget::HandleScenarioButtonClicked);
	}
	if (OpenRobotButton)
	{
		OpenRobotButton->OnBaseClicked.RemoveDynamic(this, &UProjectOverviewScreenWidget::HandleRobotButtonClicked);
	}
	if (OpenPolicyButton)
	{
		OpenPolicyButton->OnBaseClicked.RemoveDynamic(this, &UProjectOverviewScreenWidget::HandlePolicyButtonClicked);
	}
	if (OpenExperimentButton)
	{
		OpenExperimentButton->OnBaseClicked.RemoveDynamic(
			this,
			&UProjectOverviewScreenWidget::HandleExperimentButtonClicked);
	}
	if (OpenResultButton)
	{
		OpenResultButton->OnBaseClicked.RemoveDynamic(
			this,
			&UProjectOverviewScreenWidget::HandleExperimentButtonClicked);
	}

	Super::NativeDestruct();
}

void UProjectOverviewScreenWidget::RefreshFromViewModel()
{
	UProjectWorkspaceViewModel* viewModel = ResolveViewModel();
	if (!viewModel)
	{
		// Editor widget captures keep authored preview status; runtime screens report missing bindings.
		if (UWorld* world = GetWorld(); world && world->WorldType == EWorldType::Editor)
		{
			return;
		}
		if (StatusText)
		{
			SetOverviewWidgetText(
				StatusText,
				NSLOCTEXT("OdiroPlatform", "OverviewViewModelMissing", "Workspace ViewModel 없음"));
		}
		return;
	}

	viewModel->RefreshFromProjectSession();
	if (UWidget* projectNameWidget = GetWidgetFromName(TEXT("ProjectNameTitle")))
	{
		SetOverviewWidgetText(projectNameWidget, FText::FromString(viewModel->GetActiveProjectId()));
	}
	if (ProjectPathText)
	{
		SetOverviewWidgetText(ProjectPathText, FText::FromString(viewModel->GetActiveProjectPath()));
	}
	ApplyScenarioThumbnail(viewModel->GetActiveProjectPath());
	if (ScenarioPathText)
	{
		SetOverviewWidgetText(ScenarioPathText, FText::FromString(viewModel->GetActiveScenarioPath()));
	}
	if (RunCountText)
	{
		SetOverviewWidgetText(
			RunCountText,
			FText::Format(
				NSLOCTEXT("OdiroPlatform", "OverviewRunCountLine", "총 {0}회 실행"),
				FText::AsNumber(viewModel->GetRunItems().Num())));
	}
	if (StatusText)
	{
		SetOverviewWidgetText(StatusText, FText::FromString(viewModel->GetStatusText()));
	}
}

UProjectWorkspaceViewModel* UProjectOverviewScreenWidget::ResolveViewModel()
{
	if (ProjectWorkspaceViewModel)
	{
		return ProjectWorkspaceViewModel.Get();
	}

	UPlatformUiSubsystem* platformUiSubsystem = UPlatformUiSubsystem::ResolveForWorldContext(this);
	ProjectWorkspaceViewModel = platformUiSubsystem ? platformUiSubsystem->GetProjectWorkspaceViewModel() : nullptr;
	return ProjectWorkspaceViewModel.Get();
}

bool UProjectOverviewScreenWidget::ApplyScenarioThumbnail(const FString& projectPath)
{
	if (!ScenarioThumbnailImage)
	{
		return false;
	}

	FString previewPath = FPaths::Combine(projectPath.TrimStartAndEnd(), TEXT("preview.png"));
	FPaths::NormalizeFilename(previewPath);
	if (projectPath.TrimStartAndEnd().IsEmpty() || !FPaths::FileExists(previewPath))
	{
		ScenarioThumbnailTexture = nullptr;
		ScenarioThumbnailImage->SetBrushResourceObject(nullptr);
		return false;
	}

	UTexture2D* thumbnailTexture = FImageUtils::ImportFileAsTexture2D(previewPath);
	if (!thumbnailTexture)
	{
		ScenarioThumbnailTexture = nullptr;
		ScenarioThumbnailImage->SetBrushResourceObject(nullptr);
		return false;
	}

	ScenarioThumbnailTexture = thumbnailTexture;
	ScenarioThumbnailImage->SetBrushFromTexture(ScenarioThumbnailTexture.Get(), false);
	ScenarioThumbnailImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	return true;
}

void UProjectOverviewScreenWidget::HandleScenarioButtonClicked(UBaseButtonWidget* button)
{
	if (IsValid(button) && button == OpenScenarioButton)
	{
		OnScenarioRequested.Broadcast(this);
	}
}

void UProjectOverviewScreenWidget::HandleRobotButtonClicked(UBaseButtonWidget* button)
{
	if (IsValid(button) && button == OpenRobotButton)
	{
		OnRobotRequested.Broadcast(this);
	}
}

void UProjectOverviewScreenWidget::HandlePolicyButtonClicked(UBaseButtonWidget* button)
{
	if (!IsValid(button) || button != OpenPolicyButton)
	{
		return;
	}

	UProjectWorkspaceViewModel* viewModel = ResolveViewModel();
	if (!viewModel)
	{
		SetOverviewWidgetText(
			StatusText,
			NSLOCTEXT("OdiroPlatform", "OverviewPolicyViewModelMissing", "Workspace ViewModel 없음"));
		return;
	}

	viewModel->RefreshFromProjectSession();
	const FString projectPath = viewModel->GetActiveProjectPath().TrimStartAndEnd();
	FString policyEntrypointPath = FPaths::Combine(projectPath, TEXT("policy"), TEXT("__init__.py"));
	FPaths::NormalizeFilename(policyEntrypointPath);
	if (projectPath.IsEmpty() || !FPaths::FileExists(policyEntrypointPath))
	{
		SetOverviewWidgetText(
			StatusText,
			NSLOCTEXT("OdiroPlatform", "OverviewPolicyEntrypointMissing", "policy/__init__.py 없음"));
		return;
	}

	FString openedEditorLabel;
	if (TryLaunchPolicyWorkspaceEditor(projectPath, policyEntrypointPath, openedEditorLabel))
	{
		SetOverviewWidgetText(
			StatusText,
			FText::Format(
				NSLOCTEXT("OdiroPlatform", "OverviewPolicyWorkspaceEditorOpenRequested", "policy 편집기 열기 요청됨 ({0})"),
				FText::FromString(openedEditorLabel)));
		return;
	}

	if (!FPlatformProcess::LaunchFileInDefaultExternalApplication(*policyEntrypointPath))
	{
		SetOverviewWidgetText(
			StatusText,
			NSLOCTEXT("OdiroPlatform", "OverviewPolicyEntrypointOpenFailed", "policy/__init__.py 열기 실패"));
		return;
	}

	SetOverviewWidgetText(
		StatusText,
		NSLOCTEXT("OdiroPlatform", "OverviewPolicyEntrypointOpenRequested", "policy/__init__.py 열기 요청됨"));
}

void UProjectOverviewScreenWidget::HandleExperimentButtonClicked(UBaseButtonWidget* button)
{
	if (IsValid(button) && (button == OpenExperimentButton || button == OpenResultButton))
	{
		OnExperimentRequested.Broadcast(this);
	}
}
