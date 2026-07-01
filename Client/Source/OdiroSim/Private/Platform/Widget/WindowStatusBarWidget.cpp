#include "Platform/Widget/WindowStatusBarWidget.h"

#include "Platform/Widget/WindowActionBarWidget.h"
#include "Platform/Widget/WindowTabBarWidget.h"

namespace
{
	const FName ConfirmActionId(TEXT("Confirm"));
	const FName RunActionId(TEXT("Run"));

	// Status Bar action slot의 기본 logical 상태를 만든다.
	FWindowActionButtonConfig MakeDefaultActionConfig(const EWindowStatusBarActionSlot slot)
	{
		FWindowActionButtonConfig config;
		config.ActionId = slot == EWindowStatusBarActionSlot::Run ? RunActionId : ConfirmActionId;
		config.bVisible = true;
		config.bEnabled = true;
		return config;
	}
}

FName UWindowStatusBarWidget::GetConfirmActionId()
{
	return ConfirmActionId;
}

FName UWindowStatusBarWidget::GetRunActionId()
{
	return RunActionId;
}

FName UWindowStatusBarWidget::GetActionIdForSlot(const EWindowStatusBarActionSlot actionSlot)
{
	return actionSlot == EWindowStatusBarActionSlot::Run ? RunActionId : ConfirmActionId;
}

UWindowStatusBarWidget::UWindowStatusBarWidget(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
	, ConfirmActionConfig(MakeDefaultActionConfig(EWindowStatusBarActionSlot::Confirm))
	, RunActionConfig(MakeDefaultActionConfig(EWindowStatusBarActionSlot::Run))
{
}

void UWindowStatusBarWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ConfigureActionButtons();
}

void UWindowStatusBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ConfigureActionButtons();
	BindControls();
}

void UWindowStatusBarWidget::NativeDestruct()
{
	UnbindControls();
	Super::NativeDestruct();
}

void UWindowStatusBarWidget::SetActiveTab(const FName tabId)
{
	if (TabBar)
	{
		TabBar->SetActiveTab(tabId);
	}
}

void UWindowStatusBarWidget::SetTabVisible(const FName tabId, const bool bVisible)
{
	if (TabBar)
	{
		TabBar->SetTabVisible(tabId, bVisible);
	}
}

