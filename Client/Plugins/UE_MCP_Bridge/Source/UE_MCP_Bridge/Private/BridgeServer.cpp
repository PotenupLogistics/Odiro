#include "BridgeServer.h"
#include "EditorCoordination.h"
#include "MCPBridgeOperationCoordinator.h"
#include "UE_MCP_BridgeModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "Misc/Timespan.h"
#include "Async/Async.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Handlers/EditorHandlers.h"
#include "Handlers/AssetHandlers.h"
#include "Handlers/BlueprintHandlers.h"
#include "Handlers/ProjectHandlers.h"
#include "Handlers/LevelHandlers.h"
#include "Handlers/ReflectionHandlers.h"
#include "Handlers/GasHandlers.h"
#include "Handlers/GameplayHandlers.h"
#include "Handlers/DialogHandlers.h"
#include "Handlers/MaterialHandlers.h"
#include "Handlers/AnimationHandlers.h"
#include "Handlers/AudioHandlers.h"
#include "Handlers/WidgetHandlers.h"
#include "Handlers/FoliageHandlers.h"
#include "Handlers/LandscapeHandlers.h"
#include "Handlers/NetworkingHandlers.h"
#include "Handlers/NiagaraHandlers.h"
#include "Handlers/PCGHandlers.h"
#include "Handlers/SequencerHandlers.h"
#include "Handlers/SplineHandlers.h"
#include "Handlers/PhysicsHandlers.h"
#include "Handlers/DemoHandlers.h"
#include "Handlers/StateTreeHandlers.h"

// Platform-specific socket includes
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Windows/HideWindowsPlatformTypes.h"
#pragma comment(lib, "ws2_32.lib")
#elif PLATFORM_LINUX || PLATFORM_MAC
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#endif

#include "Misc/Base64.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <wincrypt.h>
#include "Windows/HideWindowsPlatformTypes.h"
#pragma comment(lib, "advapi32.lib")
#endif

namespace
{
	/** Tracks one accepted non-coordination request until it has really stopped executing. */
	class FBridgeActiveRequestLease : public TSharedFromThis<FBridgeActiveRequestLease>
	{
	public:
		/** Begin tracking an accepted bridge request. */
		void Begin()
		{
			FScopeLock Lock(&Mutex);
			if (!bTracking)
			{
				bTracking = true;
				FMCPBridgeCoordination::BeginActiveRequest();
			}
		}

		/** Mark that the request reached the game-thread handler body. */
		void MarkStarted()
		{
			FScopeLock Lock(&Mutex);
			bStarted = true;
		}

		/** Release the active request counter exactly once. */
		void Release()
		{
			bool bShouldRelease = false;
			{
				FScopeLock Lock(&Mutex);
				bShouldRelease = bTracking && !bReleased;
				if (bShouldRelease)
				{
					bReleased = true;
				}
			}

			if (bShouldRelease)
			{
				FMCPBridgeCoordination::EndActiveRequest();
			}
		}

		/** Release if the request timed out before the game-thread body started. */
		void ReleaseIfNeverStarted()
		{
			bool bShouldRelease = false;
			{
				FScopeLock Lock(&Mutex);
				bShouldRelease = bTracking && !bStarted && !bReleased;
				if (bShouldRelease)
				{
					bReleased = true;
				}
			}

			if (bShouldRelease)
			{
				FMCPBridgeCoordination::EndActiveRequest();
			}
		}

	private:
		FCriticalSection Mutex;
		bool bTracking = false;
		bool bStarted = false;
		bool bReleased = false;
	};

	/** Return true for legacy handlers that are safe during reload maintenance. */
	bool IsMaintenanceAllowedLegacyMethod(const FString& MethodName, const TSharedPtr<FJsonObject>& Params)
	{
		if (MethodName != TEXT("pie_control"))
		{
			return false;
		}

		FString Action;
		return Params.IsValid() &&
			Params->TryGetStringField(TEXT("action"), Action) &&
			(Action.Equals(TEXT("status"), ESearchCase::IgnoreCase) ||
				Action.Equals(TEXT("stop"), ESearchCase::IgnoreCase));
	}

	/** Return the exclusive byte offset after the HTTP header terminator. */
	int32 FindHttpHeaderEnd(const TArray<uint8>& Bytes)
	{
		for (int32 Index = 3; Index < Bytes.Num(); ++Index)
		{
			if (Bytes[Index - 3] == '\r' &&
				Bytes[Index - 2] == '\n' &&
				Bytes[Index - 1] == '\r' &&
				Bytes[Index] == '\n')
			{
				return Index + 1;
			}
		}
		return INDEX_NONE;
	}

	/** Return whether an asset path appears to target a Widget Blueprint or widget asset. */
	bool IsLikelyWidgetAssetPath(const FString& AssetPath)
	{
		const FString AssetName = FPaths::GetBaseFilename(AssetPath);
		return AssetPath.Contains(TEXT("/Widgets/"), ESearchCase::IgnoreCase) ||
			AssetPath.Contains(TEXT("/Widget/"), ESearchCase::IgnoreCase) ||
			AssetName.StartsWith(TEXT("WBP_"), ESearchCase::IgnoreCase);
	}

