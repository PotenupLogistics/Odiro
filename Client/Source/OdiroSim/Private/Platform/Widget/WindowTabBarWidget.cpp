#include "Platform/Widget/WindowTabBarWidget.h"

#include "Blueprint/UserWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "UI/BaseButtonWidget.h"
#include "UI/BaseTabWidget.h"
#include "Platform/Widget/WindowResultTabWidget.h"

namespace
{
	const FName StartupTabId(TEXT("Startup"));
	const FName OverviewTabId(TEXT("Overview"));
	const FName ScenarioTabId(TEXT("Scenario"));
	const FName RobotTabId(TEXT("Robot"));
	const FName ExperimentTabId(TEXT("Experiment"));

	// 기본 active tab은 startup entry와 맞춘다.
	FName ResolveDefaultWindowTabId()
	{
		return StartupTabId;
	}

	bool HasExplicitSizeConstraints(const FBaseWidgetSizeConstraints& constraints)
	{
		return constraints.MinWidth > 0.0f
			|| constraints.MinHeight > 0.0f
			|| constraints.MaxWidth > 0.0f
			|| constraints.MaxHeight > 0.0f;
	}

}

FName UWindowTabBarWidget::GetStartupTabId()
{
	return StartupTabId;
}

FName UWindowTabBarWidget::GetOverviewTabId()
{
	return OverviewTabId;
}

FName UWindowTabBarWidget::GetScenarioTabId()
{
	return ScenarioTabId;
}

FName UWindowTabBarWidget::GetRobotTabId()
{
	return RobotTabId;
}

FName UWindowTabBarWidget::GetExperimentTabId()
{
	return ExperimentTabId;
}

void UWindowTabBarWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ConfigureFixedTabs();
	ApplyTabStates();
}

void UWindowTabBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ConfigureFixedTabs();
	RebuildResultTabs();
	BindControls();
	ApplyTabStates();
}

void UWindowTabBarWidget::NativeDestruct()
{
	UnbindControls();
	Super::NativeDestruct();
}

void UWindowTabBarWidget::SetCompactMode(const bool bInCompactMode)
{
	if (bCompactMode == bInCompactMode)
	{
		return;
	}

	bCompactMode = bInCompactMode;
	for (const FName& tabId : ResultTabOrder)
	{
		if (const FWindowTabConfig* config = ResultTabConfigsById.Find(tabId))
		{
			if (TObjectPtr<UBaseTabWidget>* tabWidget = ResultTabsById.Find(tabId))
			{
				ConfigureTab(tabWidget->Get(), *config);
			}
		}
	}
	BP_OnCompactModeChanged(bCompactMode);
	ApplyTabStates();
}

void UWindowTabBarWidget::SetActiveTab(const FName tabId)
{
	const FName normalizedTabId = tabId.IsNone() ? ResolveDefaultWindowTabId() : tabId;
	if (!IsKnownTabId(normalizedTabId))
	{
		return;
	}

	ActiveTabId = normalizedTabId;
	ApplyTabStates();
}

void UWindowTabBarWidget::SetTabVisible(const FName tabId, const bool bVisible)
{
	if (!IsKnownTabId(tabId) && !IsFixedTabId(tabId))
	{
		return;
	}

	TabVisibilityById.FindOrAdd(tabId) = bVisible;
	ApplyTabVisibility(tabId, bVisible);
}

FName UWindowTabBarWidget::GetActiveTab() const
{
	return ActiveTabId.IsNone() ? ResolveDefaultWindowTabId() : ActiveTabId;
}

void UWindowTabBarWidget::SetResultTabs(const TArray<FWindowTabConfig>& tabs)
{
	UnbindControls();
	ResultTabOrder.Reset();
	ResultTabConfigsById.Reset();
	ResultTabsById.Reset();

	for (const FWindowTabConfig& tab : tabs)
	{
		if (tab.TabId.IsNone() || IsFixedTabId(tab.TabId))
		{
			continue;
		}

		ResultTabOrder.Add(tab.TabId);
		ResultTabConfigsById.Add(tab.TabId, tab);
		TabVisibilityById.FindOrAdd(tab.TabId) = tab.bVisible;
	}

	RebuildResultTabs();
	BindControls();
	ApplyTabStates();
}

