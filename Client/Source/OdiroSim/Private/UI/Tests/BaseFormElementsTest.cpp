#if WITH_DEV_AUTOMATION_TESTS

#include "UI/BaseFormElements.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBaseFormElementsTextInputTest,
	"OdiroSim.UI.BaseFormElements.TextInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBaseFormElementsTextInputTest::RunTest(const FString& parameters)
{
	(void)parameters;

	UBaseTextInputWidget* input = NewObject<UBaseTextInputWidget>();
	TestNotNull(TEXT("text input created"), input);
	if (!input)
	{
		return false;
	}

	input->SetInputMode(EBaseTextInputMode::Text);
	input->SetText(FText::FromString(TEXT("draft path")));
	TestEqual(TEXT("current text returns stored text without bound editable"), input->GetCurrentText().ToString(), FString(TEXT("draft path")));
	TestTrue(TEXT("commit current text succeeds in text mode"), input->CommitCurrentText());
	TestEqual(TEXT("commit current text preserves text"), input->GetText().ToString(), FString(TEXT("draft path")));

	input->SetValueRange(0.0f, 10.0f);
	input->SetInputMode(EBaseTextInputMode::Number);
	input->SetNumericValue(15.0f);
	TestEqual(TEXT("numeric value clamps to max"), input->GetNumericValue(), 10.0f);
	TestFalse(TEXT("invalid number commit fails"), input->CommitText(FText::FromString(TEXT("bad"))));
	TestEqual(TEXT("invalid number preserves value"), input->GetNumericValue(), 10.0f);
	TestFalse(TEXT("invalid number sets error"), input->GetErrorText().IsEmpty());
	TestTrue(TEXT("valid number commit succeeds"), input->CommitText(FText::FromString(TEXT("4.5"))));
	TestEqual(TEXT("valid number commits value"), input->GetNumericValue(), 4.5f);
	TestTrue(TEXT("valid number clears error"), input->GetErrorText().IsEmpty());

	input->SetInputMode(EBaseTextInputMode::NumberRange);
	input->SetValueRange(0.0f, 20.0f);
	input->SetRangeValue(18.0f, 3.0f);
	TestEqual(TEXT("range lower and upper are ordered"), input->GetLowerValue(), 3.0f);
	TestEqual(TEXT("range upper and lower are ordered"), input->GetUpperValue(), 18.0f);
	TestTrue(TEXT("valid range commit succeeds"), input->CommitText(FText::FromString(TEXT("2 - 9"))));
	TestEqual(TEXT("range lower commits"), input->GetLowerValue(), 2.0f);
	TestEqual(TEXT("range upper commits"), input->GetUpperValue(), 9.0f);
	TestTrue(TEXT("valid en dash range commit succeeds"), input->CommitText(FText::FromString(TEXT("3 \u2013 7"))));
	TestEqual(TEXT("en dash range lower commits"), input->GetLowerValue(), 3.0f);
	TestEqual(TEXT("en dash range upper commits"), input->GetUpperValue(), 7.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBaseFormElementsSliderTest,
	"OdiroSim.UI.BaseFormElements.Slider",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBaseFormElementsSliderTest::RunTest(const FString& parameters)
{
	(void)parameters;

	UBaseSliderWidget* slider = NewObject<UBaseSliderWidget>();
	TestNotNull(TEXT("slider created"), slider);
	if (!slider)
	{
		return false;
	}

	slider->SetValueRange(0.0f, 100.0f);
	slider->SetValue(125.0f);
	TestEqual(TEXT("single slider value clamps"), slider->GetValue(), 100.0f);
	slider->SetRangeMode(true);
	slider->SetRangeValue(75.0f, 25.0f);
	TestEqual(TEXT("range slider lower ordered"), slider->GetLowerValue(), 25.0f);
	TestEqual(TEXT("range slider upper ordered"), slider->GetUpperValue(), 75.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBaseFormElementsSelectionTest,
	"OdiroSim.UI.BaseFormElements.Selection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBaseFormElementsSelectionTest::RunTest(const FString& parameters)
{
	(void)parameters;

	FBaseDropdownItem dropdownA;
	dropdownA.Id = TEXT("A");
	dropdownA.Label = FText::FromString(TEXT("A"));
	FBaseDropdownItem dropdownB;
	dropdownB.Id = TEXT("B");
	dropdownB.Label = FText::FromString(TEXT("B"));
	dropdownB.bDisabled = true;

	UBaseDropdownWidget* dropdown = NewObject<UBaseDropdownWidget>();
	TestNotNull(TEXT("dropdown created"), dropdown);
	if (!dropdown)
	{
		return false;
	}

	dropdown->SetItems({ dropdownA, dropdownB });
	TestTrue(TEXT("dropdown selects enabled id"), dropdown->SelectItemById(TEXT("A")));
	TestEqual(TEXT("dropdown selected id"), dropdown->GetSelectedId(), FName(TEXT("A")));
	TestFalse(TEXT("dropdown rejects missing id"), dropdown->SelectItemById(TEXT("Missing")));
	TestEqual(TEXT("dropdown preserves selection after missing id"), dropdown->GetSelectedId(), FName(TEXT("A")));
	TestFalse(TEXT("dropdown rejects disabled id"), dropdown->SelectItemById(TEXT("B")));
	TestEqual(TEXT("dropdown preserves selection after disabled id"), dropdown->GetSelectedId(), FName(TEXT("A")));

	FBaseSwitcherItem switcherA;
	switcherA.Id = TEXT("One");
	switcherA.Label = FText::FromString(TEXT("One"));
	FBaseSwitcherItem switcherB;
	switcherB.Id = TEXT("Two");
	switcherB.Label = FText::FromString(TEXT("Two"));
	switcherB.bDisabled = true;

	UBaseSwitcherWidget* switcher = NewObject<UBaseSwitcherWidget>();
	TestNotNull(TEXT("switcher created"), switcher);
	if (!switcher)
	{
		return false;
	}

	switcher->SetItems({ switcherA, switcherB });
	TestTrue(TEXT("switcher selects enabled id"), switcher->SelectItemById(TEXT("One")));
	TestEqual(TEXT("switcher selected id"), switcher->GetSelectedId(), FName(TEXT("One")));
	TestFalse(TEXT("switcher rejects disabled id"), switcher->SelectItemById(TEXT("Two")));
	TestEqual(TEXT("switcher preserves selection after disabled id"), switcher->GetSelectedId(), FName(TEXT("One")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBaseFormElementsCheckBoxGroupTest,
	"OdiroSim.UI.BaseFormElements.CheckBoxGroup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBaseFormElementsCheckBoxGroupTest::RunTest(const FString& parameters)
{
	(void)parameters;

	FBaseCheckBoxGroupItem parent;
	parent.Id = TEXT("Parent");
	parent.Label = FText::FromString(TEXT("Parent"));

	FBaseCheckBoxGroupItem childA;
	childA.Id = TEXT("ChildA");
	childA.ParentId = parent.Id;
	childA.Label = FText::FromString(TEXT("Child A"));

	FBaseCheckBoxGroupItem childB;
	childB.Id = TEXT("ChildB");
	childB.ParentId = parent.Id;
	childB.Label = FText::FromString(TEXT("Child B"));

	UBaseCheckBoxGroupWidget* group = NewObject<UBaseCheckBoxGroupWidget>();
	TestNotNull(TEXT("checkbox group created"), group);
	if (!group)
	{
		return false;
	}

	group->SetItems({ parent, childA, childB });
	TestTrue(TEXT("child A checked"), group->SetItemCheckState(childA.Id, ECheckBoxState::Checked));
	TestEqual(TEXT("partial children set parent indeterminate"), group->GetItemCheckState(parent.Id), ECheckBoxState::Undetermined);
	TestTrue(TEXT("child B checked"), group->SetItemCheckState(childB.Id, ECheckBoxState::Checked));
	TestEqual(TEXT("all children set parent checked"), group->GetItemCheckState(parent.Id), ECheckBoxState::Checked);
	TestTrue(TEXT("parent unchecked cascades"), group->SetItemCheckState(parent.Id, ECheckBoxState::Unchecked));
	TestEqual(TEXT("child A unchecked by parent"), group->GetItemCheckState(childA.Id), ECheckBoxState::Unchecked);
	TestEqual(TEXT("child B unchecked by parent"), group->GetItemCheckState(childB.Id), ECheckBoxState::Unchecked);
	TestEqual(TEXT("parent unchecked after cascade"), group->GetItemCheckState(parent.Id), ECheckBoxState::Unchecked);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBaseFormElementsSurfaceSmokeTest,
	"OdiroSim.UI.BaseFormElements.SurfaceSmoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBaseFormElementsSurfaceSmokeTest::RunTest(const FString& parameters)
{
	(void)parameters;

	UBaseToggleButtonWidget* toggle = NewObject<UBaseToggleButtonWidget>();
	TestNotNull(TEXT("toggle created"), toggle);
	if (!toggle)
	{
		return false;
	}
	toggle->SetCheckState(ECheckBoxState::Undetermined);
	TestEqual(TEXT("toggle stores indeterminate"), toggle->GetCheckState(), ECheckBoxState::Undetermined);

	UBaseThumbnailCardWidget* thumbnail = NewObject<UBaseThumbnailCardWidget>();
	TestNotNull(TEXT("thumbnail created"), thumbnail);
	if (!thumbnail)
	{
		return false;
	}
	TestEqual(TEXT("thumbnail default full bleed"), thumbnail->GetMediaPaddingMode(), EBaseThumbnailMediaPaddingMode::FullBleed);
	thumbnail->SetMediaPaddingMode(EBaseThumbnailMediaPaddingMode::Inset);
	TestEqual(TEXT("thumbnail inset stored"), thumbnail->GetMediaPaddingMode(), EBaseThumbnailMediaPaddingMode::Inset);

	FBaseTreeRowItem treeRow;
	treeRow.Id = TEXT("Row");
	treeRow.Label = FText::FromString(TEXT("Row"));
	FBaseTreeRowItem disabledRow;
	disabledRow.Id = TEXT("Disabled");
	disabledRow.Label = FText::FromString(TEXT("Disabled"));
	disabledRow.bDisabled = true;

	UBaseTreeViewWidget* tree = NewObject<UBaseTreeViewWidget>();
	TestNotNull(TEXT("tree created"), tree);
	if (!tree)
	{
		return false;
	}
	tree->SetItems({ treeRow, disabledRow });
	TestTrue(TEXT("tree selects enabled row"), tree->SelectItemById(treeRow.Id));
	TestFalse(TEXT("tree rejects disabled row"), tree->SelectItemById(disabledRow.Id));
	TestEqual(TEXT("tree preserves selected row"), tree->GetSelectedId(), treeRow.Id);

	UBaseTooltipAnchorWidget* tooltipAnchor = NewObject<UBaseTooltipAnchorWidget>();
	TestNotNull(TEXT("tooltip anchor created"), tooltipAnchor);
	if (!tooltipAnchor)
	{
		return false;
	}
	tooltipAnchor->SetTooltipDelay(0.2f);
	tooltipAnchor->SetTooltipMessage(FText::FromString(TEXT("Tip")));
	TestEqual(TEXT("tooltip delay stored"), tooltipAnchor->GetTooltipDelay(), 0.2f);

	UBaseContextMenuAnchorWidget* contextAnchor = NewObject<UBaseContextMenuAnchorWidget>();
	TestNotNull(TEXT("context menu anchor created"), contextAnchor);
	if (!contextAnchor)
	{
		return false;
	}
	FBaseContextMenuItem menuItem;
	menuItem.Id = TEXT("Open");
	menuItem.Label = FText::FromString(TEXT("Open"));
	contextAnchor->SetItems({ menuItem });
	TestEqual(TEXT("context menu item stored"), contextAnchor->GetItems().Num(), 1);
	return true;
}

#endif