	/** Return whether params identify a Widget Blueprint or widget asset path. */
	bool ParamsReferenceWidgetAsset(const TSharedPtr<FJsonObject>& Params)
	{
		if (!Params.IsValid())
		{
			return false;
		}

		static const TCHAR* PathFields[] = {
			TEXT("assetPath"),
			TEXT("path"),
			TEXT("widgetBlueprintPath"),
			TEXT("widget_blueprint_path"),
			TEXT("blueprintPath"),
			TEXT("blueprint_path")
		};

		for (const TCHAR* Field : PathFields)
		{
			FString AssetPath;
			if (Params->TryGetStringField(Field, AssetPath) && IsLikelyWidgetAssetPath(AssetPath))
			{
				return true;
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* AssetPaths = nullptr;
		if (Params->TryGetArrayField(TEXT("assetPaths"), AssetPaths) && AssetPaths)
		{
			for (const TSharedPtr<FJsonValue>& Value : *AssetPaths)
			{
				if (Value.IsValid() && Value->Type == EJson::String && IsLikelyWidgetAssetPath(Value->AsString()))
				{
					return true;
				}
			}
		}

		return false;
	}

	/** Return whether the MCP method may mutate UMG/WBP editor state. */
	bool IsWidgetEditorMutationMethod(const FString& MethodName, const TSharedPtr<FJsonObject>& Params)
	{
		static const TCHAR* Methods[] = {
			TEXT("create_widget_blueprint"),
			TEXT("apply_widget_tree_spec"),
			TEXT("create_editor_utility_widget"),
			TEXT("create_editor_utility_blueprint"),
			TEXT("clear_widget_binding"),
			TEXT("set_widget_property"),
			TEXT("ensure_widget_render_opacity_animations"),
			TEXT("run_editor_utility_widget"),
			TEXT("run_editor_utility_blueprint"),
			TEXT("add_widget"),
			TEXT("replace_widget_classes"),
			TEXT("set_named_slot_content"),
			TEXT("remove_widget"),
			TEXT("rename_widget"),
			TEXT("move_widget"),
			TEXT("wrap_widget"),
			TEXT("unwrap_widget"),
			TEXT("repair_widget_blueprint"),
			TEXT("set_root_widget"),
			TEXT("wrap_root_widget"),
			TEXT("create_widget"),
			TEXT("set_widget_properties"),
			TEXT("delete_widget"),
			TEXT("reparent_widget"),
			TEXT("apply_layout"),
			TEXT("apply_json_to_umg"),
			TEXT("apply_html_to_umg"),
			TEXT("create_animation"),
			TEXT("delete_animation"),
			TEXT("set_property_keys"),
			TEXT("remove_property_track"),
			TEXT("remove_keys"),
			TEXT("animation_append_widget_tracks"),
			TEXT("animation_append_time_slice"),
			TEXT("animation_delete_widget_keys")
		};

		for (const TCHAR* Method : Methods)
		{
			if (MethodName == Method)
			{
				return true;
			}
		}

		if (MethodName == TEXT("open_asset") ||
			MethodName == TEXT("save_asset") ||
			MethodName == TEXT("compile_blueprint") ||
			MethodName == TEXT("compile_blueprints"))
		{
			return ParamsReferenceWidgetAsset(Params);
		}

		return false;
	}

	/** Block UMG/WBP mutations while PIE owns a world so transient widget previews are not captured by editor transactions. */
	TSharedPtr<FJsonValue> MakeWidgetMutationBlockedDuringPieResult(const FString& MethodName)
	{
		const FString PlayWorld = (GEditor && GEditor->PlayWorld)
			? GEditor->PlayWorld->GetPathName()
			: TEXT("<unknown PIE world>");
		UE_LOG(LogMCPBridge, Error, TEXT("[UE-MCP] Refusing %s while PIE is active: %s"), *MethodName, *PlayWorld);
		return FMCPBridgeOperationCoordinator::MakeBlockedResult(
			TEXT("active_pie_blocks_widget_editor_mutation"),
			FString::Printf(TEXT("Refusing to run %s while PIE is active: %s. Stop PIE and wait for world teardown before mutating Widget Blueprint/editor state."), *MethodName, *PlayWorld),
			TEXT("pie_control.stop"),
			1000.0);
	}
}

FMCPBridgeServer::FMCPBridgeServer(int32 Port)
	: ServerPort(Port)
	, ServerThread(nullptr)
	, bShouldStop(false)
	, bIsRunning(false)
	, ServerSocket(nullptr)
{
	// Register core handlers
	FEditorHandlers::RegisterHandlers(HandlerRegistry);
	FAssetHandlers::RegisterHandlers(HandlerRegistry);
	FBlueprintHandlers::RegisterHandlers(HandlerRegistry);
	FLevelHandlers::RegisterHandlers(HandlerRegistry);
	FReflectionHandlers::RegisterHandlers(HandlerRegistry);
	FGasHandlers::RegisterHandlers(HandlerRegistry);
	FGameplayHandlers::RegisterHandlers(HandlerRegistry);
	FDialogHandlers::RegisterHandlers(HandlerRegistry);
	FMaterialHandlers::RegisterHandlers(HandlerRegistry);
	FAnimationHandlers::RegisterHandlers(HandlerRegistry);
	FAudioHandlers::RegisterHandlers(HandlerRegistry);
	FWidgetHandlers::RegisterHandlers(HandlerRegistry);
	FFoliageHandlers::RegisterHandlers(HandlerRegistry);
	FLandscapeHandlers::RegisterHandlers(HandlerRegistry);
	FNetworkingHandlers::RegisterHandlers(HandlerRegistry);
	FNiagaraHandlers::RegisterHandlers(HandlerRegistry);
	FPCGHandlers::RegisterHandlers(HandlerRegistry);
	FSequencerHandlers::RegisterHandlers(HandlerRegistry);
	FSplineHandlers::RegisterHandlers(HandlerRegistry);
	FPhysicsHandlers::RegisterHandlers(HandlerRegistry);
	FDemoHandlers::RegisterHandlers(HandlerRegistry);
	FProjectHandlers::RegisterHandlers(HandlerRegistry);
	FStateTreeHandlers::RegisterHandlers(HandlerRegistry);
}

FMCPBridgeServer::~FMCPBridgeServer()
{
	Shutdown();
}

bool FMCPBridgeServer::Start()
{
	if (bIsRunning)
	{
		return false;
	}

	bShouldStop = false;
	ServerThread = FRunnableThread::Create(this, TEXT("MCPBridgeServer"), 0, TPri_Normal);
	return ServerThread != nullptr;
}

void FMCPBridgeServer::Shutdown()
{
	if (!bIsRunning)
	{
		return;
	}

	bShouldStop = true;

	if (ServerThread)
	{
		ServerThread->WaitForCompletion();
		delete ServerThread;
		ServerThread = nullptr;
	}

	bIsRunning = false;
}

bool FMCPBridgeServer::Init()
{
	bIsRunning = true;
	return true;
}

uint32 FMCPBridgeServer::Run()
{
	UE_LOG(LogMCPBridge, Log, TEXT("[UE-MCP] Bridge server thread started on port %d"), ServerPort);

	// Initialize platform sockets
#if PLATFORM_WINDOWS
	WSADATA WsaData;
	if (WSAStartup(MAKEWORD(2, 2), &WsaData) != 0)
	{
		UE_LOG(LogMCPBridge, Error, TEXT("[UE-MCP] Failed to initialize Winsock"));
		return 1;
	}
#endif

	// Create server socket
#if PLATFORM_WINDOWS
	SOCKET ServerSocketFD = socket(AF_INET, SOCK_STREAM, 0);
	if (ServerSocketFD == INVALID_SOCKET)
#else
	int32 ServerSocketFD = socket(AF_INET, SOCK_STREAM, 0);
	if (ServerSocketFD < 0)
#endif
	{
		UE_LOG(LogMCPBridge, Error, TEXT("[UE-MCP] Failed to create socket"));
#if PLATFORM_WINDOWS
		WSACleanup();
#endif
		return 1;
	}

	// Set socket options
	int32 ReuseAddr = 1;
	setsockopt(ServerSocketFD, SOL_SOCKET, SO_REUSEADDR, (char*)&ReuseAddr, sizeof(ReuseAddr));

	// Set TCP_NODELAY for immediate send (disable Nagle's algorithm)
	int32 NoDelay = 1;
	setsockopt(ServerSocketFD, IPPROTO_TCP, TCP_NODELAY, (char*)&NoDelay, sizeof(NoDelay));

	// Bind socket to loopback only. The bridge has no authentication on the
	// WebSocket upgrade, so binding to 0.0.0.0 (INADDR_ANY) would expose every
	// editor-side handler (including execute_python) to any client on the LAN.
	//
	// #492: when more than one editor is open locally, the default port is
	// already taken. Walk up to ServerPort+kMaxPortProbe so a second editor
	// can boot side-by-side; the actual bound port is published via a per-
	// project lockfile (see WritePortLockfile below).
	const int32 RequestedPort = ServerPort;
	constexpr int32 kMaxPortProbe = 50;
	int32 BoundPort = 0;
	bool bBound = false;
	for (int32 Offset = 0; Offset <= kMaxPortProbe; ++Offset)
	{
		sockaddr_in ServerAddr;
		FMemory::Memset(&ServerAddr, 0, sizeof(ServerAddr));
		ServerAddr.sin_family = AF_INET;
		ServerAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		ServerAddr.sin_port = htons((uint16)(RequestedPort + Offset));

		if (bind(ServerSocketFD, (sockaddr*)&ServerAddr, sizeof(ServerAddr)) == 0)
		{
			BoundPort = RequestedPort + Offset;
			ServerPort = BoundPort;
			bBound = true;
			if (Offset > 0)
			{
				UE_LOG(LogMCPBridge, Log, TEXT("[UE-MCP] Default port %d in use; bound to %d instead (#492)"), RequestedPort, BoundPort);
			}
			break;
		}
	}
	if (!bBound)
	{
		int32 ErrorCode = 0;
#if PLATFORM_WINDOWS
		ErrorCode = WSAGetLastError();
		UE_LOG(LogMCPBridge, Error, TEXT("[UE-MCP] Failed to bind to any port in [%d, %d], last error: %d"), RequestedPort, RequestedPort + kMaxPortProbe, ErrorCode);
		closesocket(ServerSocketFD);
		WSACleanup();
#else
		UE_LOG(LogMCPBridge, Error, TEXT("[UE-MCP] Failed to bind to any port in [%d, %d]"), RequestedPort, RequestedPort + kMaxPortProbe);
		close(ServerSocketFD);
#endif
		return 1;
	}

	// Listen
	if (listen(ServerSocketFD, 5) < 0)
	{
		int32 ErrorCode = 0;
#if PLATFORM_WINDOWS
		ErrorCode = WSAGetLastError();
		UE_LOG(LogMCPBridge, Error, TEXT("[UE-MCP] Failed to listen on socket, error: %d"), ErrorCode);
		closesocket(ServerSocketFD);
		WSACleanup();
#else
		UE_LOG(LogMCPBridge, Error, TEXT("[UE-MCP] Failed to listen on socket"));
		close(ServerSocketFD);
#endif
		return 1;
	}

	UE_LOG(LogMCPBridge, Log, TEXT("[UE-MCP] Bridge listening on ws://127.0.0.1:%d (loopback only)"), ServerPort);
	bIsRunning = true;

	// #492: publish the bound port to <Project>/Saved/UE_MCP_Bridge/port.json
	// so the npm client (which was started against this project's .uproject)
	// can find us even when the default port was already taken by another editor.
	WritePortLockfile(ServerPort);

	// Accept connections
	while (!bShouldStop)
	{
		fd_set ReadSet;
		FD_ZERO(&ReadSet);
		FD_SET(ServerSocketFD, &ReadSet);

		timeval Timeout;
		Timeout.tv_sec = 1;
		Timeout.tv_usec = 0;

		int32 SelectResult = select(ServerSocketFD + 1, &ReadSet, nullptr, nullptr, &Timeout);
#if PLATFORM_WINDOWS
		if (SelectResult > 0 && FD_ISSET(ServerSocketFD, &ReadSet))
#else
		if (SelectResult > 0 && FD_ISSET(ServerSocketFD, &ReadSet))
#endif
		{
			sockaddr_in ClientAddr;
			socklen_t ClientAddrLen = sizeof(ClientAddr);
#if PLATFORM_WINDOWS
			SOCKET ClientSocketFD = accept(ServerSocketFD, (sockaddr*)&ClientAddr, &ClientAddrLen);
			if (ClientSocketFD != INVALID_SOCKET)
			{
#else
			int32 ClientSocketFD = accept(ServerSocketFD, (sockaddr*)&ClientAddr, &ClientAddrLen);
			if (ClientSocketFD >= 0)
			{
#endif
			char AddrStr[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &ClientAddr.sin_addr, AddrStr, INET_ADDRSTRLEN);
			UE_LOG(LogMCPBridge, Log, TEXT("[UE-MCP] Client connected from %s:%d"),
				ANSI_TO_TCHAR(AddrStr), ntohs(ClientAddr.sin_port));

				// Handle each WebSocket connection in its own thread
				Async(EAsyncExecution::Thread, [this, ClientSocketFD]() {
					HandleWebSocketConnection(ClientSocketFD);
				});
			}
		}
	}

	// Cleanup
#if PLATFORM_WINDOWS
	closesocket(ServerSocketFD);
	WSACleanup();
#else
	close(ServerSocketFD);
#endif

	bIsRunning = false;
	return 0;
}

void FMCPBridgeServer::Stop()
{
	bShouldStop = true;
}

void FMCPBridgeServer::Exit()
{
	bIsRunning = false;
	// #492: remove the lockfile on graceful shutdown so the next editor boot
	// doesn't see a stale entry. A hard-crash leaves the file, but the next
	// startup overwrites it with the live PID.
	DeletePortLockfile();
}

// #492: per-project port lockfile. Multiple editors can run side-by-side as
// long as each one's npm client can find the right bridge. Publishing the
// bound port in <Project>/Saved/UE_MCP_Bridge/port.json (resolved from the
// .uproject path the client was given) is the cheapest way to do that.
FString FMCPBridgeServer::GetPortLockfilePath()
{
	const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UE_MCP_Bridge"));
	return FPaths::Combine(Dir, TEXT("port.json"));
}

void FMCPBridgeServer::WritePortLockfile(int32 PortValue)
{
	const FString FilePath = GetPortLockfilePath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), /*Tree*/ true);

	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetNumberField(TEXT("port"), PortValue);
	Obj->SetNumberField(TEXT("pid"), (double)FPlatformProcess::GetCurrentProcessId());
	Obj->SetStringField(TEXT("startedAt"), FDateTime::UtcNow().ToIso8601());
	Obj->SetNumberField(TEXT("apiVersion"), 1.0);

	FString Serialized;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);

