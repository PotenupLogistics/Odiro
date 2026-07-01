#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

/** Operation lane used to serialize editor-backed MCP work. */
enum class EMCPBridgeOperationKind : uint8
{
	ReadOnly,
	AssetWrite,
	RuntimePIE,
	Maintenance,
	SourceControl,
	GlobalEditorWrite
};

/** Admission lease for one operation that is allowed to execute. */
class UE_MCP_BRIDGE_API FMCPBridgeOperationLease
{
public:
	/** Create a lease for an admitted operation. */
	FMCPBridgeOperationLease(const FString& InOperationId, EMCPBridgeOperationKind InKind, const FString& InMethodName);
	~FMCPBridgeOperationLease();

	/** Mark that the operation body has started on its execution thread. */
	void MarkStarted();

	/** Release the operation lane exactly once. */
	void Release();

	/** Release a lease that never reached its execution body. */
	void ReleaseIfNeverStarted();

	/** Return the stable operation id associated with this lease. */
	const FString& GetOperationId() const { return OperationId; }

private:
	FCriticalSection Mutex;
	FString OperationId;
	FString MethodName;
	EMCPBridgeOperationKind Kind;
	bool bStarted = false;
	bool bReleased = false;
};

/** Scoped marker used while a handler runs under BridgeServer admission. */
class UE_MCP_BRIDGE_API FMCPBridgeOperationAdmittedScope
{
public:
	/** Mark the current thread as already admitted until this scope exits. */
	FMCPBridgeOperationAdmittedScope();
	~FMCPBridgeOperationAdmittedScope();

private:
	bool bPreviousValue = false;
};

/** Result of operation admission before a handler executes. */
struct FMCPBridgeOperationAdmission
{
	/** True only when the caller may execute the handler now. */
	bool bCanExecute = true;

	/** Lease to hold while the admitted operation executes. */
	TSharedPtr<FMCPBridgeOperationLease> Lease;

	/** Structured MCP result when the operation must not execute now. */
	TSharedPtr<FJsonValue> Response;
};

/** Shared reservation and wait coordinator for editor-backed MCP requests. */
class UE_MCP_BRIDGE_API FMCPBridgeOperationCoordinator
{
public:
	/** Admit immediately, wait briefly, or reserve a retry ticket based on params.concurrency. */
	static FMCPBridgeOperationAdmission TryBeginOrReserve(const FString& MethodName, const TSharedPtr<FJsonObject>& Params);

	/** Build status for all operations or one operationId. */
	static TSharedPtr<FJsonValue> BuildOperationStatusResult(const TSharedPtr<FJsonObject>& Params);

	/** Wait until a queued operation is ready to retry or the requested timeout elapses. */
	static TSharedPtr<FJsonValue> BuildOperationWaitResult(const TSharedPtr<FJsonObject>& Params);

	/** Cancel a queued operation before it starts. */
	static TSharedPtr<FJsonValue> BuildOperationCancelResult(const TSharedPtr<FJsonObject>& Params);

	/** Build the operation snapshot embedded in coordination_get_status. */
	static TSharedPtr<FJsonObject> BuildSnapshotObject();

	/** Return a typed blocked response for non-admission editor invariants. */
	static TSharedPtr<FJsonValue> MakeBlockedResult(const FString& Code, const FString& Message, const FString& RequiredAction, double RetryAfterMs = 0.0);

	/** Classify one MCP method into the lane it must use. */
	static EMCPBridgeOperationKind ClassifyMethod(const FString& MethodName, const TSharedPtr<FJsonObject>& Params);

	/** Return whether the current handler call already passed BridgeServer admission. */
	static bool IsCurrentThreadOperationAdmitted();

private:
	friend class FMCPBridgeOperationAdmittedScope;
	friend class FMCPBridgeOperationLease;

	/** Release the active operation held by a lease. */
	static void ReleaseOperation(const FString& OperationId);

	/** Set the current-thread admission marker and return its previous value. */
	static bool SetCurrentThreadOperationAdmitted(bool bAdmitted);
};
