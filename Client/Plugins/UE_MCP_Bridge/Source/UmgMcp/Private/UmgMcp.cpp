// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "UmgMcp.h"
#include "Bridge/UmgMcpBridge.h"
#include "MCPHandlerRegistration.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "XmlFile.h"

DEFINE_LOG_CATEGORY(LogUmgMcp);

#define LOCTEXT_NAMESPACE "FUmgMcpModule"

namespace
{
/** Names from the retired UmgMcp stdio surface that are now served by UE_MCP_Bridge. */
const TCHAR* GExternalUmgMcpTools[] = {
	TEXT("get_target_umg_asset"),
	TEXT("get_target_widget"),
	TEXT("get_last_edited_umg_asset"),
	TEXT("get_recently_edited_umg_assets"),
	TEXT("set_target_umg_asset"),
	TEXT("set_target_widget"),
	TEXT("get_widget_schema"),
	TEXT("get_creatable_widget_types"),
	TEXT("get_widget_tree"),
	TEXT("query_widget_properties"),
	TEXT("get_layout_data"),
	TEXT("check_widget_overlap"),
	TEXT("create_widget"),
	TEXT("set_widget_properties"),
	TEXT("delete_widget"),
	TEXT("reparent_widget"),
	TEXT("save_asset"),
	TEXT("export_umg_to_json"),
	TEXT("apply_layout"),
	TEXT("apply_json_to_umg"),
	TEXT("apply_html_to_umg"),
	TEXT("refresh_asset_registry"),
	TEXT("get_actors_in_level"),
	TEXT("spawn_actor"),
	TEXT("list_assets"),
	TEXT("compile_blueprint"),
	TEXT("add_step"),
	TEXT("prepare_value"),
	TEXT("connect_data_to_pin"),
	TEXT("get_function_nodes"),
	TEXT("add_variable"),
	TEXT("delete_variable"),
	TEXT("get_variables"),
	TEXT("delete_node"),
	TEXT("set_edit_function"),
	TEXT("set_cursor_node"),
	TEXT("search_function_library"),
	TEXT("set_animation_scope"),
	TEXT("set_widget_scope"),
	TEXT("get_all_animations"),
	TEXT("get_animation_keyframes"),
	TEXT("get_animated_widgets"),
	TEXT("get_animation_full_data"),
	TEXT("get_widget_animation_data"),
	TEXT("animation_widget_properties"),
	TEXT("animation_time_properties"),
	TEXT("animation_overview"),
	TEXT("create_animation"),
	TEXT("delete_animation"),
	TEXT("set_property_keys"),
	TEXT("remove_property_track"),
	TEXT("remove_keys"),
	TEXT("animation_append_widget_tracks"),
	TEXT("animation_append_time_slice"),
	TEXT("animation_delete_widget_keys"),
	TEXT("material_set_target"),
	TEXT("material_define_variable"),
	TEXT("material_add_node"),
	TEXT("material_delete"),
	TEXT("material_connect_nodes"),
	TEXT("material_connect_pins"),
	TEXT("material_set_hlsl_node_io"),
	TEXT("material_set_node_properties"),
	TEXT("material_compile_asset"),
	TEXT("material_get_pins"),
	TEXT("material_get_graph"),
	TEXT("hlsl_set_target"),
	TEXT("hlsl_get"),
	TEXT("hlsl_set"),
	TEXT("hlsl_compile")
};

constexpr float GExternalUmgMcpTimeoutSeconds = 30.0f;

/** Creates a Bridge-compatible error payload for migrated UmgMcp handlers. */
TSharedPtr<FJsonValue> MakeUmgMcpBridgeError(const FString& Message)
{
	TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
	Error->SetStringField(TEXT("status"), TEXT("error"));
	Error->SetBoolField(TEXT("success"), false);
	Error->SetStringField(TEXT("error"), Message);
	return MakeShared<FJsonValueObject>(Error);
}

/** Copies params before wrapper normalization so caller-owned JSON remains untouched. */
TSharedPtr<FJsonObject> CopyParams(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Copy = MakeShared<FJsonObject>();
	if (Params.IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Params->Values)
		{
			Copy->SetField(Field.Key, Field.Value);
		}
	}
	return Copy;
}

