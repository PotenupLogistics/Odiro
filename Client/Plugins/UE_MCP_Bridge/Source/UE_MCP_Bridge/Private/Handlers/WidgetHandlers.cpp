#include "WidgetHandlers.h"
#include "HandlerRegistry.h"
#include "HandlerUtils.h"
#include "HandlerAssetCreate.h"
#include "HandlerJsonProperty.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "EditorScriptingUtilities/Public/EditorAssetLibrary.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/ProgressBar.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/EditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "Components/ContentWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Components/Overlay.h"
#include "Components/GridPanel.h"
#include "Components/UniformGridPanel.h"
#include "Components/WidgetSwitcher.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/Border.h"
#include "Components/Spacer.h"
#include "Components/RichTextBlock.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBoxSlot.h"
#include "Animation/WidgetAnimation.h"
#include "MovieScene.h"
#include "MovieScenePossessable.h"
#include "MovieSceneSpawnable.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UObject/UnrealType.h"
#include "Editor.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidget.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "EditorUtilityBlueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"
#include "Styling/SlateTypes.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "RenderingThread.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "Widgets/SWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "WidgetBlueprintEditorUtils.h"

namespace
{
	const TCHAR* McpRegularFontPath =
		TEXT("/Game/Fonts/Freesentation/Freesentation-4Regular_Font.Freesentation-4Regular_Font");
	const TCHAR* McpBoldFontPath =
		TEXT("/Game/Fonts/Freesentation/Freesentation-7Bold_Font.Freesentation-7Bold_Font");

	void RefreshBorderMaterialElementSizes(UUserWidget* UserWidget)
	{
		if (!UserWidget || !UserWidget->WidgetTree)
		{
			return;
		}

		UserWidget->WidgetTree->ForEachWidget([](UWidget* Widget)
		{
			if (UBorder* Border = Cast<UBorder>(Widget))
			{
				if (UMaterialInstanceDynamic* Material =
					Cast<UMaterialInstanceDynamic>(Border->Background.GetResourceObject()))
				{
					const FVector2D Size = Border->GetCachedGeometry().GetLocalSize();
					if (Size.X >= 1.0f && Size.Y >= 1.0f)
					{
						Material->SetVectorParameterValue(TEXT("ElementSize"), FLinearColor(Size.X, Size.Y, 0.0f, 0.0f));
						float RadiusPx = 0.0f;
						if (Material->GetScalarParameterValue(TEXT("RadiusPx"), RadiusPx))
						{
							const float MaxRadius = FMath::Max(0.0f, FMath::Min(Size.X, Size.Y) * 0.5f - 0.5f);
							Material->SetScalarParameterValue(TEXT("RadiusPx"), FMath::Min(RadiusPx, MaxRadius));
						}
					}
				}
			}
			if (UUserWidget* ChildUserWidget = Cast<UUserWidget>(Widget))
			{
				RefreshBorderMaterialElementSizes(ChildUserWidget);
			}
		});
	}

	bool McpMoveWidgetTemplateIntoTree(UWidgetBlueprint* WidgetBP, UWidget* Widget, FName DesiredName = NAME_None)
	{
		if (!WidgetBP || !WidgetBP->WidgetTree || !Widget)
		{
			return false;
		}

		const FName TargetName = DesiredName.IsNone() ? Widget->GetFName() : DesiredName;
		if (TargetName.IsNone())
		{
			return false;
		}

		UObject* ExistingObject = StaticFindObjectFast(UObject::StaticClass(), WidgetBP->WidgetTree, TargetName);
		if (ExistingObject && ExistingObject != Widget)
		{
			if (UWidget* ExistingWidget = Cast<UWidget>(ExistingObject))
			{
				if (ExistingWidget == WidgetBP->WidgetTree->RootWidget || ExistingWidget->GetParent())
				{
					return false;
				}
			}

			ExistingObject->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty);
		}

