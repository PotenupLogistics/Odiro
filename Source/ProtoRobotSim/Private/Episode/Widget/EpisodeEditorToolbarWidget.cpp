#include "Episode/Widget/EpisodeEditorToolbarWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Episode/Editor/EpisodeEditorController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Shared/SimulationSetupTypes.h"

namespace
{
	FString MakeUniqueSavePath(const FString& preferredPath)
	{
		FString directory = FPaths::GetPath(preferredPath);
		if (directory.IsEmpty())
		{
			directory = TEXT("Json/Input");
		}

		const FString baseName = FPaths::GetBaseFilename(preferredPath).IsEmpty()
			? FString(TEXT("EpisodeSetupNew"))
			: FPaths::GetBaseFilename(preferredPath);
		const FString extension = FPaths::GetExtension(preferredPath).IsEmpty()
			? FString(TEXT("json"))
			: FPaths::GetExtension(preferredPath);

		for (int32 index = 0; index < 1000; ++index)
		{
			const FString fileName = index == 0
				? FString::Printf(TEXT("%s.%s"), *baseName, *extension)
				: FString::Printf(TEXT("%s_%d.%s"), *baseName, index, *extension);
			FString candidatePath = FPaths::Combine(directory, fileName);
			candidatePath.ReplaceInline(TEXT("\\"), TEXT("/"));
			if (!FPaths::FileExists(FSimulationSetupJson::ResolveProjectPath(candidatePath)))
			{
				return candidatePath;
			}
		}

		FString fallbackPath = FPaths::Combine(
			directory,
			FString::Printf(TEXT("%s_%s.%s"), *baseName, *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8), *extension));
		fallbackPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		return fallbackPath;
	}
}

void UEpisodeEditorToolbarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindControls();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	RequestEditorWidgetInputMode();
	SetStatusText(TEXT("준비됨"));
}

void UEpisodeEditorToolbarWidget::NativeDestruct()
{
	ReleaseEditorWidgetInputMode();
	Super::NativeDestruct();
}

bool UEpisodeEditorToolbarWidget::SaveEpisode()
{
	AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		SetStatusText(TEXT("저장 실패: EpisodeEditorController unavailable."));
		return false;
	}

	FString resolvedPath;
	TArray<FString> diagnostics;
	const FString savePath = ResolveSavePath();
	if (!editorController->SaveEpisodeSetupJsonFile(savePath, resolvedPath, diagnostics))
	{
		SetStatusText(diagnostics.IsEmpty()
			? FString::Printf(TEXT("저장 실패: %s"), *savePath)
			: FString::Join(diagnostics, TEXT("\n")));
		return false;
	}

	SetStatusText(FString::Printf(TEXT("저장됨: %s"), *resolvedPath));
	return true;
}

void UEpisodeEditorToolbarWidget::ReturnToMainMenu()
{
	if (UWorld* world = GetWorld())
	{
		UGameplayStatics::OpenLevel(world, FName(*MainMenuMapId));
	}
}

void UEpisodeEditorToolbarWidget::HandleSaveButtonClicked()
{
	SaveEpisode();
}

void UEpisodeEditorToolbarWidget::HandleReturnButtonClicked()
{
	ReturnToMainMenu();
}

void UEpisodeEditorToolbarWidget::BindControls()
{
	if (SaveButton)
	{
		SaveButton->OnClicked.RemoveDynamic(this, &UEpisodeEditorToolbarWidget::HandleSaveButtonClicked);
		SaveButton->OnClicked.AddDynamic(this, &UEpisodeEditorToolbarWidget::HandleSaveButtonClicked);
	}

	if (ReturnToMainMenuButton)
	{
		ReturnToMainMenuButton->OnClicked.RemoveDynamic(this, &UEpisodeEditorToolbarWidget::HandleReturnButtonClicked);
		ReturnToMainMenuButton->OnClicked.AddDynamic(this, &UEpisodeEditorToolbarWidget::HandleReturnButtonClicked);
	}
}

void UEpisodeEditorToolbarWidget::RequestEditorWidgetInputMode()
{
	if (AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(GetOwningPlayer()))
	{
		UWidget* focusWidget = ResolveInputModeFocusWidget();
		RequestedInputModeFocusWidget = focusWidget;
		editorController->RequestEditorWidgetInputMode(focusWidget);
	}
}

void UEpisodeEditorToolbarWidget::ReleaseEditorWidgetInputMode()
{
	if (AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(GetOwningPlayer()))
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

void UEpisodeEditorToolbarWidget::SetStatusText(const FString& message)
{
	if (StatusTextBlock)
	{
		StatusTextBlock->SetText(FText::FromString(message));
	}
}

UWidget* UEpisodeEditorToolbarWidget::ResolveInputModeFocusWidget() const
{
	return ToolbarInputModeFocus.Get();
}

FString UEpisodeEditorToolbarWidget::ResolveSavePath() const
{
	if (const AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(GetOwningPlayer()))
	{
		const FString sourcePath = editorController->GetSourceEpisodeSetupJsonPath();
		if (!sourcePath.IsEmpty())
		{
			return sourcePath;
		}
	}

	return MakeUniqueSavePath(DefaultSavePath);
}
