#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

/** Shared editor-maintenance state used by the UE bridge and reload MCP. */
class FMCPBridgeCoordination
{
public:
	/** Return the directory that stores bridge runtime coordination files. */
	static FString GetStateDirectory();

	/** Return the maintenance sentinel path used by all MCP sessions. */
	static FString GetMaintenanceFilePath();

	/** Return whether the bridge is currently gated for editor maintenance. */
	static bool IsMaintenanceActive();

	/** Return whether a method may run while editor maintenance is active. */
	static bool IsCoordinationMethod(const FString& MethodName);

	/** Track a non-coordination MCP request that has entered the bridge. */
	static void BeginActiveRequest();

	/** Release a previously tracked non-coordination MCP request. */
	static void EndActiveRequest();

	/** Return the number of accepted non-coordination requests still active. */
	static int32 GetActiveRequestCount();

	/** Create the standard maintenance-pending result for gated calls. */
	static TSharedPtr<FJsonValue> MakeMaintenancePendingResult(const FString& MethodName);

	/** Build a status result for coordination_get_status. */
	static TSharedPtr<FJsonValue> BuildStatusResult();

	/** Persist the maintenance sentinel from an editor-side coordination call. */
	static bool WriteMaintenanceFile(const FString& JobId, const FString& Reason, const FString& Operation, const FString& Phase, FString& OutError);
};
