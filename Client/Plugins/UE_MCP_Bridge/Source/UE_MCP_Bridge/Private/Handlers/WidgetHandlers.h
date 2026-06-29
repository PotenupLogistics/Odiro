#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"

class FWidgetHandlers
{
public:
	static void RegisterHandlers(class FMCPHandlerRegistry& Registry);

private:
	static TSharedPtr<FJsonValue> ListWidgetBlueprints(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> CreateWidgetBlueprint(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> ReadWidgetTree(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> ApplyWidgetTreeSpec(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> CreateEditorUtilityWidget(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> CreateEditorUtilityBlueprint(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> GetWidgetProperties(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> GetWidgetFullProperties(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> ListWidgetBindings(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> ClearWidgetBinding(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> SetWidgetProperty(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> ReadWidgetAnimations(const TSharedPtr<FJsonObject>& Params);
	// Creates or replaces UMG animations that drive a widget RenderOpacity track.
	static TSharedPtr<FJsonValue> EnsureWidgetRenderOpacityAnimations(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> RunEditorUtilityWidget(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> RunEditorUtilityBlueprint(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> AddWidget(const TSharedPtr<FJsonObject>& Params);
	// Replaces named widgets while preserving WBP-owned names, slots, and common visual properties.
	static TSharedPtr<FJsonValue> ReplaceWidgetClasses(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> SetNamedSlotContent(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> RemoveWidget(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> RenameWidget(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> MoveWidget(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> RepairWidgetBlueprint(const TSharedPtr<FJsonObject>& Params);
	// #365: root-widget swap + "Wrap With" container insertion. Required to
	// reshape an existing WBP root without rebuilding the whole tree.
	static TSharedPtr<FJsonValue> SetRoot(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> WrapRoot(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> ListWidgetClasses(const TSharedPtr<FJsonObject>& Params);

	// Renders a Widget Blueprint in isolation to a PNG (no editor-window occlusion).
	static TSharedPtr<FJsonValue> CaptureWidget(const TSharedPtr<FJsonObject>& Params);

	// Runtime (PIE) widget inspection (#160)
	static TSharedPtr<FJsonValue> ListRuntimeWidgets(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> GetRuntimeWidget(const TSharedPtr<FJsonObject>& Params);
	// Runtime widget geometry dump for screenshot/layout triage.
	static TSharedPtr<FJsonValue> DumpRuntimeWidgetGeometry(const TSharedPtr<FJsonObject>& Params);
	// Spawns a Widget Blueprint into the PIE viewport for interaction checks.
	static TSharedPtr<FJsonValue> SpawnRuntimeWidgetPreview(const TSharedPtr<FJsonObject>& Params);
	// Sends a simple pointer event to a PIE widget or named child.
	static TSharedPtr<FJsonValue> DispatchRuntimeWidgetPointerEvent(const TSharedPtr<FJsonObject>& Params);
	// #161: Runtime delegate inspection
	static TSharedPtr<FJsonValue> GetRuntimeDelegates(const TSharedPtr<FJsonObject>& Params);

	// Helper: recursively search for a widget by name in the tree
	static class UWidget* FindWidgetByNameRecursive(class UWidget* Root, const FString& WidgetName);
};