		Widget->ClearFlags(RF_Transient);
		Widget->SetFlags(RF_Transactional);
		return Widget->Rename(*TargetName.ToString(), WidgetBP->WidgetTree, REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty);
	}

	// Keeps MCP-authored widget tree edits aligned with the UMG compiler's
	// serialized widget list and generated variable GUID bookkeeping.
	void McpSynchronizeWidgetBlueprintCompilerState(UWidgetBlueprint* WidgetBP)
	{
		if (!WidgetBP || !WidgetBP->WidgetTree)
		{
			return;
		}

		TArray<UWidget*> ReachableWidgetArray;
		WidgetBP->WidgetTree->GetAllWidgets(ReachableWidgetArray);
		for (UWidget* Widget : ReachableWidgetArray)
		{
			if (Widget && Widget->GetOuter() != WidgetBP->WidgetTree)
			{
				McpMoveWidgetTemplateIntoTree(WidgetBP, Widget);
			}
		}

		ReachableWidgetArray.Reset();
		WidgetBP->WidgetTree->GetAllWidgets(ReachableWidgetArray);
		TSet<UWidget*> ReachableWidgets;
		ReachableWidgets.Append(ReachableWidgetArray);
		TSet<UWidget*> UnreachableSourceWidgets;
		WidgetBP->ForEachSourceWidget([&ReachableWidgets, &UnreachableSourceWidgets](UWidget* Widget)
		{
			if (Widget && !ReachableWidgets.Contains(Widget))
			{
				UnreachableSourceWidgets.Add(Widget);
			}
		});

		TSet<UWidget*> UnreachableRoots;
		for (UWidget* Widget : UnreachableSourceWidgets)
		{
			UWidget* Parent = Widget ? Widget->GetParent() : nullptr;
			if (!Parent || !UnreachableSourceWidgets.Contains(Parent))
			{
				UnreachableRoots.Add(Widget);
			}
		}
		if (!UnreachableRoots.IsEmpty())
		{
			FWidgetBlueprintEditorUtils::DeleteWidgets(
				WidgetBP,
				UnreachableRoots,
				FWidgetBlueprintEditorUtils::EDeleteWidgetWarningType::DeleteSilently);
		}

		TArray<UWidget*> TreeWidgets;
		WidgetBP->WidgetTree->GetAllWidgets(TreeWidgets);

#if WITH_EDITORONLY_DATA
		if (FArrayProperty* AllWidgetsProperty = FindFProperty<FArrayProperty>(WidgetBP->WidgetTree->GetClass(), TEXT("AllWidgets")))
		{
			if (FObjectPropertyBase* InnerObjectProperty = CastField<FObjectPropertyBase>(AllWidgetsProperty->Inner))
			{
				FScriptArrayHelper AllWidgetsHelper(
					AllWidgetsProperty,
					AllWidgetsProperty->ContainerPtrToValuePtr<void>(WidgetBP->WidgetTree));
				AllWidgetsHelper.EmptyAndAddValues(TreeWidgets.Num());
				for (int32 Index = 0; Index < TreeWidgets.Num(); ++Index)
				{
					InnerObjectProperty->SetObjectPropertyValue(AllWidgetsHelper.GetRawPtr(Index), TreeWidgets[Index]);
				}
			}
		}
#endif

		TSet<FName> CurrentVariableNames;
		WidgetBP->ForEachSourceWidget([&CurrentVariableNames, WidgetBP](UWidget* Widget)
		{
			if (!Widget || Widget->GetFName().IsNone())
			{
				return;
			}

			const FName WidgetName = Widget->GetFName();
			CurrentVariableNames.Add(WidgetName);
			if (!WidgetBP->WidgetVariableNameToGuidMap.Contains(WidgetName))
			{
				WidgetBP->WidgetVariableNameToGuidMap.Add(WidgetName, FGuid::NewDeterministicGuid(Widget->GetPathName()));
			}
		});

		for (UWidgetAnimation* Animation : WidgetBP->Animations)
		{
			if (!Animation || Animation->GetFName().IsNone())
			{
				continue;
			}

			const FName AnimationName = Animation->GetFName();
			CurrentVariableNames.Add(AnimationName);
			if (!WidgetBP->WidgetVariableNameToGuidMap.Contains(AnimationName))
			{
				WidgetBP->WidgetVariableNameToGuidMap.Add(AnimationName, FGuid::NewDeterministicGuid(Animation->GetPathName()));
			}
		}

		TSet<FGuid> CurrentGuids;
		for (auto It = WidgetBP->WidgetVariableNameToGuidMap.CreateIterator(); It; ++It)
		{
			if (!CurrentVariableNames.Contains(It.Key()))
			{
				It.RemoveCurrent();
				continue;
			}

			if (!It.Value().IsValid() || CurrentGuids.Contains(It.Value()))
			{
				It.Value() = FGuid::NewGuid();
			}

			CurrentGuids.Add(It.Value());
		}
	}

	FLinearColor McpColorFromJson(const TSharedPtr<FJsonValue>& Value, const FLinearColor& Fallback = FLinearColor::White)
	{
		if (!Value.IsValid() || Value->IsNull())
		{
			return Fallback;
		}

		FString ColorString;
		if (Value->TryGetString(ColorString))
		{
			ColorString.TrimStartAndEndInline();
			if (ColorString.StartsWith(TEXT("#")))
			{
				ColorString.RightChopInline(1);
			}
			if (ColorString.Len() == 6 || ColorString.Len() == 8)
			{
				// Slate UI brush colors are applied without an sRGB->linear
				// decode; reinterpret the authored sRGB bytes directly so panel
				// surfaces and the accent match the design-system hex values.
				return FColor::FromHex(ColorString).ReinterpretAsLinear();
			}
		}

		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (Value->TryGetObject(Obj) && Obj && (*Obj).IsValid())
		{
			return FLinearColor(
				static_cast<float>((*Obj)->GetNumberField(TEXT("r"))),
				static_cast<float>((*Obj)->GetNumberField(TEXT("g"))),
				static_cast<float>((*Obj)->GetNumberField(TEXT("b"))),
				static_cast<float>((*Obj)->HasField(TEXT("a")) ? (*Obj)->GetNumberField(TEXT("a")) : 1.0));
		}

		return Fallback;
	}

	FMargin McpMarginFromJson(const TSharedPtr<FJsonValue>& Value, const FMargin& Fallback = FMargin())
	{
		if (!Value.IsValid() || Value->IsNull())
		{
			return Fallback;
		}

		double Uniform = 0.0;
		if (Value->TryGetNumber(Uniform))
		{
			return FMargin(static_cast<float>(Uniform));
		}

		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (Value->TryGetArray(Array) && Array)
		{
			if (Array->Num() == 1)
			{
				return FMargin(static_cast<float>((*Array)[0]->AsNumber()));
			}
			if (Array->Num() >= 4)
			{
				return FMargin(
					static_cast<float>((*Array)[0]->AsNumber()),
					static_cast<float>((*Array)[1]->AsNumber()),
					static_cast<float>((*Array)[2]->AsNumber()),
					static_cast<float>((*Array)[3]->AsNumber()));
			}
		}

		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (Value->TryGetObject(Obj) && Obj && (*Obj).IsValid())
		{
			return FMargin(
				static_cast<float>((*Obj)->GetNumberField(TEXT("left"))),
				static_cast<float>((*Obj)->GetNumberField(TEXT("top"))),
				static_cast<float>((*Obj)->GetNumberField(TEXT("right"))),
				static_cast<float>((*Obj)->GetNumberField(TEXT("bottom"))));
		}

		return Fallback;
	}

	FVector2D McpVector2FromJson(const TSharedPtr<FJsonValue>& Value, const FVector2D& Fallback = FVector2D::ZeroVector)
	{
		if (!Value.IsValid() || Value->IsNull())
		{
			return Fallback;
		}

		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (Value->TryGetArray(Array) && Array && Array->Num() >= 2)
		{
			return FVector2D(
				static_cast<float>((*Array)[0]->AsNumber()),
				static_cast<float>((*Array)[1]->AsNumber()));
		}

		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (Value->TryGetObject(Obj) && Obj && (*Obj).IsValid())
		{
			return FVector2D(
				static_cast<float>((*Obj)->GetNumberField(TEXT("x"))),
				static_cast<float>((*Obj)->GetNumberField(TEXT("y"))));
		}

		return Fallback;
	}

	ESlateVisibility McpVisibilityFromString(const FString& Value)
	{
		if (Value.Equals(TEXT("Collapsed"), ESearchCase::IgnoreCase)) return ESlateVisibility::Collapsed;
		if (Value.Equals(TEXT("Hidden"), ESearchCase::IgnoreCase)) return ESlateVisibility::Hidden;
		if (Value.Equals(TEXT("HitTestInvisible"), ESearchCase::IgnoreCase)) return ESlateVisibility::HitTestInvisible;
		if (Value.Equals(TEXT("SelfHitTestInvisible"), ESearchCase::IgnoreCase)) return ESlateVisibility::SelfHitTestInvisible;
		return ESlateVisibility::Visible;
	}

	EHorizontalAlignment McpHAlignFromString(const FString& Value)
	{
		if (Value.Equals(TEXT("left"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("HAlign_Left"), ESearchCase::IgnoreCase)) return HAlign_Left;
		if (Value.Equals(TEXT("center"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("HAlign_Center"), ESearchCase::IgnoreCase)) return HAlign_Center;
		if (Value.Equals(TEXT("right"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("HAlign_Right"), ESearchCase::IgnoreCase)) return HAlign_Right;
		return HAlign_Fill;
	}

	EVerticalAlignment McpVAlignFromString(const FString& Value)
	{
		if (Value.Equals(TEXT("top"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("VAlign_Top"), ESearchCase::IgnoreCase)) return VAlign_Top;
		if (Value.Equals(TEXT("center"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("VAlign_Center"), ESearchCase::IgnoreCase)) return VAlign_Center;
		if (Value.Equals(TEXT("bottom"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("VAlign_Bottom"), ESearchCase::IgnoreCase)) return VAlign_Bottom;
		return VAlign_Fill;
	}

	FSlateBrush McpMakeBoxBrush(const FLinearColor& Color, const float Radius = 4.0f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.TintColor = FSlateColor(Color);
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		return Brush;
	}

	FSlateFontInfo McpMakeFont(const int32 Size, const bool bBold)
	{
		FSlateFontInfo FontInfo;
		FontInfo.FontObject = LoadObject<UObject>(nullptr, bBold ? McpBoldFontPath : McpRegularFontPath);
		FontInfo.Size = Size;
		FontInfo.TypefaceFontName = bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
		return FontInfo;
	}

	template <typename StructType>
	void McpSetStructPropertyValue(UObject* Object, const TCHAR* PropertyName, const StructType& Value)
	{
		if (!Object)
		{
			return;
		}

		if (FStructProperty* StructProperty = CastField<FStructProperty>(Object->GetClass()->FindPropertyByName(FName(PropertyName))))
		{
			StructProperty->CopyCompleteValue(StructProperty->ContainerPtrToValuePtr<void>(Object), &Value);
		}
	}

	void McpApplyButtonStyle(UButton* Button, const TSharedPtr<FJsonObject>& StyleObj)
	{
		if (!Button || !StyleObj.IsValid())
		{
			return;
		}

		const FLinearColor Normal = McpColorFromJson(StyleObj->TryGetField(TEXT("normal")), FLinearColor(0.22f, 0.22f, 0.22f, 1.0f));
		const FLinearColor Hovered = McpColorFromJson(StyleObj->TryGetField(TEXT("hover")), FLinearColor(0.34f, 0.34f, 0.34f, 1.0f));
		const FLinearColor Pressed = McpColorFromJson(StyleObj->TryGetField(TEXT("pressed")), FLinearColor(0.18f, 0.18f, 0.18f, 1.0f));
		const float Radius = StyleObj->HasField(TEXT("radius")) ? static_cast<float>(StyleObj->GetNumberField(TEXT("radius"))) : 4.0f;

		FButtonStyle Style = Button->GetStyle();
		Style.Normal = McpMakeBoxBrush(Normal, Radius);
		Style.Hovered = McpMakeBoxBrush(Hovered, Radius);
		Style.Pressed = McpMakeBoxBrush(Pressed, Radius);
		Style.Disabled = McpMakeBoxBrush(Normal.CopyWithNewOpacity(0.45f), Radius);
		// Optional content padding; lets callers make compact icon buttons (e.g.
		// stepper carets) without the default 8x4 inset.
		FMargin NormalPadding(8.0f, 4.0f);
		if (const TSharedPtr<FJsonValue> PaddingValue = StyleObj->TryGetField(TEXT("padding")))
		{
			NormalPadding = McpMarginFromJson(PaddingValue);
		}
		Style.NormalPadding = NormalPadding;
		Style.PressedPadding = NormalPadding;
		Button->SetStyle(Style);
	}

	void McpApplyEditableTextBoxStyle(UEditableTextBox* TextBox, const TSharedPtr<FJsonObject>& StyleObj)
	{
		if (!TextBox || !StyleObj.IsValid())
		{
			return;
		}

		FEditableTextBoxStyle Style = TextBox->WidgetStyle;
		const FLinearColor Background = McpColorFromJson(StyleObj->TryGetField(TEXT("background")), FLinearColor(0.015f, 0.015f, 0.015f, 1.0f));
		const FLinearColor Border = McpColorFromJson(StyleObj->TryGetField(TEXT("border")), FLinearColor(0.035f, 0.035f, 0.035f, 1.0f));
		const FLinearColor Focus = McpColorFromJson(StyleObj->TryGetField(TEXT("focus")), FLinearColor(0.0f, 0.44f, 0.88f, 1.0f));
		const FLinearColor Text = McpColorFromJson(StyleObj->TryGetField(TEXT("text")), FLinearColor(0.86f, 0.86f, 0.86f, 1.0f));
		const int32 FontSize = StyleObj->HasField(TEXT("fontSize")) ? static_cast<int32>(StyleObj->GetNumberField(TEXT("fontSize"))) : 13;

		Style.BackgroundImageNormal = McpMakeBoxBrush(Background);
		Style.BackgroundImageHovered = McpMakeBoxBrush(Border);
		Style.BackgroundImageFocused = McpMakeBoxBrush(Focus);
		Style.BackgroundImageReadOnly = McpMakeBoxBrush(Background.CopyWithNewOpacity(0.55f));
		Style.ForegroundColor = FSlateColor(Text);
		Style.Padding = FMargin(8.0f, 2.0f, 8.0f, 2.0f);
		Style.TextStyle.SetFont(McpMakeFont(FontSize, false));
		Style.TextStyle.SetColorAndOpacity(FSlateColor(Text));
		TextBox->WidgetStyle = Style;
	}

	void McpApplyMultiLineEditableTextBoxStyle(UMultiLineEditableTextBox* TextBox, const TSharedPtr<FJsonObject>& StyleObj)
	{
		if (!TextBox || !StyleObj.IsValid())
		{
			return;
		}

		FEditableTextBoxStyle Style = TextBox->WidgetStyle;
		const FLinearColor Background = McpColorFromJson(StyleObj->TryGetField(TEXT("background")), FLinearColor(0.015f, 0.015f, 0.015f, 1.0f));
		const FLinearColor Border = McpColorFromJson(StyleObj->TryGetField(TEXT("border")), FLinearColor(0.035f, 0.035f, 0.035f, 1.0f));
		const FLinearColor Focus = McpColorFromJson(StyleObj->TryGetField(TEXT("focus")), FLinearColor(0.0f, 0.44f, 0.88f, 1.0f));
		const FLinearColor Text = McpColorFromJson(StyleObj->TryGetField(TEXT("text")), FLinearColor(0.86f, 0.86f, 0.86f, 1.0f));
		const int32 FontSize = StyleObj->HasField(TEXT("fontSize")) ? static_cast<int32>(StyleObj->GetNumberField(TEXT("fontSize"))) : 13;

		Style.BackgroundImageNormal = McpMakeBoxBrush(Background);
		Style.BackgroundImageHovered = McpMakeBoxBrush(Border);
		Style.BackgroundImageFocused = McpMakeBoxBrush(Focus);
		Style.BackgroundImageReadOnly = McpMakeBoxBrush(Background.CopyWithNewOpacity(0.55f));
		Style.ForegroundColor = FSlateColor(Text);
		Style.Padding = FMargin(8.0f, 4.0f);
		Style.TextStyle.SetFont(McpMakeFont(FontSize, false));
		Style.TextStyle.SetColorAndOpacity(FSlateColor(Text));
		TextBox->WidgetStyle = Style;
		TextBox->SetTextStyle(Style.TextStyle);
		TextBox->SetForegroundColor(Text);
	}

	void McpApplyComboBoxStyle(UComboBoxString* ComboBox, const TSharedPtr<FJsonObject>& StyleObj)
	{
		if (!ComboBox || !StyleObj.IsValid())
		{
			return;
		}

		const FLinearColor Background = McpColorFromJson(StyleObj->TryGetField(TEXT("background")), FLinearColor(0.015f, 0.015f, 0.015f, 1.0f));
		const FLinearColor Hover = McpColorFromJson(StyleObj->TryGetField(TEXT("hover")), FLinearColor(0.09f, 0.09f, 0.09f, 1.0f));
		const FLinearColor Text = McpColorFromJson(StyleObj->TryGetField(TEXT("text")), FLinearColor(0.86f, 0.86f, 0.86f, 1.0f));
		const int32 FontSize = StyleObj->HasField(TEXT("fontSize")) ? static_cast<int32>(StyleObj->GetNumberField(TEXT("fontSize"))) : 13;

		McpSetStructPropertyValue(ComboBox, TEXT("Font"), McpMakeFont(FontSize, false));
		McpSetStructPropertyValue(ComboBox, TEXT("ForegroundColor"), FSlateColor(Text));

		FComboBoxStyle WidgetStyle = ComboBox->GetWidgetStyle();
		WidgetStyle.ComboButtonStyle.ButtonStyle.Normal = McpMakeBoxBrush(Background);
		WidgetStyle.ComboButtonStyle.ButtonStyle.Hovered = McpMakeBoxBrush(Hover);
		WidgetStyle.ComboButtonStyle.ButtonStyle.Pressed = McpMakeBoxBrush(Background * 0.85f);
		ComboBox->SetWidgetStyle(WidgetStyle);

		FTableRowStyle ItemStyle = ComboBox->GetItemStyle();
		ItemStyle.TextColor = FSlateColor(Text);
		ComboBox->SetItemStyle(ItemStyle);
	}
}

static UClass* ResolveWidgetClass(const FString& ClassName);

static FString McpVisibilityToString(const ESlateVisibility Visibility)
{
	switch (Visibility)
	{
	case ESlateVisibility::Visible:
		return TEXT("Visible");
	case ESlateVisibility::Collapsed:
		return TEXT("Collapsed");
	case ESlateVisibility::Hidden:
		return TEXT("Hidden");
	case ESlateVisibility::HitTestInvisible:
		return TEXT("HitTestInvisible");
	case ESlateVisibility::SelfHitTestInvisible:
		return TEXT("SelfHitTestInvisible");
	default:
		return TEXT("Unknown");
	}
}

// Walks a constructed UserWidget's runtime tree (descending into nested child
// UserWidgets) and records each widget's effective color state. Used to debug
// why a runtime-applied brush tint can render differently than an identical
// asset-baked tint.
static void DumpWidgetColorsRecursive(UUserWidget* Owner, const FString& Prefix, TArray<TSharedPtr<FJsonValue>>& Out)
{
	if (!Owner || !Owner->WidgetTree)
	{
		return;
	}
	TArray<UWidget*> Children;
	Owner->WidgetTree->GetAllWidgets(Children);
	for (UWidget* W : Children)
	{
		if (!W)
		{
			continue;
		}
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), Prefix + W->GetName());
		Entry->SetStringField(TEXT("class"), W->GetClass()->GetName());
		Entry->SetStringField(TEXT("visibility"), McpVisibilityToString(W->GetVisibility()));
		Entry->SetBoolField(TEXT("isVisible"), W->IsVisible());
		Entry->SetNumberField(TEXT("renderOpacity"), W->GetRenderOpacity());
		Entry->SetStringField(TEXT("desiredSize"), W->GetDesiredSize().ToString());
		if (W->GetCachedWidget().IsValid())
		{
			const FGeometry Geometry = W->GetCachedGeometry();
			Entry->SetStringField(TEXT("cachedLocalSize"), Geometry.GetLocalSize().ToString());
			Entry->SetStringField(TEXT("cachedAbsolutePosition"), Geometry.GetAbsolutePosition().ToString());
		}
		else
		{
			Entry->SetBoolField(TEXT("hasCachedGeometry"), false);
		}
		if (UBorder* B = Cast<UBorder>(W))
		{
			Entry->SetStringField(TEXT("brushColor"), B->GetBrushColor().ToString());
			const FSlateBrush& Br = B->Background;
			Entry->SetNumberField(TEXT("drawAs"), static_cast<int32>(Br.DrawAs));
			Entry->SetStringField(TEXT("tintColorFull"), Br.TintColor.GetSpecifiedColor().ToString());
			Entry->SetStringField(TEXT("imageSize"), Br.ImageSize.ToString());
			Entry->SetStringField(TEXT("margin"), FString::Printf(TEXT("%.2f,%.2f,%.2f,%.2f"),
				Br.Margin.Left, Br.Margin.Top, Br.Margin.Right, Br.Margin.Bottom));
			Entry->SetBoolField(TEXT("hasResource"), Br.GetResourceObject() != nullptr);
			Entry->SetNumberField(TEXT("outlineWidth"), Br.OutlineSettings.Width);
			Entry->SetStringField(TEXT("outlineColor"), Br.OutlineSettings.Color.GetSpecifiedColor().ToString());
			if (UMaterialInstanceDynamic* Material =
				Cast<UMaterialInstanceDynamic>(Br.GetResourceObject()))
			{
				Entry->SetStringField(TEXT("materialName"), Material->GetName());
				for (const TCHAR* ParamName : { TEXT("FillColor"), TEXT("StrokeColor"), TEXT("TrackColor"), TEXT("ElementSize") })
				{
					FLinearColor Value;
					if (Material->GetVectorParameterValue(FName(ParamName), Value))
					{
						Entry->SetStringField(ParamName, Value.ToString());
					}
				}
				for (const TCHAR* ParamName : { TEXT("RadiusPx"), TEXT("BorderWidthPx"), TEXT("Percent") })
				{
					float Value = 0.0f;
					if (Material->GetScalarParameterValue(FName(ParamName), Value))
					{
						Entry->SetNumberField(ParamName, Value);
					}
				}
			}
		}
		// Reflectively read any FLinearColor named ColorAndOpacity to find a
		// content tint introduced by an intermediate node (e.g. CommonButton).
		for (const TCHAR* PropName : { TEXT("ColorAndOpacity"), TEXT("ContentColorAndOpacity") })
		{
			if (FStructProperty* SP = CastField<FStructProperty>(W->GetClass()->FindPropertyByName(FName(PropName))))
			{
				if (SP->Struct == TBaseStructure<FLinearColor>::Get())
				{
					const FLinearColor* C = SP->ContainerPtrToValuePtr<FLinearColor>(W);
					if (C)
					{
						Entry->SetStringField(PropName, C->ToString());
					}
				}
			}
		}
		if (UUserWidget* ChildUW = Cast<UUserWidget>(W))
		{
			Entry->SetStringField(TEXT("colorAndOpacity"), ChildUW->GetColorAndOpacity().ToString());
			Entry->SetBoolField(TEXT("isEnabled"), ChildUW->GetIsEnabled());
			Out.Add(MakeShared<FJsonValueObject>(Entry));
			DumpWidgetColorsRecursive(ChildUW, Prefix + W->GetName() + TEXT("/"), Out);
			continue;
		}
		Out.Add(MakeShared<FJsonValueObject>(Entry));
	}
}

// Renders one Widget Blueprint in isolation to a PNG via FWidgetRenderer.
// Unlike capture_slate_window (whole editor window, where the designer canvas
// and Details panel occlude the preview), this draws only the widget at its
// own size onto a chosen backdrop, so the design system can be verified clean.
TSharedPtr<FJsonValue> FWidgetHandlers::CaptureWidget(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (auto Err = RequireString(Params, TEXT("assetPath"), AssetPath)) return Err;

	// Resolve the generated widget class ("/Game/.../WBP_X.WBP_X_C").
	FString Package, ObjectName;
	if (!AssetPath.Split(TEXT("."), &Package, &ObjectName))
	{
		Package = AssetPath;
		ObjectName = FPackageName::GetShortName(AssetPath);
	}
	if (ObjectName.EndsWith(TEXT("_C")))
	{
		ObjectName.LeftChopInline(2);
	}
	const FString ClassPath = Package + TEXT(".") + ObjectName + TEXT("_C");
	UClass* WidgetClass = LoadObject<UClass>(nullptr, *ClassPath);
	if (!WidgetClass || !WidgetClass->IsChildOf(UUserWidget::StaticClass()))
	{
		return MCPError(FString::Printf(TEXT("Not a UserWidget class: %s"), *ClassPath));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return MCPError(TEXT("Editor world not available for widget render"));
	}

	UUserWidget* Widget = CreateWidget<UUserWidget>(World, WidgetClass);
	if (!Widget)
	{
		return MCPError(FString::Printf(TEXT("CreateWidget failed for %s"), *ClassPath));
	}

	TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
	TFunction<void(UUserWidget*)> SynchronizeUserWidgetTree =
		[&SynchronizeUserWidgetTree](UUserWidget* UserWidget)
	{
		if (!UserWidget)
		{
			return;
		}

		UserWidget->SynchronizeProperties();
		if (!UserWidget->WidgetTree)
		{
			return;
		}

		UserWidget->WidgetTree->ForEachWidget([&SynchronizeUserWidgetTree](UWidget* ChildWidget)
		{
			if (!ChildWidget)
			{
				return;
			}

			ChildWidget->SynchronizeProperties();
			if (UUserWidget* ChildUserWidget = Cast<UUserWidget>(ChildWidget))
			{
				SynchronizeUserWidgetTree(ChildUserWidget);
			}
		});
	};
	SynchronizeUserWidgetTree(Widget);

	// Optional diagnostic: dump each runtime widget's effective color/opacity.
	TArray<TSharedPtr<FJsonValue>> ColorDump;

	// Size: explicit params win; otherwise the widget's prepass desired size.
	int32 Width = OptionalInt(Params, TEXT("width"), 0);
	int32 Height = OptionalInt(Params, TEXT("height"), 0);
	if (Width <= 0 || Height <= 0)
	{
		SlateWidget->SlatePrepass(1.0f);
		const FVector2D Desired = SlateWidget->GetDesiredSize();
		if (Width <= 0) Width = FMath::CeilToInt(Desired.X);
		if (Height <= 0) Height = FMath::CeilToInt(Desired.Y);
	}
	if (Width <= 0 || Height <= 0)
	{
		return MCPError(TEXT("Could not determine widget size; pass width/height"));
	}
	Width = FMath::Clamp(Width, 8, 8192);
	Height = FMath::Clamp(Height, 8, 8192);

	// Backdrop fill for areas the widget leaves transparent (default surface-app).
	const FLinearColor Background = McpColorFromJson(
		Params->TryGetField(TEXT("background")),
		FColor::FromHex(TEXT("1A1A1A")).ReinterpretAsLinear());

	// Slate UI colors are authored without sRGB->linear decode, so render
	// without gamma correction by default to match the on-screen appearance.
	const bool bGammaCorrect = OptionalBool(Params, TEXT("gammaCorrect"), false);

	UTextureRenderTarget2D* RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
		World, Width, Height, ETextureRenderTargetFormat::RTF_RGBA8_SRGB, Background, false);
	if (!RenderTarget)
	{
		return MCPError(TEXT("Failed to create render target for widget"));
	}

	FWidgetRenderer* Renderer = new FWidgetRenderer(bGammaCorrect, /*bInClearTarget=*/true);
	const FVector2D DrawSize(Width, Height);
	// Draw twice: first pass settles layout, second pass renders final pixels.
	Renderer->DrawWidget(RenderTarget, SlateWidget, DrawSize, 0.0f);
	FlushRenderingCommands();
	RefreshBorderMaterialElementSizes(Widget);
	Renderer->DrawWidget(RenderTarget, SlateWidget, DrawSize, 0.0f);
	FlushRenderingCommands();
	BeginCleanup(Renderer);
	if (OptionalBool(Params, TEXT("dumpColors"), false))
	{
		DumpWidgetColorsRecursive(Widget, TEXT(""), ColorDump);
	}

	FString Filename;
	if (auto Err = RequireString(Params, TEXT("filename"), Filename)) return Err;
	if (FPaths::IsRelative(Filename))
	{
		Filename = FPaths::Combine(FPaths::ProjectDir(), Filename);
	}
	if (!Filename.EndsWith(TEXT(".png")))
	{
		Filename += TEXT(".png");
	}
	const FString OutDir = FPaths::GetPath(Filename);
	const FString OutName = FPaths::GetCleanFilename(Filename);
	IFileManager::Get().MakeDirectory(*OutDir, /*Tree=*/true);
	UKismetRenderingLibrary::ExportRenderTarget(World, RenderTarget, OutDir, OutName);

	const int64 FileSize = IFileManager::Get().FileSize(*Filename);
	if (FileSize <= 0)
	{
		return MCPError(FString::Printf(TEXT("Widget capture did not write a file: %s"), *Filename));
	}

	auto Result = MCPSuccess();
	Result->SetStringField(TEXT("filename"), Filename);
	Result->SetStringField(TEXT("widgetClass"), ClassPath);
	Result->SetNumberField(TEXT("width"), Width);
	Result->SetNumberField(TEXT("height"), Height);
	Result->SetNumberField(TEXT("sizeBytes"), static_cast<double>(FileSize));
	Result->SetBoolField(TEXT("gammaCorrect"), bGammaCorrect);
	if (ColorDump.Num() > 0)
	{
		Result->SetArrayField(TEXT("colorDump"), ColorDump);
	}
	return MCPResult(Result);
}

TSharedPtr<FJsonValue> FWidgetHandlers::EnsureWidgetRenderOpacityAnimations(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (auto Err = RequireStringAlt(Params, TEXT("assetPath"), TEXT("path"), AssetPath)) return Err;

	const TArray<TSharedPtr<FJsonValue>>* AnimationSpecs = nullptr;
	if (!Params->TryGetArrayField(TEXT("animations"), AnimationSpecs) || !AnimationSpecs)
	{
		return MCPError(TEXT("Missing 'animations' array."));
	}

	UWidgetBlueprint* WidgetBP = LoadAssetByPath<UWidgetBlueprint>(AssetPath);
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		return MCPError(FString::Printf(TEXT("Failed to load WidgetBlueprint at '%s'"), *AssetPath));
	}

	WidgetBP->Modify();
	WidgetBP->WidgetTree->Modify();

	auto FindWidgetByName = [WidgetBP](const FString& WidgetName) -> UWidget*
	{
		UWidget* FoundWidget = nullptr;
		WidgetBP->WidgetTree->ForEachWidget([&](UWidget* Widget)
		{
			if (Widget && Widget->GetName() == WidgetName)
			{
				FoundWidget = Widget;
			}
		});
		return FoundWidget;
	};

	TArray<TSharedPtr<FJsonValue>> CreatedAnimations;
	TArray<FString> Errors;
	constexpr double DefaultDurationSeconds = 0.12;
	const FFrameRate TickResolution(60000, 1);
	const FFrameRate DisplayRate(60, 1);

	for (const TSharedPtr<FJsonValue>& SpecValue : *AnimationSpecs)
	{
		const TSharedPtr<FJsonObject>* SpecPtr = nullptr;
		if (!SpecValue.IsValid() || !SpecValue->TryGetObject(SpecPtr) || !SpecPtr || !(*SpecPtr).IsValid())
		{
			Errors.Add(TEXT("Animation spec must be an object."));
			continue;
		}

		const TSharedPtr<FJsonObject>& Spec = *SpecPtr;
		FString AnimationName;
		FString WidgetName;
		if (!Spec->TryGetStringField(TEXT("animationName"), AnimationName) || AnimationName.IsEmpty())
		{
			Errors.Add(TEXT("Animation spec missing 'animationName'."));
			continue;
		}
		if (!Spec->TryGetStringField(TEXT("widgetName"), WidgetName) || WidgetName.IsEmpty())
		{
			Errors.Add(FString::Printf(TEXT("%s missing 'widgetName'."), *AnimationName));
			continue;
		}

		UWidget* TargetWidget = FindWidgetByName(WidgetName);
		if (!TargetWidget)
		{
			Errors.Add(FString::Printf(TEXT("%s target widget not found: %s"), *AnimationName, *WidgetName));
			continue;
		}

		for (int32 Index = WidgetBP->Animations.Num() - 1; Index >= 0; --Index)
		{
			UWidgetAnimation* ExistingAnimation = WidgetBP->Animations[Index];
			if (!ExistingAnimation)
			{
				WidgetBP->Animations.RemoveAt(Index);
				continue;
			}

			const bool bNameMatches = ExistingAnimation->GetName() == AnimationName;
#if WITH_EDITOR
			const bool bLabelMatches = ExistingAnimation->GetDisplayLabel() == AnimationName;
#else
			const bool bLabelMatches = false;
#endif
			if (bNameMatches || bLabelMatches)
			{
				WidgetBP->Animations.RemoveAt(Index);
				WidgetBP->WidgetVariableNameToGuidMap.Remove(ExistingAnimation->GetFName());
				if (ExistingAnimation->GetMovieScene())
				{
					WidgetBP->WidgetVariableNameToGuidMap.Remove(ExistingAnimation->GetMovieScene()->GetFName());
				}
				const FString RemovedName = FString::Printf(
					TEXT("__McpRemoved_%s_%s"),
					*ExistingAnimation->GetName(),
					*FGuid::NewGuid().ToString(EGuidFormats::Digits));
				ExistingAnimation->Rename(
					*RemovedName,
					GetTransientPackage(),
					REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional);
				ExistingAnimation->SetFlags(RF_Transient);
			}
		}

		double DurationSeconds = DefaultDurationSeconds;
		Spec->TryGetNumberField(TEXT("duration"), DurationSeconds);
		DurationSeconds = FMath::Max(DurationSeconds, 1.0 / DisplayRate.AsDecimal());
		const FFrameNumber EndFrame(static_cast<int32>(
			FMath::Max<int64>(1, FMath::RoundToInt(DurationSeconds * TickResolution.AsDecimal()))));

		UWidgetAnimation* NewAnimation = NewObject<UWidgetAnimation>(
			WidgetBP,
			FName(*AnimationName),
			RF_Transactional);
		if (!NewAnimation)
		{
			Errors.Add(FString::Printf(TEXT("Failed to create animation: %s"), *AnimationName));
			continue;
		}

#if WITH_EDITOR
		NewAnimation->SetDisplayLabel(AnimationName);
#endif
		UMovieScene* MovieScene = NewObject<UMovieScene>(NewAnimation, FName(*AnimationName), RF_Transactional);
		NewAnimation->MovieScene = MovieScene;
		MovieScene->SetTickResolutionDirectly(TickResolution);
		MovieScene->SetDisplayRate(DisplayRate);
		MovieScene->SetPlaybackRange(FFrameNumber(0), EndFrame.Value);

		const FGuid BindingGuid = MovieScene->AddPossessable(WidgetName, TargetWidget->GetClass());
		FWidgetAnimationBinding Binding;
		Binding.WidgetName = TargetWidget->GetFName();
		Binding.AnimationGuid = BindingGuid;
		Binding.bIsRootWidget = WidgetBP->WidgetTree->RootWidget == TargetWidget;
		NewAnimation->AnimationBindings.Add(Binding);

		UMovieSceneFloatTrack* FloatTrack = MovieScene->AddTrack<UMovieSceneFloatTrack>(BindingGuid);
		if (!FloatTrack)
		{
			Errors.Add(FString::Printf(TEXT("Failed to create RenderOpacity track for: %s"), *AnimationName));
			continue;
		}
		FloatTrack->SetPropertyNameAndPath(FName(TEXT("RenderOpacity")), TEXT("RenderOpacity"));

		UMovieSceneFloatSection* FloatSection = Cast<UMovieSceneFloatSection>(FloatTrack->CreateNewSection());
		if (!FloatSection)
		{
			Errors.Add(FString::Printf(TEXT("Failed to create RenderOpacity section for: %s"), *AnimationName));
			continue;
		}
		FloatTrack->AddSection(*FloatSection);
		FloatSection->SetRange(TRange<FFrameNumber>::Inclusive(FFrameNumber(0), EndFrame));

		FMovieSceneFloatChannel& Channel = FloatSection->GetChannel();
		const TArray<TSharedPtr<FJsonValue>>* KeySpecs = nullptr;
		if (Spec->TryGetArrayField(TEXT("keys"), KeySpecs) && KeySpecs && KeySpecs->Num() > 0)
		{
			for (const TSharedPtr<FJsonValue>& KeyValue : *KeySpecs)
			{
				const TSharedPtr<FJsonObject>* KeyObjPtr = nullptr;
				if (!KeyValue.IsValid() || !KeyValue->TryGetObject(KeyObjPtr) || !KeyObjPtr || !(*KeyObjPtr).IsValid())
				{
					continue;
				}

				double KeyTime = 0.0;
				double KeyOpacity = 0.0;
				(*KeyObjPtr)->TryGetNumberField(TEXT("time"), KeyTime);
				(*KeyObjPtr)->TryGetNumberField(TEXT("value"), KeyOpacity);
				const FFrameNumber KeyFrame(static_cast<int32>(FMath::Clamp<int64>(
					FMath::RoundToInt(KeyTime * TickResolution.AsDecimal()),
					0,
					EndFrame.Value)));
				Channel.AddCubicKey(KeyFrame, static_cast<float>(KeyOpacity));
			}
		}
		else
		{
			Channel.AddCubicKey(FFrameNumber(0), 0.0f);
			Channel.AddCubicKey(EndFrame, 1.0f);
		}

		WidgetBP->Animations.Add(NewAnimation);
		if (!WidgetBP->WidgetVariableNameToGuidMap.Contains(NewAnimation->GetFName()))
		{
			WidgetBP->WidgetVariableNameToGuidMap.Add(NewAnimation->GetFName(), FGuid::NewGuid());
		}
		if (!WidgetBP->WidgetVariableNameToGuidMap.Contains(MovieScene->GetFName()))
		{
			WidgetBP->WidgetVariableNameToGuidMap.Add(MovieScene->GetFName(), FGuid::NewGuid());
		}

		TSharedPtr<FJsonObject> Created = MakeShared<FJsonObject>();
		Created->SetStringField(TEXT("animationName"), AnimationName);
		Created->SetStringField(TEXT("widgetName"), WidgetName);
		Created->SetNumberField(TEXT("duration"), DurationSeconds);
		CreatedAnimations.Add(MakeShared<FJsonValueObject>(Created));
	}

	McpSynchronizeWidgetBlueprintCompilerState(WidgetBP);
	WidgetBP->MarkPackageDirty();
	FCompilerResultsLog CompileLog;
	FKismetEditorUtilities::CompileBlueprint(WidgetBP, EBlueprintCompileOptions::None, &CompileLog);
	UEditorAssetLibrary::SaveAsset(AssetPath);

	if (Errors.Num() > 0)
	{
		return MCPError(FString::Printf(
			TEXT("One or more widget opacity animations failed: %s"),
			*FString::Join(Errors, TEXT("; "))));
	}

	auto Result = MCPSuccess();
	Result->SetStringField(TEXT("assetPath"), AssetPath);
	Result->SetArrayField(TEXT("createdAnimations"), CreatedAnimations);
	Result->SetNumberField(TEXT("compileErrors"), CompileLog.NumErrors);
	Result->SetNumberField(TEXT("compileWarnings"), CompileLog.NumWarnings);
	return MCPResult(Result);
}

void FWidgetHandlers::RegisterHandlers(FMCPHandlerRegistry& Registry)
{
	Registry.RegisterHandler(TEXT("capture_widget"), &CaptureWidget);
	Registry.RegisterHandler(TEXT("list_widget_blueprints"), &ListWidgetBlueprints);
	Registry.RegisterHandler(TEXT("create_widget_blueprint"), &CreateWidgetBlueprint);
	Registry.RegisterHandler(TEXT("read_widget_tree"), &ReadWidgetTree);
	Registry.RegisterHandler(TEXT("apply_widget_tree_spec"), &ApplyWidgetTreeSpec);
	Registry.RegisterHandler(TEXT("create_editor_utility_widget"), &CreateEditorUtilityWidget);
	Registry.RegisterHandler(TEXT("create_editor_utility_blueprint"), &CreateEditorUtilityBlueprint);
	Registry.RegisterHandler(TEXT("get_widget_details"), &GetWidgetProperties);
	Registry.RegisterHandler(TEXT("get_widget_properties"), &GetWidgetFullProperties);
	Registry.RegisterHandler(TEXT("list_widget_bindings"), &ListWidgetBindings);
	Registry.RegisterHandler(TEXT("clear_widget_binding"), &ClearWidgetBinding);
	Registry.RegisterHandler(TEXT("set_widget_property"), &SetWidgetProperty);
	Registry.RegisterHandler(TEXT("read_widget_animations"), &ReadWidgetAnimations);
	Registry.RegisterHandler(TEXT("ensure_widget_render_opacity_animations"), &EnsureWidgetRenderOpacityAnimations);
	Registry.RegisterHandler(TEXT("run_editor_utility_widget"), &RunEditorUtilityWidget);
	Registry.RegisterHandler(TEXT("run_editor_utility_blueprint"), &RunEditorUtilityBlueprint);
	Registry.RegisterHandler(TEXT("add_widget"), &AddWidget);
	Registry.RegisterHandler(TEXT("replace_widget_classes"), &ReplaceWidgetClasses);
	Registry.RegisterHandler(TEXT("set_named_slot_content"), &SetNamedSlotContent);
	Registry.RegisterHandler(TEXT("remove_widget"), &RemoveWidget);
	Registry.RegisterHandler(TEXT("rename_widget"), &RenameWidget);
	Registry.RegisterHandler(TEXT("move_widget"), &MoveWidget);
	Registry.RegisterHandler(TEXT("repair_widget_blueprint"), &RepairWidgetBlueprint);
	Registry.RegisterHandler(TEXT("set_root_widget"), &SetRoot);
	Registry.RegisterHandler(TEXT("wrap_root_widget"), &WrapRoot);
	Registry.RegisterHandler(TEXT("list_widget_classes"), &ListWidgetClasses);
	Registry.RegisterHandler(TEXT("list_runtime_widgets"), &ListRuntimeWidgets);
	Registry.RegisterHandler(TEXT("get_runtime_widget"), &GetRuntimeWidget);
	Registry.RegisterHandler(TEXT("dump_runtime_widget_geometry"), &DumpRuntimeWidgetGeometry);
	Registry.RegisterHandler(TEXT("spawn_runtime_widget_preview"), &SpawnRuntimeWidgetPreview);
	Registry.RegisterHandler(TEXT("dispatch_runtime_widget_pointer_event"), &DispatchRuntimeWidgetPointerEvent);
	// #161: Runtime delegate inspection
	Registry.RegisterHandler(TEXT("get_runtime_delegates"), &GetRuntimeDelegates);
}

UWidget* FWidgetHandlers::FindWidgetByNameRecursive(UWidget* Root, const FString& WidgetName)
{
	if (!Root) return nullptr;

	if (Root->GetName() == WidgetName)
	{
		return Root;
	}

	UPanelWidget* PanelWidget = Cast<UPanelWidget>(Root);
	if (PanelWidget)
	{
		for (int32 i = 0; i < PanelWidget->GetChildrenCount(); ++i)
		{
			UWidget* Child = PanelWidget->GetChildAt(i);
			UWidget* Found = FindWidgetByNameRecursive(Child, WidgetName);
			if (Found)
			{
				return Found;
			}
		}
	}

	return nullptr;
}

TSharedPtr<FJsonValue> FWidgetHandlers::ListWidgetBlueprints(const TSharedPtr<FJsonObject>& Params)
{
	bool bRecursive = OptionalBool(Params, TEXT("recursive"), true);

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/UMGEditor"), TEXT("WidgetBlueprint")), AssetDataList, bRecursive);

	TArray<TSharedPtr<FJsonValue>> AssetsArray;
	for (const FAssetData& AssetData : AssetDataList)
	{
		TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
		AssetObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
		AssetObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
		AssetObj->SetStringField(TEXT("packagePath"), AssetData.PackagePath.ToString());
		AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
	}

	auto Result = MCPSuccess();
	Result->SetArrayField(TEXT("assets"), AssetsArray);
	Result->SetNumberField(TEXT("count"), AssetsArray.Num());

	return MCPResult(Result);
}

TSharedPtr<FJsonValue> FWidgetHandlers::CreateWidgetBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (auto Err = RequireString(Params, TEXT("name"), Name)) return Err;

	FString PackagePath = OptionalString(Params, TEXT("packagePath"), TEXT("/Game/UI/Widgets"));
	const FString OnConflict = OptionalString(Params, TEXT("onConflict"), TEXT("skip"));
	FString ParentClassName = OptionalString(Params, TEXT("parentClass"), TEXT("UserWidget"));

	// (#134) Resolve parentClass string - accept short names ("UserWidget"),
	// short names with U prefix, and full class paths. Default to UUserWidget
	// only when the caller didn't pass a parentClass.
	UClass* ParentClass = nullptr;
	ParentClass = FindClassByShortName(ParentClassName);
	if (!ParentClass)
	{
		ParentClass = LoadObject<UClass>(nullptr, *ParentClassName);
	}
	if (!ParentClass)
	{
		ParentClass = UUserWidget::StaticClass();
	}
	if (!ParentClass->IsChildOf(UUserWidget::StaticClass()))
	{
		return MCPError(FString::Printf(TEXT("parentClass '%s' is not a UUserWidget subclass"), *ParentClassName));
	}

	UWidgetBlueprintFactory* WidgetFactory = NewObject<UWidgetBlueprintFactory>();
	WidgetFactory->ParentClass = ParentClass;

	auto Created = MCPCreateAssetIdempotent<UWidgetBlueprint>(Name, PackagePath, OnConflict, TEXT("WidgetBlueprint"), WidgetFactory);
	if (Created.EarlyReturn) return Created.EarlyReturn;

	UEditorAssetLibrary::SaveAsset(Created.Asset->GetPathName());

	auto Result = MCPSuccess();
	MCPSetCreated(Result);
	Result->SetStringField(TEXT("path"), Created.Asset->GetPathName());
	Result->SetStringField(TEXT("name"), Name);
	Result->SetStringField(TEXT("parentClass"), ParentClass->GetPathName());
	MCPSetDeleteAssetRollback(Result, Created.Asset->GetPathName());

	return MCPResult(Result);
}