/** Mirrors the old Python wrapper's "NodeId:PinName" pin shorthand. */
void SplitPinReference(const FString& PinReference, const FString& DefaultPinName, FString& OutNodeId, FString& OutPinName)
{
	if (!PinReference.Split(TEXT(":"), &OutNodeId, &OutPinName))
	{
		OutNodeId = PinReference;
		OutPinName = DefaultPinName;
	}
}

/** Returns the old get_creatable_widget_types informational response. */
TSharedPtr<FJsonValue> MakeCreatableWidgetTypesResponse()
{
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Theoretically, any UMG widget class can be created. In practice, it depends on valid class names. Start with common types like 'Button', 'TextBlock', 'Image', 'CanvasPanel', 'VerticalBox'."));

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("status"), TEXT("info"));
	Result->SetObjectField(TEXT("data"), Data);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetStringField(TEXT("status"), TEXT("success"));
	Response->SetObjectField(TEXT("result"), Result);
	return MakeShared<FJsonValueObject>(Response);
}

/** Maps simple HTML/XML-like tags to UMG class paths. */
FString MapHtmlTagToWidgetClass(const FString& Tag)
{
	static const TMap<FString, FString> TagToClass = {
		{ TEXT("button"), TEXT("/Script/UMG.Button") },
		{ TEXT("textblock"), TEXT("/Script/UMG.TextBlock") },
		{ TEXT("image"), TEXT("/Script/UMG.Image") },
		{ TEXT("canvaspanel"), TEXT("/Script/UMG.CanvasPanel") },
		{ TEXT("verticalbox"), TEXT("/Script/UMG.VerticalBox") },
		{ TEXT("horizontalbox"), TEXT("/Script/UMG.HorizontalBox") },
		{ TEXT("overlay"), TEXT("/Script/UMG.Overlay") },
		{ TEXT("border"), TEXT("/Script/UMG.Border") },
		{ TEXT("editabletextbox"), TEXT("/Script/UMG.EditableTextBox") },
		{ TEXT("progressbar"), TEXT("/Script/UMG.ProgressBar") },
		{ TEXT("spacer"), TEXT("/Script/UMG.Spacer") },
		{ TEXT("sizebox"), TEXT("/Script/UMG.SizeBox") },
		{ TEXT("scrollbox"), TEXT("/Script/UMG.ScrollBox") },
		{ TEXT("gridpanel"), TEXT("/Script/UMG.GridPanel") },
		{ TEXT("uniformgridpanel"), TEXT("/Script/UMG.UniformGridPanel") },
		{ TEXT("wrapbox"), TEXT("/Script/UMG.WrapBox") },
		{ TEXT("widgetswitcher"), TEXT("/Script/UMG.WidgetSwitcher") },
		{ TEXT("safezone"), TEXT("/Script/UMG.SafeZone") },
		{ TEXT("scalebox"), TEXT("/Script/UMG.ScaleBox") },
		{ TEXT("invalidationbox"), TEXT("/Script/UMG.InvalidationBox") },
		{ TEXT("retainerbox"), TEXT("/Script/UMG.RetainerBox") },
		{ TEXT("checkbox"), TEXT("/Script/UMG.CheckBox") },
		{ TEXT("slider"), TEXT("/Script/UMG.Slider") },
		{ TEXT("comboboxstring"), TEXT("/Script/UMG.ComboBoxString") },
		{ TEXT("div"), TEXT("/Script/UMG.CanvasPanel") },
		{ TEXT("span"), TEXT("/Script/UMG.TextBlock") },
		{ TEXT("p"), TEXT("/Script/UMG.TextBlock") },
		{ TEXT("img"), TEXT("/Script/UMG.Image") },
		{ TEXT("input"), TEXT("/Script/UMG.EditableTextBox") }
	};

	const FString LowerTag = Tag.ToLower();
	if (const FString* MappedClass = TagToClass.Find(LowerTag))
	{
		return *MappedClass;
	}

	return FString::Printf(TEXT("/Script/UMG.%s"), *Tag);
}

