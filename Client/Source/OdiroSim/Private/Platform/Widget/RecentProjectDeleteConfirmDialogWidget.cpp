#include "Platform/Widget/RecentProjectDeleteConfirmDialogWidget.h"

#include "Components/TextBlock.h"
#include "Misc/Paths.h"
#include "Platform/Widget/PlatformWidgetRuntime.h"
#include "UI/BaseButtonWidget.h"

void URecentProjectDeleteConfirmDialogWidget::SetDeleteTarget(const FString& projectPath)
{
	DeleteTargetProjectPath = projectPath.TrimStartAndEnd();
	if (!DeleteTargetProjectPath.IsEmpty())
	{
		DeleteTargetProjectPath = FPaths::ConvertRelativePathToFull(DeleteTargetProjectPath);
		FPaths::NormalizeFilename(DeleteTargetProjectPath);
		FPaths::CollapseRelativeDirectories(DeleteTargetProjectPath);
	}

	RefreshDeleteTargetTexts();
}

void URecentProjectDeleteConfirmDialogWidget::NativeConstruct()
{
	Super::NativeConstruct();

	PlatformWidgetRuntime::ApplyFullscreenViewportSlot(this);

	if (ConfirmButton)
	{
		ConfirmButton->OnBaseClicked.RemoveDynamic(
			this,
			&URecentProjectDeleteConfirmDialogWidget::HandleConfirmClicked);
		ConfirmButton->OnBaseClicked.AddDynamic(
			this,
			&URecentProjectDeleteConfirmDialogWidget::HandleConfirmClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnBaseClicked.RemoveDynamic(
			this,
			&URecentProjectDeleteConfirmDialogWidget::HandleCancelClicked);
		CancelButton->OnBaseClicked.AddDynamic(
			this,
			&URecentProjectDeleteConfirmDialogWidget::HandleCancelClicked);
	}

	RefreshDeleteTargetTexts();
}

void URecentProjectDeleteConfirmDialogWidget::NativeDestruct()
{
	if (ConfirmButton)
	{
		ConfirmButton->OnBaseClicked.RemoveDynamic(
			this,
			&URecentProjectDeleteConfirmDialogWidget::HandleConfirmClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnBaseClicked.RemoveDynamic(
			this,
			&URecentProjectDeleteConfirmDialogWidget::HandleCancelClicked);
	}

	Super::NativeDestruct();
}

void URecentProjectDeleteConfirmDialogWidget::HandleConfirmClicked(UBaseButtonWidget*)
{
	OnConfirmed.Broadcast(this);
}

void URecentProjectDeleteConfirmDialogWidget::HandleCancelClicked(UBaseButtonWidget*)
{
	OnCanceled.Broadcast(this);
}

void URecentProjectDeleteConfirmDialogWidget::RefreshDeleteTargetTexts()
{
	if (ProjectFolderNameText)
	{
		ProjectFolderNameText->SetText(FText::FromString(FPaths::GetCleanFilename(DeleteTargetProjectPath)));
	}
	if (ProjectFolderPathText)
	{
		ProjectFolderPathText->SetText(FText::FromString(DeleteTargetProjectPath));
	}
}
