#include "Platform/Widget/FileListItemWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"

void UFileListItemWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (PathTextBox)
	{
		PathTextBox->OnTextChanged.RemoveDynamic(this, &UFileListItemWidget::HandlePathTextChanged);
		PathTextBox->OnTextChanged.AddDynamic(this, &UFileListItemWidget::HandlePathTextChanged);
	}

	if (RenameButton)
	{
		RenameButton->OnClicked.RemoveDynamic(this, &UFileListItemWidget::HandleRenameClicked);
		RenameButton->OnClicked.AddDynamic(this, &UFileListItemWidget::HandleRenameClicked);
	}

	if (PrimaryActionButton)
	{
		PrimaryActionButton->OnClicked.RemoveDynamic(this, &UFileListItemWidget::HandlePrimaryActionClicked);
		PrimaryActionButton->OnClicked.AddDynamic(this, &UFileListItemWidget::HandlePrimaryActionClicked);
	}

	if (SecondaryActionButton)
	{
		SecondaryActionButton->OnClicked.RemoveDynamic(this, &UFileListItemWidget::HandleSecondaryActionClicked);
		SecondaryActionButton->OnClicked.AddDynamic(this, &UFileListItemWidget::HandleSecondaryActionClicked);
	}

	RefreshButtonState();
}

void UFileListItemWidget::InitializeItem(
	const FString& itemPath,
	FString primaryActionLabel,
	FString secondaryActionLabel,
	const bool bInAllowRename,
	const bool bInAllowPrimaryAction,
	const bool bInAllowSecondaryAction)
{
	OriginalPath = itemPath.TrimStartAndEnd();
	bAllowRename = bInAllowRename;
	bAllowPrimaryAction = bInAllowPrimaryAction;
	bAllowSecondaryAction = bInAllowSecondaryAction;

	if (PathTextBox)
	{
		PathTextBox->SetText(FText::FromString(OriginalPath));
	}

	if (RenameButtonLabel)
	{
		RenameButtonLabel->SetText(FText::FromString(TEXT("이름 변경")));
	}

	if (PrimaryActionLabel)
	{
		PrimaryActionLabel->SetText(FText::FromString(primaryActionLabel));
	}

	if (SecondaryActionLabel)
	{
		SecondaryActionLabel->SetText(FText::FromString(secondaryActionLabel));
	}

	RefreshButtonState();
}

FString UFileListItemWidget::GetEditedPath() const
{
	return PathTextBox ? PathTextBox->GetText().ToString().TrimStartAndEnd() : FString();
}

void UFileListItemWidget::HandlePathTextChanged(const FText& text)
{
	(void)text;
	RefreshButtonState();
}

void UFileListItemWidget::HandleRenameClicked()
{
	OnRenameRequested.Broadcast(this, GetEditedPath());
}

void UFileListItemWidget::HandlePrimaryActionClicked()
{
	if (bAllowPrimaryAction)
	{
		OnPrimaryActionRequested.Broadcast(this);
	}
}

void UFileListItemWidget::HandleSecondaryActionClicked()
{
	if (bAllowSecondaryAction)
	{
		OnSecondaryActionRequested.Broadcast(this);
	}
}

void UFileListItemWidget::RefreshButtonState()
{
	if (RenameButton)
	{
		const FString editedPath = GetEditedPath();
		const bool bCanRename = bAllowRename
			&& !editedPath.IsEmpty()
			&& !editedPath.Equals(OriginalPath, ESearchCase::IgnoreCase);
		RenameButton->SetVisibility(bAllowRename ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		RenameButton->SetIsEnabled(bCanRename);
	}

	if (PrimaryActionButton)
	{
		PrimaryActionButton->SetVisibility(bAllowPrimaryAction ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		PrimaryActionButton->SetIsEnabled(bAllowPrimaryAction);
	}

	if (SecondaryActionButton)
	{
		SecondaryActionButton->SetVisibility(bAllowSecondaryAction ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		SecondaryActionButton->SetIsEnabled(bAllowSecondaryAction);
	}
}