/** Parses scalar-ish XML attribute values into JSON values. */
TSharedPtr<FJsonValue> ParseHtmlAttributeValue(const FString& RawValue)
{
	const FString Value = RawValue.TrimStartAndEnd();
	if (Value.Equals(TEXT("true"), ESearchCase::IgnoreCase))
	{
		return MakeShared<FJsonValueBoolean>(true);
	}
	if (Value.Equals(TEXT("false"), ESearchCase::IgnoreCase))
	{
		return MakeShared<FJsonValueBoolean>(false);
	}

	double NumberValue = 0.0;
	if (LexTryParseString(NumberValue, *Value))
	{
		return MakeShared<FJsonValueNumber>(NumberValue);
	}

	if (Value.StartsWith(TEXT("{")))
	{
		TSharedPtr<FJsonObject> ObjectValue;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Value);
		if (FJsonSerializer::Deserialize(Reader, ObjectValue) && ObjectValue.IsValid())
		{
			return MakeShared<FJsonValueObject>(ObjectValue);
		}
	}
	else if (Value.StartsWith(TEXT("[")))
	{
		TArray<TSharedPtr<FJsonValue>> ArrayValue;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Value);
		if (FJsonSerializer::Deserialize(Reader, ArrayValue))
		{
			return MakeShared<FJsonValueArray>(ArrayValue);
		}
	}

	return MakeShared<FJsonValueString>(RawValue);
}

/** Converts one XML node to the JSON shape consumed by apply_json_to_umg. */
TSharedPtr<FJsonObject> HtmlNodeToUmgJson(const FXmlNode* Node, int32& GeneratedNameIndex)
{
	TSharedPtr<FJsonObject> WidgetJson = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> PropertiesJson = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> SlotPropertiesJson = MakeShared<FJsonObject>();

	const FString Tag = Node->GetTag();
	FString WidgetName;

	for (const FXmlAttribute& Attribute : Node->GetAttributes())
	{
		const FString Key = Attribute.GetTag();
		if (Key.Equals(TEXT("name"), ESearchCase::IgnoreCase))
		{
			WidgetName = Attribute.GetValue();
			continue;
		}

		if (Key.StartsWith(TEXT("Slot.")))
		{
			SlotPropertiesJson->SetField(Key.Mid(5), ParseHtmlAttributeValue(Attribute.GetValue()));
		}
		else
		{
			PropertiesJson->SetField(Key, ParseHtmlAttributeValue(Attribute.GetValue()));
		}
	}

	if (WidgetName.IsEmpty())
	{
		WidgetName = FString::Printf(TEXT("%s_%d"), *Tag, ++GeneratedNameIndex);
	}

	if (SlotPropertiesJson->Values.Num() > 0)
	{
		PropertiesJson->SetObjectField(TEXT("Slot"), SlotPropertiesJson);
	}

	WidgetJson->SetStringField(TEXT("widget_name"), WidgetName);
	WidgetJson->SetStringField(TEXT("widget_class"), MapHtmlTagToWidgetClass(Tag));
	WidgetJson->SetObjectField(TEXT("properties"), PropertiesJson);

	TArray<TSharedPtr<FJsonValue>> ChildrenJson;
	for (const FXmlNode* ChildNode : Node->GetChildrenNodes())
	{
		if (ChildNode)
		{
			ChildrenJson.Add(MakeShared<FJsonValueObject>(HtmlNodeToUmgJson(ChildNode, GeneratedNameIndex)));
		}
	}

	if (ChildrenJson.Num() > 0)
	{
		WidgetJson->SetArrayField(TEXT("children"), ChildrenJson);
	}

	return WidgetJson;
}

/** Converts the retired apply_html_to_umg input format to apply_layout JSON. */
bool ParseHtmlLayoutToJsonString(const FString& HtmlContent, FString& OutJson, FString& OutError)
{
	FXmlFile XmlFile(HtmlContent, EConstructMethod::ConstructFromBuffer);
	if (!XmlFile.IsValid())
	{
		OutError = FString::Printf(TEXT("Invalid HTML/XML content: %s"), *XmlFile.GetLastError());
		return false;
	}

	const FXmlNode* RootNode = XmlFile.GetRootNode();
	if (!RootNode)
	{
		OutError = TEXT("Invalid HTML/XML content: no root node.");
		return false;
	}

	int32 GeneratedNameIndex = 0;
	TSharedPtr<FJsonObject> RootJson = HtmlNodeToUmgJson(RootNode, GeneratedNameIndex);

	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	return FJsonSerializer::Serialize(RootJson.ToSharedRef(), Writer);
}