	if (!FFileHelper::SaveStringToFile(Serialized, *FilePath))
	{
		UE_LOG(LogMCPBridge, Warning, TEXT("[UE-MCP] Failed to write port lockfile: %s"), *FilePath);
		return;
	}
	UE_LOG(LogMCPBridge, Log, TEXT("[UE-MCP] Port lockfile published: %s (port=%d)"), *FilePath, PortValue);
}

void FMCPBridgeServer::DeletePortLockfile()
{
	const FString FilePath = GetPortLockfilePath();
	if (!FPaths::FileExists(FilePath)) return;
	if (IFileManager::Get().Delete(*FilePath))
	{
		UE_LOG(LogMCPBridge, Log, TEXT("[UE-MCP] Port lockfile removed: %s"), *FilePath);
	}
}

TSharedPtr<FJsonObject> FMCPBridgeServer::ParseJsonRpcRequest(const FString& Message)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);

	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		return JsonObject;
	}

	return nullptr;
}

FString FMCPBridgeServer::CreateJsonRpcResponse(const TSharedPtr<FJsonObject>& Request, const TSharedPtr<FJsonValue>& Result)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));

	if (Request.IsValid() && Request->HasField(TEXT("id")))
	{
		Response->SetField(TEXT("id"), Request->TryGetField(TEXT("id")));
	}

	Response->SetField(TEXT("result"), Result);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
	return OutputString;
}

