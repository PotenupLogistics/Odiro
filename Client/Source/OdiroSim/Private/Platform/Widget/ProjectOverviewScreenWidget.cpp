#include "Platform/Widget/ProjectOverviewScreenWidget.h"

#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/Spacer.h"
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

// WBP-authored scroll style에서 가로 scrollbar가 차지하는 높이를 계산한다.
float ResolveHorizontalScrollbarHeight(const UScrollBox& scrollBox)
{
	const FVector2D scrollbarThickness = scrollBox.GetScrollbarThickness();
	const FMargin scrollbarPadding = scrollBox.GetScrollbarPadding();
	return FMath::Max(
		scrollBox.GetWidgetBarStyle().Thickness,
		scrollbarThickness.Y + scrollbarPadding.Top + scrollbarPadding.Bottom);
}

// WBP-authored scroll style에서 세로 scrollbar가 차지하는 너비를 계산한다.
float ResolveVerticalScrollbarWidth(const UScrollBox& scrollBox)
{
	const FVector2D scrollbarThickness = scrollBox.GetScrollbarThickness();
	const FMargin scrollbarPadding = scrollBox.GetScrollbarPadding();
	return FMath::Max(
		scrollBox.GetWidgetBarStyle().Thickness,
		scrollbarThickness.X + scrollbarPadding.Left + scrollbarPadding.Right);
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

	BindOverviewScrollbars();
	CaptureOverviewAuthoredScrollPadding();
	RefreshFromViewModel();
}

void UProjectOverviewScreenWidget::NativeTick(const FGeometry& myGeometry, const float inDeltaTime)
{
	Super::NativeTick(myGeometry, inDeltaTime);
	UpdateOverviewOverlayScrollbars(myGeometry.GetLocalSize());
}

