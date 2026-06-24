#include "Platform/Widget/ExperimentResultIterationSelectorWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Platform/ViewModel/OdiroListItemViewModel.h"

void UExperimentResultIterationSelectorWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (SelectorButton)
	{
		SelectorButton->OnClicked.RemoveDynamic(
			this,
			&UExperimentResultIterationSelectorWidget::HandleSelectorButtonClicked);
		SelectorButton->OnClicked.AddDynamic(
			this,
			&UExperimentResultIterationSelectorWidget::HandleSelectorButtonClicked);
	}
}

void UExperimentResultIterationSelectorWidget::NativeDestruct()
{
	if (SelectorButton)
	{
		SelectorButton->OnClicked.RemoveDynamic(
			this,
			&UExperimentResultIterationSelectorWidget::HandleSelectorButtonClicked);
	}

	OnSelectorClicked.Clear();
	Super::NativeDestruct();
}

void UExperimentResultIterationSelectorWidget::InitializeFromItemViewModel(
	UOdiroListItemViewModel* itemViewModel)
{
	ItemViewModel = itemViewModel;
	RefreshFromItemViewModel();
}

FString UExperimentResultIterationSelectorWidget::GetResultPath() const
{
	return IsValid(ItemViewModel) ? ItemViewModel->GetPayloadPath() : FString();
}

void UExperimentResultIterationSelectorWidget::HandleSelectorButtonClicked()
{
	OnSelectorClicked.Broadcast(this);
}

void UExperimentResultIterationSelectorWidget::RefreshFromItemViewModel()
{
	if (!IsValid(ItemViewModel))
	{
		if (SelectorLabelText)
		{
			SelectorLabelText->SetText(FText::GetEmpty());
		}
		if (SelectorButton)
		{
			SelectorButton->SetIsEnabled(false);
		}
		if (SelectedIndicator)
		{
			SelectedIndicator->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (SelectorLabelText)
	{
		SelectorLabelText->SetText(FText::FromString(ItemViewModel->GetTitle()));
	}
	if (SelectorButton)
	{
		SelectorButton->SetIsEnabled(ItemViewModel->IsEnabled());
	}
	if (SelectedIndicator)
	{
		SelectedIndicator->SetVisibility(
			ItemViewModel->IsSelected()
				? ESlateVisibility::SelfHitTestInvisible
				: ESlateVisibility::Collapsed);
	}
}
