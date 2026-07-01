#include "MCPBridgeOperationCoordinator.h"

#include "UE_MCP_BridgeModule.h"
#include "HAL/PlatformProcess.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include <initializer_list>

namespace
{
	constexpr int32 MaxGlobalQueueSize = 32;
	constexpr int32 MaxQueueSizePerClient = 4;
	constexpr double DefaultWaitMs = 5000.0;
	constexpr double DefaultTicketTtlSeconds = 30.0;
	constexpr double DefaultRetryAfterMs = 750.0;

	struct FQueuedOperation
	{
		FString OperationId;
		FString MethodName;
		FString PayloadKey;
		FString ClientRequestId;
		EMCPBridgeOperationKind Kind = EMCPBridgeOperationKind::GlobalEditorWrite;
		FDateTime EnqueuedAtUtc;
		FDateTime LastHeartbeatUtc;
		FDateTime ExpiresAtUtc;
	};

	struct FActiveOperation
	{
		FString OperationId;
		FString MethodName;
		EMCPBridgeOperationKind Kind = EMCPBridgeOperationKind::GlobalEditorWrite;
		FDateTime StartedAtUtc;
	};

	FCriticalSection GOperationMutex;
	TOptional<FActiveOperation> GActiveOperation;
	TArray<FQueuedOperation> GQueuedOperations;
	thread_local bool GCurrentThreadOperationAdmitted = false;

	/** Return a stable string for operation kind values. */
	FString OperationKindToString(EMCPBridgeOperationKind Kind)
	{
		switch (Kind)
		{
		case EMCPBridgeOperationKind::ReadOnly: return TEXT("read_only");
		case EMCPBridgeOperationKind::AssetWrite: return TEXT("asset_write");
		case EMCPBridgeOperationKind::RuntimePIE: return TEXT("runtime_pie");
		case EMCPBridgeOperationKind::Maintenance: return TEXT("maintenance");
		case EMCPBridgeOperationKind::SourceControl: return TEXT("source_control");
		case EMCPBridgeOperationKind::GlobalEditorWrite: return TEXT("global_editor_write");
		default: return TEXT("unknown");
		}
	}

	/** Return whether the operation kind requires exclusive editor mutation access. */
	bool RequiresExclusiveEditorLane(EMCPBridgeOperationKind Kind)
	{
		return Kind != EMCPBridgeOperationKind::ReadOnly;
	}

	/** Read params.concurrency or return null when the caller did not provide it. */
	TSharedPtr<FJsonObject> GetConcurrencyObject(const TSharedPtr<FJsonObject>& Params)
	{
		if (!Params.IsValid())
		{
			return nullptr;
		}

		const TSharedPtr<FJsonObject>* Concurrency = nullptr;
		if (Params->TryGetObjectField(TEXT("concurrency"), Concurrency) && Concurrency && Concurrency->IsValid())
		{
			return *Concurrency;
		}
		return nullptr;
	}

	/** Read a string from params.concurrency first, then from top-level params. */
	FString GetConcurrencyString(const TSharedPtr<FJsonObject>& Params, const FString& FieldName, const FString& DefaultValue = TEXT(""))
	{
		if (TSharedPtr<FJsonObject> Concurrency = GetConcurrencyObject(Params))
		{
			FString Value;
			if (Concurrency->TryGetStringField(FieldName, Value))
			{
				return Value;
			}
		}

		if (Params.IsValid())
		{
			FString Value;
			if (Params->TryGetStringField(FieldName, Value))
			{
				return Value;
			}
		}

		return DefaultValue;
	}

	/** Read a number from params.concurrency first, then from top-level params. */
	double GetConcurrencyNumber(const TSharedPtr<FJsonObject>& Params, const FString& FieldName, double DefaultValue)
	{
		if (TSharedPtr<FJsonObject> Concurrency = GetConcurrencyObject(Params))
		{
			double Value = 0.0;
			if (Concurrency->TryGetNumberField(FieldName, Value))
			{
				return Value;
			}
		}

		if (Params.IsValid())
		{
			double Value = 0.0;
			if (Params->TryGetNumberField(FieldName, Value))
			{
				return Value;
			}
		}

		return DefaultValue;
	}