TSharedPtr<FJsonValue> FWidgetHandlers::ReadWidgetTree(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (auto Err = RequireStringAlt(Params, TEXT("assetPath"), TEXT("path"), AssetPath)) return Err;

	UWidgetBlueprint* WidgetBP = LoadAssetByPath<UWidgetBlueprint>(AssetPath);
	if (!WidgetBP)
	{
		return MCPError(FString::Printf(TEXT("Failed to load WidgetBlueprint at '%s'"), *AssetPath));
	}

	auto Result = MCPSuccess();

	// Recursive lambda to build widget hierarchy
	TFunction<TSharedPtr<FJsonObject>(UWidget*)> BuildWidgetJson = [&](UWidget* Widget) -> TSharedPtr<FJsonObject>
	{
		if (!Widget) return nullptr;

		TSharedPtr<FJsonObject> WidgetObj = MakeShared<FJsonObject>();
		WidgetObj->SetStringField(TEXT("name"), Widget->GetName());
		WidgetObj->SetStringField(TEXT("class"), Widget->GetClass()->GetName());
		WidgetObj->SetBoolField(TEXT("isVisible"), Widget->IsVisible());

		// If it's a panel widget, recurse into children
		UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget);
		if (PanelWidget)
		{
			TArray<TSharedPtr<FJsonValue>> ChildrenArray;
			for (int32 i = 0; i < PanelWidget->GetChildrenCount(); ++i)
			{
				UWidget* Child = PanelWidget->GetChildAt(i);
				TSharedPtr<FJsonObject> ChildObj = BuildWidgetJson(Child);
				if (ChildObj.IsValid())
				{
					ChildrenArray.Add(MakeShared<FJsonValueObject>(ChildObj));
				}
			}
			WidgetObj->SetArrayField(TEXT("children"), ChildrenArray);
		}

		return WidgetObj;
	};

	// Get the root widget from the WidgetTree
	UWidget* RootWidget = WidgetBP->WidgetTree ? WidgetBP->WidgetTree->RootWidget : nullptr;
	if (RootWidget)
	{
		TSharedPtr<FJsonObject> TreeObj = BuildWidgetJson(RootWidget);
		Result->SetObjectField(TEXT("widgetTree"), TreeObj);
	}
	else
	{
		Result->SetStringField(TEXT("widgetTree"), TEXT("empty"));
	}

	return MCPResult(Result);
}

TSharedPtr<FJsonValue> FWidgetHandlers::ApplyWidgetTreeSpec(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (auto Err = RequireStringAlt(Params, TEXT("assetPath"), TEXT("path"), AssetPath)) return Err;

	const TSharedPtr<FJsonObject>* RootSpecPtr = nullptr;
	if (!Params->TryGetObjectField(TEXT("root"), RootSpecPtr) || !RootSpecPtr || !(*RootSpecPtr).IsValid())
	{
		return MCPError(TEXT("Missing 'root' widget spec object."));
	}

	UWidgetBlueprint* WidgetBP = LoadAssetByPath<UWidgetBlueprint>(AssetPath);
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		return MCPError(FString::Printf(TEXT("Failed to load WidgetBlueprint at '%s'"), *AssetPath));
	}

	WidgetBP->Modify();
	WidgetBP->WidgetTree->Modify();

	// clearExisting:false performs a name-keyed merge (see BuildWidget): existing
	// widgets are reused in place with their authored props/slots preserved, and
	// only spec nodes absent from the tree are constructed. clearExisting:true
	// wipes the tree first for a full rebuild.
	const bool bClearExisting = OptionalBool(Params, TEXT("clearExisting"), true);
	if (bClearExisting)
	{
		TArray<UWidget*> ExistingWidgets;
		WidgetBP->WidgetTree->ForEachWidget([&](UWidget* Widget)
		{
			if (Widget)
			{
				ExistingWidgets.Add(Widget);
			}
		});
		WidgetBP->WidgetTree->RootWidget = nullptr;
		int32 RemovedWidgetIndex = 0;
		for (UWidget* Widget : ExistingWidgets)
		{
			if (Widget)
			{
				WidgetBP->WidgetVariableNameToGuidMap.Remove(Widget->GetFName());
				const FString RemovedName = FString::Printf(
					TEXT("__McpRemoved_%s_%d_%s"),
					*Widget->GetName(),
					RemovedWidgetIndex++,
					*FGuid::NewGuid().ToString(EGuidFormats::Digits));
				WidgetBP->WidgetTree->RemoveWidget(Widget);
				Widget->Rename(
					*RemovedName,
					GetTransientPackage(),
					REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional);
				Widget->SetFlags(RF_Transient);
			}
		}
		McpSynchronizeWidgetBlueprintCompilerState(WidgetBP);
	}

	int32 CreatedCount = 0;
	int32 PropertySetCount = 0;
	TArray<FString> Errors;

	TFunction<void(UWidget*, const TSharedPtr<FJsonObject>&)> ApplyProperties =
		[&](UWidget* Widget, const TSharedPtr<FJsonObject>& Spec)
	{
		const TSharedPtr<FJsonObject>* PropsPtr = nullptr;
		if (!Widget || !Spec->TryGetObjectField(TEXT("props"), PropsPtr) || !PropsPtr || !(*PropsPtr).IsValid())
		{
			return;
		}

		const TSharedPtr<FJsonObject>& Props = *PropsPtr;

		// UBorder brush authoring: compose rounded-box / 9-slice texture surfaces in
		// one pass so brushColor, drawAs, radius, outlineColor, outlineWidth, texture,
		// brushMargin, and imageSize cooperate instead of overwriting each other.
		// These keys are skipped in the per-key loop below for borders.
		if (UBorder* BrushBorder = Cast<UBorder>(Widget))
		{
			static const TCHAR* BrushKeys[] = {
				TEXT("brushColor"), TEXT("drawAs"), TEXT("radius"), TEXT("outlineColor"),
				TEXT("outlineWidth"), TEXT("texture"), TEXT("resourceObject"),
				TEXT("brushMargin"), TEXT("imageSize") };
			bool bAnyBrushKey = false;
			for (const TCHAR* K : BrushKeys) { if (Props->HasField(K)) { bAnyBrushKey = true; break; } }
			if (bAnyBrushKey)
			{
				FSlateBrush Brush = BrushBorder->Background;
				FLinearColor Fill = BrushBorder->GetBrushColor();

				FString DrawAsStr;
				if (Props->TryGetStringField(TEXT("drawAs"), DrawAsStr))
				{
					const FString V = DrawAsStr.ToLower();
					if (V == TEXT("box")) Brush.DrawAs = ESlateBrushDrawType::Box;
					else if (V == TEXT("roundedbox")) Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
					else if (V == TEXT("image")) Brush.DrawAs = ESlateBrushDrawType::Image;
					else if (V == TEXT("border")) Brush.DrawAs = ESlateBrushDrawType::Border;
					else if (V == TEXT("none") || V == TEXT("nodrawtype")) Brush.DrawAs = ESlateBrushDrawType::NoDrawType;
				}
				double Radius = 0.0;
				if (Props->TryGetNumberField(TEXT("radius"), Radius))
				{
					Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
					Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
					Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
				}
				if (Props->HasField(TEXT("outlineColor")))
				{
					Brush.OutlineSettings.Color = FSlateColor(McpColorFromJson(Props->TryGetField(TEXT("outlineColor"))));
				}
				double OutlineWidth = 0.0;
				if (Props->TryGetNumberField(TEXT("outlineWidth"), OutlineWidth))
				{
					Brush.OutlineSettings.Width = static_cast<float>(OutlineWidth);
				}
				FString TexturePath;
				if (Props->TryGetStringField(TEXT("texture"), TexturePath)
					|| Props->TryGetStringField(TEXT("resourceObject"), TexturePath))
				{
					if (UObject* Resource = LoadObject<UObject>(nullptr, *TexturePath))
					{
						Brush.SetResourceObject(Resource);
					}
				}
				if (Props->HasField(TEXT("brushMargin")))
				{
					Brush.Margin = McpMarginFromJson(Props->TryGetField(TEXT("brushMargin")));
				}
				const TArray<TSharedPtr<FJsonValue>>* SizeArr = nullptr;
				if (Props->TryGetArrayField(TEXT("imageSize"), SizeArr) && SizeArr && SizeArr->Num() >= 2)
				{
					Brush.ImageSize = FVector2D((*SizeArr)[0]->AsNumber(), (*SizeArr)[1]->AsNumber());
				}
				if (Props->HasField(TEXT("brushColor")))
				{
					Fill = McpColorFromJson(Props->TryGetField(TEXT("brushColor")));
					Brush.TintColor = FSlateColor(Fill);
				}
				BrushBorder->SetBrush(Brush);
				BrushBorder->SetBrushColor(Fill);
				PropertySetCount++;
			}
		}

		// UImage brush authoring: mirror the border pass for textures, sizing, and
		// tint so icon/glyph images (e.g. stepper carets, dropdown checks) can be
		// composed in one spec node. Image tint is applied via ColorAndOpacity.
		if (UImage* BrushImage = Cast<UImage>(Widget))
		{
			static const TCHAR* ImageBrushKeys[] = {
				TEXT("brushColor"), TEXT("drawAs"), TEXT("radius"), TEXT("outlineColor"),
				TEXT("outlineWidth"), TEXT("texture"), TEXT("resourceObject"),
				TEXT("brushMargin"), TEXT("imageSize") };
			bool bAnyImageBrushKey = false;
			for (const TCHAR* K : ImageBrushKeys) { if (Props->HasField(K)) { bAnyImageBrushKey = true; break; } }
			if (bAnyImageBrushKey)
			{
				FSlateBrush Brush = BrushImage->GetBrush();
				FString TexturePath;
				if (Props->TryGetStringField(TEXT("texture"), TexturePath)
					|| Props->TryGetStringField(TEXT("resourceObject"), TexturePath))
				{
					if (UObject* Resource = LoadObject<UObject>(nullptr, *TexturePath))
					{
						Brush.SetResourceObject(Resource);
						if (Brush.DrawAs == ESlateBrushDrawType::NoDrawType)
						{
							Brush.DrawAs = ESlateBrushDrawType::Image;
						}
					}
				}
				FString DrawAsStr;
				if (Props->TryGetStringField(TEXT("drawAs"), DrawAsStr))
				{
					const FString V = DrawAsStr.ToLower();
					if (V == TEXT("box")) Brush.DrawAs = ESlateBrushDrawType::Box;
					else if (V == TEXT("roundedbox")) Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
					else if (V == TEXT("image")) Brush.DrawAs = ESlateBrushDrawType::Image;
					else if (V == TEXT("border")) Brush.DrawAs = ESlateBrushDrawType::Border;
					else if (V == TEXT("none") || V == TEXT("nodrawtype")) Brush.DrawAs = ESlateBrushDrawType::NoDrawType;
				}
				double Radius = 0.0;
				if (Props->TryGetNumberField(TEXT("radius"), Radius))
				{
					Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
					Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
					Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
				}
				if (Props->HasField(TEXT("outlineColor")))
				{
					Brush.OutlineSettings.Color = FSlateColor(McpColorFromJson(Props->TryGetField(TEXT("outlineColor"))));
				}
				double OutlineWidth = 0.0;
				if (Props->TryGetNumberField(TEXT("outlineWidth"), OutlineWidth))
				{
					Brush.OutlineSettings.Width = static_cast<float>(OutlineWidth);
				}
				const TArray<TSharedPtr<FJsonValue>>* SizeArr = nullptr;
				if (Props->TryGetArrayField(TEXT("imageSize"), SizeArr) && SizeArr && SizeArr->Num() >= 2)
				{
					Brush.ImageSize = FVector2D((*SizeArr)[0]->AsNumber(), (*SizeArr)[1]->AsNumber());
				}
				if (Props->HasField(TEXT("brushMargin")))
				{
					Brush.Margin = McpMarginFromJson(Props->TryGetField(TEXT("brushMargin")));
				}
				BrushImage->SetBrush(Brush);
				if (Props->HasField(TEXT("brushColor")))
				{
					BrushImage->SetColorAndOpacity(McpColorFromJson(Props->TryGetField(TEXT("brushColor"))));
				}
				PropertySetCount++;
			}
		}

		for (const auto& Pair : Props->Values)
		{
			const FString& Key = Pair.Key;
			const TSharedPtr<FJsonValue>& Value = Pair.Value;

			if (Key == TEXT("text"))
			{
				FString Text;
				if (Value->TryGetString(Text))
				{
					if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
					{
						TextBlock->SetText(FText::FromString(Text));
						PropertySetCount++;
						continue;
					}
					if (UEditableTextBox* TextBox = Cast<UEditableTextBox>(Widget))
					{
						TextBox->SetText(FText::FromString(Text));
						PropertySetCount++;
						continue;
					}
					if (UMultiLineEditableTextBox* MultiLineTextBox = Cast<UMultiLineEditableTextBox>(Widget))
					{
						MultiLineTextBox->SetText(FText::FromString(Text));
						PropertySetCount++;
						continue;
					}
				}
			}
			else if (Key == TEXT("font"))
			{
				const TSharedPtr<FJsonObject>* FontObj = nullptr;
				if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget);
					TextBlock && Value->TryGetObject(FontObj) && FontObj && (*FontObj).IsValid())
				{
					const int32 Size = (*FontObj)->HasField(TEXT("size"))
						? static_cast<int32>((*FontObj)->GetNumberField(TEXT("size")))
						: TextBlock->GetFont().Size;
					const bool bBold = (*FontObj)->HasField(TEXT("bold")) && (*FontObj)->GetBoolField(TEXT("bold"));
					TextBlock->SetFont(McpMakeFont(Size, bBold));
					PropertySetCount++;
					continue;
				}
			}
			else if (Key == TEXT("color"))
			{
				if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
				{
					TextBlock->SetColorAndOpacity(FSlateColor(McpColorFromJson(Value)));
					PropertySetCount++;
					continue;
				}
			}
			else if (Key == TEXT("brushColor") || Key == TEXT("drawAs") || Key == TEXT("radius")
				|| Key == TEXT("outlineColor") || Key == TEXT("outlineWidth") || Key == TEXT("texture")
				|| Key == TEXT("resourceObject") || Key == TEXT("brushMargin") || Key == TEXT("imageSize"))
			{
				// Borders and Images: handled by the brush-composition passes above.
				if (Cast<UBorder>(Widget) || Cast<UImage>(Widget))
				{
					continue;
				}
			}
			else if (Key == TEXT("padding"))
			{
				if (UBorder* Border = Cast<UBorder>(Widget))
				{
					Border->SetPadding(McpMarginFromJson(Value));
					PropertySetCount++;
					continue;
				}
			}
			else if (Key == TEXT("visibility"))
			{
				FString Visibility;
				if (Value->TryGetString(Visibility))
				{
					Widget->SetVisibility(McpVisibilityFromString(Visibility));
					PropertySetCount++;
					continue;
				}
			}
			else if (Key == TEXT("buttonStyle"))
			{
				const TSharedPtr<FJsonObject>* StyleObj = nullptr;
				if (UButton* Button = Cast<UButton>(Widget);
					Button && Value->TryGetObject(StyleObj) && StyleObj && (*StyleObj).IsValid())
				{
					McpApplyButtonStyle(Button, *StyleObj);
					PropertySetCount++;
					continue;
				}
			}
			else if (Key == TEXT("inputStyle"))
			{
				const TSharedPtr<FJsonObject>* StyleObj = nullptr;
				if (Value->TryGetObject(StyleObj) && StyleObj && (*StyleObj).IsValid())
				{
					if (UEditableTextBox* TextBox = Cast<UEditableTextBox>(Widget))
					{
						McpApplyEditableTextBoxStyle(TextBox, *StyleObj);
						PropertySetCount++;
						continue;
					}
					if (UMultiLineEditableTextBox* TextBox = Cast<UMultiLineEditableTextBox>(Widget))
					{
						McpApplyMultiLineEditableTextBoxStyle(TextBox, *StyleObj);
						PropertySetCount++;
						continue;
					}
					if (UComboBoxString* ComboBox = Cast<UComboBoxString>(Widget))
					{
						McpApplyComboBoxStyle(ComboBox, *StyleObj);
						PropertySetCount++;
						continue;
					}
				}
			}
			else if (Key == TEXT("comboOptions"))
			{
				const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
				if (UComboBoxString* ComboBox = Cast<UComboBoxString>(Widget);
					ComboBox && Value->TryGetArray(Options) && Options)
				{
					ComboBox->ClearOptions();
					for (const TSharedPtr<FJsonValue>& Option : *Options)
					{
						FString OptionText;
						if (Option->TryGetString(OptionText))
						{
							ComboBox->AddOption(OptionText);
						}
					}
					PropertySetCount++;
					continue;
				}
			}
			else if (Key == TEXT("widthOverride"))
			{
				if (USizeBox* SizeBox = Cast<USizeBox>(Widget))
				{
					SizeBox->SetWidthOverride(static_cast<float>(Value->AsNumber()));
					PropertySetCount++;
					continue;
				}
			}
			else if (Key == TEXT("heightOverride"))
			{
				if (USizeBox* SizeBox = Cast<USizeBox>(Widget))
				{
					SizeBox->SetHeightOverride(static_cast<float>(Value->AsNumber()));
					PropertySetCount++;
					continue;
				}
			}
			else if (Key == TEXT("minDesiredWidth"))
			{
				if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
				{
					TextBlock->SetMinDesiredWidth(static_cast<float>(Value->AsNumber()));
					PropertySetCount++;
					continue;
				}
			}
			else if (Key == TEXT("autoWrapText"))
			{
				if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
				{
					TextBlock->SetAutoWrapText(Value->AsBool());
					PropertySetCount++;
					continue;
				}
			}

			FProperty* Prop = Widget->GetClass()->FindPropertyByName(FName(*Key));
			if (Prop)
			{
				void* Addr = Prop->ContainerPtrToValuePtr<void>(Widget);
				FString SetError;
				if (MCPJsonProperty::SetJsonOnProperty(Prop, Addr, Value, SetError))
				{
					PropertySetCount++;
				}
				else
				{
					Errors.Add(FString::Printf(TEXT("%s.%s: %s"), *Widget->GetName(), *Key, *SetError));
				}
			}
		}
	};

	TFunction<void(UPanelSlot*, const TSharedPtr<FJsonObject>&)> ApplySlot =
		[&](UPanelSlot* Slot, const TSharedPtr<FJsonObject>& Spec)
	{
		const TSharedPtr<FJsonObject>* SlotObjPtr = nullptr;
		if (!Slot || !Spec->TryGetObjectField(TEXT("slot"), SlotObjPtr) || !SlotObjPtr || !(*SlotObjPtr).IsValid())
		{
			return;
		}

		const TSharedPtr<FJsonObject>& SlotObj = *SlotObjPtr;
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
		{
			if (const TSharedPtr<FJsonValue> AnchorsValue = SlotObj->TryGetField(TEXT("anchors")))
			{
				const TArray<TSharedPtr<FJsonValue>>* Parts = nullptr;
				if (AnchorsValue->TryGetArray(Parts) && Parts && Parts->Num() >= 4)
				{
					CanvasSlot->SetAnchors(FAnchors(
						static_cast<float>((*Parts)[0]->AsNumber()),
						static_cast<float>((*Parts)[1]->AsNumber()),
						static_cast<float>((*Parts)[2]->AsNumber()),
						static_cast<float>((*Parts)[3]->AsNumber())));
				}
			}
			if (const TSharedPtr<FJsonValue> PositionValue = SlotObj->TryGetField(TEXT("position")))
			{
				CanvasSlot->SetPosition(McpVector2FromJson(PositionValue));
			}
			if (const TSharedPtr<FJsonValue> SizeValue = SlotObj->TryGetField(TEXT("size")))
			{
				CanvasSlot->SetSize(McpVector2FromJson(SizeValue));
			}
			if (const TSharedPtr<FJsonValue> AlignmentValue = SlotObj->TryGetField(TEXT("alignment")))
			{
				CanvasSlot->SetAlignment(McpVector2FromJson(AlignmentValue));
			}
			if (SlotObj->HasField(TEXT("zOrder")))
			{
				CanvasSlot->SetZOrder(static_cast<int32>(SlotObj->GetNumberField(TEXT("zOrder"))));
			}
			if (SlotObj->HasField(TEXT("autoSize")))
			{
				CanvasSlot->SetAutoSize(SlotObj->GetBoolField(TEXT("autoSize")));
			}
			return;
		}

		if (const TSharedPtr<FJsonValue> PaddingValue = SlotObj->TryGetField(TEXT("padding")))
		{
			if (UHorizontalBoxSlot* HorizontalSlot = Cast<UHorizontalBoxSlot>(Slot))
			{
				HorizontalSlot->SetPadding(McpMarginFromJson(PaddingValue));
			}
			else if (UVerticalBoxSlot* VerticalSlot = Cast<UVerticalBoxSlot>(Slot))
			{
				VerticalSlot->SetPadding(McpMarginFromJson(PaddingValue));
			}
			else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Slot))
			{
				OverlaySlot->SetPadding(McpMarginFromJson(PaddingValue));
			}
			else if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Slot))
			{
				ButtonSlot->SetPadding(McpMarginFromJson(PaddingValue));
			}
			else if (UScrollBoxSlot* ScrollBoxSlot = Cast<UScrollBoxSlot>(Slot))
			{
				ScrollBoxSlot->SetPadding(McpMarginFromJson(PaddingValue));
			}
			else
			{
				FProperty* PaddingProp = Slot->GetClass()->FindPropertyByName(TEXT("Padding"));
				if (PaddingProp)
				{
					FString SetError;
					MCPJsonProperty::SetJsonOnProperty(PaddingProp, PaddingProp->ContainerPtrToValuePtr<void>(Slot), PaddingValue, SetError);
				}
			}
		}

		FString HAlign;
		FString VAlign;
		SlotObj->TryGetStringField(TEXT("hAlign"), HAlign);
		SlotObj->TryGetStringField(TEXT("vAlign"), VAlign);
		if (UHorizontalBoxSlot* HorizontalSlot = Cast<UHorizontalBoxSlot>(Slot))
		{
			if (!HAlign.IsEmpty()) HorizontalSlot->SetHorizontalAlignment(McpHAlignFromString(HAlign));
			if (!VAlign.IsEmpty()) HorizontalSlot->SetVerticalAlignment(McpVAlignFromString(VAlign));
			if (SlotObj->HasField(TEXT("sizeRule")) || SlotObj->HasField(TEXT("fillWeight")))
			{
				const bool bFill = OptionalString(SlotObj, TEXT("sizeRule"), TEXT("auto")).Equals(TEXT("fill"), ESearchCase::IgnoreCase);
				const float FillWeight = SlotObj->HasField(TEXT("fillWeight")) ? static_cast<float>(SlotObj->GetNumberField(TEXT("fillWeight"))) : 1.0f;
				FSlateChildSize ChildSize;
				ChildSize.SizeRule = bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic;
				ChildSize.Value = FillWeight;
				HorizontalSlot->SetSize(ChildSize);
			}
		}
		else if (UVerticalBoxSlot* VerticalSlot = Cast<UVerticalBoxSlot>(Slot))
		{
			if (!HAlign.IsEmpty()) VerticalSlot->SetHorizontalAlignment(McpHAlignFromString(HAlign));
			if (!VAlign.IsEmpty()) VerticalSlot->SetVerticalAlignment(McpVAlignFromString(VAlign));
			if (SlotObj->HasField(TEXT("sizeRule")) || SlotObj->HasField(TEXT("fillWeight")))
			{
				const bool bFill = OptionalString(SlotObj, TEXT("sizeRule"), TEXT("auto")).Equals(TEXT("fill"), ESearchCase::IgnoreCase);
				const float FillWeight = SlotObj->HasField(TEXT("fillWeight")) ? static_cast<float>(SlotObj->GetNumberField(TEXT("fillWeight"))) : 1.0f;
				FSlateChildSize ChildSize;
				ChildSize.SizeRule = bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic;
				ChildSize.Value = FillWeight;
				VerticalSlot->SetSize(ChildSize);
			}
		}
		else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Slot))
		{
			if (!HAlign.IsEmpty()) OverlaySlot->SetHorizontalAlignment(McpHAlignFromString(HAlign));
			if (!VAlign.IsEmpty()) OverlaySlot->SetVerticalAlignment(McpVAlignFromString(VAlign));
		}
		else if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Slot))
		{
			if (!HAlign.IsEmpty()) ButtonSlot->SetHorizontalAlignment(McpHAlignFromString(HAlign));
			if (!VAlign.IsEmpty()) ButtonSlot->SetVerticalAlignment(McpVAlignFromString(VAlign));
		}
		else if (UScrollBoxSlot* ScrollBoxSlot = Cast<UScrollBoxSlot>(Slot))
		{
			if (!HAlign.IsEmpty()) ScrollBoxSlot->SetHorizontalAlignment(McpHAlignFromString(HAlign));
			if (!VAlign.IsEmpty()) ScrollBoxSlot->SetVerticalAlignment(McpVAlignFromString(VAlign));
		}

		for (const auto& Pair : SlotObj->Values)
		{
			if (Pair.Key == TEXT("padding") || Pair.Key == TEXT("hAlign") || Pair.Key == TEXT("vAlign")
				|| Pair.Key == TEXT("sizeRule") || Pair.Key == TEXT("fillWeight")
				|| Pair.Key == TEXT("anchors") || Pair.Key == TEXT("position") || Pair.Key == TEXT("size")
				|| Pair.Key == TEXT("alignment") || Pair.Key == TEXT("zOrder") || Pair.Key == TEXT("autoSize"))
			{
				continue;
			}
			if (FProperty* Prop = Slot->GetClass()->FindPropertyByName(FName(*Pair.Key)))
			{
				FString SetError;
				MCPJsonProperty::SetJsonOnProperty(Prop, Prop->ContainerPtrToValuePtr<void>(Slot), Pair.Value, SetError);
			}
		}
	};

	TFunction<UWidget*(const TSharedPtr<FJsonObject>&, UWidget*)> BuildWidget =
		[&](const TSharedPtr<FJsonObject>& Spec, UWidget* Parent) -> UWidget*
	{
		FString WidgetClassName;
		if (!Spec->TryGetStringField(TEXT("class"), WidgetClassName))
		{
			Errors.Add(TEXT("Widget spec missing class."));
			return nullptr;
		}

		FString WidgetName = OptionalString(Spec, TEXT("name"));
		UClass* WidgetClass = ResolveWidgetClass(WidgetClassName);
		if (!WidgetClass)
		{
			Errors.Add(FString::Printf(TEXT("Widget class not found: %s"), *WidgetClassName));
			return nullptr;
		}

		// Merge mode (clearExisting:false): reuse an existing same-named widget in
		// place so its authored props and slot survive; only construct nodes that
		// are genuinely new. This makes targeted, additive edits non-destructive.
		UWidget* Widget = nullptr;
		bool bReused = false;
		if (!bClearExisting && !WidgetName.IsEmpty())
		{
			Widget = WidgetBP->WidgetTree->FindWidget(FName(*WidgetName));
			if (Widget)
			{
				bReused = true;
				if (!Widget->IsA(WidgetClass))
				{
					Errors.Add(FString::Printf(
						TEXT("Merge: reusing existing '%s' as %s though spec class is %s"),
						*WidgetName, *Widget->GetClass()->GetName(), *WidgetClassName));
				}
			}
		}

		if (!Widget)
		{
			Widget = WidgetBP->WidgetTree->ConstructWidget<UWidget>(
				WidgetClass,
				WidgetName.IsEmpty() ? NAME_None : FName(*WidgetName));
			if (!Widget)
			{
				Errors.Add(FString::Printf(TEXT("Failed to construct widget: %s"), *WidgetClassName));
				return nullptr;
			}
			if (!WidgetName.IsEmpty() && !WidgetBP->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
			{
				WidgetBP->WidgetVariableNameToGuidMap.Add(Widget->GetFName(), FGuid::NewGuid());
			}
			CreatedCount++;
		}

		// Reused widgets keep their current parent/slot; only newly constructed
		// widgets are attached to the spec parent (or set as the tree root).
		UPanelSlot* Slot = bReused ? Widget->Slot : nullptr;
		if (!bReused)
		{
			if (UPanelWidget* ParentPanel = Cast<UPanelWidget>(Parent))
			{
				Slot = ParentPanel->AddChild(Widget);
				if (!Slot)
				{
					Errors.Add(FString::Printf(TEXT("Failed to add '%s' to parent '%s'"), *Widget->GetName(), *Parent->GetName()));
				}
			}
			else if (UContentWidget* ParentContent = Cast<UContentWidget>(Parent))
			{
				if (ParentContent->GetContent())
				{
					Errors.Add(FString::Printf(TEXT("Content widget '%s' already has child content."), *Parent->GetName()));
				}
				else
				{
					Slot = ParentContent->AddChild(Widget);
					if (!Slot)
					{
						Errors.Add(FString::Printf(TEXT("Failed to set '%s' as content of '%s'"), *Widget->GetName(), *Parent->GetName()));
					}
				}
			}
			else if (Parent)
			{
				Errors.Add(FString::Printf(TEXT("Parent widget '%s' (%s) cannot host children."), *Parent->GetName(), *Parent->GetClass()->GetName()));
			}
			else
			{
				WidgetBP->WidgetTree->RootWidget = Widget;
			}
		}

		ApplyProperties(Widget, Spec);
		ApplySlot(Slot, Spec);

		const TArray<TSharedPtr<FJsonValue>>* Children = nullptr;
		if (Spec->TryGetArrayField(TEXT("children"), Children) && Children)
		{
			if (UPanelWidget* ParentPanel = Cast<UPanelWidget>(Widget))
			{
				for (const TSharedPtr<FJsonValue>& ChildValue : *Children)
				{
					const TSharedPtr<FJsonObject>* ChildSpec = nullptr;
					if (ChildValue->TryGetObject(ChildSpec) && ChildSpec && (*ChildSpec).IsValid())
					{
						BuildWidget(*ChildSpec, ParentPanel);
					}
				}
			}
			else if (UContentWidget* ParentContent = Cast<UContentWidget>(Widget))
			{
				if (Children->Num() > 1)
				{
					Errors.Add(FString::Printf(TEXT("Content widget '%s' can host only one child."), *Widget->GetName()));
				}
				for (int32 ChildIndex = 0; ChildIndex < FMath::Min(Children->Num(), 1); ++ChildIndex)
				{
					const TSharedPtr<FJsonObject>* ChildSpec = nullptr;
					if ((*Children)[ChildIndex]->TryGetObject(ChildSpec) && ChildSpec && (*ChildSpec).IsValid())
					{
						BuildWidget(*ChildSpec, ParentContent);
					}
				}
			}
			else
			{
				Errors.Add(FString::Printf(TEXT("Widget '%s' cannot host children."), *Widget->GetName()));
			}
		}

		return Widget;
	};

	UWidget* RootWidget = BuildWidget(*RootSpecPtr, nullptr);
	if (!RootWidget)
	{
		return MCPError(TEXT("Failed to build root widget from spec."));
	}

	WidgetBP->MarkPackageDirty();
	McpSynchronizeWidgetBlueprintCompilerState(WidgetBP);
	FCompilerResultsLog CompileLog;
	FKismetEditorUtilities::CompileBlueprint(WidgetBP, EBlueprintCompileOptions::None, &CompileLog);
	const bool bSave = OptionalBool(Params, TEXT("save"), true);
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(AssetPath);
	}

	auto Result = MCPSuccess();
	MCPSetUpdated(Result);
	Result->SetStringField(TEXT("assetPath"), AssetPath);
	Result->SetStringField(TEXT("rootWidget"), RootWidget->GetName());
	Result->SetNumberField(TEXT("createdWidgets"), CreatedCount);
	Result->SetNumberField(TEXT("propertiesSet"), PropertySetCount);
	Result->SetNumberField(TEXT("compileErrors"), CompileLog.NumErrors);
	Result->SetNumberField(TEXT("compileWarnings"), CompileLog.NumWarnings);
	if (!Errors.IsEmpty())
	{
		TArray<TSharedPtr<FJsonValue>> ErrorArray;
		for (const FString& Error : Errors)
		{
			ErrorArray.Add(MakeShared<FJsonValueString>(Error));
		}
		Result->SetArrayField(TEXT("errors"), ErrorArray);
	}
	return MCPResult(Result);
}