void UWindowStatusBarWidget::SetTabBarVisible(const bool bVisible)
{
	if (TabBar)
	{
		TabBar->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
}

FName UWindowStatusBarWidget::GetActiveTab() const
{
	return TabBar ? TabBar->GetActiveTab() : UWindowTabBarWidget::GetStartupTabId();
}

void UWindowStatusBarWidget::SetResultTabs(const TArray<FWindowTabConfig>& tabs)
{
	if (TabBar)
	{
		TabBar->SetResultTabs(tabs);
	}
}

void UWindowStatusBarWidget::SetActionButtons(const TArray<FWindowActionButtonConfig>& actions)
{
	if (ActionBar)
	{
		ActionBar->SetActionButtons(actions);
	}
}

void UWindowStatusBarWidget::SetActionButtonConfig(
	const EWindowStatusBarActionSlot actionSlot,
	const FWindowActionButtonConfig& config)
{
	FWindowActionButtonConfig normalizedConfig = config;
	normalizedConfig.ActionId = ResolveActionIdBySlot(actionSlot);
	ResolveActionConfigBySlot(actionSlot) = normalizedConfig;
	ConfigureActionButtons();
}

void UWindowStatusBarWidget::SetConfirmActionButtonConfig(const FWindowActionButtonConfig& config)
{
	SetActionButtonConfig(EWindowStatusBarActionSlot::Confirm, config);
}

void UWindowStatusBarWidget::SetRunActionButtonConfig(const FWindowActionButtonConfig& config)
{
	SetActionButtonConfig(EWindowStatusBarActionSlot::Run, config);
}

void UWindowStatusBarWidget::SetActionButtonVisible(
	const EWindowStatusBarActionSlot actionSlot,
	const bool bVisible)
{
	FWindowActionButtonConfig& config = ResolveActionConfigBySlot(actionSlot);
	config.ActionId = ResolveActionIdBySlot(actionSlot);
	config.bVisible = bVisible;
	ConfigureActionButtons();
}

void UWindowStatusBarWidget::ResetActionButtonConfigs()
{
	ConfirmActionConfig = MakeDefaultActionConfig(EWindowStatusBarActionSlot::Confirm);
	RunActionConfig = MakeDefaultActionConfig(EWindowStatusBarActionSlot::Run);
	ConfigureActionButtons();
}

void UWindowStatusBarWidget::ConfigureActionButtons()
{
	if (!ActionBar)
	{
		return;
	}

	TArray<FWindowActionButtonConfig> actionConfigs;
	actionConfigs.Reserve(2);
	actionConfigs.Add(ConfirmActionConfig);
	actionConfigs.Add(RunActionConfig);
	ActionBar->SetActionButtons(actionConfigs);
}

void UWindowStatusBarWidget::BindControls()
{
	if (TabBar)
	{
		TabBar->OnTabSelectedNative.RemoveAll(this);
		TabBar->OnTabSelectedNative.AddUObject(this, &UWindowStatusBarWidget::HandleTabBarTabSelected);
		TabBar->OnResultTabCloseRequestedNative.RemoveAll(this);
		TabBar->OnResultTabCloseRequestedNative.AddUObject(
			this,
			&UWindowStatusBarWidget::HandleTabBarResultTabCloseRequested);
	}
	if (ActionBar)
	{
		ActionBar->OnActionRequestedNative.RemoveAll(this);
		ActionBar->OnActionRequestedNative.AddUObject(this, &UWindowStatusBarWidget::HandleActionBarActionRequested);
	}
}

void UWindowStatusBarWidget::UnbindControls()
{
	if (TabBar)
	{
		TabBar->OnTabSelectedNative.RemoveAll(this);
		TabBar->OnResultTabCloseRequestedNative.RemoveAll(this);
	}
	if (ActionBar)
	{
		ActionBar->OnActionRequestedNative.RemoveAll(this);
	}
}

FWindowActionButtonConfig& UWindowStatusBarWidget::ResolveActionConfigBySlot(
	const EWindowStatusBarActionSlot actionSlot)
{
	return actionSlot == EWindowStatusBarActionSlot::Run
		? RunActionConfig
		: ConfirmActionConfig;
}

const FWindowActionButtonConfig& UWindowStatusBarWidget::ResolveActionConfigBySlot(
	const EWindowStatusBarActionSlot actionSlot) const
{
	return actionSlot == EWindowStatusBarActionSlot::Run
		? RunActionConfig
		: ConfirmActionConfig;
}

FName UWindowStatusBarWidget::ResolveActionIdBySlot(const EWindowStatusBarActionSlot actionSlot) const
{
	return GetActionIdForSlot(actionSlot);
}

void UWindowStatusBarWidget::HandleTabBarTabSelected(const FName tabId)
{
	OnTabSelectedNative.Broadcast(tabId);
	OnTabSelected.Broadcast(tabId);
	BP_OnTabSelected(tabId);
}

void UWindowStatusBarWidget::HandleTabBarResultTabCloseRequested(const FName tabId)
{
	OnResultTabCloseRequestedNative.Broadcast(tabId);
}

void UWindowStatusBarWidget::HandleActionBarActionRequested(const FName actionId)
{
	OnActionRequestedNative.Broadcast(actionId);
	OnActionRequested.Broadcast(actionId);
	BP_OnActionRequested(actionId);

	if (actionId == GetConfirmActionId())
	{
		OnConfirmActionRequestedNative.Broadcast();
		OnConfirmActionRequested.Broadcast();
		BP_OnConfirmActionRequested();
	}
	else if (actionId == GetRunActionId())
	{
		OnRunActionRequestedNative.Broadcast();
		OnRunActionRequested.Broadcast();
		BP_OnRunActionRequested();
	}
}