FString FMCPBridgeServer::CreateJsonRpcError(const TSharedPtr<FJsonObject>& Request, int32 ErrorCode, const FString& ErrorMessage)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));

	if (Request.IsValid() && Request->HasField(TEXT("id")))
	{
		Response->SetField(TEXT("id"), Request->TryGetField(TEXT("id")));
	}
	else
	{
		Response->SetField(TEXT("id"), MakeShared<FJsonValueNull>());
	}

	TSharedPtr<FJsonObject> ErrorObject = MakeShared<FJsonObject>();
	ErrorObject->SetNumberField(TEXT("code"), ErrorCode);
	ErrorObject->SetStringField(TEXT("message"), ErrorMessage);
	Response->SetObjectField(TEXT("error"), ErrorObject);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
	return OutputString;
}

FString FMCPBridgeServer::ProcessMessage(const FString& Message)
{
	TSharedPtr<FJsonObject> Request = ParseJsonRpcRequest(Message);
	if (!Request.IsValid())
	{
		return CreateJsonRpcError(nullptr, -32700, TEXT("Parse error"));
	}

	FString Method;
	if (!Request->TryGetStringField(TEXT("method"), Method))
	{
		return CreateJsonRpcError(Request, -32600, TEXT("Invalid Request"));
	}

	UE_LOG(LogMCPBridge, Log, TEXT("[UE-MCP] Processing method: %s"), *Method);
	TSharedPtr<FJsonObject> Params;
	if (Request->HasField(TEXT("params")))
	{
		TSharedPtr<FJsonValue> ParamsValue = Request->TryGetField(TEXT("params"));
		if (ParamsValue.IsValid() && ParamsValue->Type == EJson::Object)
		{
			Params = ParamsValue->AsObject();
		}
		else
		{
			Params = MakeShared<FJsonObject>();
		}
	}
	else
	{
		Params = MakeShared<FJsonObject>();
	}

	const bool bCoordinationMethod = FMCPBridgeCoordination::IsCoordinationMethod(Method);
	if (!bCoordinationMethod && !IsMaintenanceAllowedLegacyMethod(Method, Params) && FMCPBridgeCoordination::IsMaintenanceActive())
	{
		return CreateJsonRpcResponse(Request, FMCPBridgeCoordination::MakeMaintenancePendingResult(Method));
	}

	if (!HandlerRegistry.HasHandler(Method))
	{
		FString Detail = FString::Printf(TEXT("Unknown method: %s"), *Method);
		const TArray<FString> All = HandlerRegistry.GetHandlerNames();
		TArray<FString> Hints;
		for (const FString& Name : All)
		{
			if (Name.Contains(Method, ESearchCase::IgnoreCase) || Method.Contains(Name, ESearchCase::IgnoreCase))
			{
				Hints.Add(Name);
				if (Hints.Num() >= 5) break;
			}
		}
		if (Hints.Num() == 0 && !All.IsEmpty())
		{
			Detail += FString::Printf(TEXT(" (no near-matches in %d registered handlers - the deployed plugin may be behind the TS schema; try a clean rebuild + redeploy)."), All.Num());
		}
		else if (Hints.Num() > 0)
		{
			Detail += FString::Printf(TEXT(" (did you mean: %s)"), *FString::Join(Hints, TEXT(", ")));
		}
		return CreateJsonRpcError(Request, -32601, Detail);
	}

	FMCPBridgeOperationAdmission Admission = FMCPBridgeOperationCoordinator::TryBeginOrReserve(Method, Params);
	if (!Admission.bCanExecute)
	{
		return CreateJsonRpcResponse(Request, Admission.Response);
	}

	TSharedPtr<FMCPBridgeOperationLease> OperationLease = Admission.Lease;
	TSharedRef<FBridgeActiveRequestLease> ActiveLease = MakeShared<FBridgeActiveRequestLease>();
	if (!bCoordinationMethod)
	{
		ActiveLease->Begin();
	}

	// Execute handler on game thread
	FMCPHandlerRegistry::FHandlerFunction Handler = [this, Method, ActiveLease, OperationLease](const TSharedPtr<FJsonObject>& HandlerParams) -> TSharedPtr<FJsonValue>
	{
		FDialogHandlers::FScopedAutomationDialogPolicy DialogPolicyScope;
		ActiveLease->MarkStarted();
		if (OperationLease.IsValid())
		{
			OperationLease->MarkStarted();
		}

		auto ReleaseLeases = [&ActiveLease, &OperationLease]()
		{
			if (OperationLease.IsValid())
			{
				OperationLease->Release();
			}
			ActiveLease->Release();
		};

		if (GEditor && GEditor->PlayWorld && IsWidgetEditorMutationMethod(Method, HandlerParams))
		{
			TSharedPtr<FJsonValue> BlockedResult = MakeWidgetMutationBlockedDuringPieResult(Method);
			ReleaseLeases();
			return BlockedResult;
		}

		FMCPBridgeOperationAdmittedScope AdmittedScope;
		TSharedPtr<FJsonValue> HandlerResult = HandlerRegistry.ExecuteHandler(Method, HandlerParams);
		ReleaseLeases();
		return HandlerResult;
	};

	// Some handlers (create_cpp_class regenerates IDE project files;
	// long-running compiles) legitimately need minutes. Honor per-handler
	// timeouts registered via FMCPHandlerRegistry::RegisterHandlerWithTimeout.
	const float PerHandlerTimeout = HandlerRegistry.GetHandlerTimeout(Method);
	TSharedPtr<FJsonValue> Result = (PerHandlerTimeout > 0.0f)
		? GameThreadExecutor.ExecuteOnGameThread(Handler, Params, PerHandlerTimeout)
		: GameThreadExecutor.ExecuteOnGameThread(Handler, Params);
	ActiveLease->ReleaseIfNeverStarted();
	if (OperationLease.IsValid())
	{
		OperationLease->ReleaseIfNeverStarted();
	}

	if (Result.IsValid())
	{
		return CreateJsonRpcResponse(Request, Result);
	}
	else
	{
		return CreateJsonRpcError(Request, -32603, FString::Printf(TEXT("Handler returned no result: %s"), *Method));
	}
}