TSharedPtr<FJsonValue> FWidgetHandlers::CreateEditorUtilityWidget(const TSharedPtr<FJsonObject>& Params)
{
	FString Path;
	if (auto Err = RequireStringAlt(Params, TEXT("path"), TEXT("assetPath"), Path)) return Err;

	FString PackagePath;
	FString AssetName;
	Path.Split(TEXT("/"), &PackagePath, &AssetName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	if (AssetName.IsEmpty())
	{
		return MCPError(TEXT("Invalid path format. Expected '/Game/.../AssetName'"));
	}

	const FString OnConflict = OptionalString(Params, TEXT("onConflict"), TEXT("skip"));

	UClass* EUWBClass = FindObject<UClass>(nullptr, TEXT("/Script/Blutility.EditorUtilityWidgetBlueprint"));
	if (!EUWBClass)
	{
		return MCPError(TEXT("EditorUtilityWidgetBlueprint class not found. Enable Blutility plugin."));
	}

	UWidgetBlueprintFactory* WidgetFactory = NewObject<UWidgetBlueprintFactory>();
	WidgetFactory->ParentClass = UUserWidget::StaticClass();
	WidgetFactory->BlueprintType = BPTYPE_Normal;

	auto Created = MCPCreateAssetIdempotent<UObject>(AssetName, PackagePath, OnConflict, TEXT("EditorUtilityWidgetBlueprint"), EUWBClass, WidgetFactory);
	if (Created.EarlyReturn) return Created.EarlyReturn;

	UEditorAssetLibrary::SaveAsset(Created.Asset->GetPathName());

	auto Result = MCPSuccess();
	MCPSetCreated(Result);
	Result->SetStringField(TEXT("path"), Created.Asset->GetPathName());
	Result->SetStringField(TEXT("name"), AssetName);
	MCPSetDeleteAssetRollback(Result, Created.Asset->GetPathName());

	return MCPResult(Result);
}

TSharedPtr<FJsonValue> FWidgetHandlers::CreateEditorUtilityBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	FString Path;
	if (auto Err = RequireStringAlt(Params, TEXT("path"), TEXT("assetPath"), Path)) return Err;

	FString PackagePath;
	FString AssetName;
	Path.Split(TEXT("/"), &PackagePath, &AssetName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	if (AssetName.IsEmpty())
	{
		return MCPError(TEXT("Invalid path format. Expected '/Game/.../AssetName'"));
	}

	const FString OnConflict = OptionalString(Params, TEXT("onConflict"), TEXT("skip"));

	UClass* EUBClass = FindObject<UClass>(nullptr, TEXT("/Script/Blutility.EditorUtilityBlueprint"));
	if (!EUBClass)
	{
		return MCPError(TEXT("EditorUtilityBlueprint class not found. Enable Blutility plugin."));
	}

	auto Created = MCPCreateAssetIdempotent<UObject>(AssetName, PackagePath, OnConflict, TEXT("EditorUtilityBlueprint"), EUBClass, nullptr);
	if (Created.EarlyReturn) return Created.EarlyReturn;

	UEditorAssetLibrary::SaveAsset(Created.Asset->GetPathName());

	auto Result = MCPSuccess();
	MCPSetCreated(Result);
	Result->SetStringField(TEXT("path"), Created.Asset->GetPathName());
	Result->SetStringField(TEXT("name"), AssetName);
	MCPSetDeleteAssetRollback(Result, Created.Asset->GetPathName());

	return MCPResult(Result);
}
TSharedPtr<FJsonValue> FWidgetHandlers::RunEditorUtilityWidget(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (auto Err = RequireStringAlt(Params, TEXT("assetPath"), TEXT("path"), AssetPath)) return Err;

	UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(AssetPath);
	UEditorUtilityWidgetBlueprint* EUWidget = Cast<UEditorUtilityWidgetBlueprint>(LoadedAsset);
	if (!EUWidget)
	{
		return MCPError(FString::Printf(TEXT("Failed to load EditorUtilityWidgetBlueprint at '%s'"), *AssetPath));
	}

	UEditorUtilitySubsystem* Subsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
	if (!Subsystem)
	{
		return MCPError(TEXT("EditorUtilitySubsystem not available"));
	}

	// No rollback: destructive/external — opens a dockable tab in the editor.
	Subsystem->SpawnAndRegisterTab(EUWidget);

	auto Result = MCPSuccess();
	MCPSetUpdated(Result);
	Result->SetStringField(TEXT("assetPath"), AssetPath);
	Result->SetStringField(TEXT("name"), EUWidget->GetName());

	return MCPResult(Result);
}

TSharedPtr<FJsonValue> FWidgetHandlers::RunEditorUtilityBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (auto Err = RequireStringAlt(Params, TEXT("assetPath"), TEXT("path"), AssetPath)) return Err;

	UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(AssetPath);
	UEditorUtilityBlueprint* EUBlueprint = Cast<UEditorUtilityBlueprint>(LoadedAsset);
	if (!EUBlueprint)
	{
		return MCPError(FString::Printf(TEXT("Failed to load EditorUtilityBlueprint at '%s'"), *AssetPath));
	}

	UEditorUtilitySubsystem* Subsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
	if (!Subsystem)
	{
		return MCPError(TEXT("EditorUtilitySubsystem not available"));
	}

	// No rollback: destructive/external — runs an editor utility script.
	Subsystem->TryRun(LoadedAsset);

	auto Result = MCPSuccess();
	MCPSetUpdated(Result);
	Result->SetStringField(TEXT("assetPath"), AssetPath);
	Result->SetStringField(TEXT("name"), EUBlueprint->GetName());

	return MCPResult(Result);
}

// ── Well-known short names → UClass lookup ────────────────────────────
static UClass* ResolveWidgetClass(const FString& ClassName)
{
	// Try well-known short names first (case-insensitive matching)
	static const TMap<FString, FString> ShortNames = {
		// Panels / containers
		{ TEXT("canvaspanel"),       TEXT("/Script/UMG.CanvasPanel") },
		{ TEXT("horizontalbox"),     TEXT("/Script/UMG.HorizontalBox") },
		{ TEXT("verticalbox"),       TEXT("/Script/UMG.VerticalBox") },
		{ TEXT("overlay"),           TEXT("/Script/UMG.Overlay") },
		{ TEXT("gridpanel"),         TEXT("/Script/UMG.GridPanel") },
		{ TEXT("uniformgridpanel"),  TEXT("/Script/UMG.UniformGridPanel") },
		{ TEXT("widgetswitcher"),    TEXT("/Script/UMG.WidgetSwitcher") },
		{ TEXT("scrollbox"),         TEXT("/Script/UMG.ScrollBox") },
		{ TEXT("sizebox"),           TEXT("/Script/UMG.SizeBox") },
		{ TEXT("scalebox"),          TEXT("/Script/UMG.ScaleBox") },
		{ TEXT("border"),            TEXT("/Script/UMG.Border") },
		// Common widgets
		{ TEXT("textblock"),         TEXT("/Script/UMG.TextBlock") },
		{ TEXT("image"),             TEXT("/Script/UMG.Image") },
		{ TEXT("button"),            TEXT("/Script/UMG.Button") },
		{ TEXT("progressbar"),       TEXT("/Script/UMG.ProgressBar") },
		{ TEXT("checkbox"),          TEXT("/Script/UMG.CheckBox") },
		{ TEXT("slider"),            TEXT("/Script/UMG.Slider") },
		{ TEXT("editabletextbox"),   TEXT("/Script/UMG.EditableTextBox") },
		{ TEXT("multilineeditabletextbox"), TEXT("/Script/UMG.MultiLineEditableTextBox") },
		{ TEXT("comboboxstring"),    TEXT("/Script/UMG.ComboBoxString") },
		{ TEXT("spacer"),            TEXT("/Script/UMG.Spacer") },
		{ TEXT("richtextblock"),     TEXT("/Script/UMG.RichTextBlock") },
	};

	FString Key = ClassName.ToLower();
	if (const FString* FullPath = ShortNames.Find(Key))
	{
		UClass* Found = FindObject<UClass>(nullptr, **FullPath);
		if (Found) return Found;
	}

	// Try as full class path  e.g. /Script/UMG.CanvasPanel
	UClass* FullPathClass = FindObject<UClass>(nullptr, *ClassName);
	if (FullPathClass && FullPathClass->IsChildOf(UWidget::StaticClass()))
	{
		return FullPathClass;
	}

	FullPathClass = LoadObject<UClass>(nullptr, *ClassName);
	if (FullPathClass && FullPathClass->IsChildOf(UWidget::StaticClass()))
	{
		return FullPathClass;
	}

	if (ClassName.StartsWith(TEXT("/")))
	{
		FString GeneratedClassPath = ClassName;
		if (!GeneratedClassPath.EndsWith(TEXT("_C")))
		{
			GeneratedClassPath += TEXT("_C");
		}
		FullPathClass = LoadObject<UClass>(nullptr, *GeneratedClassPath);
		if (FullPathClass && FullPathClass->IsChildOf(UWidget::StaticClass()))
		{
			return FullPathClass;
		}
	}

	// Try /Script/UMG.<ClassName>
	FString Guess = FString::Printf(TEXT("/Script/UMG.%s"), *ClassName);
	UClass* GuessClass = FindObject<UClass>(nullptr, *Guess);
	if (GuessClass && GuessClass->IsChildOf(UWidget::StaticClass()))
	{
		return GuessClass;
	}

	return nullptr;
}