void UWindowTabBarWidget::AddOrUpdateResultTab(const FWindowTabConfig& tab)
{
	if (tab.TabId.IsNone() || IsFixedTabId(tab.TabId))
	{
		return;
	}

	if (!ResultTabConfigsById.Contains(tab.TabId))
	{
		ResultTabOrder.Add(tab.TabId);
	}
	ResultTabConfigsById.Add(tab.TabId, tab);
	TabVisibilityById.FindOrAdd(tab.TabId) = tab.bVisible;

	RebuildResultTabs();
	BindControls();
	ApplyTabStates();
}

void UWindowTabBarWidget::RemoveResultTab(const FName tabId)
{
	if (tabId.IsNone() || IsFixedTabId(tabId))
	{
		return;
	}

	if (TObjectPtr<UBaseTabWidget>* tabWidget = ResultTabsById.Find(tabId))
	{
		if (tabWidget->Get())
		{
			UnbindTab(tabWidget->Get());
			UnbindResultTabClose(tabWidget->Get());
			tabWidget->Get()->RemoveFromParent();
		}
	}
	ResultTabsById.Remove(tabId);
	ResultTabConfigsById.Remove(tabId);
	ResultTabOrder.Remove(tabId);
	TabVisibilityById.Remove(tabId);

	if (GetActiveTab() == tabId)
	{
		SetActiveTab(ResolveDefaultWindowTabId());
	}
}

void UWindowTabBarWidget::ClearResultTabs()
{
	UnbindControls();
	if (ResultTabContainer)
	{
		ResultTabContainer->ClearChildren();
	}
	ResultTabsById.Reset();
	ResultTabConfigsById.Reset();
	ResultTabOrder.Reset();
	ApplyTabStates();
}

void UWindowTabBarWidget::ConfigureFixedTabs()
{
	ConfigureFixedTab(StartupTab.Get(), StartupTabConfig);
	ConfigureFixedTab(OverviewTab.Get(), OverviewTabConfig);
	ConfigureFixedTab(ScenarioTab.Get(), ScenarioTabConfig);
	ConfigureFixedTab(RobotTab.Get(), RobotTabConfig);
	ConfigureFixedTab(ExperimentTab.Get(), ExperimentTabConfig);

	ApplyTabVisibility(StartupTabId, IsTabVisible(StartupTabId));
	ApplyTabVisibility(OverviewTabId, IsTabVisible(OverviewTabId));
	ApplyTabVisibility(ScenarioTabId, IsTabVisible(ScenarioTabId));
	ApplyTabVisibility(RobotTabId, IsTabVisible(RobotTabId));
	ApplyTabVisibility(ExperimentTabId, IsTabVisible(ExperimentTabId));
}

void UWindowTabBarWidget::ConfigureFixedTab(
	UBaseTabWidget* tabWidget,
	const FWindowFixedTabConfig& config)
{
	if (!tabWidget)
	{
		return;
	}

	ApplyTabDimensions(tabWidget, config.SizeConstraints);
}

void UWindowTabBarWidget::ApplyTabDimensions(UBaseTabWidget* tabWidget, const FBaseWidgetSizeConstraints& constraints) const
{
	if (!tabWidget || !HasExplicitSizeConstraints(constraints))
	{
		return;
	}

	tabWidget->SetSizeConstraints(constraints);
}

void UWindowTabBarWidget::RebuildResultTabs()
{
	if (!ResultTabContainer)
	{
		return;
	}

	for (const TPair<FName, TObjectPtr<UBaseTabWidget>>& tabPair : ResultTabsById)
	{
		if (tabPair.Value)
		{
			UnbindTab(tabPair.Value.Get());
			UnbindResultTabClose(tabPair.Value.Get());
		}
	}
	ResultTabsById.Reset();
	ResultTabContainer->ClearChildren();

	if (!ResultTabWidgetClass)
	{
		return;
	}

	for (const FName& tabId : ResultTabOrder)
	{
		const FWindowTabConfig* config = ResultTabConfigsById.Find(tabId);
		if (!config)
		{
			continue;
		}

		UBaseTabWidget* tabWidget = CreateWidget<UBaseTabWidget>(GetWorld(), ResultTabWidgetClass);
		if (!tabWidget)
		{
			continue;
		}

		ResultTabContainer->AddChild(tabWidget);
		ResultTabsById.Add(tabId, tabWidget);
		ConfigureTab(tabWidget, *config);
		BindResultTabClose(tabWidget);
	}
}