/** Translates retired Python-only wrapper tools to their native UmgMcp bridge command. */
void NormalizeLegacyWrapperCommand(FString& CommandName, TSharedPtr<FJsonObject>& Params)
{
	if (CommandName == TEXT("add_step"))
	{
		CommandName = TEXT("manage_blueprint_graph");
		Params->SetStringField(TEXT("subAction"), TEXT("add_function_step"));

		FString Name;
		if (Params->TryGetStringField(TEXT("name"), Name) && !Params->HasField(TEXT("nodeName")))
		{
			Params->SetStringField(TEXT("nodeName"), Name);
		}
		if (TSharedPtr<FJsonValue>* Args = Params->Values.Find(TEXT("args")); Args && !Params->HasField(TEXT("extraArgs")))
		{
			Params->SetField(TEXT("extraArgs"), *Args);
		}
		if (TSharedPtr<FJsonValue>* InputWires = Params->Values.Find(TEXT("input_wires")); InputWires && !Params->HasField(TEXT("inputWires")))
		{
			Params->SetField(TEXT("inputWires"), *InputWires);
		}
	}
	else if (CommandName == TEXT("prepare_value"))
	{
		CommandName = TEXT("manage_blueprint_graph");
		Params->SetStringField(TEXT("subAction"), TEXT("create_node"));

		FString Name;
		if (Params->TryGetStringField(TEXT("name"), Name) && !Params->HasField(TEXT("nodeName")))
		{
			Params->SetStringField(TEXT("nodeName"), Name);
		}
		if (TSharedPtr<FJsonValue>* Args = Params->Values.Find(TEXT("args")); Args && !Params->HasField(TEXT("extraArgs")))
		{
			Params->SetField(TEXT("extraArgs"), *Args);
		}
		Params->SetStringField(TEXT("autoConnectToNodeId"), TEXT(""));
	}
	else if (CommandName == TEXT("connect_data_to_pin"))
	{
		CommandName = TEXT("manage_blueprint_graph");
		Params->SetStringField(TEXT("subAction"), TEXT("connect_pins"));

		FString Source;
		FString Target;
		Params->TryGetStringField(TEXT("source"), Source);
		Params->TryGetStringField(TEXT("target"), Target);

		FString SourceId;
		FString SourcePin;
		FString TargetId;
		FString TargetPin;
		SplitPinReference(Source, TEXT("Return Value"), SourceId, SourcePin);
		SplitPinReference(Target, TEXT("InPin"), TargetId, TargetPin);

		Params->SetStringField(TEXT("nodeIdA"), SourceId);
		Params->SetStringField(TEXT("pinNameA"), SourcePin);
		Params->SetStringField(TEXT("nodeIdB"), TargetId);
		Params->SetStringField(TEXT("pinNameB"), TargetPin);
	}
	else if (CommandName == TEXT("get_function_nodes"))
	{
		CommandName = TEXT("manage_blueprint_graph");
		Params->SetStringField(TEXT("subAction"), TEXT("get_nodes"));
	}
	else if (CommandName == TEXT("add_variable"))
	{
		CommandName = TEXT("manage_blueprint_graph");
		Params->SetStringField(TEXT("subAction"), TEXT("add_variable"));
	}
	else if (CommandName == TEXT("delete_variable"))
	{
		CommandName = TEXT("manage_blueprint_graph");
		Params->SetStringField(TEXT("subAction"), TEXT("delete_variable"));
	}
	else if (CommandName == TEXT("get_variables"))
	{
		CommandName = TEXT("manage_blueprint_graph");
		Params->SetStringField(TEXT("subAction"), TEXT("get_variables"));
	}
	else if (CommandName == TEXT("delete_node"))
	{
		CommandName = TEXT("manage_blueprint_graph");
		Params->SetStringField(TEXT("subAction"), TEXT("delete_node"));

		FString NodeId;
		if (Params->TryGetStringField(TEXT("node_id"), NodeId) && !Params->HasField(TEXT("nodeId")))
		{
			Params->SetStringField(TEXT("nodeId"), NodeId);
		}
	}
	else if (CommandName == TEXT("search_function_library"))
	{
		CommandName = TEXT("manage_blueprint_graph");
		Params->SetStringField(TEXT("subAction"), TEXT("search_function_library"));

		FString ClassName;
		if (Params->TryGetStringField(TEXT("class_name"), ClassName) && !Params->HasField(TEXT("className")))
		{
			Params->SetStringField(TEXT("className"), ClassName);
		}
	}
}