TSharedPtr<FJsonValue> FWidgetHandlers::AddWidget(const TSharedPtr<FJsonObject>& Params)
{
	// ── Required: assetPath ──
	FString AssetPath;
	if (auto Err = RequireStringAlt(Params, TEXT("assetPath"), TEXT("path"), AssetPath)) return Err;

	// ── Required: widgetClass (e.g. "TextBlock", "CanvasPanel") ──
	FString WidgetClassName;
	if (auto Err = RequireStringAlt(Params, TEXT("widgetClass"), TEXT("typeName"), WidgetClassName)) return Err;

	// ── Optional: widgetName, parentWidgetName ──
	FString WidgetName = OptionalString(Params, TEXT("widgetName"));
	if (WidgetName.IsEmpty()) WidgetName = OptionalString(Params, TEXT("name"));

	FString ParentWidgetName = OptionalString(Params, TEXT("parentWidgetName"));

	// ── Load the WidgetBlueprint ──
	UWidgetBlueprint* WidgetBP = LoadAssetByPath<UWidgetBlueprint>(AssetPath);
	if (!WidgetBP)
	{
		return MCPError(FString::Printf(TEXT("Failed to load WidgetBlueprint at '%s'"), *AssetPath));
	}

	if (!WidgetBP->WidgetTree)
	{
		return MCPError(TEXT("WidgetTree is null"));
	}

	// Idempotency: if widget with this name already exists, return existed
	if (!WidgetName.IsEmpty())
	{
		UWidget* Existing = nullptr;
		WidgetBP->WidgetTree->ForEachWidget([&](UWidget* Widget)
		{
			if (Widget && Widget->GetName() == WidgetName) Existing = Widget;
		});
		if (Existing)
		{
			auto ExistingResult = MCPSuccess();
			MCPSetExisted(ExistingResult);
			ExistingResult->SetStringField(TEXT("widgetName"), WidgetName);
			ExistingResult->SetStringField(TEXT("widgetClass"), Existing->GetClass()->GetName());
			ExistingResult->SetStringField(TEXT("assetPath"), AssetPath);
			return MCPResult(ExistingResult);
		}
	}

	// ── Resolve the UClass ──
	UClass* WClass = ResolveWidgetClass(WidgetClassName);
	if (!WClass)
	{
		return MCPError(FString::Printf(TEXT("Unknown widget class '%s'. Use short names like TextBlock, CanvasPanel, Image, Button, etc."), *WidgetClassName));
	}

	// ── Construct the widget ──
	UWidget* NewWidget = WidgetBP->WidgetTree->ConstructWidget<UWidget>(WClass, WidgetName.IsEmpty() ? NAME_None : FName(*WidgetName));
	if (!NewWidget)
	{
		return MCPError(FString::Printf(TEXT("Failed to construct widget of class '%s'"), *WidgetClassName));
	}

	// ── Place in hierarchy ──
	bool bIsRoot = false;
	if (!ParentWidgetName.IsEmpty())
	{
		// Find specified parent
		UWidget* ParentRaw = nullptr;
		WidgetBP->WidgetTree->ForEachWidget([&](UWidget* Widget)
		{
			if (Widget && Widget->GetName() == ParentWidgetName)
			{
				ParentRaw = Widget;
			}
		});

		if (!ParentRaw)
		{
			return MCPError(FString::Printf(TEXT("Parent widget '%s' not found"), *ParentWidgetName));
		}

		UPanelSlot* Slot = nullptr;
		if (UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentRaw))
		{
			Slot = ParentPanel->AddChild(NewWidget);
		}
		else if (UContentWidget* ParentContent = Cast<UContentWidget>(ParentRaw))
		{
			if (ParentContent->GetContent())
			{
				return MCPError(FString::Printf(TEXT("Parent widget '%s' (%s) already has child content"), *ParentWidgetName, *ParentRaw->GetClass()->GetName()));
			}
			Slot = ParentContent->AddChild(NewWidget);
		}
		else
		{
			return MCPError(FString::Printf(TEXT("Parent widget '%s' (%s) cannot have children"), *ParentWidgetName, *ParentRaw->GetClass()->GetName()));
		}

		if (!Slot)
		{
			return MCPError(FString::Printf(TEXT("Failed to add '%s' as child of '%s'"), *NewWidget->GetName(), *ParentWidgetName));
		}
	}
	else if (WidgetBP->WidgetTree->RootWidget == nullptr)
	{
		// No root yet — make this the root widget
		WidgetBP->WidgetTree->RootWidget = NewWidget;
		bIsRoot = true;
	}
	else
	{
		// Root exists, try to add as child of root if it's a panel
		UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
		if (RootPanel)
		{
			RootPanel->AddChild(NewWidget);
		}
		else
		{
			return MCPError(TEXT("Root widget is not a panel. Specify parentWidgetName or set a panel as root first."));
		}
	}

	if (!WidgetBP->WidgetVariableNameToGuidMap.Contains(NewWidget->GetFName()))
	{
		WidgetBP->WidgetVariableNameToGuidMap.Add(NewWidget->GetFName(), FGuid::NewGuid());
	}

	// ── Save ──
	WidgetBP->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBP);
	UEditorAssetLibrary::SaveAsset(AssetPath);

	auto Result = MCPSuccess();
	MCPSetCreated(Result);
	Result->SetStringField(TEXT("widgetName"), NewWidget->GetName());
	Result->SetStringField(TEXT("widgetClass"), WClass->GetName());
	Result->SetBoolField(TEXT("isRoot"), bIsRoot);
	if (!ParentWidgetName.IsEmpty())
	{
		Result->SetStringField(TEXT("parentWidgetName"), ParentWidgetName);
	}

	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("assetPath"), AssetPath);
	Payload->SetStringField(TEXT("widgetName"), NewWidget->GetName());
	MCPSetRollback(Result, TEXT("remove_widget"), Payload);

	return MCPResult(Result);
}

TSharedPtr<FJsonValue> FWidgetHandlers::ReplaceWidgetClasses(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (auto Err = RequireStringAlt(Params, TEXT("assetPath"), TEXT("path"), AssetPath)) return Err;

	const TArray<TSharedPtr<FJsonValue>>* ReplacementValues = nullptr;
	if (!Params->TryGetArrayField(TEXT("replacements"), ReplacementValues) || !ReplacementValues)
	{
		return MCPError(TEXT("Missing required array: replacements"));
	}

	UWidgetBlueprint* WidgetBP = LoadAssetByPath<UWidgetBlueprint>(AssetPath);
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		return MCPError(FString::Printf(TEXT("Failed to load WidgetBlueprint at '%s'"), *AssetPath));
	}

	auto FindWidgetByName = [&](const FString& Name) -> UWidget*
	{
		if (Name.IsEmpty())
		{
			return nullptr;
		}
		return WidgetBP->WidgetTree->FindWidget(FName(*Name));
	};

	auto CopyMatchingProperty = [](UObject* Source, UObject* Target, const FName PropertyName)
	{
		if (!Source || !Target)
		{
			return;
		}
		FProperty* SourceProperty = Source->GetClass()->FindPropertyByName(PropertyName);
		FProperty* TargetProperty = Target->GetClass()->FindPropertyByName(PropertyName);
		if (!SourceProperty || !TargetProperty)
		{
			return;
		}

		FString ExportedValue;
		const void* SourceValue = SourceProperty->ContainerPtrToValuePtr<void>(Source);
		SourceProperty->ExportText_Direct(ExportedValue, SourceValue, SourceValue, Source, PPF_None);
		TargetProperty->ImportText_Direct(
			*ExportedValue,
			TargetProperty->ContainerPtrToValuePtr<void>(Target),
			Target,
			PPF_None);
	};

	auto CopyCommonWidgetProperties = [&](UWidget* Source, UWidget* Target)
	{
		static const FName CommonProperties[] = {
			TEXT("Visibility"),
			TEXT("RenderTransform"),
			TEXT("RenderTransformPivot"),
			TEXT("RenderOpacity"),
			TEXT("Clipping"),
			TEXT("Cursor"),
			TEXT("ToolTipText"),
			TEXT("FlowDirectionPreference"),
		};
		for (const FName PropertyName : CommonProperties)
		{
			CopyMatchingProperty(Source, Target, PropertyName);
		}
		if (Source && Target)
		{
			Target->SetIsEnabled(Source->GetIsEnabled());
		}
	};

	struct FSlotSnapshot
	{
		UClass* SlotClass = nullptr;
		FMargin Padding;
		EHorizontalAlignment HorizontalAlignment = HAlign_Fill;
		EVerticalAlignment VerticalAlignment = VAlign_Fill;
		FSlateChildSize ChildSize;
		FAnchors Anchors;
		FMargin Offsets;
		FVector2D Alignment = FVector2D::ZeroVector;
		bool bAutoSize = false;
		int32 ZOrder = 0;
		bool bHasPadding = false;
		bool bHasAlignment = false;
		bool bHasChildSize = false;
		bool bHasCanvas = false;
	};

	auto CaptureSlot = [](UPanelSlot* Slot)
	{
		FSlotSnapshot Snapshot;
		if (!Slot)
		{
			return Snapshot;
		}
		Snapshot.SlotClass = Slot->GetClass();

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
		{
			Snapshot.Anchors = CanvasSlot->GetAnchors();
			Snapshot.Offsets = CanvasSlot->GetOffsets();
			Snapshot.Alignment = CanvasSlot->GetAlignment();
			Snapshot.bAutoSize = CanvasSlot->GetAutoSize();
			Snapshot.ZOrder = CanvasSlot->GetZOrder();
			Snapshot.bHasCanvas = true;
			return Snapshot;
		}

		if (UHorizontalBoxSlot* HorizontalSlot = Cast<UHorizontalBoxSlot>(Slot))
		{
			Snapshot.Padding = HorizontalSlot->GetPadding();
			Snapshot.HorizontalAlignment = HorizontalSlot->GetHorizontalAlignment();
			Snapshot.VerticalAlignment = HorizontalSlot->GetVerticalAlignment();
			Snapshot.ChildSize = HorizontalSlot->GetSize();
			Snapshot.bHasPadding = true;
			Snapshot.bHasAlignment = true;
			Snapshot.bHasChildSize = true;
			return Snapshot;
		}

		if (UVerticalBoxSlot* VerticalSlot = Cast<UVerticalBoxSlot>(Slot))
		{
			Snapshot.Padding = VerticalSlot->GetPadding();
			Snapshot.HorizontalAlignment = VerticalSlot->GetHorizontalAlignment();
			Snapshot.VerticalAlignment = VerticalSlot->GetVerticalAlignment();
			Snapshot.ChildSize = VerticalSlot->GetSize();
			Snapshot.bHasPadding = true;
			Snapshot.bHasAlignment = true;
			Snapshot.bHasChildSize = true;
			return Snapshot;
		}

		if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Slot))
		{
			Snapshot.Padding = OverlaySlot->GetPadding();
			Snapshot.HorizontalAlignment = OverlaySlot->GetHorizontalAlignment();
			Snapshot.VerticalAlignment = OverlaySlot->GetVerticalAlignment();
			Snapshot.bHasPadding = true;
			Snapshot.bHasAlignment = true;
			return Snapshot;
		}

		if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Slot))
		{
			Snapshot.Padding = ButtonSlot->GetPadding();
			Snapshot.HorizontalAlignment = ButtonSlot->GetHorizontalAlignment();
			Snapshot.VerticalAlignment = ButtonSlot->GetVerticalAlignment();
			Snapshot.bHasPadding = true;
			Snapshot.bHasAlignment = true;
			return Snapshot;
		}

		if (UScrollBoxSlot* ScrollBoxSlot = Cast<UScrollBoxSlot>(Slot))
		{
			Snapshot.Padding = ScrollBoxSlot->GetPadding();
			Snapshot.HorizontalAlignment = ScrollBoxSlot->GetHorizontalAlignment();
			Snapshot.VerticalAlignment = ScrollBoxSlot->GetVerticalAlignment();
			Snapshot.bHasPadding = true;
			Snapshot.bHasAlignment = true;
		}
		return Snapshot;
	};

	auto ApplySlot = [](UPanelSlot* Slot, const FSlotSnapshot& Snapshot)
	{
		if (!Slot)
		{
			return;
		}
		if (Snapshot.bHasCanvas)
		{
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
			{
				CanvasSlot->SetAnchors(Snapshot.Anchors);
				CanvasSlot->SetOffsets(Snapshot.Offsets);
				CanvasSlot->SetAlignment(Snapshot.Alignment);
				CanvasSlot->SetAutoSize(Snapshot.bAutoSize);
				CanvasSlot->SetZOrder(Snapshot.ZOrder);
			}
			return;
		}

		if (Snapshot.bHasPadding)
		{
			if (UHorizontalBoxSlot* HorizontalSlot = Cast<UHorizontalBoxSlot>(Slot))
			{
				HorizontalSlot->SetPadding(Snapshot.Padding);
			}
			else if (UVerticalBoxSlot* VerticalSlot = Cast<UVerticalBoxSlot>(Slot))
			{
				VerticalSlot->SetPadding(Snapshot.Padding);
			}
			else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Slot))
			{
				OverlaySlot->SetPadding(Snapshot.Padding);
			}
			else if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Slot))
			{
				ButtonSlot->SetPadding(Snapshot.Padding);
			}
			else if (UScrollBoxSlot* ScrollBoxSlot = Cast<UScrollBoxSlot>(Slot))
			{
				ScrollBoxSlot->SetPadding(Snapshot.Padding);
			}
		}

		if (Snapshot.bHasAlignment)
		{
			if (UHorizontalBoxSlot* HorizontalSlot = Cast<UHorizontalBoxSlot>(Slot))
			{
				HorizontalSlot->SetHorizontalAlignment(Snapshot.HorizontalAlignment);
				HorizontalSlot->SetVerticalAlignment(Snapshot.VerticalAlignment);
			}
			else if (UVerticalBoxSlot* VerticalSlot = Cast<UVerticalBoxSlot>(Slot))
			{
				VerticalSlot->SetHorizontalAlignment(Snapshot.HorizontalAlignment);
				VerticalSlot->SetVerticalAlignment(Snapshot.VerticalAlignment);
			}
			else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Slot))
			{
				OverlaySlot->SetHorizontalAlignment(Snapshot.HorizontalAlignment);
				OverlaySlot->SetVerticalAlignment(Snapshot.VerticalAlignment);
			}
			else if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Slot))
			{
				ButtonSlot->SetHorizontalAlignment(Snapshot.HorizontalAlignment);
				ButtonSlot->SetVerticalAlignment(Snapshot.VerticalAlignment);
			}
			else if (UScrollBoxSlot* ScrollBoxSlot = Cast<UScrollBoxSlot>(Slot))
			{
				ScrollBoxSlot->SetHorizontalAlignment(Snapshot.HorizontalAlignment);
				ScrollBoxSlot->SetVerticalAlignment(Snapshot.VerticalAlignment);
			}
		}

		if (Snapshot.bHasChildSize)
		{
			if (UHorizontalBoxSlot* HorizontalSlot = Cast<UHorizontalBoxSlot>(Slot))
			{
				HorizontalSlot->SetSize(Snapshot.ChildSize);
			}
			else if (UVerticalBoxSlot* VerticalSlot = Cast<UVerticalBoxSlot>(Slot))
			{
				VerticalSlot->SetSize(Snapshot.ChildSize);
			}
		}
	};

	TFunction<FText(UWidget*)> ExtractFirstText = [&](UWidget* Widget) -> FText
	{
		if (!Widget)
		{
			return FText::GetEmpty();
		}
		if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
		{
			return TextBlock->GetText();
		}
		if (UEditableTextBox* TextBox = Cast<UEditableTextBox>(Widget))
		{
			return TextBox->GetText();
		}
		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 ChildIndex = 0; ChildIndex < Panel->GetChildrenCount(); ++ChildIndex)
			{
				const FText Text = ExtractFirstText(Panel->GetChildAt(ChildIndex));
				if (!Text.IsEmpty())
				{
					return Text;
				}
			}
		}
		if (UContentWidget* ContentWidget = Cast<UContentWidget>(Widget))
		{
			const FText Text = ExtractFirstText(ContentWidget->GetContent());
			if (!Text.IsEmpty())
			{
				return Text;
			}
		}
		return FText::GetEmpty();
	};

	auto SetFTextProperty = [](UObject* Target, const FName PropertyName, const FText& Value)
	{
		if (!Target || Value.IsEmpty())
		{
			return false;
		}
		if (FTextProperty* TextProperty = FindFProperty<FTextProperty>(Target->GetClass(), PropertyName))
		{
			TextProperty->SetPropertyValue_InContainer(Target, Value);
			return true;
		}
		return false;
	};

	auto ApplyDefaultTextTransfer = [&](UWidget* Source, UWidget* Target)
	{
		if (!Source || !Target)
		{
			return;
		}
		if (UEditableTextBox* TextBox = Cast<UEditableTextBox>(Source))
		{
			SetFTextProperty(Target, TEXT("Text"), TextBox->GetText());
			SetFTextProperty(Target, TEXT("PlaceholderText"), TextBox->GetHintText());
			return;
		}

		const FText PrimaryText = ExtractFirstText(Source);
		if (PrimaryText.IsEmpty())
		{
			return;
		}
		SetFTextProperty(Target, TEXT("Label"), PrimaryText);
		SetFTextProperty(Target, TEXT("Text"), PrimaryText);
		SetFTextProperty(Target, TEXT("ValueText"), PrimaryText);
	};

	auto ApplyJsonProperties = [&](UObject* Target, const TSharedPtr<FJsonObject>& Properties, TArray<FString>& Errors)
	{
		if (!Target || !Properties.IsValid())
		{
			return 0;
		}

		int32 PropertiesSet = 0;
		for (const auto& Pair : Properties->Values)
		{
			FProperty* Property = Target->GetClass()->FindPropertyByName(FName(*Pair.Key));
			if (!Property)
			{
				Errors.Add(FString::Printf(TEXT("%s: property not found on %s"), *Pair.Key, *Target->GetClass()->GetName()));
				continue;
			}

			if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
			{
				FString StringValue;
				if (Pair.Value->TryGetString(StringValue))
				{
					TextProperty->SetPropertyValue_InContainer(Target, FText::FromString(StringValue));
					PropertiesSet++;
					continue;
				}
			}

			FString SetError;
			if (MCPJsonProperty::SetJsonOnProperty(Property, Property->ContainerPtrToValuePtr<void>(Target), Pair.Value, SetError))
			{
				PropertiesSet++;
			}
			else
			{
				Errors.Add(FString::Printf(TEXT("%s.%s: %s"), *Target->GetName(), *Pair.Key, *SetError));
			}
		}
		return PropertiesSet;
	};

	int32 ReplacedCount = 0;
	int32 ReusedCount = 0;
	int32 RemovedCount = 0;
	int32 PropertiesSetCount = 0;
	TArray<FString> Errors;

	for (const TSharedPtr<FJsonValue>& ReplacementValue : *ReplacementValues)
	{
		const TSharedPtr<FJsonObject>* ReplacementPtr = nullptr;
		if (!ReplacementValue.IsValid() || !ReplacementValue->TryGetObject(ReplacementPtr) || !ReplacementPtr || !(*ReplacementPtr).IsValid())
		{
			Errors.Add(TEXT("Replacement entry is not an object."));
			continue;
		}

		const TSharedPtr<FJsonObject>& Replacement = *ReplacementPtr;
		FString WidgetName;
		FString WidgetClassName;
		if (!Replacement->TryGetStringField(TEXT("widgetName"), WidgetName) || WidgetName.IsEmpty())
		{
			Errors.Add(TEXT("Replacement entry missing widgetName."));
			continue;
		}
		if (!Replacement->TryGetStringField(TEXT("widgetClass"), WidgetClassName) || WidgetClassName.IsEmpty())
		{
			if (!Replacement->TryGetStringField(TEXT("class"), WidgetClassName) || WidgetClassName.IsEmpty())
			{
				Errors.Add(FString::Printf(TEXT("%s: replacement missing widgetClass."), *WidgetName));
				continue;
			}
		}

		const FString ReplacementName = OptionalString(Replacement, TEXT("replacementName"), WidgetName);
		const bool bOptional = OptionalBool(Replacement, TEXT("optional"), false);
		UClass* ReplacementClass = ResolveWidgetClass(WidgetClassName);
		if (!ReplacementClass)
		{
			Errors.Add(FString::Printf(TEXT("%s: widget class not found: %s"), *WidgetName, *WidgetClassName));
			continue;
		}

		UWidget* ExistingWidget = FindWidgetByName(WidgetName);
		if (!ExistingWidget && ReplacementName != WidgetName)
		{
			ExistingWidget = FindWidgetByName(ReplacementName);
		}
		if (!ExistingWidget)
		{
			if (!bOptional)
			{
				Errors.Add(FString::Printf(TEXT("Widget not found: %s"), *WidgetName));
			}
			continue;
		}

		const bool bAlreadyMigrated =
			ExistingWidget->GetName() == ReplacementName &&
			ExistingWidget->GetClass()->IsChildOf(ReplacementClass);
		if (bAlreadyMigrated)
		{
			const TSharedPtr<FJsonObject>* PropertiesPtr = nullptr;
			if (Replacement->TryGetObjectField(TEXT("properties"), PropertiesPtr) && PropertiesPtr && (*PropertiesPtr).IsValid())
			{
				PropertiesSetCount += ApplyJsonProperties(ExistingWidget, *PropertiesPtr, Errors);
			}
			ReusedCount++;
			continue;
		}

		UPanelWidget* Parent = ExistingWidget->GetParent();
		const bool bWasRoot = WidgetBP->WidgetTree->RootWidget == ExistingWidget;
		int32 ChildIndex = INDEX_NONE;
		if (Parent)
		{
			for (int32 Index = 0; Index < Parent->GetChildrenCount(); ++Index)
			{
				if (Parent->GetChildAt(Index) == ExistingWidget)
				{
					ChildIndex = Index;
					break;
				}
			}
		}

		FSlotSnapshot SlotSnapshot = CaptureSlot(ExistingWidget->Slot);
		const bool bOldWasVariable = ExistingWidget->bIsVariable;
		FName OldName = ExistingWidget->GetFName();
		const FName TemporaryOldName = MakeUniqueObjectName(
			WidgetBP->WidgetTree,
			ExistingWidget->GetClass(),
			FName(*(WidgetName + TEXT("_Old"))));
		ExistingWidget->Rename(*TemporaryOldName.ToString(), WidgetBP->WidgetTree, REN_DontCreateRedirectors | REN_NonTransactional);

		UWidget* ReplacementWidget = WidgetBP->WidgetTree->ConstructWidget<UWidget>(ReplacementClass, FName(*ReplacementName));
		if (!ReplacementWidget)
		{
			ExistingWidget->Rename(*OldName.ToString(), WidgetBP->WidgetTree, REN_DontCreateRedirectors | REN_NonTransactional);
			Errors.Add(FString::Printf(TEXT("%s: failed to construct replacement widget."), *WidgetName));
			continue;
		}

		ReplacementWidget->bIsVariable = true;
		CopyCommonWidgetProperties(ExistingWidget, ReplacementWidget);
		ApplyDefaultTextTransfer(ExistingWidget, ReplacementWidget);

		const TSharedPtr<FJsonObject>* PropertiesPtr = nullptr;
		if (Replacement->TryGetObjectField(TEXT("properties"), PropertiesPtr) && PropertiesPtr && (*PropertiesPtr).IsValid())
		{
			PropertiesSetCount += ApplyJsonProperties(ReplacementWidget, *PropertiesPtr, Errors);
		}

		FGuid ExistingGuid;
		const bool bHadGuid = WidgetBP->WidgetVariableNameToGuidMap.RemoveAndCopyValue(OldName, ExistingGuid);

		if (Parent)
		{
			Parent->RemoveChild(ExistingWidget);
		}
		if (bWasRoot)
		{
			WidgetBP->WidgetTree->RootWidget = nullptr;
		}
		WidgetBP->WidgetTree->RemoveWidget(ExistingWidget);

		UPanelSlot* NewSlot = nullptr;
		if (Parent)
		{
			if (ChildIndex != INDEX_NONE)
			{
				NewSlot = Parent->InsertChildAt(ChildIndex, ReplacementWidget);
			}
			else
			{
				NewSlot = Parent->AddChild(ReplacementWidget);
			}
		}
		else if (bWasRoot)
		{
			WidgetBP->WidgetTree->RootWidget = ReplacementWidget;
		}
		else
		{
			Errors.Add(FString::Printf(TEXT("%s: replacement widget has no parent and is not root."), *WidgetName));
			continue;
		}

		ApplySlot(NewSlot, SlotSnapshot);

		WidgetBP->WidgetVariableNameToGuidMap.Add(
			ReplacementWidget->GetFName(),
			bHadGuid ? ExistingGuid : FGuid::NewGuid());
		ReplacementWidget->bIsVariable = bOldWasVariable || ReplacementWidget->bIsVariable;
		ReplacedCount++;
	}

	const TArray<TSharedPtr<FJsonValue>>* RemoveValues = nullptr;
	if (Params->TryGetArrayField(TEXT("removeWidgetNames"), RemoveValues) && RemoveValues)
	{
		for (const TSharedPtr<FJsonValue>& RemoveValue : *RemoveValues)
		{
			FString WidgetName;
			if (!RemoveValue.IsValid() || !RemoveValue->TryGetString(WidgetName) || WidgetName.IsEmpty())
			{
				continue;
			}

			UWidget* Widget = FindWidgetByName(WidgetName);
			if (!Widget)
			{
				continue;
			}
			if (UPanelWidget* Parent = Widget->GetParent())
			{
				Parent->RemoveChild(Widget);
			}
			if (WidgetBP->WidgetTree->RootWidget == Widget)
			{
				WidgetBP->WidgetTree->RootWidget = nullptr;
			}
			WidgetBP->WidgetVariableNameToGuidMap.Remove(Widget->GetFName());
			WidgetBP->WidgetTree->RemoveWidget(Widget);
			RemovedCount++;
		}
	}

	WidgetBP->MarkPackageDirty();
	McpSynchronizeWidgetBlueprintCompilerState(WidgetBP);
	FCompilerResultsLog CompileLog;
	FKismetEditorUtilities::CompileBlueprint(WidgetBP, EBlueprintCompileOptions::None, &CompileLog);
	if (OptionalBool(Params, TEXT("save"), true))
	{
		UEditorAssetLibrary::SaveAsset(AssetPath);
	}

	auto Result = MCPSuccess();
	MCPSetUpdated(Result);
	Result->SetStringField(TEXT("assetPath"), AssetPath);
	Result->SetNumberField(TEXT("replacedWidgets"), ReplacedCount);
	Result->SetNumberField(TEXT("reusedWidgets"), ReusedCount);
	Result->SetNumberField(TEXT("removedWidgets"), RemovedCount);
	Result->SetNumberField(TEXT("propertiesSet"), PropertiesSetCount);
	Result->SetNumberField(TEXT("compileErrors"), CompileLog.NumErrors);
	Result->SetNumberField(TEXT("compileWarnings"), CompileLog.NumWarnings);
	if (!Errors.IsEmpty())
	{
		TArray<TSharedPtr<FJsonValue>> ErrorValues;
		for (const FString& Error : Errors)
		{
			ErrorValues.Add(MakeShared<FJsonValueString>(Error));
		}
		Result->SetArrayField(TEXT("errors"), ErrorValues);
		Result->SetBoolField(TEXT("success"), false);
	}
	return MCPResult(Result);
}