void UProjectOverviewScreenWidget::NativeDestruct()
{
	UnbindOverviewScrollbars();
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

void UProjectOverviewScreenWidget::BindOverviewScrollbars()
{
	if (ProjectOverviewHorizontalScrollBox)
	{
		ProjectOverviewHorizontalScrollBox->OnUserScrolled.RemoveDynamic(
			this,
			&UProjectOverviewScreenWidget::HandleOverviewContentHorizontalScrolled);
		ProjectOverviewHorizontalScrollBox->OnUserScrolled.AddDynamic(
			this,
			&UProjectOverviewScreenWidget::HandleOverviewContentHorizontalScrolled);
	}
	if (ProjectOverviewScrollBox)
	{
		ProjectOverviewScrollBox->OnUserScrolled.RemoveDynamic(
			this,
			&UProjectOverviewScreenWidget::HandleOverviewContentVerticalScrolled);
		ProjectOverviewScrollBox->OnUserScrolled.AddDynamic(
			this,
			&UProjectOverviewScreenWidget::HandleOverviewContentVerticalScrolled);
	}
	if (ProjectOverviewStickyHorizontalScrollBox)
	{
		ProjectOverviewStickyHorizontalScrollBox->OnUserScrolled.RemoveDynamic(
			this,
			&UProjectOverviewScreenWidget::HandleOverviewStickyHorizontalScrolled);
		ProjectOverviewStickyHorizontalScrollBox->OnUserScrolled.AddDynamic(
			this,
			&UProjectOverviewScreenWidget::HandleOverviewStickyHorizontalScrolled);
	}
	if (ProjectOverviewStickyVerticalScrollBox)
	{
		ProjectOverviewStickyVerticalScrollBox->OnUserScrolled.RemoveDynamic(
			this,
			&UProjectOverviewScreenWidget::HandleOverviewStickyVerticalScrolled);
		ProjectOverviewStickyVerticalScrollBox->OnUserScrolled.AddDynamic(
			this,
			&UProjectOverviewScreenWidget::HandleOverviewStickyVerticalScrolled);
	}
}

void UProjectOverviewScreenWidget::UnbindOverviewScrollbars()
{
	if (ProjectOverviewHorizontalScrollBox)
	{
		ProjectOverviewHorizontalScrollBox->OnUserScrolled.RemoveDynamic(
			this,
			&UProjectOverviewScreenWidget::HandleOverviewContentHorizontalScrolled);
	}
	if (ProjectOverviewScrollBox)
	{
		ProjectOverviewScrollBox->OnUserScrolled.RemoveDynamic(
			this,
			&UProjectOverviewScreenWidget::HandleOverviewContentVerticalScrolled);
	}
	if (ProjectOverviewStickyHorizontalScrollBox)
	{
		ProjectOverviewStickyHorizontalScrollBox->OnUserScrolled.RemoveDynamic(
			this,
			&UProjectOverviewScreenWidget::HandleOverviewStickyHorizontalScrolled);
	}
	if (ProjectOverviewStickyVerticalScrollBox)
	{
		ProjectOverviewStickyVerticalScrollBox->OnUserScrolled.RemoveDynamic(
			this,
			&UProjectOverviewScreenWidget::HandleOverviewStickyVerticalScrolled);
	}
}

void UProjectOverviewScreenWidget::UpdateOverviewOverlayScrollbars(const FVector2D& screenSize)
{
	if (!ProjectOverviewScrollBox || !ProjectOverviewHorizontalScrollBox || !ProjectOverviewMainStack)
	{
		return;
	}
	if (!bHasProjectOverviewScrollBasePadding && !CaptureOverviewAuthoredScrollPadding())
	{
		return;
	}

	FVector2D verticalViewportSize = ProjectOverviewScrollBox->GetCachedGeometry().GetLocalSize();
	if (verticalViewportSize.IsNearlyZero())
	{
		verticalViewportSize = screenSize;
	}
	FVector2D horizontalViewportSize = ProjectOverviewHorizontalScrollBox->GetCachedGeometry().GetLocalSize();
	if (horizontalViewportSize.IsNearlyZero())
	{
		horizontalViewportSize = screenSize;
	}
	const FVector2D viewportSize(horizontalViewportSize.X, verticalViewportSize.Y);
	if (viewportSize.X <= 0.0f || viewportSize.Y <= 0.0f)
	{
		return;
	}

	ProjectOverviewMainStack->ForceLayoutPrepass();
	const FVector2D contentDesiredSize = ProjectOverviewMainStack->GetDesiredSize();
	if (contentDesiredSize.X <= 0.0f || contentDesiredSize.Y <= 0.0f)
	{
		return;
	}

	const bool bUsesStickyHorizontalScroll = ProjectOverviewStickyHorizontalScrollBox
		&& ProjectOverviewStickyHorizontalScrollSpacer;
	const bool bUsesStickyVerticalScroll = ProjectOverviewStickyVerticalScrollBox
		&& ProjectOverviewStickyVerticalScrollSpacer;
	const float horizontalScrollbarHeight = bUsesStickyHorizontalScroll
		? 0.0f
		: ResolveHorizontalScrollbarHeight(*ProjectOverviewHorizontalScrollBox);
	const float verticalScrollbarWidth = bUsesStickyVerticalScroll
		? 0.0f
		: ResolveVerticalScrollbarWidth(*ProjectOverviewScrollBox);
	const float baseContentWidth = contentDesiredSize.X
		+ ProjectOverviewMainStackBasePadding.Left
		+ ProjectOverviewMainStackBasePadding.Right;
	const float baseContentHeight = contentDesiredSize.Y
		+ ProjectOverviewMainStackBasePadding.Top
		+ ProjectOverviewMainStackBasePadding.Bottom
		+ ProjectOverviewHorizontalScrollBoxBasePadding.Top
		+ ProjectOverviewHorizontalScrollBoxBasePadding.Bottom;
	bool bNeedsHorizontalScroll = false;
	bool bNeedsVerticalScroll = false;
	for (int32 passIndex = 0; passIndex < 2; ++passIndex)
	{
		const float availableWidth = viewportSize.X - (bNeedsVerticalScroll ? verticalScrollbarWidth : 0.0f);
		bNeedsHorizontalScroll = availableWidth + KINDA_SMALL_NUMBER < baseContentWidth;
		const float requiredHeight = baseContentHeight + (bNeedsHorizontalScroll ? horizontalScrollbarHeight : 0.0f);
		bNeedsVerticalScroll = viewportSize.Y + KINDA_SMALL_NUMBER < requiredHeight;
	}

	if (CachedOverviewScrollViewportSize.Equals(viewportSize, KINDA_SMALL_NUMBER)
		&& CachedOverviewScrollContentSize.Equals(contentDesiredSize, KINDA_SMALL_NUMBER)
		&& bCachedOverviewNeedsHorizontalScroll == bNeedsHorizontalScroll
		&& bCachedOverviewNeedsVerticalScroll == bNeedsVerticalScroll)
	{
		return;
	}

	CachedOverviewScrollViewportSize = viewportSize;
	CachedOverviewScrollContentSize = contentDesiredSize;
	bCachedOverviewNeedsHorizontalScroll = bNeedsHorizontalScroll;
	bCachedOverviewNeedsVerticalScroll = bNeedsVerticalScroll;

	ProjectOverviewHorizontalScrollBox->SetAlwaysShowScrollbar(false);
	ProjectOverviewHorizontalScrollBox->SetScrollBarVisibility(
		!bUsesStickyHorizontalScroll && bNeedsHorizontalScroll
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	ProjectOverviewScrollBox->SetAlwaysShowScrollbar(false);
	ProjectOverviewScrollBox->SetScrollBarVisibility(
		!bUsesStickyVerticalScroll && bNeedsVerticalScroll
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);

	const float availableCenteringWidth = viewportSize.X - (bNeedsVerticalScroll ? verticalScrollbarWidth : 0.0f);
	const float horizontalExtraPadding = FMath::Max(0.0f, (availableCenteringWidth - baseContentWidth) * 0.5f);
	const FMargin adjustedMainStackPadding(
		ProjectOverviewMainStackBasePadding.Left + horizontalExtraPadding,
		ProjectOverviewMainStackBasePadding.Top,
		ProjectOverviewMainStackBasePadding.Right + horizontalExtraPadding,
		ProjectOverviewMainStackBasePadding.Bottom);
	if (UScrollBoxSlot* mainStackSlot = Cast<UScrollBoxSlot>(ProjectOverviewMainStack->Slot))
	{
		mainStackSlot->SetPadding(adjustedMainStackPadding);
	}

	const float availableCenteringHeight = viewportSize.Y - (bNeedsHorizontalScroll ? horizontalScrollbarHeight : 0.0f);
	const float verticalExtraPadding = FMath::Max(0.0f, (availableCenteringHeight - baseContentHeight) * 0.5f);
	const FMargin adjustedHorizontalScrollBoxPadding(
		ProjectOverviewHorizontalScrollBoxBasePadding.Left,
		ProjectOverviewHorizontalScrollBoxBasePadding.Top + verticalExtraPadding,
		ProjectOverviewHorizontalScrollBoxBasePadding.Right,
		ProjectOverviewHorizontalScrollBoxBasePadding.Bottom + verticalExtraPadding);
	if (UScrollBoxSlot* horizontalScrollBoxSlot = Cast<UScrollBoxSlot>(ProjectOverviewHorizontalScrollBox->Slot))
	{
		horizontalScrollBoxSlot->SetPadding(adjustedHorizontalScrollBoxPadding);
	}

	if (ProjectOverviewStickyHorizontalScrollBox)
	{
		ProjectOverviewStickyHorizontalScrollBox->SetVisibility(
			bUsesStickyHorizontalScroll && bNeedsHorizontalScroll
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);

		if (ProjectOverviewStickyHorizontalScrollSpacer)
		{
			const float stickyRangeCompensation = bUsesStickyVerticalScroll && bNeedsVerticalScroll
				? ResolveVerticalScrollbarWidth(*ProjectOverviewStickyVerticalScrollBox)
				: 0.0f;
			ProjectOverviewStickyHorizontalScrollSpacer->SetSize(FVector2D(
				FMath::Max(1.0f, contentDesiredSize.X
					+ adjustedMainStackPadding.Left
					+ adjustedMainStackPadding.Right
					- stickyRangeCompensation),
				1.0f));
		}
	}

	if (ProjectOverviewStickyVerticalScrollBox)
	{
		ProjectOverviewStickyVerticalScrollBox->SetVisibility(
			bUsesStickyVerticalScroll && bNeedsVerticalScroll
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);

		if (ProjectOverviewStickyVerticalScrollSpacer)
		{
			const float stickyRangeCompensation = bUsesStickyHorizontalScroll && bNeedsHorizontalScroll
				? ResolveHorizontalScrollbarHeight(*ProjectOverviewStickyHorizontalScrollBox)
				: 0.0f;
			ProjectOverviewStickyVerticalScrollSpacer->SetSize(FVector2D(
				1.0f,
				FMath::Max(1.0f, contentDesiredSize.Y
					+ adjustedMainStackPadding.Top
					+ adjustedMainStackPadding.Bottom
					+ adjustedHorizontalScrollBoxPadding.Top
					+ adjustedHorizontalScrollBoxPadding.Bottom
					- stickyRangeCompensation)));
		}
	}

	if (!bNeedsHorizontalScroll)
	{
		TGuardValue<bool> syncGuard(bSyncingOverviewHorizontalScroll, true);
		ProjectOverviewHorizontalScrollBox->SetScrollOffset(0.0f);
		if (ProjectOverviewStickyHorizontalScrollBox)
		{
			ProjectOverviewStickyHorizontalScrollBox->SetScrollOffset(0.0f);
		}
	}
	if (!bNeedsVerticalScroll)
	{
		TGuardValue<bool> syncGuard(bSyncingOverviewVerticalScroll, true);
		ProjectOverviewScrollBox->SetScrollOffset(0.0f);
		if (ProjectOverviewStickyVerticalScrollBox)
		{
			ProjectOverviewStickyVerticalScrollBox->SetScrollOffset(0.0f);
		}
	}
}

bool UProjectOverviewScreenWidget::CaptureOverviewAuthoredScrollPadding()
{
	if (!ProjectOverviewHorizontalScrollBox || !ProjectOverviewMainStack)
	{
		return false;
	}

	const UScrollBoxSlot* mainStackSlot = Cast<UScrollBoxSlot>(ProjectOverviewMainStack->Slot);
	const UScrollBoxSlot* horizontalScrollBoxSlot = Cast<UScrollBoxSlot>(ProjectOverviewHorizontalScrollBox->Slot);
	if (!mainStackSlot || !horizontalScrollBoxSlot)
	{
		return false;
	}

	ProjectOverviewMainStackBasePadding = mainStackSlot->GetPadding();
	ProjectOverviewHorizontalScrollBoxBasePadding = horizontalScrollBoxSlot->GetPadding();
	bHasProjectOverviewScrollBasePadding = true;
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

void UProjectOverviewScreenWidget::HandleOverviewStickyHorizontalScrolled(const float currentOffset)
{
	if (bSyncingOverviewHorizontalScroll || !ProjectOverviewHorizontalScrollBox)
	{
		return;
	}

	TGuardValue<bool> syncGuard(bSyncingOverviewHorizontalScroll, true);
	ProjectOverviewHorizontalScrollBox->SetScrollOffset(currentOffset);
}

void UProjectOverviewScreenWidget::HandleOverviewContentHorizontalScrolled(const float currentOffset)
{
	if (bSyncingOverviewHorizontalScroll || !ProjectOverviewStickyHorizontalScrollBox)
	{
		return;
	}

	TGuardValue<bool> syncGuard(bSyncingOverviewHorizontalScroll, true);
	ProjectOverviewStickyHorizontalScrollBox->SetScrollOffset(currentOffset);
}

void UProjectOverviewScreenWidget::HandleOverviewStickyVerticalScrolled(const float currentOffset)
{
	if (bSyncingOverviewVerticalScroll || !ProjectOverviewScrollBox)
	{
		return;
	}

	TGuardValue<bool> syncGuard(bSyncingOverviewVerticalScroll, true);
	ProjectOverviewScrollBox->SetScrollOffset(currentOffset);
}

void UProjectOverviewScreenWidget::HandleOverviewContentVerticalScrolled(const float currentOffset)
{
	if (bSyncingOverviewVerticalScroll || !ProjectOverviewStickyVerticalScrollBox)
	{
		return;
	}

	TGuardValue<bool> syncGuard(bSyncingOverviewVerticalScroll, true);
	ProjectOverviewStickyVerticalScrollBox->SetScrollOffset(currentOffset);
}