	/** Copy payload fields that identify the requested operation body. */
	TSharedRef<FJsonObject> BuildPayloadIdentityObject(const TSharedPtr<FJsonObject>& Params)
	{
		TSharedRef<FJsonObject> Identity = MakeShared<FJsonObject>();
		if (!Params.IsValid())
		{
			return Identity;
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Params->Values)
		{
			if (Field.Key == TEXT("concurrency") || Field.Key == TEXT("operationId"))
			{
				continue;
			}
			Identity->SetField(Field.Key, Field.Value);
		}
		return Identity;
	}

	/** Build a stable payload key for idempotent queued-operation retries. */
	FString BuildPayloadKey(const FString& MethodName, const TSharedPtr<FJsonObject>& Params)
	{
		const FString IdempotencyKey = GetConcurrencyString(Params, TEXT("idempotencyKey"));
		if (!IdempotencyKey.IsEmpty())
		{
			return FString::Printf(TEXT("%s|idempotency:%s"), *MethodName, *IdempotencyKey);
		}

		FString Serialized;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
		FJsonSerializer::Serialize(BuildPayloadIdentityObject(Params), Writer);
		const FString Hash = FMD5::HashAnsiString(*Serialized);
		return FString::Printf(TEXT("%s|payload:%s"), *MethodName, *Hash);
	}