#if PLATFORM_WINDOWS
void FMCPBridgeServer::HandleWebSocketConnection(SOCKET ClientSocketFD)
#else
void FMCPBridgeServer::HandleWebSocketConnection(int32 ClientSocketFD)
#endif
{
	// Set TCP_NODELAY on client socket for immediate send
	int32 NoDelay = 1;
	setsockopt(ClientSocketFD, IPPROTO_TCP, TCP_NODELAY, (char*)&NoDelay, sizeof(NoDelay));

	// Perform WebSocket handshake
	TArray<uint8> InitialFrameBytes;
	FString Response = PerformWebSocketHandshake(ClientSocketFD, InitialFrameBytes);
	if (Response.IsEmpty())
	{
#if PLATFORM_WINDOWS
		closesocket(ClientSocketFD);
#else
		close(ClientSocketFD);
#endif
		return;
	}

	// Send handshake response
	// HTTP headers are ASCII, FString uses TCHAR (which is wchar_t on Windows)
	// Convert to UTF-8 bytes for network transmission
	FTCHARToUTF8 UTF8Response(*Response);
	const char* ResponseBytes = (const char*)UTF8Response.Get();
	int32 TotalBytes = UTF8Response.Length();

	// Send response - ensure all bytes are sent
	int32 SentBytes = 0;
	while (SentBytes < TotalBytes)
	{
		int32 BytesSent = send(ClientSocketFD, ResponseBytes + SentBytes, TotalBytes - SentBytes, 0);
		if (BytesSent < 0)
		{
			int32 ErrorCode = 0;
#if PLATFORM_WINDOWS
			ErrorCode = WSAGetLastError();
			UE_LOG(LogMCPBridge, Error, TEXT("[UE-MCP] Failed to send WebSocket handshake response, error: %d"), ErrorCode);
			closesocket(ClientSocketFD);
#else
			UE_LOG(LogMCPBridge, Error, TEXT("[UE-MCP] Failed to send WebSocket handshake response"));
			close(ClientSocketFD);
#endif
			return;
		}
		SentBytes += BytesSent;
	}

	UE_LOG(LogMCPBridge, Log, TEXT("[UE-MCP] Sent WebSocket handshake response (%d/%d bytes)"), SentBytes, TotalBytes);

	// Small delay to ensure response is fully sent and received by client
	FPlatformProcess::Sleep(0.01f); // 10ms

	// Process WebSocket messages
	UE_LOG(LogMCPBridge, Log, TEXT("[UE-MCP] Starting WebSocket message processing"));
	ProcessWebSocketMessages(ClientSocketFD, MoveTemp(InitialFrameBytes));
	UE_LOG(LogMCPBridge, Log, TEXT("[UE-MCP] WebSocket message processing ended"));

#if PLATFORM_WINDOWS
	closesocket(ClientSocketFD);
#else
	close(ClientSocketFD);
#endif
}

