#include "Platform/Widget/WindowActionBarWidget.h"

#include "Blueprint/UserWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/Widget.h"
#include "UI/BaseButtonWidget.h"

namespace
{
	bool HasActionSizeConstraints(const FBaseWidgetSizeConstraints& constraints)
	{
		return constraints.MinWidth > 0.0f
			|| constraints.MinHeight > 0.0f
			|| constraints.MaxWidth > 0.0f
			|| constraints.MaxHeight > 0.0f;
	}
}

void UWindowActionBarWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	EnsureActionButtonConfigsFromDefaults();
	RebuildActionButtons();
}

void UWindowActionBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureActionButtonConfigsFromDefaults();
	RebuildActionButtons();
	BindControls();
}

void UWindowActionBarWidget::NativeDestruct()
{
	UnbindControls();
	Super::NativeDestruct();
}

void UWindowActionBarWidget::SetActionButtons(const TArray<FWindowActionButtonConfig>& actions)
{
	UnbindControls();
	ActionButtonConfigs = actions;
	RebuildActionButtons();
	BindControls();
}

void UWindowActionBarWidget::SetActionButtonConfig(
	const FName actionId,
	const FWindowActionButtonConfig& config)
{
	if (actionId.IsNone())
	{
		return;
	}

	EnsureActionButtonConfigsFromDefaults();

	FWindowActionButtonConfig normalizedConfig = config;
	normalizedConfig.ActionId = actionId;

	const int32 configIndex = FindActionConfigIndex(actionId);
	if (configIndex == INDEX_NONE)
	{
		ActionButtonConfigs.Add(normalizedConfig);
	}
	else
	{
		ActionButtonConfigs[configIndex] = normalizedConfig;
	}

	RebuildActionButtons();
	BindControls();
}

void UWindowActionBarWidget::SetActionButtonVisible(const FName actionId, const bool bVisible)
{
	if (actionId.IsNone())
	{
		return;
	}

	EnsureActionButtonConfigsFromDefaults();

	const int32 configIndex = FindActionConfigIndex(actionId);
	if (configIndex == INDEX_NONE)
	{
		FWindowActionButtonConfig config;
		config.ActionId = actionId;
		config.bVisible = bVisible;
		ActionButtonConfigs.Add(config);
	}
	else
	{
		ActionButtonConfigs[configIndex].bVisible = bVisible;
	}

	RebuildActionButtons();
	BindControls();
}

void UWindowActionBarWidget::ClearActionButtons()
{
	UnbindControls();
	ActionButtonConfigs.Reset();
	if (ActionButtonContainer)
	{
		ActionButtonContainer->ClearChildren();
	}
	ActionButtonsById.Reset();
}

void UWindowActionBarWidget::RebuildActionButtons()
{
	if (!ActionButtonContainer)
	{
		return;
	}

	for (const TPair<FName, TObjectPtr<UBaseButtonWidget>>& buttonPair : ActionButtonsById)
	{
		if (buttonPair.Value)
		{
			UnbindActionButton(buttonPair.Value.Get());
		}
	}
	ActionButtonsById.Reset();
	ActionButtonContainer->ClearChildren();

	if (!ActionButtonWidgetClass)
	{
		return;
	}

	for (const FWindowActionButtonConfig& config : ActionButtonConfigs)
	{
		if (config.ActionId.IsNone())
		{
			continue;
		}

		UBaseButtonWidget* actionButton = CreateWidget<UBaseButtonWidget>(GetWorld(), ActionButtonWidgetClass);
		if (!actionButton)
		{
			continue;
		}

		ActionButtonContainer->AddChild(actionButton);
		ActionButtonsById.Add(config.ActionId, actionButton);
		ConfigureActionButton(actionButton, config);
	}
}

void UWindowActionBarWidget::BindControls()
{
	for (const TPair<FName, TObjectPtr<UBaseButtonWidget>>& buttonPair : ActionButtonsById)
	{
		BindActionButton(buttonPair.Value.Get());
	}
}

void UWindowActionBarWidget::UnbindControls()
{
	for (const TPair<FName, TObjectPtr<UBaseButtonWidget>>& buttonPair : ActionButtonsById)
	{
		UnbindActionButton(buttonPair.Value.Get());
	}
}

void UWindowActionBarWidget::ConfigureActionButton(
	UBaseButtonWidget* actionButton,
	const FWindowActionButtonConfig& config) const
{
	if (!actionButton)
	{
		return;
	}

	const ESlateVisibility visibility = config.bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	actionButton->SetVisibility(visibility);
	actionButton->SetDisabled(!config.bEnabled);
	if (!config.Label.IsEmpty())
	{
		actionButton->SetToolTipText(config.Label);
	}
	if (config.Icon)
	{
		actionButton->SetIcon(config.Icon.Get());
	}
	if (!config.IconGlyphText.IsEmpty())
	{
		actionButton->SetIconGlyphText(config.IconGlyphText);
	}
	if (config.IconSize > 0.0f)
	{
		actionButton->SetIconSize(config.IconSize);
	}
	if (config.bOverrideSize)
	{
		actionButton->SetSizesOverride(UBaseWidgetSizeCatalog::MakePresetCatalogReference(config.Size));
	}
	if (HasActionSizeConstraints(config.SizeConstraints))
	{
		actionButton->SetSizeConstraints(config.SizeConstraints);
	}
	if (config.bOverridePrimary)
	{
		actionButton->SetPrimary(config.bPrimary);
	}
	if (config.bOverrideVariant)
	{
		actionButton->SetVariant(config.Variant);
	}
}

void UWindowActionBarWidget::EnsureActionButtonConfigsFromDefaults()
{
	if (ActionButtonConfigs.IsEmpty() && !DefaultActionButtons.IsEmpty())
	{
		ActionButtonConfigs = DefaultActionButtons;
	}
}

void UWindowActionBarWidget::BindActionButton(UBaseButtonWidget* actionButton)
{
	if (!actionButton)
	{
		return;
	}

	actionButton->OnBaseClicked.RemoveDynamic(this, &UWindowActionBarWidget::HandleActionClicked);
	actionButton->OnBaseClicked.AddDynamic(this, &UWindowActionBarWidget::HandleActionClicked);
}

void UWindowActionBarWidget::UnbindActionButton(UBaseButtonWidget* actionButton)
{
	if (actionButton)
	{
		actionButton->OnBaseClicked.RemoveDynamic(this, &UWindowActionBarWidget::HandleActionClicked);
	}
}

FName UWindowActionBarWidget::ResolveActionIdByButton(const UBaseButtonWidget* actionButton) const
{
	for (const TPair<FName, TObjectPtr<UBaseButtonWidget>>& buttonPair : ActionButtonsById)
	{
		if (actionButton == buttonPair.Value.Get())
		{
			return buttonPair.Key;
		}
	}
	return NAME_None;
}

int32 UWindowActionBarWidget::FindActionConfigIndex(const FName actionId) const
{
	for (int32 configIndex = 0; configIndex < ActionButtonConfigs.Num(); ++configIndex)
	{
		if (ActionButtonConfigs[configIndex].ActionId == actionId)
		{
			return configIndex;
		}
	}
	return INDEX_NONE;
}

void UWindowActionBarWidget::HandleActionClicked(UBaseButtonWidget* button)
{
	if (!IsValid(button))
	{
		return;
	}

	const FName actionId = ResolveActionIdByButton(button);
	if (actionId.IsNone())
	{
		return;
	}

	OnActionRequestedNative.Broadcast(actionId);
	OnActionRequested.Broadcast(actionId);
}