	/** Build a compact JSON description of an active operation. */
	TSharedPtr<FJsonObject> MakeActiveOperationObjectLocked()
	{
		if (!GActiveOperation.IsSet())
		{
			return nullptr;
		}

		const FActiveOperation& Active = GActiveOperation.GetValue();
		TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("operationId"), Active.OperationId);
		Object->SetStringField(TEXT("method"), Active.MethodName);
		Object->SetStringField(TEXT("kind"), OperationKindToString(Active.Kind));
		Object->SetStringField(TEXT("startedAt"), Active.StartedAtUtc.ToIso8601());
		return Object;
	}

	/** Build the common busy response used by reject and wait timeout. */
	TSharedPtr<FJsonValue> MakeBusyResultLocked(const FString& MethodName, const FString& Message, bool bQueueable)
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), false);
		Result->SetBoolField(TEXT("accepted"), false);
		Result->SetBoolField(TEXT("completed"), false);
		Result->SetStringField(TEXT("state"), TEXT("busy"));
		Result->SetStringField(TEXT("code"), TEXT("operation_busy"));
		Result->SetStringField(TEXT("error"), Message);
		Result->SetStringField(TEXT("method"), MethodName);
		Result->SetBoolField(TEXT("queueable"), bQueueable);
		Result->SetNumberField(TEXT("retryAfterMs"), DefaultRetryAfterMs);
		if (TSharedPtr<FJsonObject> Active = MakeActiveOperationObjectLocked())
		{
			Result->SetObjectField(TEXT("activeOperation"), Active);
		}
		Result->SetNumberField(TEXT("queueLength"), GQueuedOperations.Num());
		return MakeShared<FJsonValueObject>(Result);
	}

	/** Build a queued/reserved response for a retry ticket. */
	TSharedPtr<FJsonValue> MakeQueuedResultLocked(const FQueuedOperation& Operation, int32 Position)
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), false);
		Result->SetBoolField(TEXT("accepted"), true);
		Result->SetBoolField(TEXT("completed"), false);
		Result->SetStringField(TEXT("state"), TEXT("queued"));
		Result->SetStringField(TEXT("code"), TEXT("operation_queued"));
		Result->SetStringField(TEXT("operationId"), Operation.OperationId);
		Result->SetStringField(TEXT("method"), Operation.MethodName);
		Result->SetStringField(TEXT("kind"), OperationKindToString(Operation.Kind));
		Result->SetNumberField(TEXT("position"), Position);
		Result->SetNumberField(TEXT("retryAfterMs"), DefaultRetryAfterMs);
		Result->SetStringField(TEXT("expiresAt"), Operation.ExpiresAtUtc.ToIso8601());
		Result->SetStringField(TEXT("message"), TEXT("Operation is reserved but has not executed. Retry the original request with this operationId when it is ready."));
		return MakeShared<FJsonValueObject>(Result);
	}

	/** Build a ready response for status/wait calls. */
	TSharedPtr<FJsonValue> MakeReadyResultLocked(const FQueuedOperation& Operation)
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), true);
		Result->SetBoolField(TEXT("accepted"), true);
		Result->SetBoolField(TEXT("completed"), false);
		Result->SetStringField(TEXT("state"), TEXT("ready"));
		Result->SetStringField(TEXT("code"), TEXT("operation_ready"));
		Result->SetStringField(TEXT("operationId"), Operation.OperationId);
		Result->SetStringField(TEXT("method"), Operation.MethodName);
		Result->SetStringField(TEXT("kind"), OperationKindToString(Operation.Kind));
		Result->SetNumberField(TEXT("retryAfterMs"), 0.0);
		Result->SetStringField(TEXT("message"), TEXT("Retry the original request with this operationId to execute now."));
		return MakeShared<FJsonValueObject>(Result);
	}

	/** Build a typed rejection response. */
	TSharedPtr<FJsonValue> MakeRejectedResult(const FString& Code, const FString& Message)
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), false);
		Result->SetBoolField(TEXT("accepted"), false);
		Result->SetBoolField(TEXT("completed"), false);
		Result->SetStringField(TEXT("state"), TEXT("rejected"));
		Result->SetStringField(TEXT("code"), Code);
		Result->SetStringField(TEXT("error"), Message);
		return MakeShared<FJsonValueObject>(Result);
	}

	/** Remove queue entries whose clients stopped heartbeating. */
	void RemoveExpiredQueuedOperationsLocked()
	{
		const FDateTime Now = FDateTime::UtcNow();
		for (int32 Index = GQueuedOperations.Num() - 1; Index >= 0; --Index)
		{
			if (GQueuedOperations[Index].ExpiresAtUtc <= Now)
			{
				UE_LOG(LogMCPBridge, Warning, TEXT("[UE-MCP] Expired queued operation %s (%s)"),
					*GQueuedOperations[Index].OperationId,
					*GQueuedOperations[Index].MethodName);
				GQueuedOperations.RemoveAt(Index);
			}
		}
	}

	/** Return the queue index for an operation id, or INDEX_NONE. */
	int32 FindQueuedOperationIndexLocked(const FString& OperationId)
	{
		for (int32 Index = 0; Index < GQueuedOperations.Num(); ++Index)
		{
			if (GQueuedOperations[Index].OperationId == OperationId)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	/** Return an existing queued retry ticket with the same client or payload identity. */
	int32 FindDuplicateQueuedOperationLocked(const FString& MethodName, const FString& PayloadKey, const FString& ClientRequestId)
	{
		for (int32 Index = 0; Index < GQueuedOperations.Num(); ++Index)
		{
			const FQueuedOperation& Operation = GQueuedOperations[Index];
			if (Operation.MethodName != MethodName)
			{
				continue;
			}
			if (!ClientRequestId.IsEmpty() && Operation.ClientRequestId == ClientRequestId)
			{
				return Index;
			}
			if (Operation.PayloadKey == PayloadKey)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	/** Count queue reservations owned by a logical client id. */
	int32 CountQueuedOperationsForClientLocked(const FString& ClientRequestId)
	{
		if (ClientRequestId.IsEmpty())
		{
			return 0;
		}

		int32 Count = 0;
		for (const FQueuedOperation& Operation : GQueuedOperations)
		{
			if (Operation.ClientRequestId == ClientRequestId)
			{
				++Count;
			}
		}
		return Count;
	}

	/** Refresh a queued reservation heartbeat. */
	void HeartbeatQueuedOperation(FQueuedOperation& Operation)
	{
		Operation.LastHeartbeatUtc = FDateTime::UtcNow();
		Operation.ExpiresAtUtc = Operation.LastHeartbeatUtc + FTimespan::FromSeconds(DefaultTicketTtlSeconds);
	}

	/** Create a new active-operation lease. */
	TSharedPtr<FMCPBridgeOperationLease> BeginOperationLocked(const FString& OperationId, const FString& MethodName, EMCPBridgeOperationKind Kind)
	{
		const FString EffectiveOperationId = OperationId.IsEmpty()
			? FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens)
			: OperationId;

		if (RequiresExclusiveEditorLane(Kind))
		{
			FActiveOperation Active;
			Active.OperationId = EffectiveOperationId;
			Active.MethodName = MethodName;
			Active.Kind = Kind;
			Active.StartedAtUtc = FDateTime::UtcNow();
			GActiveOperation = Active;
		}

		return MakeShared<FMCPBridgeOperationLease>(EffectiveOperationId, Kind, MethodName);
	}

	/** Return whether the queued operation may be executed by retrying now. */
	bool IsQueueHeadReadyLocked(int32 QueueIndex)
	{
		return QueueIndex == 0 && !GActiveOperation.IsSet();
	}

	/** Add a retry reservation and return its index. */
	int32 AddQueuedOperationLocked(const FString& MethodName, EMCPBridgeOperationKind Kind, const FString& PayloadKey, const FString& ClientRequestId)
	{
		FQueuedOperation Operation;
		Operation.OperationId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
		Operation.MethodName = MethodName;
		Operation.Kind = Kind;
		Operation.PayloadKey = PayloadKey;
		Operation.ClientRequestId = ClientRequestId;
		Operation.EnqueuedAtUtc = FDateTime::UtcNow();
		HeartbeatQueuedOperation(Operation);
		return GQueuedOperations.Add(Operation);
	}

	/** Return whether a method name starts with any prefix in the list. */
	bool StartsWithAny(const FString& MethodName, std::initializer_list<const TCHAR*> Prefixes)
	{
		for (const TCHAR* Prefix : Prefixes)
		{
			if (MethodName.StartsWith(Prefix, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	/** Return whether a method name contains any marker in the list. */
	bool ContainsAny(const FString& MethodName, std::initializer_list<const TCHAR*> Markers)
	{
		for (const TCHAR* Marker : Markers)
		{
			if (MethodName.Contains(Marker, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}
}

FMCPBridgeOperationLease::FMCPBridgeOperationLease(const FString& InOperationId, EMCPBridgeOperationKind InKind, const FString& InMethodName)
	: OperationId(InOperationId)
	, MethodName(InMethodName)
	, Kind(InKind)
{
}

FMCPBridgeOperationLease::~FMCPBridgeOperationLease()
{
	Release();
}

void FMCPBridgeOperationLease::MarkStarted()
{
	FScopeLock Lock(&Mutex);
	bStarted = true;
}

void FMCPBridgeOperationLease::Release()
{
	bool bShouldRelease = false;
	{
		FScopeLock Lock(&Mutex);
		if (!bReleased)
		{
			bReleased = true;
			bShouldRelease = true;
		}
	}

	if (bShouldRelease)
	{
		FMCPBridgeOperationCoordinator::ReleaseOperation(OperationId);
	}
}

void FMCPBridgeOperationLease::ReleaseIfNeverStarted()
{
	bool bShouldRelease = false;
	{
		FScopeLock Lock(&Mutex);
		if (!bStarted && !bReleased)
		{
			bReleased = true;
			bShouldRelease = true;
		}
	}

	if (bShouldRelease)
	{
		FMCPBridgeOperationCoordinator::ReleaseOperation(OperationId);
	}
}

FMCPBridgeOperationAdmittedScope::FMCPBridgeOperationAdmittedScope()
{
	bPreviousValue = FMCPBridgeOperationCoordinator::SetCurrentThreadOperationAdmitted(true);
}

FMCPBridgeOperationAdmittedScope::~FMCPBridgeOperationAdmittedScope()
{
	FMCPBridgeOperationCoordinator::SetCurrentThreadOperationAdmitted(bPreviousValue);
}

FMCPBridgeOperationAdmission FMCPBridgeOperationCoordinator::TryBeginOrReserve(const FString& MethodName, const TSharedPtr<FJsonObject>& Params)
{
	FMCPBridgeOperationAdmission Admission;
	const EMCPBridgeOperationKind Kind = ClassifyMethod(MethodName, Params);
	if (!RequiresExclusiveEditorLane(Kind))
	{
		Admission.bCanExecute = true;
		return Admission;
	}

	const FString PayloadKey = BuildPayloadKey(MethodName, Params);
	const FString ClientRequestId = GetConcurrencyString(Params, TEXT("clientRequestId"));
	const FString RequestedOperationId = GetConcurrencyString(Params, TEXT("operationId"));
	FString Policy = GetConcurrencyString(Params, TEXT("policy"), TEXT("wait")).ToLower();
	if (Policy != TEXT("wait") && Policy != TEXT("enqueue") && Policy != TEXT("reject"))
	{
		Policy = TEXT("wait");
	}

	double MaxWaitMs = GetConcurrencyNumber(Params, TEXT("maxWaitMs"), DefaultWaitMs);
	if (IsInGameThread())
	{
		MaxWaitMs = 0.0;
	}
	const double DeadlineSeconds = FPlatformTime::Seconds() + FMath::Max(0.0, MaxWaitMs) / 1000.0;

	while (true)
	{
		{
			FScopeLock Lock(&GOperationMutex);
			RemoveExpiredQueuedOperationsLocked();

			if (!RequestedOperationId.IsEmpty())
			{
				const int32 QueueIndex = FindQueuedOperationIndexLocked(RequestedOperationId);
				if (QueueIndex == INDEX_NONE)
				{
					Admission.bCanExecute = false;
					Admission.Response = MakeRejectedResult(
						TEXT("operation_not_found"),
						FString::Printf(TEXT("Queued operationId '%s' was not found or has expired."), *RequestedOperationId));
					return Admission;
				}

				FQueuedOperation& Queued = GQueuedOperations[QueueIndex];
				if (Queued.MethodName != MethodName || Queued.PayloadKey != PayloadKey)
				{
					Admission.bCanExecute = false;
					Admission.Response = MakeRejectedResult(
						TEXT("operation_payload_mismatch"),
						TEXT("Queued operationId does not match this method or payload."));
					return Admission;
				}

				HeartbeatQueuedOperation(Queued);
				if (IsQueueHeadReadyLocked(QueueIndex))
				{
					const FString OperationId = Queued.OperationId;
					GQueuedOperations.RemoveAt(QueueIndex);
					Admission.bCanExecute = true;
					Admission.Lease = BeginOperationLocked(OperationId, MethodName, Kind);
					return Admission;
				}

				Admission.bCanExecute = false;
				Admission.Response = MakeQueuedResultLocked(Queued, QueueIndex + 1);
				return Admission;
			}

			if (!GActiveOperation.IsSet() && GQueuedOperations.IsEmpty())
			{
				Admission.bCanExecute = true;
				Admission.Lease = BeginOperationLocked(TEXT(""), MethodName, Kind);
				return Admission;
			}

			if (Policy == TEXT("enqueue"))
			{
				const int32 DuplicateIndex = FindDuplicateQueuedOperationLocked(MethodName, PayloadKey, ClientRequestId);
				if (DuplicateIndex != INDEX_NONE)
				{
					HeartbeatQueuedOperation(GQueuedOperations[DuplicateIndex]);
					Admission.bCanExecute = false;
					Admission.Response = MakeQueuedResultLocked(GQueuedOperations[DuplicateIndex], DuplicateIndex + 1);
					return Admission;
				}

				if (GQueuedOperations.Num() >= MaxGlobalQueueSize)
				{
					Admission.bCanExecute = false;
					Admission.Response = MakeRejectedResult(TEXT("operation_queue_full"), TEXT("Operation reservation queue is full."));
					return Admission;
				}

				if (!ClientRequestId.IsEmpty() && CountQueuedOperationsForClientLocked(ClientRequestId) >= MaxQueueSizePerClient)
				{
					Admission.bCanExecute = false;
					Admission.Response = MakeRejectedResult(TEXT("operation_client_queue_full"), TEXT("Client has too many queued operation reservations."));
					return Admission;
				}

				const int32 QueueIndex = AddQueuedOperationLocked(MethodName, Kind, PayloadKey, ClientRequestId);
				Admission.bCanExecute = false;
				Admission.Response = MakeQueuedResultLocked(GQueuedOperations[QueueIndex], QueueIndex + 1);
				return Admission;
			}

			if (Policy == TEXT("reject") || FPlatformTime::Seconds() >= DeadlineSeconds)
			{
				Admission.bCanExecute = false;
				Admission.Response = MakeBusyResultLocked(MethodName, TEXT("Another editor operation is active or queued. Retry after retryAfterMs, or use concurrency.policy='enqueue' to reserve a retry ticket."), true);
				return Admission;
			}
		}

		FPlatformProcess::Sleep(0.05f);
	}
}

TSharedPtr<FJsonValue> FMCPBridgeOperationCoordinator::BuildOperationStatusResult(const TSharedPtr<FJsonObject>& Params)
{
	const FString OperationId = GetConcurrencyString(Params, TEXT("operationId"));

	FScopeLock Lock(&GOperationMutex);
	RemoveExpiredQueuedOperationsLocked();

	if (OperationId.IsEmpty())
	{
		return MakeShared<FJsonValueObject>(BuildSnapshotObject());
	}

	if (GActiveOperation.IsSet() && GActiveOperation->OperationId == OperationId)
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("state"), TEXT("running"));
		Result->SetObjectField(TEXT("activeOperation"), MakeActiveOperationObjectLocked());
		return MakeShared<FJsonValueObject>(Result);
	}

	const int32 QueueIndex = FindQueuedOperationIndexLocked(OperationId);
	if (QueueIndex == INDEX_NONE)
	{
		return MakeRejectedResult(TEXT("operation_not_found"), FString::Printf(TEXT("Operation '%s' is not active or queued."), *OperationId));
	}

	HeartbeatQueuedOperation(GQueuedOperations[QueueIndex]);
	if (IsQueueHeadReadyLocked(QueueIndex))
	{
		return MakeReadyResultLocked(GQueuedOperations[QueueIndex]);
	}
	return MakeQueuedResultLocked(GQueuedOperations[QueueIndex], QueueIndex + 1);
}

TSharedPtr<FJsonValue> FMCPBridgeOperationCoordinator::BuildOperationWaitResult(const TSharedPtr<FJsonObject>& Params)
{
	if (IsInGameThread())
	{
		return BuildOperationStatusResult(Params);
	}

	const FString OperationId = GetConcurrencyString(Params, TEXT("operationId"));
	if (OperationId.IsEmpty())
	{
		return MakeRejectedResult(TEXT("operation_id_required"), TEXT("operationId is required."));
	}

	const double TimeoutMs = FMath::Clamp(GetConcurrencyNumber(Params, TEXT("timeoutMs"), DefaultWaitMs), 0.0, 30000.0);
	const double DeadlineSeconds = FPlatformTime::Seconds() + TimeoutMs / 1000.0;

	while (true)
	{
		{
			FScopeLock Lock(&GOperationMutex);
			RemoveExpiredQueuedOperationsLocked();

			if (GActiveOperation.IsSet() && GActiveOperation->OperationId == OperationId)
			{
				TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
				Result->SetBoolField(TEXT("success"), true);
				Result->SetStringField(TEXT("state"), TEXT("running"));
				Result->SetObjectField(TEXT("activeOperation"), MakeActiveOperationObjectLocked());
				return MakeShared<FJsonValueObject>(Result);
			}

			const int32 QueueIndex = FindQueuedOperationIndexLocked(OperationId);
			if (QueueIndex == INDEX_NONE)
			{
				return MakeRejectedResult(TEXT("operation_not_found"), FString::Printf(TEXT("Operation '%s' is not active or queued."), *OperationId));
			}

			HeartbeatQueuedOperation(GQueuedOperations[QueueIndex]);
			if (IsQueueHeadReadyLocked(QueueIndex))
			{
				return MakeReadyResultLocked(GQueuedOperations[QueueIndex]);
			}

			if (FPlatformTime::Seconds() >= DeadlineSeconds)
			{
				return MakeQueuedResultLocked(GQueuedOperations[QueueIndex], QueueIndex + 1);
			}
		}

		FPlatformProcess::Sleep(0.05f);
	}
}

TSharedPtr<FJsonValue> FMCPBridgeOperationCoordinator::BuildOperationCancelResult(const TSharedPtr<FJsonObject>& Params)
{
	const FString OperationId = GetConcurrencyString(Params, TEXT("operationId"));
	if (OperationId.IsEmpty())
	{
		return MakeRejectedResult(TEXT("operation_id_required"), TEXT("operationId is required."));
	}

	FScopeLock Lock(&GOperationMutex);
	RemoveExpiredQueuedOperationsLocked();

	const int32 QueueIndex = FindQueuedOperationIndexLocked(OperationId);
	if (QueueIndex != INDEX_NONE)
	{
		GQueuedOperations.RemoveAt(QueueIndex);
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("state"), TEXT("cancelled"));
		Result->SetStringField(TEXT("operationId"), OperationId);
		return MakeShared<FJsonValueObject>(Result);
	}

	if (GActiveOperation.IsSet() && GActiveOperation->OperationId == OperationId)
	{
		return MakeRejectedResult(TEXT("operation_running"), TEXT("Started editor operations cannot be cancelled safely."));
	}

	return MakeRejectedResult(TEXT("operation_not_found"), FString::Printf(TEXT("Operation '%s' is not active or queued."), *OperationId));
}

TSharedPtr<FJsonObject> FMCPBridgeOperationCoordinator::BuildSnapshotObject()
{
	FScopeLock Lock(&GOperationMutex);
	RemoveExpiredQueuedOperationsLocked();

	TSharedPtr<FJsonObject> Snapshot = MakeShared<FJsonObject>();
	Snapshot->SetBoolField(TEXT("hasActiveOperation"), GActiveOperation.IsSet());
	Snapshot->SetNumberField(TEXT("queueLength"), GQueuedOperations.Num());
	if (TSharedPtr<FJsonObject> Active = MakeActiveOperationObjectLocked())
	{
		Snapshot->SetObjectField(TEXT("activeOperation"), Active);
	}

	TArray<TSharedPtr<FJsonValue>> Queue;
	for (int32 Index = 0; Index < GQueuedOperations.Num(); ++Index)
	{
		const FQueuedOperation& Operation = GQueuedOperations[Index];
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("operationId"), Operation.OperationId);
		Entry->SetStringField(TEXT("method"), Operation.MethodName);
		Entry->SetStringField(TEXT("kind"), OperationKindToString(Operation.Kind));
		Entry->SetNumberField(TEXT("position"), Index + 1);
		Entry->SetStringField(TEXT("expiresAt"), Operation.ExpiresAtUtc.ToIso8601());
		Queue.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Snapshot->SetArrayField(TEXT("queue"), Queue);
	return Snapshot;
}

TSharedPtr<FJsonValue> FMCPBridgeOperationCoordinator::MakeBlockedResult(const FString& Code, const FString& Message, const FString& RequiredAction, double RetryAfterMs)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), false);
	Result->SetBoolField(TEXT("accepted"), false);
	Result->SetBoolField(TEXT("completed"), false);
	Result->SetStringField(TEXT("state"), TEXT("blocked"));
	Result->SetStringField(TEXT("code"), Code);
	Result->SetStringField(TEXT("error"), Message);
	Result->SetStringField(TEXT("requiredAction"), RequiredAction);
	if (RetryAfterMs > 0.0)
	{
		Result->SetNumberField(TEXT("retryAfterMs"), RetryAfterMs);
	}
	return MakeShared<FJsonValueObject>(Result);
}

EMCPBridgeOperationKind FMCPBridgeOperationCoordinator::ClassifyMethod(const FString& MethodName, const TSharedPtr<FJsonObject>& Params)
{
	if (MethodName == TEXT("coordination_get_status") ||
		MethodName == TEXT("coordination_operation_status") ||
		MethodName == TEXT("coordination_operation_wait") ||
		MethodName == TEXT("coordination_operation_cancel"))
	{
		return EMCPBridgeOperationKind::ReadOnly;
	}

	if (MethodName == TEXT("coordination_prepare_maintenance") ||
		MethodName == TEXT("coordination_live_coding_compile") ||
		MethodName == TEXT("coordination_request_exit") ||
		MethodName == TEXT("build_project") ||
		MethodName == TEXT("generate_project_files") ||
		MethodName == TEXT("hot_reload"))
	{
		return EMCPBridgeOperationKind::Maintenance;
	}

	if (MethodName == TEXT("coordination_save_dirty") ||
		MethodName == TEXT("save_dirty") ||
		MethodName == TEXT("save_asset") ||
		MethodName == TEXT("save_current_level") ||
		MethodName == TEXT("compile_blueprint"))
	{
		return EMCPBridgeOperationKind::AssetWrite;
	}

	if (MethodName == TEXT("pie_control"))
	{
		FString Action;
		if (Params.IsValid() && Params->TryGetStringField(TEXT("action"), Action) && Action.Equals(TEXT("status"), ESearchCase::IgnoreCase))
		{
			return EMCPBridgeOperationKind::ReadOnly;
		}
		return EMCPBridgeOperationKind::RuntimePIE;
	}

	if (MethodName == TEXT("execute_command") ||
		MethodName == TEXT("execute_python") ||
		MethodName == TEXT("run_python_file"))
	{
		return EMCPBridgeOperationKind::GlobalEditorWrite;
	}

	if (StartsWithAny(MethodName, { TEXT("get_"), TEXT("list_"), TEXT("search_"), TEXT("capture_"), TEXT("dump_"), TEXT("check_"), TEXT("find_") }) ||
		MethodName.StartsWith(TEXT("is_"), ESearchCase::IgnoreCase))
	{
		return EMCPBridgeOperationKind::ReadOnly;
	}

	if (ContainsAny(MethodName, {
		TEXT("save"),
		TEXT("create"),
		TEXT("delete"),
		TEXT("remove"),
		TEXT("add"),
		TEXT("set"),
		TEXT("move"),
		TEXT("reparent"),
		TEXT("apply"),
		TEXT("compile"),
		TEXT("build"),
		TEXT("spawn"),
		TEXT("open")
	}))
	{
		return EMCPBridgeOperationKind::AssetWrite;
	}

	return EMCPBridgeOperationKind::GlobalEditorWrite;
}

bool FMCPBridgeOperationCoordinator::IsCurrentThreadOperationAdmitted()
{
	return GCurrentThreadOperationAdmitted;
}

bool FMCPBridgeOperationCoordinator::SetCurrentThreadOperationAdmitted(bool bAdmitted)
{
	const bool bPrevious = GCurrentThreadOperationAdmitted;
	GCurrentThreadOperationAdmitted = bAdmitted;
	return bPrevious;
}

void FMCPBridgeOperationCoordinator::ReleaseOperation(const FString& OperationId)
{
	FScopeLock Lock(&GOperationMutex);
	if (GActiveOperation.IsSet() && GActiveOperation->OperationId == OperationId)
	{
		GActiveOperation.Reset();
	}
}