TSharedPtr<FJsonValue> FWidgetHandlers::SetNamedSlotContent(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (auto Err = RequireStringAlt(Params, TEXT("assetPath"), TEXT("path"), AssetPath)) return Err;

	FString HostWidgetName;
	if (auto Err = RequireStringAlt(Params, TEXT("hostWidgetName"), TEXT("widgetName"), HostWidgetName)) return Err;

	FString SlotName;
	if (auto Err = RequireString(Params, TEXT("slotName"), SlotName)) return Err;

	UWidgetBlueprint* WidgetBP = LoadAssetByPath<UWidgetBlueprint>(AssetPath);
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		return MCPError(FString::Printf(TEXT("Failed to load WidgetBlueprint at '%s'"), *AssetPath));
	}

	UUserWidget* HostWidget = nullptr;
	WidgetBP->WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		if (Widget && Widget->GetName() == HostWidgetName)
		{
			HostWidget = Cast<UUserWidget>(Widget);
		}
	});
	if (!HostWidget)
	{
		return MCPError(FString::Printf(TEXT("Named-slot host UserWidget not found: '%s'"), *HostWidgetName));
	}

	int32 CreatedCount = 0;
	int32 ReusedCount = 0;
	int32 PropertySetCount = 0;
	TArray<FString> Errors;

	auto ApplySimpleProperties = [&](UWidget* Widget, const TSharedPtr<FJsonObject>& Spec)
	{
		const TSharedPtr<FJsonObject>* PropsPtr = nullptr;
		if (!Widget || !Spec->TryGetObjectField(TEXT("properties"), PropsPtr) || !PropsPtr || !(*PropsPtr).IsValid())
		{
			return;
		}

		const TSharedPtr<FJsonObject>& Props = *PropsPtr;
		for (const auto& Pair : Props->Values)
		{
			const FString& Key = Pair.Key;
			const TSharedPtr<FJsonValue>& Value = Pair.Value;

			if (Key == TEXT("text"))
			{
				FString Text;
				if (Value->TryGetString(Text))
				{
					if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
					{
						TextBlock->SetText(FText::FromString(Text));
						PropertySetCount++;
						continue;
					}
				}
			}
			else if (Key == TEXT("font"))
			{
				const TSharedPtr<FJsonObject>* FontObj = nullptr;
				if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget);
					TextBlock && Value->TryGetObject(FontObj) && FontObj && (*FontObj).IsValid())
				{
					const int32 Size = (*FontObj)->HasField(TEXT("size"))
						? static_cast<int32>((*FontObj)->GetNumberField(TEXT("size")))
						: TextBlock->GetFont().Size;
					const bool bBold = (*FontObj)->HasField(TEXT("bold")) && (*FontObj)->GetBoolField(TEXT("bold"));
					TextBlock->SetFont(McpMakeFont(Size, bBold));
					PropertySetCount++;
					continue;
				}
			}
			else if (Key == TEXT("color"))
			{
				if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
				{
					TextBlock->SetColorAndOpacity(FSlateColor(McpColorFromJson(Value)));
					PropertySetCount++;
					continue;
				}
				if (UImage* Image = Cast<UImage>(Widget))
				{
					Image->SetColorAndOpacity(McpColorFromJson(Value));
					PropertySetCount++;
					continue;
				}
			}
			else if (Key == TEXT("visibility"))
			{
				FString Visibility;
				if (Value->TryGetString(Visibility))
				{
					Widget->SetVisibility(McpVisibilityFromString(Visibility));
					PropertySetCount++;
					continue;
				}
			}
			else if (Key == TEXT("padding"))
			{
				if (UBorder* Border = Cast<UBorder>(Widget))
				{
					Border->SetPadding(McpMarginFromJson(Value));
					PropertySetCount++;
					continue;
				}
			}
			else if (Key == TEXT("brushColor") || Key == TEXT("drawAs") || Key == TEXT("radius")
				|| Key == TEXT("outlineColor") || Key == TEXT("outlineWidth") || Key == TEXT("texture")
				|| Key == TEXT("resourceObject") || Key == TEXT("brushMargin") || Key == TEXT("imageSize"))
			{
				if (UBorder* Border = Cast<UBorder>(Widget))
				{
					FSlateBrush Brush = Border->Background;
					if (Props->HasField(TEXT("drawAs")))
					{
						const FString DrawAs = OptionalString(Props, TEXT("drawAs")).ToLower();
						if (DrawAs == TEXT("box")) Brush.DrawAs = ESlateBrushDrawType::Box;
						else if (DrawAs == TEXT("border")) Brush.DrawAs = ESlateBrushDrawType::Border;
						else if (DrawAs == TEXT("image")) Brush.DrawAs = ESlateBrushDrawType::Image;
						else if (DrawAs == TEXT("none") || DrawAs == TEXT("nodrawtype")) Brush.DrawAs = ESlateBrushDrawType::NoDrawType;
					}
					if (Props->HasField(TEXT("brushColor")))
					{
						Brush.TintColor = FSlateColor(McpColorFromJson(Props->TryGetField(TEXT("brushColor"))));
					}
					if (Props->HasField(TEXT("radius")))
					{
						const float Radius = static_cast<float>(Props->GetNumberField(TEXT("radius")));
						Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
						Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
					}
					if (Props->HasField(TEXT("outlineColor")))
					{
						Brush.OutlineSettings.Color = FSlateColor(McpColorFromJson(Props->TryGetField(TEXT("outlineColor"))));
					}
					if (Props->HasField(TEXT("outlineWidth")))
					{
						Brush.OutlineSettings.Width = static_cast<float>(Props->GetNumberField(TEXT("outlineWidth")));
					}
					Border->SetBrush(Brush);
					Border->SetBrushColor(McpColorFromJson(Props->TryGetField(TEXT("brushColor")), FLinearColor::White));
					PropertySetCount++;
					continue;
				}
				if (UImage* Image = Cast<UImage>(Widget))
				{
					FSlateBrush Brush = Image->GetBrush();
					if (Props->HasField(TEXT("texture")))
					{
						if (UObject* TextureObject = LoadObject<UObject>(nullptr, *OptionalString(Props, TEXT("texture"))))
						{
							Brush.SetResourceObject(TextureObject);
						}
					}
					if (Props->HasField(TEXT("imageSize")))
					{
						Brush.ImageSize = McpVector2FromJson(Props->TryGetField(TEXT("imageSize")));
					}
					Image->SetBrush(Brush);
					PropertySetCount++;
					continue;
				}
			}
			else if (Key == TEXT("widthOverride"))
			{
				if (USizeBox* SizeBox = Cast<USizeBox>(Widget))
				{
					SizeBox->SetWidthOverride(static_cast<float>(Value->AsNumber()));
					PropertySetCount++;
					continue;
				}
			}
			else if (Key == TEXT("heightOverride"))
			{
				if (USizeBox* SizeBox = Cast<USizeBox>(Widget))
				{
					SizeBox->SetHeightOverride(static_cast<float>(Value->AsNumber()));
					PropertySetCount++;
					continue;
				}
			}
			else if (Key == TEXT("minDesiredWidth"))
			{
				if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
				{
					TextBlock->SetMinDesiredWidth(static_cast<float>(Value->AsNumber()));
					PropertySetCount++;
					continue;
				}
			}
			else if (Key == TEXT("justification"))
			{
				if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
				{
					const FString Justification = OptionalString(Props, TEXT("justification")).ToLower();
					TextBlock->SetJustification(Justification == TEXT("center")
						? ETextJustify::Center
						: (Justification == TEXT("right") ? ETextJustify::Right : ETextJustify::Left));
					PropertySetCount++;
					continue;
				}
			}

			if (FProperty* Prop = Widget->GetClass()->FindPropertyByName(FName(*Key)))
			{
				void* Addr = Prop->ContainerPtrToValuePtr<void>(Widget);
				FString SetError;
				if (MCPJsonProperty::SetJsonOnProperty(Prop, Addr, Value, SetError))
				{
					PropertySetCount++;
				}
				else
				{
					Errors.Add(FString::Printf(TEXT("%s.%s: %s"), *Widget->GetName(), *Key, *SetError));
				}
			}
		}
	};

	auto ApplySimpleSlot = [&](UPanelSlot* Slot, const TSharedPtr<FJsonObject>& Spec)
	{
		const TSharedPtr<FJsonObject>* SlotObjPtr = nullptr;
		if (!Slot || !Spec->TryGetObjectField(TEXT("slot"), SlotObjPtr) || !SlotObjPtr || !(*SlotObjPtr).IsValid())
		{
			return;
		}

		const TSharedPtr<FJsonObject>& SlotObj = *SlotObjPtr;
		if (const TSharedPtr<FJsonValue> PaddingValue = SlotObj->TryGetField(TEXT("padding")))
		{
			if (UHorizontalBoxSlot* HorizontalSlot = Cast<UHorizontalBoxSlot>(Slot))
			{
				HorizontalSlot->SetPadding(McpMarginFromJson(PaddingValue));
			}
			else if (UVerticalBoxSlot* VerticalSlot = Cast<UVerticalBoxSlot>(Slot))
			{
				VerticalSlot->SetPadding(McpMarginFromJson(PaddingValue));
			}
			else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Slot))
			{
				OverlaySlot->SetPadding(McpMarginFromJson(PaddingValue));
			}
		}

		FString HAlign;
		FString VAlign;
		SlotObj->TryGetStringField(TEXT("hAlign"), HAlign);
		SlotObj->TryGetStringField(TEXT("vAlign"), VAlign);
		if (UHorizontalBoxSlot* HorizontalSlot = Cast<UHorizontalBoxSlot>(Slot))
		{
			if (!HAlign.IsEmpty()) HorizontalSlot->SetHorizontalAlignment(McpHAlignFromString(HAlign));
			if (!VAlign.IsEmpty()) HorizontalSlot->SetVerticalAlignment(McpVAlignFromString(VAlign));
			if (SlotObj->HasField(TEXT("sizeRule")) || SlotObj->HasField(TEXT("fillWeight")))
			{
				const bool bFill = OptionalString(SlotObj, TEXT("sizeRule"), TEXT("auto")).Equals(TEXT("fill"), ESearchCase::IgnoreCase);
				FSlateChildSize ChildSize;
				ChildSize.SizeRule = bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic;
				ChildSize.Value = SlotObj->HasField(TEXT("fillWeight")) ? static_cast<float>(SlotObj->GetNumberField(TEXT("fillWeight"))) : 1.0f;
				HorizontalSlot->SetSize(ChildSize);
			}
		}
		else if (UVerticalBoxSlot* VerticalSlot = Cast<UVerticalBoxSlot>(Slot))
		{
			if (!HAlign.IsEmpty()) VerticalSlot->SetHorizontalAlignment(McpHAlignFromString(HAlign));
			if (!VAlign.IsEmpty()) VerticalSlot->SetVerticalAlignment(McpVAlignFromString(VAlign));
			if (SlotObj->HasField(TEXT("sizeRule")) || SlotObj->HasField(TEXT("fillWeight")))
			{
				const bool bFill = OptionalString(SlotObj, TEXT("sizeRule"), TEXT("auto")).Equals(TEXT("fill"), ESearchCase::IgnoreCase);
				FSlateChildSize ChildSize;
				ChildSize.SizeRule = bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic;
				ChildSize.Value = SlotObj->HasField(TEXT("fillWeight")) ? static_cast<float>(SlotObj->GetNumberField(TEXT("fillWeight"))) : 1.0f;
				VerticalSlot->SetSize(ChildSize);
			}
		}
		else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Slot))
		{
			if (!HAlign.IsEmpty()) OverlaySlot->SetHorizontalAlignment(McpHAlignFromString(HAlign));
			if (!VAlign.IsEmpty()) OverlaySlot->SetVerticalAlignment(McpVAlignFromString(VAlign));
		}
	};

	TFunction<UWidget*(const TSharedPtr<FJsonObject>&, UPanelWidget*)> BuildContent =
		[&](const TSharedPtr<FJsonObject>& Spec, UPanelWidget* Parent) -> UWidget*
	{
		FString WidgetClassName;
		if (!Spec->TryGetStringField(TEXT("class"), WidgetClassName))
		{
			Errors.Add(TEXT("Named slot content spec missing class."));
			return nullptr;
		}

		UClass* WidgetClass = ResolveWidgetClass(WidgetClassName);
		if (!WidgetClass)
		{
			Errors.Add(FString::Printf(TEXT("Widget class not found: %s"), *WidgetClassName));
			return nullptr;
		}

		const FString WidgetName = OptionalString(Spec, TEXT("name"));
		UWidget* Widget = nullptr;
		if (!WidgetName.IsEmpty())
		{
			WidgetBP->WidgetTree->ForEachWidget([&](UWidget* Existing)
			{
				if (Existing && Existing->GetName() == WidgetName)
				{
					Widget = Existing;
				}
			});
		}

		const bool bReused = Widget != nullptr;
		if (Widget && !Widget->GetClass()->IsChildOf(WidgetClass))
		{
			Errors.Add(FString::Printf(
				TEXT("Existing widget '%s' is %s, not %s"),
				*WidgetName,
				*Widget->GetClass()->GetName(),
				*WidgetClass->GetName()));
			return nullptr;
		}
		if (!Widget)
		{
			Widget = WidgetBP->WidgetTree->ConstructWidget<UWidget>(
				WidgetClass,
				WidgetName.IsEmpty() ? NAME_None : FName(*WidgetName));
			if (!Widget)
			{
				Errors.Add(FString::Printf(TEXT("Failed to construct named slot widget: %s"), *WidgetClassName));
				return nullptr;
			}
			CreatedCount++;
		}
		else
		{
			ReusedCount++;
		}

		UPanelSlot* Slot = Widget->Slot;
		if (Parent)
		{
			if (UPanelWidget* OldParent = Widget->GetParent())
			{
				if (OldParent != Parent)
				{
					OldParent->RemoveChild(Widget);
					Slot = nullptr;
				}
			}
			if (Widget->GetParent() != Parent)
			{
				Slot = Parent->AddChild(Widget);
			}
		}
		else if (UPanelWidget* OldParent = Widget->GetParent())
		{
			OldParent->RemoveChild(Widget);
			Slot = nullptr;
		}

		ApplySimpleProperties(Widget, Spec);
		ApplySimpleSlot(Slot, Spec);

		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			Panel->ClearChildren();
			const TArray<TSharedPtr<FJsonValue>>* Children = nullptr;
			if (Spec->TryGetArrayField(TEXT("children"), Children) && Children)
			{
				for (const TSharedPtr<FJsonValue>& ChildValue : *Children)
				{
					const TSharedPtr<FJsonObject>* ChildSpec = nullptr;
					if (ChildValue->TryGetObject(ChildSpec) && ChildSpec && (*ChildSpec).IsValid())
					{
						BuildContent(*ChildSpec, Panel);
					}
				}
			}
		}

		return Widget;
	};

	const TSharedPtr<FJsonObject>* ContentSpec = nullptr;
	if (!Params->TryGetObjectField(TEXT("content"), ContentSpec) || !ContentSpec || !(*ContentSpec).IsValid())
	{
		HostWidget->SetContentForSlot(FName(*SlotName), nullptr);
	}
	else
	{
		UWidget* Content = BuildContent(*ContentSpec, nullptr);
		if (!Content)
		{
			return MCPError(TEXT("Failed to build named slot content."));
		}
		HostWidget->SetContentForSlot(FName(*SlotName), Content);
	}

	WidgetBP->MarkPackageDirty();
	McpSynchronizeWidgetBlueprintCompilerState(WidgetBP);
	FKismetEditorUtilities::CompileBlueprint(WidgetBP);
	const bool bSave = OptionalBool(Params, TEXT("save"), true);
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(AssetPath);
	}

	auto Result = MCPSuccess();
	MCPSetUpdated(Result);
	Result->SetStringField(TEXT("assetPath"), AssetPath);
	Result->SetStringField(TEXT("hostWidgetName"), HostWidgetName);
	Result->SetStringField(TEXT("slotName"), SlotName);
	Result->SetNumberField(TEXT("createdWidgets"), CreatedCount);
	Result->SetNumberField(TEXT("reusedWidgets"), ReusedCount);
	Result->SetNumberField(TEXT("propertiesSet"), PropertySetCount);
	if (!Errors.IsEmpty())
	{
		TArray<TSharedPtr<FJsonValue>> ErrorArray;
		for (const FString& Error : Errors)
		{
			ErrorArray.Add(MakeShared<FJsonValueString>(Error));
		}
		Result->SetArrayField(TEXT("errors"), ErrorArray);
	}
	return MCPResult(Result);
}

TSharedPtr<FJsonValue> FWidgetHandlers::RemoveWidget(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (auto Err = RequireStringAlt(Params, TEXT("assetPath"), TEXT("path"), AssetPath)) return Err;

	FString WidgetName;
	if (auto Err = RequireString(Params, TEXT("widgetName"), WidgetName)) return Err;

	UWidgetBlueprint* WidgetBP = LoadAssetByPath<UWidgetBlueprint>(AssetPath);
	if (!WidgetBP)
	{
		return MCPError(FString::Printf(TEXT("Failed to load WidgetBlueprint at '%s'"), *AssetPath));
	}

	if (!WidgetBP->WidgetTree)
	{
		return MCPError(TEXT("WidgetTree is null"));
	}

	// Find the widget
	UWidget* FoundWidget = nullptr;
	WidgetBP->WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		if (Widget && Widget->GetName() == WidgetName)
		{
			FoundWidget = Widget;
		}
	});

	if (!FoundWidget)
	{
		// Idempotent: nothing to delete
		auto AlreadyResult = MCPSuccess();
		AlreadyResult->SetBoolField(TEXT("alreadyDeleted"), true);
		AlreadyResult->SetStringField(TEXT("widgetName"), WidgetName);
		AlreadyResult->SetStringField(TEXT("assetPath"), AssetPath);
		return MCPResult(AlreadyResult);
	}

	FString RemovedClass = FoundWidget->GetClass()->GetName();

	// Remove from parent if parented
	UPanelWidget* Parent = FoundWidget->GetParent();
	if (Parent)
	{
		Parent->RemoveChild(FoundWidget);
	}

	// If this was the root widget, clear it
	if (WidgetBP->WidgetTree->RootWidget == FoundWidget)
	{
		WidgetBP->WidgetTree->RootWidget = nullptr;
	}

	// Remove from widget tree
	WidgetBP->WidgetTree->RemoveWidget(FoundWidget);

	WidgetBP->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBP);
	UEditorAssetLibrary::SaveAsset(AssetPath);

	auto Result = MCPSuccess();
	Result->SetBoolField(TEXT("deleted"), true);
	Result->SetStringField(TEXT("widgetName"), WidgetName);
	Result->SetStringField(TEXT("widgetClass"), RemovedClass);
	// No rollback: remove_widget is destructive (would need to snapshot widget tree to reverse).

	return MCPResult(Result);
}

TSharedPtr<FJsonValue> FWidgetHandlers::RenameWidget(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (auto Err = RequireStringAlt(Params, TEXT("assetPath"), TEXT("path"), AssetPath)) return Err;

	FString OldWidgetName;
	if (auto Err = RequireStringAlt(Params, TEXT("oldWidgetName"), TEXT("widgetName"), OldWidgetName)) return Err;

	FString NewWidgetName;
	if (auto Err = RequireStringAlt(Params, TEXT("newWidgetName"), TEXT("newName"), NewWidgetName)) return Err;

	if (OldWidgetName == NewWidgetName)
	{
		auto Noop = MCPSuccess();
		MCPSetExisted(Noop);
		Noop->SetStringField(TEXT("widgetName"), NewWidgetName);
		Noop->SetStringField(TEXT("assetPath"), AssetPath);
		return MCPResult(Noop);
	}

	UWidgetBlueprint* WidgetBP = LoadAssetByPath<UWidgetBlueprint>(AssetPath);
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		return MCPError(FString::Printf(TEXT("Failed to load WidgetBlueprint at '%s'"), *AssetPath));
	}

	UWidget* WidgetToRename = nullptr;
	UWidget* ExistingNewNameWidget = nullptr;
	WidgetBP->WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		if (!Widget) return;
		if (Widget->GetName() == OldWidgetName) WidgetToRename = Widget;
		if (Widget->GetName() == NewWidgetName) ExistingNewNameWidget = Widget;
	});

	if (ExistingNewNameWidget)
	{
		if (!WidgetToRename)
		{
			WidgetBP->Modify();
			bool bRepairedGuidMap = false;
			FGuid ExistingGuid;
			if (WidgetBP->WidgetVariableNameToGuidMap.RemoveAndCopyValue(FName(*OldWidgetName), ExistingGuid))
			{
				WidgetBP->WidgetVariableNameToGuidMap.Add(ExistingNewNameWidget->GetFName(), ExistingGuid);
				bRepairedGuidMap = true;
			}
			else if (ExistingNewNameWidget->bIsVariable &&
				!WidgetBP->WidgetVariableNameToGuidMap.Contains(ExistingNewNameWidget->GetFName()))
			{
				WidgetBP->WidgetVariableNameToGuidMap.Add(ExistingNewNameWidget->GetFName(), FGuid::NewGuid());
				bRepairedGuidMap = true;
			}

			if (bRepairedGuidMap)
			{
				WidgetBP->MarkPackageDirty();
				FKismetEditorUtilities::CompileBlueprint(WidgetBP);
				UEditorAssetLibrary::SaveAsset(AssetPath);
			}

			auto AlreadyRenamed = MCPSuccess();
			MCPSetExisted(AlreadyRenamed);
			AlreadyRenamed->SetStringField(TEXT("oldWidgetName"), OldWidgetName);
			AlreadyRenamed->SetStringField(TEXT("newWidgetName"), NewWidgetName);
			AlreadyRenamed->SetStringField(TEXT("assetPath"), AssetPath);
			AlreadyRenamed->SetBoolField(TEXT("repairedGuidMap"), bRepairedGuidMap);
			return MCPResult(AlreadyRenamed);
		}

		return MCPError(FString::Printf(
			TEXT("Cannot rename widget '%s' to '%s': target name already exists"),
			*OldWidgetName,
			*NewWidgetName));
	}

	if (!WidgetToRename)
	{
		return MCPError(FString::Printf(TEXT("Widget not found: '%s'"), *OldWidgetName));
	}

	WidgetBP->Modify();
	WidgetBP->WidgetTree->Modify();
	WidgetToRename->Modify();

	FGuid ExistingGuid;
	const bool bHadVariableGuid = WidgetBP->WidgetVariableNameToGuidMap.RemoveAndCopyValue(WidgetToRename->GetFName(), ExistingGuid);

	const bool bRenamed = WidgetToRename->Rename(
		*NewWidgetName,
		WidgetBP->WidgetTree,
		REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
	if (!bRenamed)
	{
		return MCPError(FString::Printf(
			TEXT("Failed to rename widget '%s' to '%s'"),
			*OldWidgetName,
			*NewWidgetName));
	}

	if (bHadVariableGuid)
	{
		WidgetBP->WidgetVariableNameToGuidMap.Add(WidgetToRename->GetFName(), ExistingGuid);
	}
	else if (WidgetToRename->bIsVariable)
	{
		WidgetBP->WidgetVariableNameToGuidMap.Add(WidgetToRename->GetFName(), FGuid::NewGuid());
	}

	WidgetBP->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBP);
	UEditorAssetLibrary::SaveAsset(AssetPath);

	auto Result = MCPSuccess();
	MCPSetUpdated(Result);
	Result->SetStringField(TEXT("oldWidgetName"), OldWidgetName);
	Result->SetStringField(TEXT("newWidgetName"), WidgetToRename->GetName());
	Result->SetStringField(TEXT("assetPath"), AssetPath);

	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("assetPath"), AssetPath);
	Payload->SetStringField(TEXT("oldWidgetName"), WidgetToRename->GetName());
	Payload->SetStringField(TEXT("newWidgetName"), OldWidgetName);
	MCPSetRollback(Result, TEXT("rename_widget"), Payload);

	return MCPResult(Result);
}