/** Converts the Bridge response string back to the external handler JSON value contract. */
TSharedPtr<FJsonValue> ParseBridgeResponse(const FString& Response)
{
	TSharedPtr<FJsonObject> ResponseObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response);
	if (!FJsonSerializer::Deserialize(Reader, ResponseObject) || !ResponseObject.IsValid())
	{
		return MakeUmgMcpBridgeError(TEXT("UmgMcp command returned an invalid JSON response."));
	}
	return MakeShared<FJsonValueObject>(ResponseObject);
}

/** Dispatches migrated UmgMcp commands through the in-editor subsystem, not the old TCP server. */
TSharedPtr<FJsonValue> ExecuteUmgMcpBridgeCommand(const FString& MethodName, const TSharedPtr<FJsonObject>& Params)
{
	if (MethodName == TEXT("get_creatable_widget_types"))
	{
		return MakeCreatableWidgetTypesResponse();
	}

	if (!GEditor)
	{
		return MakeUmgMcpBridgeError(TEXT("GEditor is not available for UmgMcp bridge handler dispatch."));
	}

	UUmgMcpBridge* Bridge = GEditor->GetEditorSubsystem<UUmgMcpBridge>();
	if (!Bridge)
	{
		return MakeUmgMcpBridgeError(TEXT("UUmgMcpBridge subsystem is not available."));
	}

	FString CommandName = MethodName;
	TSharedPtr<FJsonObject> CommandParams = CopyParams(Params);

	if (CommandName == TEXT("apply_html_to_umg"))
	{
		FString AssetPath;
		FString TargetWidgetName;
		if (CommandParams->TryGetStringField(TEXT("asset_path"), AssetPath) &&
			!CommandParams->HasField(TEXT("widget_name")) &&
			AssetPath.Split(TEXT(":"), &AssetPath, &TargetWidgetName))
		{
			CommandParams->SetStringField(TEXT("asset_path"), AssetPath);
			if (!TargetWidgetName.TrimStartAndEnd().IsEmpty())
			{
				CommandParams->SetStringField(TEXT("widget_name"), TargetWidgetName.TrimStartAndEnd());
			}
		}

		FString HtmlContent;
		CommandParams->TryGetStringField(TEXT("html_content"), HtmlContent);
		if (HtmlContent.TrimStart().StartsWith(TEXT("<")))
		{
			FString JsonLayout;
			FString ParseError;
			if (!ParseHtmlLayoutToJsonString(HtmlContent, JsonLayout, ParseError))
			{
				return MakeUmgMcpBridgeError(ParseError);
			}
			CommandParams->SetStringField(TEXT("layout_content"), JsonLayout);
		}
		else if (!HtmlContent.IsEmpty() && !CommandParams->HasField(TEXT("layout_content")))
		{
			CommandParams->SetStringField(TEXT("layout_content"), HtmlContent);
		}
		CommandName = TEXT("apply_layout");
	}
	else
	{
		NormalizeLegacyWrapperCommand(CommandName, CommandParams);
	}

	return ParseBridgeResponse(Bridge->ExecuteCommand(CommandName, CommandParams));
}
}

void FUmgMcpModule::StartupModule()
{
	UE_LOG(LogUmgMcp, Display, TEXT("UmgMcp handlers registering on the UE_MCP_Bridge surface."));

	for (const TCHAR* ToolName : GExternalUmgMcpTools)
	{
		const FString MethodName(ToolName);
		UEMCP::RegisterExternalHandlerWithTimeout(
			MethodName,
			[MethodName](const TSharedPtr<FJsonObject>& Params) -> TSharedPtr<FJsonValue>
			{
				return ExecuteUmgMcpBridgeCommand(MethodName, Params);
			},
			GExternalUmgMcpTimeoutSeconds);
	}
}

void FUmgMcpModule::ShutdownModule()
{
	for (const TCHAR* ToolName : GExternalUmgMcpTools)
	{
		UEMCP::UnregisterExternalHandler(FString(ToolName));
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUmgMcpModule, UmgMcp)
