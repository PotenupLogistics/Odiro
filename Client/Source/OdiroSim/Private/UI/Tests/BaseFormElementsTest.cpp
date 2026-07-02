#if WITH_DEV_AUTOMATION_TESTS

#include "UI/BaseButtonWidget.h"
#include "UI/BaseCheckBoxWidget.h"
#include "UI/BaseContextMenuWidget.h"
#include "UI/BaseDropdownWidget.h"
#include "UI/BaseFormElementTypes.h"
#include "UI/BaseSliderComboWidget.h"
#include "UI/BaseSliderWidget.h"
#include "UI/BaseSwitcherWidget.h"
#include "UI/BaseTextInputWidget.h"
#include "UI/BaseThumbnailCardWidget.h"
#include "UI/BaseToggleButtonWidget.h"
#include "UI/BaseTabWidget.h"
#include "UI/BaseTooltipWidget.h"
#include "UI/BaseTreeViewWidget.h"
#include "UI/BaseWidgetPrivate.h"
#include "UI/BaseWidgetTokens.h"
#include "UI/DisplayDpiScalingRule.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "Fonts/SlateFontInfo.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/AutomationTest.h"

namespace
{
	// Returns the editor world used by WBP-backed widget automation tests.
	UWorld* GetWidgetAutomationWorld()
	{
		return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBaseFormElementsSizeConstraintsTest,
	"OdiroSim.UI.BaseFormElements.SizeConstraints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Verifies Base Widget root SizeBox constraints behave as responsive desired-size limits.
bool FBaseFormElementsSizeConstraintsTest::RunTest(const FString& parameters)
{
	(void)parameters;

	FBaseWidgetSizeConstraints constraints;
	constraints.MinWidth = 640.0f;
	constraints.MaxWidth = 320.0f;
	constraints.MinHeight = -12.0f;
	constraints.MaxHeight = 72.0f;

	const FBaseWidgetSizeConstraints normalized = BaseWidgetPrivate::NormalizeSizeConstraints(constraints);
	TestEqual(TEXT("width min and max are ordered"), normalized.MinWidth, 320.0f);
	TestEqual(TEXT("width max and min are ordered"), normalized.MaxWidth, 640.0f);
	TestEqual(TEXT("negative min height clamps to zero"), normalized.MinHeight, 0.0f);
	TestEqual(TEXT("positive max height is preserved"), normalized.MaxHeight, 72.0f);

	USizeBox* sizeBox = NewObject<USizeBox>();
	TestNotNull(TEXT("size box created"), sizeBox);
	if (!sizeBox)
	{
		return false;
	}

	sizeBox->SetWidthOverride(500.0f);
	sizeBox->SetHeightOverride(80.0f);

	BaseWidgetPrivate::ApplySizeConstraints(sizeBox, constraints);
	TestFalse(TEXT("fixed width override is cleared"), sizeBox->IsWidthOverride());
	TestFalse(TEXT("fixed height override is cleared"), sizeBox->IsHeightOverride());
	TestTrue(TEXT("min desired width is applied"), sizeBox->IsMinDesiredWidthOverride());
	TestTrue(TEXT("max desired width is applied"), sizeBox->IsMaxDesiredWidthOverride());
	TestFalse(TEXT("zero min desired height is cleared"), sizeBox->IsMinDesiredHeightOverride());
	TestTrue(TEXT("max desired height is applied"), sizeBox->IsMaxDesiredHeightOverride());
	TestEqual(TEXT("applied min desired width"), sizeBox->GetMinDesiredWidth(), 320.0f);
	TestEqual(TEXT("applied max desired width"), sizeBox->GetMaxDesiredWidth(), 640.0f);
	TestEqual(TEXT("applied max desired height"), sizeBox->GetMaxDesiredHeight(), 72.0f);

	BaseWidgetPrivate::ApplySizeConstraints(sizeBox, FBaseWidgetSizeConstraints());
	TestFalse(TEXT("zero min width clears override"), sizeBox->IsMinDesiredWidthOverride());
	TestFalse(TEXT("zero min height clears override"), sizeBox->IsMinDesiredHeightOverride());
	TestFalse(TEXT("zero max width clears override"), sizeBox->IsMaxDesiredWidthOverride());
	TestFalse(TEXT("zero max height clears override"), sizeBox->IsMaxDesiredHeightOverride());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBaseFormElementsIconOnlyButtonPaddingTest,
	"OdiroSim.UI.BaseFormElements.IconOnlyButtonPadding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Verifies icon-only buttons ignore authored content padding while labeled buttons restore it.
bool FBaseFormElementsIconOnlyButtonPaddingTest::RunTest(const FString& parameters)
{
	(void)parameters;

	UWorld* world = GetWidgetAutomationWorld();
	TestNotNull(TEXT("editor world exists"), world);
	if (!world)
	{
		return false;
	}

	UClass* buttonClass = LoadClass<UBaseButtonWidget>(
		nullptr,
		TEXT("/Game/Widgets/Common/WBP_BaseButton.WBP_BaseButton_C"));
	TestNotNull(TEXT("base button class loads"), buttonClass);
	if (!buttonClass)
	{
		return false;
	}

	UBaseButtonWidget* button = CreateWidget<UBaseButtonWidget>(world, buttonClass);
	TestNotNull(TEXT("base button widget creates"), button);
	if (!button || !button->WidgetTree)
	{
		return false;
	}
	button->TakeWidget();

	UBorder* surfaceBorder = Cast<UBorder>(button->WidgetTree->FindWidget(TEXT("SurfaceBorder")));
	TestNotNull(TEXT("surface border is bound"), surfaceBorder);
	if (!surfaceBorder)
	{
		return false;
	}

	const FMargin authoredPadding = surfaceBorder->GetPadding();
	TestTrue(TEXT("fixture has authored horizontal padding"), authoredPadding.Left > 0.0f);

	UTexture2D* closeIcon = LoadObject<UTexture2D>(
		nullptr,
		TEXT("/Game/Textures/Icon/T_Icon_Close.T_Icon_Close"));
	TestNotNull(TEXT("close icon loads"), closeIcon);
	if (!closeIcon)
	{
		return false;
	}

	button->SetIcon(closeIcon);
	button->SetLabel(FText::GetEmpty());
	button->SynchronizeBaseProperties();
	TestEqual(TEXT("icon-only left padding is cleared"), surfaceBorder->GetPadding().Left, 0.0f);
	TestEqual(TEXT("icon-only right padding is cleared"), surfaceBorder->GetPadding().Right, 0.0f);

	button->SetLabel(FText::FromString(TEXT("Label")));
	button->SynchronizeBaseProperties();
	TestEqual(TEXT("labeled left padding is restored"), surfaceBorder->GetPadding().Left, authoredPadding.Left);
	TestEqual(TEXT("labeled right padding is restored"), surfaceBorder->GetPadding().Right, authoredPadding.Right);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBaseFormElementsGhostButtonFrameTest,
	"OdiroSim.UI.BaseFormElements.GhostButtonFrame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Verifies ghost buttons hide their frame by matching stroke color to fill color.
bool FBaseFormElementsGhostButtonFrameTest::RunTest(const FString& parameters)
{
	(void)parameters;

	UWorld* world = GetWidgetAutomationWorld();
	TestNotNull(TEXT("editor world exists"), world);
	if (!world)
	{
		return false;
	}

	UClass* buttonClass = LoadClass<UBaseButtonWidget>(
		nullptr,
		TEXT("/Game/Widgets/Common/WBP_BaseButton.WBP_BaseButton_C"));
	TestNotNull(TEXT("base button class loads"), buttonClass);
	if (!buttonClass)
	{
		return false;
	}

	UBaseButtonWidget* button = CreateWidget<UBaseButtonWidget>(world, buttonClass);
	TestNotNull(TEXT("base button widget creates"), button);
	if (!button || !button->WidgetTree)
	{
		return false;
	}
	button->TakeWidget();

	UBorder* surfaceBorder = Cast<UBorder>(button->WidgetTree->FindWidget(TEXT("SurfaceBorder")));
	TestNotNull(TEXT("surface border is bound"), surfaceBorder);
	if (!surfaceBorder)
	{
		return false;
	}

	auto assertStrokeMatchesFill = [this, surfaceBorder](const TCHAR* label)
	{
		UMaterialInstanceDynamic* material =
			Cast<UMaterialInstanceDynamic>(surfaceBorder->Background.GetResourceObject());
		const FString materialLabel = FString::Printf(TEXT("%s material exists"), label);
		TestNotNull(*materialLabel, material);
		if (!material)
		{
			return;
		}

		const FLinearColor fillColor = material->K2_GetVectorParameterValue(TEXT("FillColor"));
		const FLinearColor strokeColor = material->K2_GetVectorParameterValue(TEXT("StrokeColor"));
		const FString colorLabel = FString::Printf(TEXT("%s stroke matches fill"), label);
		TestTrue(*colorLabel, strokeColor.Equals(fillColor));
	};

	button->SetVariant(EBaseWidgetVariant::Ghost);
	button->SetBaseState(EBaseWidgetState::Hovered);
	assertStrokeMatchesFill(TEXT("hovered ghost button"));

	button->SetBaseState(EBaseWidgetState::Pressed);
	assertStrokeMatchesFill(TEXT("pressed ghost button"));

	button->SetBaseState(EBaseWidgetState::Default);
	button->SetSelected(true);
	assertStrokeMatchesFill(TEXT("selected ghost button"));

	button->SetSelected(false);
	button->SetBaseState(EBaseWidgetState::Disabled);
	assertStrokeMatchesFill(TEXT("disabled ghost button"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBaseFormElementsClosableTabVisibilityTest,
	"OdiroSim.UI.BaseFormElements.ClosableTabVisibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Verifies BaseTab owns direct CloseButton visibility without a wrapper widget.
bool FBaseFormElementsClosableTabVisibilityTest::RunTest(const FString& parameters)
{
	(void)parameters;

	UWorld* world = GetWidgetAutomationWorld();
	TestNotNull(TEXT("editor world exists"), world);
	if (!world)
	{
		return false;
	}

	UClass* tabClass = LoadClass<UBaseTabWidget>(
		nullptr,
		TEXT("/Game/Widgets/Common/WBP_BaseTab.WBP_BaseTab_C"));
	TestNotNull(TEXT("base tab class loads"), tabClass);
	if (!tabClass)
	{
		return false;
	}

	UBaseTabWidget* tab = CreateWidget<UBaseTabWidget>(world, tabClass);
	TestNotNull(TEXT("base tab widget creates"), tab);
	if (!tab || !tab->WidgetTree)
	{
		return false;
	}
	tab->TakeWidget();

	UWidget* closeButton = tab->WidgetTree->FindWidget(TEXT("CloseButton"));
	TestNotNull(TEXT("direct close button exists"), closeButton);
	if (!closeButton)
	{
		return false;
	}

	tab->SetClosable(false);
	TestEqual(TEXT("non-closable tab hides close button"), closeButton->GetVisibility(), ESlateVisibility::Collapsed);

	tab->SetClosable(true);
	TestEqual(TEXT("closable tab shows close button"), closeButton->GetVisibility(), ESlateVisibility::Visible);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBaseFormElementsTabCloseButtonLayoutTest,
	"OdiroSim.UI.BaseFormElements.TabCloseButtonLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Verifies BaseTab keeps the close affordance aligned against the full tab surface.
bool FBaseFormElementsTabCloseButtonLayoutTest::RunTest(const FString& parameters)
{
	(void)parameters;

	UWorld* world = GetWidgetAutomationWorld();
	TestNotNull(TEXT("editor world exists"), world);
	if (!world)
	{
		return false;
	}

	UClass* tabClass = LoadClass<UBaseTabWidget>(
		nullptr,
		TEXT("/Game/Widgets/Common/WBP_BaseTab.WBP_BaseTab_C"));
	TestNotNull(TEXT("base tab class loads"), tabClass);
	if (!tabClass)
	{
		return false;
	}

	UBaseTabWidget* tab = CreateWidget<UBaseTabWidget>(world, tabClass);
	TestNotNull(TEXT("base tab widget creates"), tab);
	if (!tab || !tab->WidgetTree)
	{
		return false;
	}
	tab->TakeWidget();
	tab->SetContentAlign(EBaseHorizontalContentAlign::Center);
	tab->SetClosable(true);

	UBorder* surfaceBorder = Cast<UBorder>(tab->WidgetTree->FindWidget(TEXT("SurfaceBorder")));
	UWidget* contentStack = tab->WidgetTree->FindWidget(TEXT("ContentStack"));
	UWidget* closeButton = tab->WidgetTree->FindWidget(TEXT("CloseButton"));
	TestNotNull(TEXT("surface border exists"), surfaceBorder);
	TestNotNull(TEXT("content stack exists"), contentStack);
	TestNotNull(TEXT("close button exists"), closeButton);
	if (!surfaceBorder || !contentStack || !closeButton)
	{
		return false;
	}

	UOverlaySlot* contentStackSlot = Cast<UOverlaySlot>(contentStack->Slot);
	UOverlaySlot* closeButtonSlot = Cast<UOverlaySlot>(closeButton->Slot);
	TestNotNull(TEXT("content stack uses overlay slot"), contentStackSlot);
	TestNotNull(TEXT("close button uses overlay slot"), closeButtonSlot);
	if (!contentStackSlot || !closeButtonSlot)
	{
		return false;
	}

	TestEqual(TEXT("tab surface content fills full tab width"), surfaceBorder->GetHorizontalAlignment(), HAlign_Fill);
	TestEqual(TEXT("tab surface content fills full tab height"), surfaceBorder->GetVerticalAlignment(), VAlign_Fill);
	TestEqual(TEXT("icon and label group follows content alignment"), contentStackSlot->GetHorizontalAlignment(), HAlign_Center);
	TestEqual(TEXT("close button stays on the tab right edge"), closeButtonSlot->GetHorizontalAlignment(), HAlign_Right);
	TestEqual(TEXT("close button remains vertically centered"), closeButtonSlot->GetVerticalAlignment(), VAlign_Center);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBaseFormElementsTabDividerMetricsTest,
	"OdiroSim.UI.BaseFormElements.TabDividerMetrics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Verifies BaseTab divider options size the WBP-owned side dividers and center them on tab edges.
bool FBaseFormElementsTabDividerMetricsTest::RunTest(const FString& parameters)
{
	(void)parameters;

	UWorld* world = GetWidgetAutomationWorld();
	TestNotNull(TEXT("editor world exists"), world);
	if (!world)
	{
		return false;
	}

	UClass* tabClass = LoadClass<UBaseTabWidget>(
		nullptr,
		TEXT("/Game/Widgets/Common/WBP_BaseTab.WBP_BaseTab_C"));
	TestNotNull(TEXT("base tab class loads"), tabClass);
	if (!tabClass)
	{
		return false;
	}

	UBaseTabWidget* tab = CreateWidget<UBaseTabWidget>(world, tabClass);
	TestNotNull(TEXT("base tab widget creates"), tab);
	if (!tab || !tab->WidgetTree)
	{
		return false;
	}
	tab->TakeWidget();

	UImage* leftDivider = Cast<UImage>(tab->WidgetTree->FindWidget(TEXT("LeftDivider")));
	UImage* rightDivider = Cast<UImage>(tab->WidgetTree->FindWidget(TEXT("RightDivider")));
	TestNotNull(TEXT("left divider exists"), leftDivider);
	TestNotNull(TEXT("right divider exists"), rightDivider);
	if (!leftDivider || !rightDivider)
	{
		return false;
	}

	TestEqual(TEXT("default divider width"), tab->GetDividerWidth(), 0.5f);
	TestEqual(TEXT("default divider height"), tab->GetDividerHeight(), 16.0f);

	tab->SetDividerSize(0.5f, 16.0f);
	TestEqual(TEXT("left divider brush width"), leftDivider->GetBrush().ImageSize.X, 0.5f);
	TestEqual(TEXT("left divider brush height"), leftDivider->GetBrush().ImageSize.Y, 16.0f);
	TestEqual(TEXT("right divider brush width"), rightDivider->GetBrush().ImageSize.X, 0.5f);
	TestEqual(TEXT("right divider brush height"), rightDivider->GetBrush().ImageSize.Y, 16.0f);
	TestEqual(TEXT("left divider edge translation"), static_cast<float>(leftDivider->GetRenderTransform().Translation.X), -0.25f);
	TestEqual(TEXT("right divider edge translation"), static_cast<float>(rightDivider->GetRenderTransform().Translation.X), 0.25f);
	TestTrue(TEXT("left divider is visible by default"), tab->IsLeftDividerVisible());
	TestTrue(TEXT("right divider is visible by default"), tab->IsRightDividerVisible());
	TestEqual(TEXT("left divider widget is visible by default"), leftDivider->GetVisibility(), ESlateVisibility::Visible);
	TestEqual(TEXT("right divider widget is visible by default"), rightDivider->GetVisibility(), ESlateVisibility::Visible);

	tab->SetDividerEdgesVisible(false, true);
	TestFalse(TEXT("left divider visibility flag can be disabled"), tab->IsLeftDividerVisible());
	TestTrue(TEXT("right divider visibility flag remains enabled"), tab->IsRightDividerVisible());
	TestEqual(TEXT("left divider widget hides when disabled"), leftDivider->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("right divider widget remains visible"), rightDivider->GetVisibility(), ESlateVisibility::Visible);

	tab->SetDividerEdgesVisible(true, false);
	TestTrue(TEXT("left divider visibility flag can be restored"), tab->IsLeftDividerVisible());
	TestFalse(TEXT("right divider visibility flag can be disabled"), tab->IsRightDividerVisible());
	TestEqual(TEXT("left divider widget returns visible"), leftDivider->GetVisibility(), ESlateVisibility::Visible);
	TestEqual(TEXT("right divider widget hides when disabled"), rightDivider->GetVisibility(), ESlateVisibility::Collapsed);

	const UBaseWidgetColorCatalog* colors = UBaseWidgetColorCatalog::MakeDefaultCatalogReference().LoadSynchronous();
	TestNotNull(TEXT("base color catalog exists"), colors);
	if (!colors)
	{
		return false;
	}
	TestTrue(
		TEXT("left divider uses DA divider color"),
		leftDivider->GetColorAndOpacity().Equals(colors->LineDividerColor));
	TestTrue(
		TEXT("right divider uses DA divider color"),
		rightDivider->GetColorAndOpacity().Equals(colors->LineDividerColor));

	const UOverlaySlot* leftSlot = Cast<UOverlaySlot>(leftDivider->Slot);
	const UOverlaySlot* rightSlot = Cast<UOverlaySlot>(rightDivider->Slot);
	TestNotNull(TEXT("left divider uses overlay slot"), leftSlot);
	TestNotNull(TEXT("right divider uses overlay slot"), rightSlot);
	if (!leftSlot || !rightSlot)
	{
		return false;
	}

	TestEqual(TEXT("left divider horizontal align"), leftSlot->GetHorizontalAlignment(), HAlign_Left);
	TestEqual(TEXT("right divider horizontal align"), rightSlot->GetHorizontalAlignment(), HAlign_Right);
	TestEqual(TEXT("left divider vertical align"), leftSlot->GetVerticalAlignment(), VAlign_Center);
	TestEqual(TEXT("right divider vertical align"), rightSlot->GetVerticalAlignment(), VAlign_Center);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBaseFormElementsTokenScaleTest,
	"OdiroSim.UI.BaseFormElements.TokenScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Verifies source defaults stay neutral while size DA assets own authored token values.
bool FBaseFormElementsTokenScaleTest::RunTest(const FString& parameters)
{
	(void)parameters;

	const UBaseWidgetSizeCatalog* defaultSizes = GetDefault<UBaseWidgetSizeCatalog>();
	TestNotNull(TEXT("size catalog class defaults exist"), defaultSizes);
	if (!defaultSizes)
	{
		return false;
	}

	TestEqual(TEXT("class default space stays neutral"), defaultSizes->Space1, 0.0f);
	TestEqual(TEXT("class default control height stays neutral"), defaultSizes->ControlHeight, 0.0f);
	TestEqual(TEXT("class default icon size stays neutral"), defaultSizes->IconSize, 0.0f);
	TestEqual(TEXT("class default title typography stays neutral"), defaultSizes->TitleText.Font.Size, 0.0f);
	TestEqual(TEXT("class default body typography stays neutral"), defaultSizes->BodyText.Font.Size, 0.0f);

	const FBaseTextStyleToken defaultTextToken;
	TestEqual(TEXT("default text token size stays neutral"), defaultTextToken.Font.Size, 0.0f);

	const auto testSizeAsset = [this](
		const TCHAR* testPrefix,
		const EBaseWidgetSize size,
		const float titleSize,
		const float labelSize,
		const float bodySize,
		const float captionSize,
		const float valueSize)
	{
		const UBaseWidgetSizeCatalog* asset =
			UBaseWidgetSizeCatalog::MakePresetCatalogReference(size).LoadSynchronous();
		const FString existsLabel = FString::Printf(TEXT("%s size asset exists"), testPrefix);
		TestNotNull(*existsLabel, asset);
		if (!asset)
		{
			return;
		}

		const FString titleLabel = FString::Printf(TEXT("%s title text size"), testPrefix);
		const FString labelLabel = FString::Printf(TEXT("%s label text size"), testPrefix);
		const FString bodyLabel = FString::Printf(TEXT("%s body text size"), testPrefix);
		const FString captionLabel = FString::Printf(TEXT("%s caption text size"), testPrefix);
		const FString valueLabel = FString::Printf(TEXT("%s value text size"), testPrefix);
		TestEqual(*titleLabel, asset->TitleText.Font.Size, titleSize);
		TestEqual(*labelLabel, asset->LabelText.Font.Size, labelSize);
		TestEqual(*bodyLabel, asset->BodyText.Font.Size, bodySize);
		TestEqual(*captionLabel, asset->CaptionText.Font.Size, captionSize);
		TestEqual(*valueLabel, asset->ValueText.Font.Size, valueSize);
	};
	testSizeAsset(TEXT("small DA"), EBaseWidgetSize::Small, 14.0f, 12.0f, 12.0f, 11.0f, 22.0f);
	testSizeAsset(TEXT("medium DA"), EBaseWidgetSize::Medium, 16.0f, 14.0f, 14.0f, 12.0f, 28.0f);
	testSizeAsset(TEXT("large DA"), EBaseWidgetSize::Large, 18.0f, 16.0f, 16.0f, 14.0f, 36.0f);

	const UBaseWidgetSizeCatalog* mediumAsset =
		UBaseWidgetSizeCatalog::MakePresetCatalogReference(EBaseWidgetSize::Medium).LoadSynchronous();
	TestNotNull(TEXT("medium size asset exists for metrics"), mediumAsset);
	if (mediumAsset)
	{
		TestEqual(TEXT("medium space 1"), mediumAsset->Space1, 2.0f);
		TestEqual(TEXT("medium space 2"), mediumAsset->Space2, 4.0f);
		TestEqual(TEXT("medium space 3"), mediumAsset->Space3, 6.0f);
		TestEqual(TEXT("medium space 4"), mediumAsset->Space4, 8.0f);
		TestEqual(TEXT("medium space 5"), mediumAsset->Space5, 10.0f);
		TestEqual(TEXT("medium space 6"), mediumAsset->Space6, 12.0f);
		TestEqual(TEXT("medium space 8"), mediumAsset->Space8, 16.0f);
		TestEqual(TEXT("medium space 10"), mediumAsset->Space10, 20.0f);
		TestEqual(TEXT("medium space 12"), mediumAsset->Space12, 24.0f);
		TestEqual(TEXT("medium space 16"), mediumAsset->Space16, 36.0f);
		TestEqual(TEXT("medium space 20"), mediumAsset->Space20, 40.0f);
		TestEqual(TEXT("medium compat small spacing"), mediumAsset->SpacingSmall, 4.0f);
		TestEqual(TEXT("medium compat medium spacing"), mediumAsset->SpacingMedium, 8.0f);
		TestEqual(TEXT("medium compat large spacing"), mediumAsset->SpacingLarge, 12.0f);
		TestEqual(TEXT("medium control height"), mediumAsset->ControlHeight, 30.0f);
		TestEqual(TEXT("medium small control height"), mediumAsset->ControlHeightSmall, 28.0f);
		TestEqual(TEXT("medium field height"), mediumAsset->FieldHeight, 24.0f);
		TestEqual(TEXT("medium row height"), mediumAsset->RowHeight, 24.0f);
		TestEqual(TEXT("medium property row height"), mediumAsset->PropertyRowHeight, 26.0f);
		TestEqual(TEXT("medium title bar height"), mediumAsset->TitleBarHeight, 32.0f);
		TestEqual(TEXT("medium tab bar height"), mediumAsset->TabBarHeight, 36.0f);
		TestEqual(TEXT("medium panel header height"), mediumAsset->PanelHeaderHeight, 30.0f);
		TestEqual(TEXT("medium icon size"), mediumAsset->IconSize, 20.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBaseFormElementsTextPreservationTest,
	"OdiroSim.UI.BaseFormElements.TextPreservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBaseFormElementsTextPreservationTest::RunTest(const FString& parameters)
{
	(void)parameters;

	UTextBlock* textBlock = NewObject<UTextBlock>();
	TestNotNull(TEXT("text block created"), textBlock);
	if (!textBlock)
	{
		return false;
	}

	textBlock->SetText(FText::FromString(TEXT("Designer placeholder")));
	BaseWidgetPrivate::ApplyTextIfSet(textBlock, FText::GetEmpty());
	TestEqual(
		TEXT("empty property text preserves WBP-authored text"),
		textBlock->GetText().ToString(),
		FString(TEXT("Designer placeholder")));

	BaseWidgetPrivate::ApplyTextIfSet(textBlock, FText::FromString(TEXT("Property text")));
	TestEqual(
		TEXT("non-empty property text overrides WBP-authored text"),
		textBlock->GetText().ToString(),
		FString(TEXT("Property text")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBaseFormElementsDpiScaleTest,
	"OdiroSim.UI.BaseFormElements.DpiScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// DPI rule은 display scale bridge이며 DA font size를 역보정하지 않는다.
bool FBaseFormElementsDpiScaleTest::RunTest(const FString& parameters)
{
	(void)parameters;

	UDisplayDpiScalingRule* rule = NewObject<UDisplayDpiScalingRule>();
	TestNotNull(TEXT("display dpi scaling rule created"), rule);
	if (!rule)
	{
		return false;
	}

	const float viewportScale = rule->GetDPIScaleBasedOnSize(FIntPoint(3840, 2160));
	TestTrue(TEXT("display dpi scale is finite"), FMath::IsFinite(viewportScale));
	TestTrue(TEXT("display dpi scale stays positive"), viewportScale >= 0.01f);

	const UUserInterfaceSettings* uiSettings = GetDefault<UUserInterfaceSettings>();
	TestNotNull(TEXT("UI settings exist"), uiSettings);
	if (uiSettings)
	{
		TestEqual(TEXT("font details display native Slate units"), uiSettings->GetFontDisplayDPI(), FontConstants::RenderDPI);
	}
	return true;
}

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
	input->SetTextWrap(true);
	TestTrue(TEXT("text wrap option stores"), input->IsTextWrapEnabled());
	input->SetTextWrap(false);
	TestFalse(TEXT("text wrap option clears"), input->IsTextWrapEnabled());
	input->SetInputMode(EBaseTextInputMode::Multiline);
	TestEqual(TEXT("legacy multiline maps to text mode"), input->GetInputMode(), EBaseTextInputMode::Text);
	TestTrue(TEXT("legacy multiline enables wrapping"), input->IsTextWrapEnabled());
	input->SetTextWrap(false);
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

	UBaseSliderComboWidget* combo = NewObject<UBaseSliderComboWidget>();
	TestNotNull(TEXT("slider combo created"), combo);
	if (!combo)
	{
		return false;
	}
	combo->SetComboStyle(EBaseSliderComboStyle::Modern);
	TestEqual(TEXT("slider combo style stores"), combo->GetComboStyle(), EBaseSliderComboStyle::Modern);
	combo->SetValueRange(0.0f, 10.0f);
	combo->SetValue(12.0f);
	TestEqual(TEXT("slider combo value clamps"), combo->GetValue(), 10.0f);
	combo->SetRangeMode(true);
	combo->SetRangeValue(9.0f, 2.0f);
	TestEqual(TEXT("slider combo lower ordered"), combo->GetLowerValue(), 2.0f);
	TestEqual(TEXT("slider combo upper ordered"), combo->GetUpperValue(), 9.0f);
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
	TestEqual(TEXT("checkbox group constructor examples"), group->GetItems().Num(), 2);

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
	TestEqual(TEXT("tree constructor examples"), tree->GetItems().Num(), 2);
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