TSharedPtr<FJsonValue> FWidgetHandlers::MoveWidget(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (auto Err = RequireStringAlt(Params, TEXT("assetPath"), TEXT("path"), AssetPath)) return Err;

	FString WidgetName;
	if (auto Err = RequireString(Params, TEXT("widgetName"), WidgetName)) return Err;

	FString NewParentName;
	if (auto Err = RequireStringAlt(Params, TEXT("newParentWidgetName"), TEXT("parentWidgetName"), NewParentName)) return Err;

	UWidgetBlueprint* WidgetBP = LoadAssetByPath<UWidgetBlueprint>(AssetPath);
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		return MCPError(FString::Printf(TEXT("Failed to load WidgetBlueprint at '%s'"), *AssetPath));
	}

	// Find the widget to move
	UWidget* WidgetToMove = nullptr;
	UWidget* NewParentRaw = nullptr;
	WidgetBP->WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		if (Widget && Widget->GetName() == WidgetName) WidgetToMove = Widget;
		if (Widget && Widget->GetName() == NewParentName) NewParentRaw = Widget;
	});

	if (!WidgetToMove)
	{
		return MCPError(FString::Printf(TEXT("Widget not found: '%s'"), *WidgetName));
	}

	if (!NewParentRaw)
	{
		return MCPError(FString::Printf(TEXT("New parent not found: '%s'"), *NewParentName));
	}

	UPanelWidget* NewParentPanel = Cast<UPanelWidget>(NewParentRaw);
	if (!NewParentPanel)
	{
		return MCPError(FString::Printf(TEXT("New parent '%s' (%s) is not a panel widget"), *NewParentName, *NewParentRaw->GetClass()->GetName()));
	}

	// #315: refuse self-parenting and cyclic moves. Walking the WBP root chain
	// down from the new parent and stopping at WidgetToMove would let the move
	// succeed silently while orphaning the entire subtree (read_tree returns
	// empty, the asset cannot reload). Reject before mutating.
	if (NewParentPanel == WidgetToMove)
	{
		return MCPError(FString::Printf(
			TEXT("Refusing cyclic move: cannot reparent '%s' into itself"), *WidgetName));
	}
	{
		UWidget* Ancestor = NewParentPanel;
		while (Ancestor)
		{
			if (Ancestor == WidgetToMove)
			{
				return MCPError(FString::Printf(
					TEXT("Refusing cyclic move: '%s' is an ancestor of '%s' (would create a cycle)"),
					*WidgetName, *NewParentName));
			}
			Ancestor = Ancestor->GetParent();
		}
	}

	// #315: moving the root widget into any other panel orphans the tree (the
	// move clears RootWidget then adds it as a child with no root above it).
	// Use the dedicated wrap/set_root action for that workflow (#365).
	if (WidgetBP->WidgetTree->RootWidget == WidgetToMove)
	{
		return MCPError(FString::Printf(
			TEXT("Cannot move the root widget '%s' via move_widget — use widget(set_root) or widget(wrap_root) instead"),
			*WidgetName));
	}

	// Idempotency: already child of the target parent?
	UPanelWidget* OldParent = WidgetToMove->GetParent();
	FString OldParentName = OldParent ? OldParent->GetName() : TEXT("(root)");
	if (OldParent == NewParentPanel)
	{
		auto Noop = MCPSuccess();
		MCPSetExisted(Noop);
		Noop->SetStringField(TEXT("widgetName"), WidgetName);
		Noop->SetStringField(TEXT("oldParent"), OldParentName);
		Noop->SetStringField(TEXT("newParent"), NewParentName);
		return MCPResult(Noop);
	}

	// Remove from current parent
	if (OldParent)
	{
		OldParent->RemoveChild(WidgetToMove);
	}

	// Add to new parent
	NewParentPanel->AddChild(WidgetToMove);

	WidgetBP->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBP);
	UEditorAssetLibrary::SaveAsset(AssetPath);

	auto Result = MCPSuccess();
	MCPSetUpdated(Result);
	Result->SetStringField(TEXT("widgetName"), WidgetName);
	Result->SetStringField(TEXT("oldParent"), OldParentName);
	Result->SetStringField(TEXT("newParent"), NewParentName);

	// Rollback: move back to old parent if it was a panel
	if (OldParent)
	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("assetPath"), AssetPath);
		Payload->SetStringField(TEXT("widgetName"), WidgetName);
		Payload->SetStringField(TEXT("newParentWidgetName"), OldParentName);
		MCPSetRollback(Result, TEXT("move_widget"), Payload);
	}

	return MCPResult(Result);
}

TSharedPtr<FJsonValue> FWidgetHandlers::RepairWidgetBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (auto Err = RequireStringAlt(Params, TEXT("assetPath"), TEXT("path"), AssetPath)) return Err;

	UWidgetBlueprint* WidgetBP = LoadAssetByPath<UWidgetBlueprint>(AssetPath);
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		return MCPError(FString::Printf(TEXT("Failed to load WidgetBlueprint at '%s'"), *AssetPath));
	}

	WidgetBP->Modify();
	WidgetBP->WidgetTree->Modify();
	McpSynchronizeWidgetBlueprintCompilerState(WidgetBP);
	WidgetBP->MarkPackageDirty();

	FCompilerResultsLog CompileLog;
	FKismetEditorUtilities::CompileBlueprint(WidgetBP, EBlueprintCompileOptions::None, &CompileLog);

	const bool bSave = OptionalBool(Params, TEXT("save"), true);
	if (bSave)
	{
		UEditorAssetLibrary::SaveAsset(AssetPath);
	}

	auto Result = MCPSuccess();
	MCPSetUpdated(Result);
	Result->SetStringField(TEXT("assetPath"), AssetPath);
	Result->SetStringField(TEXT("rootWidget"), WidgetBP->WidgetTree->RootWidget ? WidgetBP->WidgetTree->RootWidget->GetName() : TEXT("(none)"));
	Result->SetNumberField(TEXT("compileErrors"), CompileLog.NumErrors);
	Result->SetNumberField(TEXT("compileWarnings"), CompileLog.NumWarnings);
	Result->SetBoolField(TEXT("saved"), bSave);
	return MCPResult(Result);
}

// #365: replace the WBP's RootWidget with an existing widget by name. The
// previous root is removed from the tree along with its descendants. Used
// when an authoring step needs to swap a placeholder root (e.g. the
// auto-created CanvasPanel) for a different layout.
TSharedPtr<FJsonValue> FWidgetHandlers::SetRoot(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (auto Err = RequireStringAlt(Params, TEXT("assetPath"), TEXT("path"), AssetPath)) return Err;

	FString WidgetName;
	if (auto Err = RequireString(Params, TEXT("widgetName"), WidgetName)) return Err;

	UWidgetBlueprint* WidgetBP = LoadAssetByPath<UWidgetBlueprint>(AssetPath);
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		return MCPError(FString::Printf(TEXT("Failed to load WidgetBlueprint at '%s'"), *AssetPath));
	}

	UWidget* NewRoot = nullptr;
	WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W)
	{
		if (W && W->GetName() == WidgetName) NewRoot = W;
	});
	if (!NewRoot)
	{
		return MCPError(FString::Printf(TEXT("Widget not found: '%s'"), *WidgetName));
	}

	UWidget* OldRoot = WidgetBP->WidgetTree->RootWidget;
	if (OldRoot == NewRoot)
	{
		WidgetBP->Modify();
		WidgetBP->WidgetTree->Modify();
		McpSynchronizeWidgetBlueprintCompilerState(WidgetBP);
		WidgetBP->MarkPackageDirty();
		FKismetEditorUtilities::CompileBlueprint(WidgetBP);
		UEditorAssetLibrary::SaveAsset(AssetPath);

		auto Noop = MCPSuccess();
		MCPSetExisted(Noop);
		Noop->SetStringField(TEXT("rootWidget"), WidgetName);
		return MCPResult(Noop);
	}

	WidgetBP->Modify();
	WidgetBP->WidgetTree->Modify();

	// Detach NewRoot from its current parent so the engine doesn't keep it as
	// a descendant of whatever was hosting it (avoids leaving the new root
	// double-parented when AddChild later reassigns it elsewhere).
	if (UPanelWidget* CurrentParent = NewRoot->GetParent())
	{
		CurrentParent->RemoveChild(NewRoot);
	}
	if (OldRoot)
	{
		TArray<UWidget*> OldRootWidgets;
		OldRootWidgets.Add(OldRoot);
		UWidgetTree::GetChildWidgets(OldRoot, OldRootWidgets);
		for (UWidget* Candidate : OldRootWidgets)
		{
			if (UPanelWidget* Panel = Cast<UPanelWidget>(Candidate))
			{
				if (Panel->GetChildIndex(NewRoot) != INDEX_NONE)
				{
					Panel->RemoveChild(NewRoot);
				}
			}
		}
	}

	if (OldRoot)
	{
		TSet<UWidget*> WidgetsToDelete;
		WidgetsToDelete.Add(OldRoot);
		FWidgetBlueprintEditorUtils::DeleteWidgets(
			WidgetBP,
			WidgetsToDelete,
			FWidgetBlueprintEditorUtils::EDeleteWidgetWarningType::DeleteSilently);
	}

	McpMoveWidgetTemplateIntoTree(WidgetBP, NewRoot, FName(*WidgetName));
	WidgetBP->WidgetTree->RootWidget = NewRoot;

	McpSynchronizeWidgetBlueprintCompilerState(WidgetBP);
	WidgetBP->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBP);
	UEditorAssetLibrary::SaveAsset(AssetPath);

	auto Result = MCPSuccess();
	MCPSetUpdated(Result);
	Result->SetStringField(TEXT("rootWidget"), WidgetName);
	Result->SetStringField(TEXT("previousRoot"), OldRoot ? OldRoot->GetName() : TEXT("(none)"));
	return MCPResult(Result);
}

// #365: insert a new container around the current root - mirrors UMG's
// "Wrap With" context-menu action. The current root becomes a child of the
// new wrapping widget.
TSharedPtr<FJsonValue> FWidgetHandlers::WrapRoot(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (auto Err = RequireStringAlt(Params, TEXT("assetPath"), TEXT("path"), AssetPath)) return Err;

	FString WrapperClassName;
	if (auto Err = RequireStringAlt(Params, TEXT("wrapperClass"), TEXT("widgetClass"), WrapperClassName)) return Err;

	UWidgetBlueprint* WidgetBP = LoadAssetByPath<UWidgetBlueprint>(AssetPath);
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		return MCPError(FString::Printf(TEXT("Failed to load WidgetBlueprint at '%s'"), *AssetPath));
	}

	UWidget* OldRoot = WidgetBP->WidgetTree->RootWidget;
	if (!OldRoot)
	{
		return MCPError(TEXT("WBP has no root widget yet - use add_widget to set a root first"));
	}

	UClass* WrapperCls = FindClassByShortName(WrapperClassName);
	if (!WrapperCls)
	{
		return MCPError(FString::Printf(TEXT("Widget class not found: %s"), *WrapperClassName));
	}
	if (!WrapperCls->IsChildOf(UPanelWidget::StaticClass()))
	{
		return MCPError(FString::Printf(
			TEXT("Wrapper class '%s' is not a UPanelWidget - cannot host children"), *WrapperClassName));
	}

	const FString NewName = OptionalString(Params, TEXT("wrapperName"));

	WidgetBP->Modify();
	WidgetBP->WidgetTree->Modify();

	UPanelWidget* Wrapper = Cast<UPanelWidget>(WidgetBP->WidgetTree->ConstructWidget<UWidget>(
		WrapperCls, NewName.IsEmpty() ? NAME_None : FName(*NewName)));
	if (!Wrapper)
	{
		return MCPError(TEXT("Failed to construct wrapper widget"));
	}

	WidgetBP->WidgetTree->RootWidget = Wrapper;
	Wrapper->AddChild(OldRoot);

	WidgetBP->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBP);
	UEditorAssetLibrary::SaveAsset(AssetPath);

	auto Result = MCPSuccess();
	MCPSetCreated(Result);
	Result->SetStringField(TEXT("wrapperName"), Wrapper->GetName());
	Result->SetStringField(TEXT("wrapperClass"), WrapperCls->GetName());
	Result->SetStringField(TEXT("wrappedChild"), OldRoot->GetName());
	return MCPResult(Result);
}

TSharedPtr<FJsonValue> FWidgetHandlers::ListWidgetClasses(const TSharedPtr<FJsonObject>& Params)
{
	struct FWidgetClassInfo { FString Name; FString Category; };
	TArray<FWidgetClassInfo> Classes = {
		// Panels / containers
		{ TEXT("CanvasPanel"),       TEXT("Panel") },
		{ TEXT("HorizontalBox"),     TEXT("Panel") },
		{ TEXT("VerticalBox"),       TEXT("Panel") },
		{ TEXT("Overlay"),           TEXT("Panel") },
		{ TEXT("GridPanel"),         TEXT("Panel") },
		{ TEXT("UniformGridPanel"),  TEXT("Panel") },
		{ TEXT("WidgetSwitcher"),    TEXT("Panel") },
		{ TEXT("ScrollBox"),         TEXT("Panel") },
		{ TEXT("SizeBox"),           TEXT("Panel") },
		{ TEXT("ScaleBox"),          TEXT("Panel") },
		{ TEXT("Border"),            TEXT("Panel") },
		// Common widgets
		{ TEXT("TextBlock"),         TEXT("Common") },
		{ TEXT("RichTextBlock"),     TEXT("Common") },
		{ TEXT("Image"),             TEXT("Common") },
		{ TEXT("Button"),            TEXT("Common") },
		{ TEXT("CheckBox"),          TEXT("Input") },
		{ TEXT("Slider"),            TEXT("Input") },
		{ TEXT("EditableTextBox"),   TEXT("Input") },
		{ TEXT("ComboBoxString"),    TEXT("Input") },
		{ TEXT("ProgressBar"),       TEXT("Common") },
		{ TEXT("Spacer"),            TEXT("Common") },
	};

	TArray<TSharedPtr<FJsonValue>> ClassesArray;
	for (const FWidgetClassInfo& Info : Classes)
	{
		FString FullPath = FString::Printf(TEXT("/Script/UMG.%s"), *Info.Name);
		UClass* WClass = FindObject<UClass>(nullptr, *FullPath);
		bool bIsPanel = WClass && WClass->IsChildOf(UPanelWidget::StaticClass());

		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Info.Name);
		Obj->SetStringField(TEXT("category"), Info.Category);
		Obj->SetBoolField(TEXT("isPanel"), bIsPanel);
		Obj->SetBoolField(TEXT("available"), WClass != nullptr);

		// Slot properties hint
		if (bIsPanel)
		{
			if (Info.Name == TEXT("CanvasPanel"))
				Obj->SetStringField(TEXT("slotProperties"), TEXT("slot.anchors, slot.alignment, slot.position, slot.size, slot.autoSize, slot.zOrder"));
			else if (Info.Name == TEXT("HorizontalBox") || Info.Name == TEXT("VerticalBox"))
				Obj->SetStringField(TEXT("slotProperties"), TEXT("slot.padding, slot.hAlign, slot.vAlign, slot.sizeRule (auto|fill), slot.fillWeight"));
			else if (Info.Name == TEXT("Overlay"))
				Obj->SetStringField(TEXT("slotProperties"), TEXT("slot.padding, slot.hAlign, slot.vAlign"));
		}

		ClassesArray.Add(MakeShared<FJsonValueObject>(Obj));
	}

	auto Result = MCPSuccess();
	Result->SetArrayField(TEXT("classes"), ClassesArray);
	Result->SetNumberField(TEXT("count"), ClassesArray.Num());

	return MCPResult(Result);
}

// ─────────────────────────────────────────────────────────────
// #160  Runtime widget inspection — live PIE UUserWidget probing
// ─────────────────────────────────────────────────────────────
namespace WidgetRuntime_Internal
{
	constexpr int32 DefaultRuntimeTreeMaxDepth = 6;
	static TMap<FString, TStrongObjectPtr<UUserWidget>> SpawnedPreviewWidgets;
	static FDelegateHandle SpawnedPreviewCleanupHandle;

	static void ClearSpawnedPreviewWidgets()
	{
		for (TPair<FString, TStrongObjectPtr<UUserWidget>>& Preview : SpawnedPreviewWidgets)
		{
			if (Preview.Value.IsValid())
			{
				Preview.Value->RemoveFromParent();
			}
		}
		SpawnedPreviewWidgets.Empty();
	}

	static void HandleEndPIE(const bool)
	{
		ClearSpawnedPreviewWidgets();
	}

	static void EnsureSpawnedPreviewCleanupRegistered()
	{
		if (!SpawnedPreviewCleanupHandle.IsValid())
		{
			SpawnedPreviewCleanupHandle = FEditorDelegates::EndPIE.AddStatic(&HandleEndPIE);
		}
	}

	static void PruneSpawnedPreviewWidgets()
	{
		for (auto It = SpawnedPreviewWidgets.CreateIterator(); It; ++It)
		{
			if (!It.Value().IsValid())
			{
				It.RemoveCurrent();
			}
		}
	}

	static UWorld* ResolveRuntimeWorld()
	{
		if (!GEditor) return nullptr;
		FWorldContext* PIE = GEditor->GetPIEWorldContext();
		return PIE ? PIE->World() : nullptr;
	}

	static FString SafeGetText(UWidget* Widget)
	{
		if (UTextBlock* T = Cast<UTextBlock>(Widget))       return T->GetText().ToString();
		if (URichTextBlock* R = Cast<URichTextBlock>(Widget)) return R->GetText().ToString();
		if (UEditableTextBox* E = Cast<UEditableTextBox>(Widget)) return E->GetText().ToString();
		if (UButton* B = Cast<UButton>(Widget))
		{
			if (B->GetChildrenCount() > 0)
			{
				return SafeGetText(B->GetChildAt(0));
			}
		}
		return FString();
	}

	static FString VisibilityToString(ESlateVisibility V)
	{
		switch (V)
		{
			case ESlateVisibility::Visible: return TEXT("Visible");
			case ESlateVisibility::Collapsed: return TEXT("Collapsed");
			case ESlateVisibility::Hidden: return TEXT("Hidden");
			case ESlateVisibility::HitTestInvisible: return TEXT("HitTestInvisible");
			case ESlateVisibility::SelfHitTestInvisible: return TEXT("SelfHitTestInvisible");
		}
		return TEXT("Unknown");
	}

	static TSharedPtr<FJsonObject> MakeVectorJson(const FVector2D& Value)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField(TEXT("x"), Value.X);
		Obj->SetNumberField(TEXT("y"), Value.Y);
		return Obj;
	}

	static TSharedPtr<FJsonObject> MakeSlateRectJson(const FSlateRect& Rect)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField(TEXT("left"), Rect.Left);
		Obj->SetNumberField(TEXT("top"), Rect.Top);
		Obj->SetNumberField(TEXT("right"), Rect.Right);
		Obj->SetNumberField(TEXT("bottom"), Rect.Bottom);
		Obj->SetNumberField(TEXT("width"), Rect.Right - Rect.Left);
		Obj->SetNumberField(TEXT("height"), Rect.Bottom - Rect.Top);
		return Obj;
	}

	static TSharedPtr<FJsonObject> MakeGeometryJson(UWidget* Widget)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		if (!Widget || !Widget->GetCachedWidget().IsValid())
		{
			Obj->SetBoolField(TEXT("hasCachedGeometry"), false);
			return Obj;
		}

		const FGeometry Geometry = Widget->GetCachedWidget()->GetTickSpaceGeometry();
		const FVector2D LocalSize = Geometry.GetLocalSize();
		const FVector2D AbsolutePosition = Geometry.GetAbsolutePosition();
		Obj->SetBoolField(TEXT("hasCachedGeometry"), true);
		Obj->SetObjectField(TEXT("localSize"), MakeVectorJson(LocalSize));
		Obj->SetObjectField(TEXT("absolutePosition"), MakeVectorJson(AbsolutePosition));
		Obj->SetObjectField(TEXT("layoutBoundingRect"), MakeSlateRectJson(Geometry.GetLayoutBoundingRect()));
		Obj->SetObjectField(TEXT("renderBoundingRect"), MakeSlateRectJson(Geometry.GetRenderBoundingRect()));
		return Obj;
	}

	static TSharedPtr<FJsonObject> BuildRuntimeNode(UWidget* Widget, int32 Depth, int32 MaxDepth)
	{
		if (!Widget) return nullptr;
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Widget->GetName());
		Obj->SetStringField(TEXT("class"), Widget->GetClass()->GetName());
		Obj->SetStringField(TEXT("visibility"), VisibilityToString(Widget->GetVisibility()));
		Obj->SetBoolField(TEXT("isVisible"), Widget->IsVisible());
		Obj->SetNumberField(TEXT("renderOpacity"), Widget->GetRenderOpacity());
		Obj->SetObjectField(TEXT("geometry"), MakeGeometryJson(Widget));

		FString Text = SafeGetText(Widget);
		if (!Text.IsEmpty())
		{
			Obj->SetStringField(TEXT("text"), Text);
		}

		if (UImage* Image = Cast<UImage>(Widget))
		{
			const FSlateBrush& Brush = Image->GetBrush();
			TSharedPtr<FJsonObject> BrushObj = MakeShared<FJsonObject>();
			BrushObj->SetNumberField(TEXT("imageSizeX"), Brush.ImageSize.X);
			BrushObj->SetNumberField(TEXT("imageSizeY"), Brush.ImageSize.Y);
			if (UObject* Resource = Brush.GetResourceObject())
			{
				BrushObj->SetStringField(TEXT("resource"), Resource->GetPathName());
			}
			Obj->SetObjectField(TEXT("brush"), BrushObj);
		}
		else if (UProgressBar* PB = Cast<UProgressBar>(Widget))
		{
			Obj->SetNumberField(TEXT("percent"), PB->GetPercent());
		}
		else if (UCheckBox* CB = Cast<UCheckBox>(Widget))
		{
			Obj->SetBoolField(TEXT("isChecked"), CB->IsChecked());
		}
		else if (USlider* Slider = Cast<USlider>(Widget))
		{
			Obj->SetNumberField(TEXT("value"), Slider->GetValue());
		}

		if (Depth >= MaxDepth) return Obj;

		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			TArray<TSharedPtr<FJsonValue>> ChildrenArr;
			for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
			{
				TSharedPtr<FJsonObject> ChildObj = BuildRuntimeNode(Panel->GetChildAt(i), Depth + 1, MaxDepth);
				if (ChildObj.IsValid())
				{
					ChildrenArr.Add(MakeShared<FJsonValueObject>(ChildObj));
				}
			}
			Obj->SetArrayField(TEXT("children"), ChildrenArr);
		}
		else if (UUserWidget* User = Cast<UUserWidget>(Widget))
		{
			// Nested UUserWidget: descend into its WidgetTree's root.
			if (User->WidgetTree && User->WidgetTree->RootWidget)
			{
				TSharedPtr<FJsonObject> RootObj = BuildRuntimeNode(User->WidgetTree->RootWidget, Depth + 1, MaxDepth);
				if (RootObj.IsValid())
				{
					Obj->SetObjectField(TEXT("root"), RootObj);
				}
			}
		}

		return Obj;
	}

}