void UWindowTabBarWidget::BindControls()
{
	BindTab(StartupTab.Get());
	BindTab(OverviewTab.Get());
	BindTab(ScenarioTab.Get());
	BindTab(RobotTab.Get());
	BindTab(ExperimentTab.Get());

	for (const TPair<FName, TObjectPtr<UBaseTabWidget>>& tabPair : ResultTabsById)
	{
		BindTab(tabPair.Value.Get());
		BindResultTabClose(tabPair.Value.Get());
	}
}

void UWindowTabBarWidget::UnbindControls()
{
	UnbindTab(StartupTab.Get());
	UnbindTab(OverviewTab.Get());
	UnbindTab(ScenarioTab.Get());
	UnbindTab(RobotTab.Get());
	UnbindTab(ExperimentTab.Get());

	for (const TPair<FName, TObjectPtr<UBaseTabWidget>>& tabPair : ResultTabsById)
	{
		UnbindTab(tabPair.Value.Get());
		UnbindResultTabClose(tabPair.Value.Get());
	}
}

void UWindowTabBarWidget::ConfigureTab(UBaseTabWidget* tabWidget, const FWindowTabConfig& config)
{
	if (!tabWidget)
	{
		return;
	}

	if (!config.Label.IsEmpty())
	{
		tabWidget->SetLabel(config.Label);
	}
	if (config.Icon)
	{
		tabWidget->SetIcon(config.Icon.Get());
	}
	if (!config.IconGlyphText.IsEmpty())
	{
		tabWidget->SetIconGlyphText(config.IconGlyphText);
	}
	if (!config.Label.IsEmpty())
	{
		tabWidget->SetToolTipText(config.Label);
	}
	ApplyTabDimensions(tabWidget, config.SizeConstraints);
	if (UWindowResultTabWidget* resultTabWidget = Cast<UWindowResultTabWidget>(tabWidget))
	{
		resultTabWidget->SetClosable(config.bClosable);
	}
	ApplyTabVisibility(config.TabId, IsTabVisible(config.TabId));
}

void UWindowTabBarWidget::BindTab(UBaseTabWidget* tabWidget)
{
	if (!tabWidget)
	{
		return;
	}

	tabWidget->OnBaseClicked.RemoveDynamic(this, &UWindowTabBarWidget::HandleTabSelected);
	tabWidget->OnBaseClicked.AddDynamic(this, &UWindowTabBarWidget::HandleTabSelected);
}

void UWindowTabBarWidget::UnbindTab(UBaseTabWidget* tabWidget)
{
	if (tabWidget)
	{
		tabWidget->OnBaseClicked.RemoveDynamic(this, &UWindowTabBarWidget::HandleTabSelected);
	}
}

void UWindowTabBarWidget::BindResultTabClose(UBaseTabWidget* tabWidget)
{
	if (UWindowResultTabWidget* resultTabWidget = Cast<UWindowResultTabWidget>(tabWidget))
	{
		resultTabWidget->OnCloseRequestedNative.RemoveAll(this);
		resultTabWidget->OnCloseRequestedNative.AddUObject(this, &UWindowTabBarWidget::HandleResultTabCloseRequested);
	}
}

void UWindowTabBarWidget::UnbindResultTabClose(UBaseTabWidget* tabWidget)
{
	if (UWindowResultTabWidget* resultTabWidget = Cast<UWindowResultTabWidget>(tabWidget))
	{
		resultTabWidget->OnCloseRequestedNative.RemoveAll(this);
	}
}

void UWindowTabBarWidget::ApplyTabStates()
{
	const FName activeTabId = GetActiveTab();
	if (StartupTab)
	{
		StartupTab->SetSelected(activeTabId == StartupTabId);
	}
	if (OverviewTab)
	{
		OverviewTab->SetSelected(activeTabId == OverviewTabId);
	}
	if (ScenarioTab)
	{
		ScenarioTab->SetSelected(activeTabId == ScenarioTabId);
	}
	if (RobotTab)
	{
		RobotTab->SetSelected(activeTabId == RobotTabId);
	}
	if (ExperimentTab)
	{
		ExperimentTab->SetSelected(activeTabId == ExperimentTabId);
	}

	for (const TPair<FName, TObjectPtr<UBaseTabWidget>>& tabPair : ResultTabsById)
	{
		if (tabPair.Value)
		{
			tabPair.Value->SetSelected(activeTabId == tabPair.Key);
		}
	}
}