#if PLATFORM_WINDOWS
FString FMCPBridgeServer::PerformWebSocketHandshake(SOCKET ClientSocketFD, TArray<uint8>& OutInitialFrameBytes)
#else
FString FMCPBridgeServer::PerformWebSocketHandshake(int32 ClientSocketFD, TArray<uint8>& OutInitialFrameBytes)
#endif
{
	FString Request = ReadHttpRequest(ClientSocketFD, OutInitialFrameBytes);
	if (Request.IsEmpty())
	{
		return TEXT("");
	}

	// Reject browser-originated upgrades from any origin other than loopback.
	// Browsers always send an Origin header on WebSocket upgrades, so a present
	// Origin that isn't loopback is a cross-site websocket hijacking attempt
	// (a malicious page on the developer's machine reaching the editor bridge).
	// Native clients (Node ws, curl) omit Origin and are allowed.
	{
		int32 OriginStart = Request.Find(TEXT("Origin:"), ESearchCase::IgnoreCase);
		if (OriginStart != INDEX_NONE)
		{
			int32 ValueStart = OriginStart + 7; // strlen("Origin:")
			while (ValueStart < Request.Len() && (Request[ValueStart] == TEXT(' ') || Request[ValueStart] == TEXT('\t')))
			{
				ValueStart++;
			}
			int32 ValueEnd = Request.Find(TEXT("\r\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueStart);
			FString Origin = (ValueEnd == INDEX_NONE)
				? Request.Mid(ValueStart).TrimStartAndEnd()
				: Request.Mid(ValueStart, ValueEnd - ValueStart).TrimStartAndEnd();

			const bool bIsLoopback =
				Origin.StartsWith(TEXT("http://localhost"), ESearchCase::IgnoreCase) ||
				Origin.StartsWith(TEXT("https://localhost"), ESearchCase::IgnoreCase) ||
				Origin.StartsWith(TEXT("http://127.0.0.1"), ESearchCase::IgnoreCase) ||
				Origin.StartsWith(TEXT("https://127.0.0.1"), ESearchCase::IgnoreCase) ||
				Origin.StartsWith(TEXT("http://[::1]"), ESearchCase::IgnoreCase) ||
				Origin.StartsWith(TEXT("https://[::1]"), ESearchCase::IgnoreCase);

			if (!bIsLoopback)
			{
				UE_LOG(LogMCPBridge, Warning, TEXT("[UE-MCP] Rejected WebSocket upgrade from Origin: %s"), *Origin);
				return TEXT("");
			}
		}
	}

	// Extract WebSocket-Key from request (case-insensitive search)
	FString WebSocketKey;
	int32 KeyStart = Request.Find(TEXT("Sec-WebSocket-Key:"), ESearchCase::IgnoreCase);
	if (KeyStart != INDEX_NONE)
	{
		// Skip past the header name and any whitespace
		int32 ValueStart = KeyStart + 18; // Length of "Sec-WebSocket-Key:"
		while (ValueStart < Request.Len() && (Request[ValueStart] == TEXT(' ') || Request[ValueStart] == TEXT('\t')))
		{
			ValueStart++;
		}
		int32 KeyEnd = Request.Find(TEXT("\r\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueStart);
		if (KeyEnd != INDEX_NONE)
		{
			WebSocketKey = Request.Mid(ValueStart, KeyEnd - ValueStart).TrimStartAndEnd();
		}
	}

	UE_LOG(LogMCPBridge, Log, TEXT("[UE-MCP] Extracted WebSocket-Key: %s"), *WebSocketKey);

	if (WebSocketKey.IsEmpty())
	{
		return TEXT("");
	}

	// Create accept key
	FString AcceptKey = CreateWebSocketAcceptKey(WebSocketKey);

	// Build response (WebSocket spec requires exact format)
	// Must be: HTTP/1.1 101 Switching Protocols\r\n
	//          Upgrade: websocket\r\n
	//          Connection: Upgrade\r\n
	//          Sec-WebSocket-Accept: <key>\r\n
	//          \r\n
	FString Response = TEXT("HTTP/1.1 101 Switching Protocols\r\n");
	Response += TEXT("Upgrade: websocket\r\n");
	Response += TEXT("Connection: Upgrade\r\n");
	Response += FString::Printf(TEXT("Sec-WebSocket-Accept: %s\r\n"), *AcceptKey);
	Response += TEXT("\r\n");

	UE_LOG(LogMCPBridge, Log, TEXT("[UE-MCP] Accept key: %s"), *AcceptKey);
	UE_LOG(LogMCPBridge, Log, TEXT("[UE-MCP] Response length: %d chars"), Response.Len());

	return Response;
}

#if PLATFORM_WINDOWS
FString FMCPBridgeServer::ReadHttpRequest(SOCKET SocketFD, TArray<uint8>& OutInitialFrameBytes)
#else
FString FMCPBridgeServer::ReadHttpRequest(int32 SocketFD, TArray<uint8>& OutInitialFrameBytes)
#endif
{
	// Read HTTP request headers (until \r\n\r\n)
	OutInitialFrameBytes.Reset();
	TArray<uint8> RequestBytes;
	RequestBytes.Reserve(4096);
	TArray<uint8> Buffer;
	Buffer.SetNum(4096);

	constexpr int32 MaxHeaderBytes = 65536;
	while (RequestBytes.Num() < MaxHeaderBytes)
	{
		// Use select to wait for data with timeout
		fd_set ReadSet;
		FD_ZERO(&ReadSet);
		FD_SET(SocketFD, &ReadSet);

		timeval Timeout;
		Timeout.tv_sec = 5; // 5 second timeout
		Timeout.tv_usec = 0;

		int32 SelectResult = select(SocketFD + 1, &ReadSet, nullptr, nullptr, &Timeout);
		if (SelectResult <= 0 || !FD_ISSET(SocketFD, &ReadSet))
		{
			UE_LOG(LogMCPBridge, Warning, TEXT("[UE-MCP] Timeout waiting for HTTP request"));
			return TEXT("");
		}

		int32 BytesReceived = recv(SocketFD, (char*)Buffer.GetData(), Buffer.Num(), 0);
		if (BytesReceived <= 0)
		{
			UE_LOG(LogMCPBridge, Warning, TEXT("[UE-MCP] Failed to read HTTP request"));
			return TEXT("");
		}

		RequestBytes.Append(Buffer.GetData(), BytesReceived);
		const int32 HeaderEnd = FindHttpHeaderEnd(RequestBytes);
		if (HeaderEnd == INDEX_NONE)
		{
			continue;
		}

		if (RequestBytes.Num() > HeaderEnd)
		{
			OutInitialFrameBytes.Append(RequestBytes.GetData() + HeaderEnd, RequestBytes.Num() - HeaderEnd);
		}

		FUTF8ToTCHAR RequestText(reinterpret_cast<const ANSICHAR*>(RequestBytes.GetData()), HeaderEnd);
		FString Request(RequestText.Length(), RequestText.Get());
		UE_LOG(LogMCPBridge, Log, TEXT("[UE-MCP] Read HTTP request (%d header bytes, %d initial frame bytes):\n%s"), HeaderEnd, OutInitialFrameBytes.Num(), *Request.Left(200));
		return Request;
	}

	UE_LOG(LogMCPBridge, Warning, TEXT("[UE-MCP] HTTP request header exceeded %d bytes"), MaxHeaderBytes);
	return TEXT("");
}

FString FMCPBridgeServer::CreateWebSocketAcceptKey(const FString& ClientKey)
{
	// WebSocket accept key = base64(sha1(client_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"))
	FString MagicString = TEXT("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
	FString Combined = ClientKey + MagicString;

	// Compute SHA1 hash (20 bytes)
	FTCHARToUTF8 UTF8String(*Combined);
	uint8 HashBytes[20];

#if PLATFORM_WINDOWS
	HCRYPTPROV hProv = 0;
	HCRYPTHASH hHash = 0;
	if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
	{
		if (CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash))
		{
			CryptHashData(hHash, (BYTE*)UTF8String.Get(), UTF8String.Length(), 0);
			DWORD HashLen = 20;
			CryptGetHashParam(hHash, HP_HASHVAL, HashBytes, &HashLen, 0);
			CryptDestroyHash(hHash);
		}
		CryptReleaseContext(hProv, 0);
	}
#else
	// UE's cross-platform SHA1
	FSHA1 Sha1;
	Sha1.Update((const uint8*)UTF8String.Get(), UTF8String.Length());
	Sha1.Final();
	Sha1.GetHash(HashBytes);
#endif

	// Base64 encode
	FString AcceptKey = FBase64::Encode(HashBytes, 20);
	return AcceptKey;
}

#if PLATFORM_WINDOWS
void FMCPBridgeServer::ProcessWebSocketMessages(SOCKET ClientSocketFD, TArray<uint8> InitialFrameBytes)
#else
void FMCPBridgeServer::ProcessWebSocketMessages(int32 ClientSocketFD, TArray<uint8> InitialFrameBytes)
#endif
{
	constexpr int32 RecvBufferSize = 65536;
	TArray<uint8> Buffer;
	Buffer.SetNumUninitialized(RecvBufferSize);
	TArray<uint8> FrameBuffer = MoveTemp(InitialFrameBytes);

	auto ConsumeFrameBuffer = [this, ClientSocketFD, &FrameBuffer]() -> bool
	{
		while (FrameBuffer.Num() > 0)
		{
			FString Message;
			bool bCloseFrame = false;
			if (!TryParseWebSocketFrame(FrameBuffer, Message, bCloseFrame))
			{
				return true;
			}

			if (bCloseFrame)
			{
				return false;
			}

			if (Message.IsEmpty())
			{
				continue;
			}

			FString Response = ProcessMessage(Message);
			TArray<uint8> ResponseFrame = CreateWebSocketFrame(Response);
			int32 TotalToSend = ResponseFrame.Num();
			int32 Sent = 0;
			while (Sent < TotalToSend)
			{
				int32 BytesSent = send(ClientSocketFD, (char*)ResponseFrame.GetData() + Sent, TotalToSend - Sent, 0);
				if (BytesSent <= 0) break;
				Sent += BytesSent;
			}
		}

		return true;
	};

	if (!ConsumeFrameBuffer())
	{
		return;
	}

	while (!bShouldStop)
	{
		fd_set ReadSet;
		FD_ZERO(&ReadSet);
		FD_SET(ClientSocketFD, &ReadSet);

		timeval Timeout;
		Timeout.tv_sec = 1;
		Timeout.tv_usec = 0;

		int32 SelectResult = select(ClientSocketFD + 1, &ReadSet, nullptr, nullptr, &Timeout);

		if (SelectResult > 0 && FD_ISSET(ClientSocketFD, &ReadSet))
		{
			int32 BytesReceived = recv(ClientSocketFD, (char*)Buffer.GetData(), RecvBufferSize, 0);
			if (BytesReceived <= 0)
			{
				break;
			}

			FrameBuffer.Append(Buffer.GetData(), BytesReceived);
			if (!ConsumeFrameBuffer())
			{
				return;
			}
		}
		else if (SelectResult < 0)
		{
			break;
		}
	}
}

TArray<uint8> FMCPBridgeServer::CreateWebSocketFrame(const FString& Message)
{
	// Simple WebSocket frame creation (text frame, no masking)
	TArray<uint8> Frame;

	// Convert to UTF-8 first to get correct byte length
	FTCHARToUTF8 UTF8String(*Message);
	int32 MessageLen = UTF8String.Length();

	// Frame header
	uint8 FirstByte = 0x81; // FIN + text frame
	Frame.Add(FirstByte);

	if (MessageLen < 126)
	{
		Frame.Add(MessageLen);
	}
	else if (MessageLen < 65536)
	{
		Frame.Add(126);
		Frame.Add((MessageLen >> 8) & 0xFF);
		Frame.Add(MessageLen & 0xFF);
	}
	else
	{
		Frame.Add(127);
		for (int32 i = 7; i >= 0; --i)
		{
			Frame.Add((MessageLen >> (i * 8)) & 0xFF);
		}
	}

	// Message payload (UTF-8 bytes)
	Frame.Append((uint8*)UTF8String.Get(), MessageLen);

	return Frame;
}

bool FMCPBridgeServer::TryParseWebSocketFrame(TArray<uint8>& Data, FString& OutMessage, bool& bOutCloseFrame)
{
	OutMessage.Reset();
	bOutCloseFrame = false;

	if (Data.Num() < 2)
	{
		return false;
	}

	const uint8 FirstByte = Data[0];
	const uint8 SecondByte = Data[1];
	const uint8 OpCode = FirstByte & 0x0F;
	const bool bMasked = (SecondByte & 0x80) != 0;
	uint64 PayloadLen = SecondByte & 0x7F;

	int32 HeaderLen = 2;
	if (PayloadLen == 126)
	{
		if (Data.Num() < 4)
		{
			return false;
		}
		PayloadLen = (static_cast<uint64>(Data[2]) << 8) | Data[3];
		HeaderLen = 4;
	}
	else if (PayloadLen == 127)
	{
		if (Data.Num() < 10)
		{
			return false;
		}
		PayloadLen = 0;
		for (int32 i = 0; i < 8; ++i)
		{
			PayloadLen = (PayloadLen << 8) | Data[2 + i];
		}
		HeaderLen = 10;
	}

	if (PayloadLen > static_cast<uint64>(MAX_int32))
	{
		UE_LOG(LogMCPBridge, Error, TEXT("[UE-MCP] WebSocket frame is too large: %llu bytes"), PayloadLen);
		Data.Reset();
		bOutCloseFrame = true;
		return true;
	}

	if (bMasked)
	{
		HeaderLen += 4;
	}

	const int32 TotalFrameLen = HeaderLen + static_cast<int32>(PayloadLen);
	if (Data.Num() < TotalFrameLen)
	{
		return false;
	}

	if (OpCode == 0x8)
	{
		Data.RemoveAt(0, TotalFrameLen, EAllowShrinking::No);
		bOutCloseFrame = true;
		return true;
	}

	if (OpCode != 0x1)
	{
		Data.RemoveAt(0, TotalFrameLen, EAllowShrinking::No);
		return true;
	}

	TArray<uint8> Payload;
	Payload.Append(Data.GetData() + HeaderLen, static_cast<int32>(PayloadLen));

	if (bMasked)
	{
		const uint8 MaskKey[4] =
		{
			Data[HeaderLen - 4],
			Data[HeaderLen - 3],
			Data[HeaderLen - 2],
			Data[HeaderLen - 1]
		};

		for (int32 i = 0; i < Payload.Num(); ++i)
		{
			Payload[i] ^= MaskKey[i % 4];
		}
	}

	FUTF8ToTCHAR UTF8ToTCHAR(reinterpret_cast<const ANSICHAR*>(Payload.GetData()), Payload.Num());
	OutMessage = FString(UTF8ToTCHAR.Length(), UTF8ToTCHAR.Get());
	Data.RemoveAt(0, TotalFrameLen, EAllowShrinking::No);
	return true;
}