TSharedPtr<FJsonValue> FWidgetHandlers::ListRuntimeWidgets(const TSharedPtr<FJsonObject>& Params)
{
	using namespace WidgetRuntime_Internal;

	UWorld* World = ResolveRuntimeWorld();
	if (!World)
	{
		return MCPError(TEXT("No PIE world available. Is Play-In-Editor running?"));
	}

	// Optional filter: class name (contains) / name prefix
	const FString ClassFilter = OptionalString(Params, TEXT("classFilter"), TEXT(""));
	const FString NamePrefix  = OptionalString(Params, TEXT("namePrefix"), TEXT(""));
	const bool bInViewportOnly = OptionalBool(Params, TEXT("viewportOnly"), false);

	TArray<TSharedPtr<FJsonValue>> WidgetsArr;
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* Widget = *It;
		if (!IsValid(Widget)) continue;
		if (Widget->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)) continue;

		UWorld* WidgetWorld = Widget->GetWorld();
		if (WidgetWorld != World) continue;

		const FString ClassName = Widget->GetClass()->GetName();
		const FString Name = Widget->GetName();
		if (!ClassFilter.IsEmpty() && !ClassName.Contains(ClassFilter)) continue;
		if (!NamePrefix.IsEmpty()  && !Name.StartsWith(NamePrefix)) continue;
		if (bInViewportOnly && !Widget->IsInViewport()) continue;

		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Name);
		Obj->SetStringField(TEXT("class"), ClassName);
		Obj->SetStringField(TEXT("visibility"), VisibilityToString(Widget->GetVisibility()));
		Obj->SetBoolField(TEXT("isVisible"), Widget->IsVisible());
		Obj->SetBoolField(TEXT("inViewport"), Widget->IsInViewport());
		if (Widget->WidgetTree && Widget->WidgetTree->RootWidget)
		{
			Obj->SetStringField(TEXT("rootWidgetName"), Widget->WidgetTree->RootWidget->GetName());
			Obj->SetStringField(TEXT("rootWidgetClass"), Widget->WidgetTree->RootWidget->GetClass()->GetName());
		}
		WidgetsArr.Add(MakeShared<FJsonValueObject>(Obj));
	}

	auto Result = MCPSuccess();
	Result->SetStringField(TEXT("world"), World->GetName());
	Result->SetArrayField(TEXT("widgets"), WidgetsArr);
	Result->SetNumberField(TEXT("count"), WidgetsArr.Num());
	return MCPResult(Result);
}

TSharedPtr<FJsonValue> FWidgetHandlers::GetRuntimeWidget(const TSharedPtr<FJsonObject>& Params)
{
	using namespace WidgetRuntime_Internal;

	UWorld* World = ResolveRuntimeWorld();
	if (!World)
	{
		return MCPError(TEXT("No PIE world available. Is Play-In-Editor running?"));
	}

	FString WidgetName;
	Params->TryGetStringField(TEXT("widgetName"), WidgetName);
	FString ClassFilter;
	Params->TryGetStringField(TEXT("className"), ClassFilter);
	if (WidgetName.IsEmpty() && ClassFilter.IsEmpty())
	{
		return MCPError(TEXT("Provide widgetName (exact instance name) or className (first match)."));
	}

	const int32 MaxDepth = OptionalInt(Params, TEXT("maxDepth"), DefaultRuntimeTreeMaxDepth);
	const FString ChildName = OptionalString(Params, TEXT("childName"), TEXT(""));

	UUserWidget* Found = nullptr;
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* Widget = *It;
		if (!IsValid(Widget) || Widget->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)) continue;
		if (Widget->GetWorld() != World) continue;

		if (!WidgetName.IsEmpty() && Widget->GetName() != WidgetName) continue;
		if (!ClassFilter.IsEmpty() && !Widget->GetClass()->GetName().Contains(ClassFilter)) continue;

		Found = Widget;
		break;
	}

	if (!Found)
	{
		return MCPError(TEXT("Runtime widget not found. Try list_runtime_widgets to see available instances."));
	}

	auto Result = MCPSuccess();
	Result->SetStringField(TEXT("name"), Found->GetName());
	Result->SetStringField(TEXT("class"), Found->GetClass()->GetName());
	Result->SetStringField(TEXT("visibility"), VisibilityToString(Found->GetVisibility()));
	Result->SetBoolField(TEXT("inViewport"), Found->IsInViewport());

	if (Found->WidgetTree && Found->WidgetTree->RootWidget)
	{
		UWidget* ScanRoot = Found->WidgetTree->RootWidget;
		if (!ChildName.IsEmpty())
		{
			// Search the widget tree for the named child.
			UWidget* Target = nullptr;
			Found->WidgetTree->ForEachWidget([&](UWidget* W)
			{
				if (W && W->GetName() == ChildName && !Target)
				{
					Target = W;
				}
			});
			if (!Target)
			{
				return MCPError(FString::Printf(TEXT("Child widget '%s' not found inside '%s'"), *ChildName, *Found->GetName()));
			}
			ScanRoot = Target;
		}

		TSharedPtr<FJsonObject> Tree = BuildRuntimeNode(ScanRoot, 0, MaxDepth);
		if (Tree.IsValid())
		{
			Result->SetObjectField(TEXT("tree"), Tree);
		}
	}
	else
	{
		Result->SetStringField(TEXT("tree"), TEXT("empty"));
	}

	return MCPResult(Result);
}

TSharedPtr<FJsonValue> FWidgetHandlers::DumpRuntimeWidgetGeometry(const TSharedPtr<FJsonObject>& Params)
{
	using namespace WidgetRuntime_Internal;

	UWorld* World = ResolveRuntimeWorld();
	if (!World)
	{
		return MCPError(TEXT("No PIE world available. Is Play-In-Editor running?"));
	}

	FString WidgetName;
	Params->TryGetStringField(TEXT("widgetName"), WidgetName);
	FString ClassFilter;
	Params->TryGetStringField(TEXT("className"), ClassFilter);
	const FString NamePrefix = OptionalString(Params, TEXT("namePrefix"), TEXT(""));
	const bool bInViewportOnly = OptionalBool(Params, TEXT("viewportOnly"), false);
	const int32 MaxDepth = OptionalInt(Params, TEXT("maxDepth"), DefaultRuntimeTreeMaxDepth);

	TArray<TSharedPtr<FJsonValue>> WidgetsArr;
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* Widget = *It;
		if (!IsValid(Widget) || Widget->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)) continue;
		if (Widget->GetWorld() != World) continue;

		const FString ClassName = Widget->GetClass()->GetName();
		const FString Name = Widget->GetName();
		if (!WidgetName.IsEmpty() && Name != WidgetName) continue;
		if (!ClassFilter.IsEmpty() && !ClassName.Contains(ClassFilter)) continue;
		if (!NamePrefix.IsEmpty() && !Name.StartsWith(NamePrefix)) continue;
		if (bInViewportOnly && !Widget->IsInViewport()) continue;

		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Name);
		Obj->SetStringField(TEXT("class"), ClassName);
		Obj->SetStringField(TEXT("visibility"), VisibilityToString(Widget->GetVisibility()));
		Obj->SetBoolField(TEXT("isVisible"), Widget->IsVisible());
		Obj->SetBoolField(TEXT("inViewport"), Widget->IsInViewport());
		Obj->SetObjectField(TEXT("geometry"), MakeGeometryJson(Widget));

		if (Widget->WidgetTree && Widget->WidgetTree->RootWidget)
		{
			if (TSharedPtr<FJsonObject> Tree = BuildRuntimeNode(Widget->WidgetTree->RootWidget, 0, MaxDepth))
			{
				Obj->SetObjectField(TEXT("tree"), Tree);
			}
		}

		WidgetsArr.Add(MakeShared<FJsonValueObject>(Obj));
	}

	auto Result = MCPSuccess();
	Result->SetStringField(TEXT("world"), World->GetName());
	Result->SetNumberField(TEXT("maxDepth"), MaxDepth);
	Result->SetArrayField(TEXT("widgets"), WidgetsArr);
	Result->SetNumberField(TEXT("count"), WidgetsArr.Num());
	Result->SetStringField(TEXT("note"), TEXT("Geometry is Slate tick-space cached geometry; capture_slate_window reports screen pixels, so OS DPI scaling can require coordinate conversion for mouse automation."));
	return MCPResult(Result);
}

TSharedPtr<FJsonValue> FWidgetHandlers::SpawnRuntimeWidgetPreview(const TSharedPtr<FJsonObject>& Params)
{
	using namespace WidgetRuntime_Internal;

	UWorld* World = ResolveRuntimeWorld();
	if (!World)
	{
		return MCPError(TEXT("No PIE world available. Start PIE before spawning a runtime widget preview."));
	}

	FString WidgetClassName;
	if (auto Err = RequireStringAlt(Params, TEXT("assetPath"), TEXT("widgetClass"), WidgetClassName)) return Err;

	UClass* WidgetClass = ResolveWidgetClass(WidgetClassName);
	if (!WidgetClass || !WidgetClass->IsChildOf(UUserWidget::StaticClass()))
	{
		return MCPError(FString::Printf(TEXT("Not a UserWidget class: %s"), *WidgetClassName));
	}

	PruneSpawnedPreviewWidgets();

	const FString InstanceName = OptionalString(Params, TEXT("instanceName"), TEXT(""));
	if (!InstanceName.IsEmpty())
	{
		if (TStrongObjectPtr<UUserWidget>* ExistingPreview = SpawnedPreviewWidgets.Find(InstanceName))
		{
			if (ExistingPreview->IsValid())
			{
				UUserWidget* Existing = ExistingPreview->Get();
				if (Existing->GetWorld() == World)
				{
					auto Result = MCPSuccess();
					MCPSetExisted(Result);
					Result->SetStringField(TEXT("name"), Existing->GetName());
					Result->SetStringField(TEXT("class"), Existing->GetClass()->GetName());
					Result->SetBoolField(TEXT("inViewport"), Existing->IsInViewport());
					return MCPResult(Result);
				}

				Existing->RemoveFromParent();
			}
			SpawnedPreviewWidgets.Remove(InstanceName);
		}

		for (TObjectIterator<UUserWidget> It; It; ++It)
		{
			UUserWidget* Existing = *It;
			if (IsValid(Existing)
				&& !Existing->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)
				&& Existing->GetWorld() == World
				&& Existing->GetName() == InstanceName)
			{
				auto Result = MCPSuccess();
				MCPSetExisted(Result);
				Result->SetStringField(TEXT("name"), Existing->GetName());
				Result->SetStringField(TEXT("class"), Existing->GetClass()->GetName());
				Result->SetBoolField(TEXT("inViewport"), Existing->IsInViewport());
				return MCPResult(Result);
			}
		}
	}

	UUserWidget* Widget = CreateWidget<UUserWidget>(World, WidgetClass);
	if (!Widget)
	{
		return MCPError(FString::Printf(TEXT("CreateWidget failed for %s"), *WidgetClassName));
	}

	if (!InstanceName.IsEmpty())
	{
		Widget->Rename(*InstanceName, World, REN_DontCreateRedirectors | REN_NonTransactional);
	}

	const int32 ZOrder = OptionalInt(Params, TEXT("zOrder"), 0);
	Widget->AddToViewport(ZOrder);

	const float X = static_cast<float>(OptionalNumber(Params, TEXT("x"), 0.0));
	const float Y = static_cast<float>(OptionalNumber(Params, TEXT("y"), 0.0));
	Widget->SetPositionInViewport(FVector2D(X, Y), false);

	const float Width = static_cast<float>(OptionalNumber(Params, TEXT("width"), 0.0));
	const float Height = static_cast<float>(OptionalNumber(Params, TEXT("height"), 0.0));
	if (Width > 0.0f && Height > 0.0f)
	{
		Widget->SetDesiredSizeInViewport(FVector2D(Width, Height));
	}

	Widget->ForceLayoutPrepass();
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().Tick();
	}

	const FString PreviewKey = InstanceName.IsEmpty() ? Widget->GetName() : InstanceName;
	EnsureSpawnedPreviewCleanupRegistered();
	SpawnedPreviewWidgets.FindOrAdd(PreviewKey) = TStrongObjectPtr<UUserWidget>(Widget);

	auto Result = MCPSuccess();
	MCPSetCreated(Result);
	Result->SetStringField(TEXT("name"), Widget->GetName());
	Result->SetStringField(TEXT("class"), Widget->GetClass()->GetName());
	Result->SetBoolField(TEXT("inViewport"), Widget->IsInViewport());
	Result->SetNumberField(TEXT("zOrder"), ZOrder);
	return MCPResult(Result);
}

TSharedPtr<FJsonValue> FWidgetHandlers::DispatchRuntimeWidgetPointerEvent(const TSharedPtr<FJsonObject>& Params)
{
	using namespace WidgetRuntime_Internal;

	UWorld* World = ResolveRuntimeWorld();
	if (!World)
	{
		return MCPError(TEXT("No PIE world available. Start PIE before dispatching widget pointer events."));
	}

	FString WidgetName;
	Params->TryGetStringField(TEXT("widgetName"), WidgetName);
	FString ClassFilter;
	Params->TryGetStringField(TEXT("className"), ClassFilter);
	if (WidgetName.IsEmpty() && ClassFilter.IsEmpty())
	{
		return MCPError(TEXT("Provide widgetName or className."));
	}

	UUserWidget* Found = nullptr;
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* Candidate = *It;
		if (!IsValid(Candidate) || Candidate->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)) continue;
		if (Candidate->GetWorld() != World) continue;
		if (!WidgetName.IsEmpty() && Candidate->GetName() != WidgetName) continue;
		if (!ClassFilter.IsEmpty() && !Candidate->GetClass()->GetName().Contains(ClassFilter)) continue;
		Found = Candidate;
		break;
	}
	if (!Found)
	{
		return MCPError(TEXT("Runtime widget not found. Use list_runtime_widgets first."));
	}

	const FString ChildName = OptionalString(Params, TEXT("childName"), TEXT(""));
	UWidget* TargetWidget = Found;
	if (!ChildName.IsEmpty())
	{
		TargetWidget = nullptr;
		if (Found->WidgetTree)
		{
			Found->WidgetTree->ForEachWidget([&](UWidget* Widget)
			{
				if (!TargetWidget && Widget && Widget->GetName() == ChildName)
				{
					TargetWidget = Widget;
				}
			});
		}
		if (!TargetWidget)
		{
			return MCPError(FString::Printf(TEXT("Child widget '%s' not found inside '%s'."), *ChildName, *Found->GetName()));
		}
	}

	Found->ForceLayoutPrepass();
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().Tick();
	}

	TSharedPtr<SWidget> TargetSlate;
	if (UUserWidget* TargetUserWidget = Cast<UUserWidget>(TargetWidget))
	{
		TargetSlate = TargetUserWidget->TakeWidget();
	}
	else
	{
		TargetSlate = TargetWidget->GetCachedWidget();
	}
	if (!TargetSlate.IsValid())
	{
		return MCPError(FString::Printf(TEXT("Target widget '%s' has no cached Slate widget."), *TargetWidget->GetName()));
	}

	const FGeometry Geometry = TargetSlate->GetTickSpaceGeometry();
	const FVector2D Size = Geometry.GetLocalSize();
	if (Size.X <= 0.0f || Size.Y <= 0.0f)
	{
		return MCPError(FString::Printf(TEXT("Target widget '%s' has zero geometry."), *TargetWidget->GetName()));
	}

	const float LocalX = static_cast<float>(OptionalNumber(Params, TEXT("localX"), Size.X * 0.5));
	const float LocalY = static_cast<float>(OptionalNumber(Params, TEXT("localY"), Size.Y * 0.5));
	const FVector2D ScreenPosition = Geometry.LocalToAbsolute(FVector2D(LocalX, LocalY));
	const FString EventType = OptionalString(Params, TEXT("event"), TEXT("hover")).ToLower();
	const FString ButtonName = OptionalString(Params, TEXT("button"), TEXT("left")).ToLower();
	const FKey EffectingButton = ButtonName == TEXT("right") ? EKeys::RightMouseButton : EKeys::LeftMouseButton;
	TSet<FKey> PressedButtons;
	if (EventType.Contains(TEXT("down")) || EventType.Contains(TEXT("click")))
	{
		PressedButtons.Add(EffectingButton);
	}

	FPointerEvent PointerEvent(
		0,
		ScreenPosition,
		ScreenPosition,
		PressedButtons,
		EffectingButton,
		0.0f,
		FModifierKeysState());

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetCursorPos(ScreenPosition);
	}

	const FString TargetName = TargetWidget->GetName();
	const bool bTargetIsUserWidget = TargetWidget->IsA<UUserWidget>();
	FReply Reply = FReply::Unhandled();
	if (EventType == TEXT("hover") || EventType == TEXT("enter") || EventType == TEXT("mouseenter"))
	{
		TargetSlate->OnMouseEnter(Geometry, PointerEvent);
		TargetSlate->OnMouseMove(Geometry, PointerEvent);
		Reply = FReply::Handled();
	}
	else if (EventType == TEXT("leave") || EventType == TEXT("mouseleave"))
	{
		TargetSlate->OnMouseLeave(PointerEvent);
		Reply = FReply::Handled();
	}
	else if (EventType == TEXT("down") || EventType == TEXT("mousedown") || EventType == TEXT("click"))
	{
		Reply = TargetSlate->OnMouseButtonDown(Geometry, PointerEvent);
	}
	else if (EventType == TEXT("up") || EventType == TEXT("mouseup"))
	{
		Reply = TargetSlate->OnMouseButtonUp(Geometry, PointerEvent);
	}
	else
	{
		return MCPError(FString::Printf(TEXT("Unsupported pointer event '%s'."), *EventType));
	}

	const float AdvanceSeconds = static_cast<float>(OptionalNumber(Params, TEXT("advanceSeconds"), 0.0));
	if (AdvanceSeconds > 0.0f)
	{
		World->GetTimerManager().Tick(AdvanceSeconds);
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().Tick();
		}
	}

	auto Result = MCPSuccess();
	Result->SetStringField(TEXT("widgetName"), Found->GetName());
	Result->SetStringField(TEXT("targetName"), TargetName);
	Result->SetBoolField(TEXT("targetIsUserWidget"), bTargetIsUserWidget);
	Result->SetStringField(TEXT("event"), EventType);
	Result->SetStringField(TEXT("button"), EffectingButton.GetFName().ToString());
	Result->SetBoolField(TEXT("handled"), Reply.IsEventHandled());
	Result->SetNumberField(TEXT("screenX"), ScreenPosition.X);
	Result->SetNumberField(TEXT("screenY"), ScreenPosition.Y);
	return MCPResult(Result);
}

// ─────────────────────────────────────────────────────────────
// #161  Runtime delegate inspection — list FMulticastDelegateProperty fields on a live UUserWidget
// ─────────────────────────────────────────────────────────────
TSharedPtr<FJsonValue> FWidgetHandlers::GetRuntimeDelegates(const TSharedPtr<FJsonObject>& Params)
{
	using namespace WidgetRuntime_Internal;

	UWorld* World = ResolveRuntimeWorld();
	if (!World)
	{
		return MCPError(TEXT("No PIE world available. Is Play-In-Editor running?"));
	}

	FString WidgetName;
	Params->TryGetStringField(TEXT("widgetName"), WidgetName);
	FString ClassFilter;
	Params->TryGetStringField(TEXT("className"), ClassFilter);
	if (WidgetName.IsEmpty() && ClassFilter.IsEmpty())
	{
		return MCPError(TEXT("Provide 'widgetName' (exact instance name) or 'className' (first match)."));
	}

	UUserWidget* Found = nullptr;
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* Widget = *It;
		if (!IsValid(Widget) || Widget->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)) continue;
		if (Widget->GetWorld() != World) continue;

		if (!WidgetName.IsEmpty() && Widget->GetName() != WidgetName) continue;
		if (!ClassFilter.IsEmpty() && !Widget->GetClass()->GetName().Contains(ClassFilter)) continue;

		Found = Widget;
		break;
	}

	if (!Found)
	{
		return MCPError(TEXT("Runtime widget not found. Try list_runtime_widgets to see available instances."));
	}

	TArray<TSharedPtr<FJsonValue>> DelegatesArr;
	for (TFieldIterator<FMulticastDelegateProperty> It(Found->GetClass()); It; ++It)
	{
		FMulticastDelegateProperty* DelegateProp = *It;
		if (!DelegateProp) continue;

		const void* DelegateAddr = DelegateProp->ContainerPtrToValuePtr<void>(Found);
		const FMulticastScriptDelegate* ScriptDelegate = DelegateProp->GetMulticastDelegate(DelegateAddr);

		TSharedPtr<FJsonObject> DelegateObj = MakeShared<FJsonObject>();
		DelegateObj->SetStringField(TEXT("delegateName"), DelegateProp->GetName());

		bool bIsBound = false;
		if (ScriptDelegate)
		{
			bIsBound = ScriptDelegate->IsBound();
		}

		DelegateObj->SetBoolField(TEXT("isBound"), bIsBound);
		DelegatesArr.Add(MakeShared<FJsonValueObject>(DelegateObj));
	}

	auto Result = MCPSuccess();
	Result->SetStringField(TEXT("widgetName"), Found->GetName());
	Result->SetStringField(TEXT("widgetClass"), Found->GetClass()->GetName());
	Result->SetArrayField(TEXT("delegates"), DelegatesArr);
	Result->SetNumberField(TEXT("delegateCount"), DelegatesArr.Num());
	return MCPResult(Result);
}