void UWindowTabBarWidget::ApplyTabVisibility(const FName tabId, const bool bVisible)
{
	const ESlateVisibility visibility = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (UBaseTabWidget* tabWidget = ResolveTabById(tabId))
	{
		tabWidget->SetVisibility(visibility);
	}
	if (UWidget* fixedTabHost = ResolveFixedTabHostById(tabId))
	{
		fixedTabHost->SetVisibility(visibility);
	}
}

bool UWindowTabBarWidget::IsTabVisible(const FName tabId) const
{
	if (const bool* bVisible = TabVisibilityById.Find(tabId))
	{
		return *bVisible;
	}

	if (const FWindowTabConfig* resultConfig = ResultTabConfigsById.Find(tabId))
	{
		return resultConfig->bVisible;
	}
	return true;
}

UBaseTabWidget* UWindowTabBarWidget::ResolveTabById(const FName tabId) const
{
	if (tabId == StartupTabId)
	{
		return StartupTab.Get();
	}
	if (tabId == OverviewTabId)
	{
		return OverviewTab.Get();
	}
	if (tabId == ScenarioTabId)
	{
		return ScenarioTab.Get();
	}
	if (tabId == RobotTabId)
	{
		return RobotTab.Get();
	}
	if (tabId == ExperimentTabId)
	{
		return ExperimentTab.Get();
	}
	if (const TObjectPtr<UBaseTabWidget>* resultTab = ResultTabsById.Find(tabId))
	{
		return resultTab->Get();
	}
	return nullptr;
}

FName UWindowTabBarWidget::ResolveTabIdByButton(const UBaseButtonWidget* tabWidget) const
{
	if (tabWidget == StartupTab.Get())
	{
		return StartupTabId;
	}
	if (tabWidget == OverviewTab.Get())
	{
		return OverviewTabId;
	}
	if (tabWidget == ScenarioTab.Get())
	{
		return ScenarioTabId;
	}
	if (tabWidget == RobotTab.Get())
	{
		return RobotTabId;
	}
	if (tabWidget == ExperimentTab.Get())
	{
		return ExperimentTabId;
	}

	for (const TPair<FName, TObjectPtr<UBaseTabWidget>>& tabPair : ResultTabsById)
	{
		if (tabWidget == tabPair.Value.Get())
		{
			return tabPair.Key;
		}
	}
	return NAME_None;
}

UWidget* UWindowTabBarWidget::ResolveFixedTabHostById(const FName tabId) const
{
	if (tabId == StartupTabId)
	{
		return StartupTabHost.Get();
	}
	if (tabId == OverviewTabId)
	{
		return OverviewTabHost.Get();
	}
	if (tabId == ScenarioTabId)
	{
		return ScenarioTabHost.Get();
	}
	if (tabId == RobotTabId)
	{
		return RobotTabHost.Get();
	}
	if (tabId == ExperimentTabId)
	{
		return ExperimentTabHost.Get();
	}
	return nullptr;
}

bool UWindowTabBarWidget::IsFixedTabId(const FName tabId) const
{
	return tabId == StartupTabId
		|| tabId == OverviewTabId
		|| tabId == ScenarioTabId
		|| tabId == RobotTabId
		|| tabId == ExperimentTabId;
}

bool UWindowTabBarWidget::IsKnownTabId(const FName tabId) const
{
	return IsFixedTabId(tabId) || ResultTabConfigsById.Contains(tabId) || ResultTabsById.Contains(tabId);
}

void UWindowTabBarWidget::BroadcastTabSelected(const FName tabId)
{
	OnTabSelectedNative.Broadcast(tabId);
	OnTabSelected.Broadcast(tabId);
	BP_OnTabSelected(tabId);
}

void UWindowTabBarWidget::HandleTabSelected(UBaseButtonWidget* tabWidget)
{
	if (!IsValid(tabWidget))
	{
		return;
	}

	const FName tabId = ResolveTabIdByButton(tabWidget);
	if (!IsKnownTabId(tabId))
	{
		return;
	}

	SetActiveTab(tabId);
	BroadcastTabSelected(tabId);
}

void UWindowTabBarWidget::HandleResultTabCloseRequested(UWindowResultTabWidget* tabWidget)
{
	if (!IsValid(tabWidget))
	{
		return;
	}

	const FName tabId = ResolveTabIdByButton(tabWidget);
	if (tabId.IsNone() || IsFixedTabId(tabId))
	{
		return;
	}

	OnResultTabCloseRequestedNative.Broadcast(tabId);
}
