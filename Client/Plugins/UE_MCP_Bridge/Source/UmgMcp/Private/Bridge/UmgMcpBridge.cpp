// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Bridge/UmgMcpBridge.h"
#include "Bridge/UmgMcpConfig.h"
#include "MCPBridgeOperationCoordinator.h"
#include "UmgMcp.h"
#include "Bridge/MCPServerRunnable.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "HAL/RunnableThread.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "JsonObjectConverter.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"
// Add Blueprint related includes
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
// UE5.5 correct includes
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "UObject/Field.h"
#include "UObject/FieldPath.h"
// Blueprint Graph specific includes
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_CallFunction.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "GameFramework/InputSettings.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
// Include our new command handler classes
#include "Bridge/UmgMcpCommonUtils.h"
#include "FileManage/UmgMcpFileTransformationCommands.h"
#include "Animation/UmgMcpSequencerCommands.h"
#include "Material/UmgMcpMaterialCommands.h" // Add Material Commands
#include "Blueprint/UmgBlueprintFunctionSubsystem.h"
#include "FileManage/UmgAttentionSubsystem.h"

namespace
{
struct FUmgDesignVerificationState
{
    bool bPending = false;
    FString PendingMutationCommand;
    TSet<FString> CompletedSteps;
};

TMap<FString, FUmgDesignVerificationState> GUmgDesignVerificationByAsset;

struct FUmgCommandExecutionState
{
    FThreadSafeBool bAbandoned{false};
};

// Serializes one coordinator result for legacy UmgMcp socket responses.
FString SerializeJsonValue(const TSharedPtr<FJsonValue>& Value)
{
    FString ResultString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
    const TSharedPtr<FJsonObject>* Object = nullptr;
    if (Value.IsValid() && Value->TryGetObject(Object) && Object && Object->IsValid())
    {
        FJsonSerializer::Serialize((*Object).ToSharedRef(), Writer);
    }
    return ResultString;
}

// Returns the asset key that owns the verification checklist for this command.
FString GetUmgDesignVerificationKey(const TSharedPtr<FJsonObject>& Params)
{
    if (!Params.IsValid())
    {
        return TEXT("__global_umg__");
    }

    FString Value;
    static const TCHAR* CandidateFields[] =
    {
        TEXT("assetPath"),
        TEXT("asset_path"),
        TEXT("path"),
        TEXT("widgetBlueprintPath"),
        TEXT("widget_blueprint_path"),
        TEXT("blueprintPath"),
        TEXT("blueprint_path")
    };

    for (const TCHAR* FieldName : CandidateFields)
    {
        if (Params->TryGetStringField(FieldName, Value) && !Value.IsEmpty())
        {
            return Value;
        }
    }

    return TEXT("__global_umg__");
}

// Identifies UMG commands whose successful result must be followed by visual layout verification.
bool IsUmgDesignMutationCommand(const FString& CommandType)
{
    return CommandType == TEXT("create_widget") ||
           CommandType == TEXT("set_widget_properties") ||
           CommandType == TEXT("delete_widget") ||
           CommandType == TEXT("reparent_widget") ||
           CommandType == TEXT("move_widget") ||
           CommandType == TEXT("apply_json_to_umg") ||
           CommandType == TEXT("apply_layout") ||
           CommandType == TEXT("create_animation") ||
           CommandType == TEXT("delete_animation") ||
           CommandType == TEXT("set_property_keys") ||
           CommandType == TEXT("remove_property_track") ||
           CommandType == TEXT("remove_keys") ||
           CommandType == TEXT("animation_append_widget_tracks") ||
           CommandType == TEXT("animation_append_time_slice") ||
           CommandType == TEXT("animation_delete_widget_keys");
}

// Identifies verification commands that satisfy the mandatory post-edit UMG checklist.
bool IsUmgDesignVerificationCommand(const FString& CommandType)
{
    return CommandType == TEXT("get_widget_tree") ||
           CommandType == TEXT("compile_blueprint") ||
           CommandType == TEXT("get_layout_data") ||
           CommandType == TEXT("check_widget_overlap");
}

// Requires cached geometry before the layout-data step can count as complete.
bool DidUmgDesignVerificationCommandPass(const FString& CommandType, const TSharedPtr<FJsonObject>& ResultJson)
{
    if (CommandType != TEXT("get_layout_data"))
    {
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* LayoutData = nullptr;
    return ResultJson.IsValid() &&
           ResultJson->TryGetArrayField(TEXT("layout_data"), LayoutData) &&
           LayoutData &&
           LayoutData->Num() > 0;
}

// Returns the fixed verification sequence required after UMG design mutations.
TArray<FString> GetUmgDesignRequiredSequence()
{
    TArray<FString> RequiredSequence;
    RequiredSequence.Add(TEXT("get_widget_tree"));
    RequiredSequence.Add(TEXT("compile_blueprint"));
    RequiredSequence.Add(TEXT("get_layout_data"));
    RequiredSequence.Add(TEXT("check_widget_overlap"));
    return RequiredSequence;
}

// Reports completed checklist items in required-sequence order for stable responses.
TArray<FString> GetCompletedUmgDesignVerificationSteps(const FUmgDesignVerificationState& State)
{
    TArray<FString> CompletedSteps;
    for (const FString& Step : GetUmgDesignRequiredSequence())
    {
        if (State.CompletedSteps.Contains(Step))
        {
            CompletedSteps.Add(Step);
        }
    }
    return CompletedSteps;
}

// Converts a string sequence to JSON values for response payloads.
TArray<TSharedPtr<FJsonValue>> MakeStringArray(const TArray<FString>& Values)
{
    TArray<TSharedPtr<FJsonValue>> JsonValues;
    for (const FString& Value : Values)
    {
        JsonValues.Add(MakeShared<FJsonValueString>(Value));
    }
    return JsonValues;
}

// Reports checklist items that have not completed since the last UMG design mutation.
TArray<FString> GetMissingUmgDesignVerificationSteps(const FUmgDesignVerificationState& State)
{
    TArray<FString> MissingSteps;
    for (const FString& Step : GetUmgDesignRequiredSequence())
    {
        if (!State.CompletedSteps.Contains(Step))
        {
            MissingSteps.Add(Step);
        }
    }
    return MissingSteps;
}

// Returns true only when all required verification steps have passed.
bool IsUmgDesignVerificationComplete(const FUmgDesignVerificationState& State)
{
    return GetMissingUmgDesignVerificationSteps(State).Num() == 0;
}

// Adds a machine-readable verification checklist to mutation responses for MCP clients.
void AddUmgDesignVerificationRequirement(const TSharedPtr<FJsonObject>& ResponseJson, const FUmgDesignVerificationState& State, const FString& CommandType)
{
    if (!ResponseJson.IsValid())
    {
        return;
    }

    TArray<TSharedPtr<FJsonValue>> VisualVerificationOptions;
    VisualVerificationOptions.Add(MakeShared<FJsonValueString>(TEXT("capture_slate_window")));
    VisualVerificationOptions.Add(MakeShared<FJsonValueString>(TEXT("dump_runtime_widget_geometry")));

    TSharedPtr<FJsonObject> Verification = MakeShared<FJsonObject>();
    Verification->SetBoolField(TEXT("required"), true);
    Verification->SetStringField(TEXT("trigger_command"), CommandType);
    Verification->SetStringField(TEXT("reason"), TEXT("UMG visual or layout state changed; verify tree, compile result, layout geometry, overlap, and visual/runtime capture before reporting completion."));
    Verification->SetArrayField(TEXT("required_sequence"), MakeStringArray(GetUmgDesignRequiredSequence()));
    Verification->SetArrayField(TEXT("completed_sequence"), MakeStringArray(GetCompletedUmgDesignVerificationSteps(State)));
    Verification->SetArrayField(TEXT("missing_sequence"), MakeStringArray(GetMissingUmgDesignVerificationSteps(State)));
    Verification->SetArrayField(TEXT("visual_verification_options"), VisualVerificationOptions);

    ResponseJson->SetObjectField(TEXT("design_verification_required"), Verification);
}
}



UUmgMcpBridge::UUmgMcpBridge()
{
    AttentionCommands = MakeShared<FUmgMcpAttentionCommands>();
    WidgetCommands = MakeShared<FUmgMcpWidgetCommands>();
    FileTransformationCommands = MakeShared<FUmgMcpFileTransformationCommands>();
    EditorCommands = MakeShared<FUmgMcpEditorCommands>();
    BlueprintCommands = MakeShared<FUmgMcpBlueprintCommands>();
    SequencerCommands = MakeShared<FUmgMcpSequencerCommands>();
    MaterialCommands = MakeShared<FUmgMcpMaterialCommands>();
}

UUmgMcpBridge::~UUmgMcpBridge()
{
    AttentionCommands.Reset();
    WidgetCommands.Reset();
    FileTransformationCommands.Reset();
    EditorCommands.Reset();
    BlueprintCommands.Reset();
    SequencerCommands.Reset();
    MaterialCommands.Reset();
}

// Initialize subsystem
void UUmgMcpBridge::Initialize(FSubsystemCollectionBase& Collection)
{
    UE_LOG(LogUmgMcp, Display, TEXT("UmgMcpBridge: Initializing"));

    bIsRunning = false;
    ListenerSocket = nullptr;
    ConnectionSocket = nullptr;
    ServerThread = nullptr;
    Port = MCP_SERVER_PORT_DEFAULT;
    FIPv4Address::Parse(MCP_SERVER_HOST_DEFAULT, ServerAddress);

    UE_LOG(LogUmgMcp, Display, TEXT("UmgMcpBridge: Legacy TCP server disabled; commands are exposed through UE_MCP_Bridge."));
}

// Clean up resources when subsystem is destroyed
void UUmgMcpBridge::Deinitialize()
{
    UE_LOG(LogUmgMcp, Display, TEXT("UmgMcpBridge: Shutting down"));
    StopServer();
}

// Initialize static member
bool UUmgMcpBridge::bGlobalServerStarted = false;

// Start the MCP server
void UUmgMcpBridge::StartServer()
{
    UE_LOG(LogUmgMcp, Display, TEXT("UmgMcpBridge: Attempting to start server on port %d..."), Port);

    if (bIsRunning)
    {
        UE_LOG(LogUmgMcp, Warning, TEXT("UmgMcpBridge: Server is already running (Instance check)"));
        return;
    }

    if (bGlobalServerStarted)
    {
        UE_LOG(LogUmgMcp, Warning, TEXT("UmgMcpBridge: Server is already running (Global check). Skipping start to avoid port conflict."));
        return;
    }

    // Create socket subsystem
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("UmgMcpBridge: Failed to get socket subsystem"));
        return;
    }

    // Create listener socket
    TSharedPtr<FSocket> NewListenerSocket = MakeShareable(SocketSubsystem->CreateSocket(NAME_Stream, TEXT("UnrealMCPListener"), false));
    if (!NewListenerSocket.IsValid())
    {
        UE_LOG(LogUmgMcp, Error, TEXT("UmgMcpBridge: Failed to create listener socket: %s"), SocketSubsystem->GetSocketError(SocketSubsystem->GetLastErrorCode()));
        return;
    }

    // Allow address reuse for quick restarts
    // Note: We are keeping this ENABLED as it is standard practice, but we are aware it can cause SE_EACCES on Windows if port is excluded.
    NewListenerSocket->SetReuseAddr(true);
    NewListenerSocket->SetNonBlocking(true);

    // Bind to address
    FIPv4Endpoint Endpoint(ServerAddress, Port);
    if (!NewListenerSocket->Bind(*Endpoint.ToInternetAddr()))
    {
        ESocketErrors LastError = SocketSubsystem->GetLastErrorCode();
        const TCHAR* ErrorString = SocketSubsystem->GetSocketError(LastError);
        int32 ErrorCode = (int32)LastError;

        UE_LOG(LogUmgMcp, Error, TEXT("UmgMcpBridge: Failed to bind listener socket to %s:%d. Error Code: %d (%s)"),
            *ServerAddress.ToString(), Port, ErrorCode, ErrorString);

        if (ErrorCode == 10013) // WSAEACCES
        {
            UE_LOG(LogUmgMcp, Error, TEXT("UmgMcpBridge: Port %d is likely reserved by Windows (Hyper-V/Docker). Please change the port in UmgMcpConfig.h."), Port);
        }
        else if (ErrorCode == 10048) // WSAEADDRINUSE
        {
            UE_LOG(LogUmgMcp, Error, TEXT("UmgMcpBridge: Port %d is already in use by another process."), Port);
        }

        return;
    }

    // Start listening
    if (!NewListenerSocket->Listen(5))
    {
        ESocketErrors LastError = SocketSubsystem->GetLastErrorCode();
        UE_LOG(LogUmgMcp, Error, TEXT("UmgMcpBridge: Failed to start listening. Error Code: %d (%s)"),
            (int32)LastError,
            SocketSubsystem->GetSocketError(LastError));
        return;
    }

    ListenerSocket = NewListenerSocket;
    bIsRunning = true;
    bGlobalServerStarted = true; // Set global flag
    UE_LOG(LogUmgMcp, Display, TEXT("UmgMcpBridge: Server started successfully on %s:%d"), *ServerAddress.ToString(), Port);

    // Start server thread
    ServerThread = FRunnableThread::Create(
        new FMCPServerRunnable(this, ListenerSocket),
        TEXT("UnrealMCPServerThread"),
        0, TPri_Normal
    );

    if (!ServerThread)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("UmgMcpBridge: Failed to create server thread"));
        StopServer();
        return;
    }
}

// Stop the MCP server
void UUmgMcpBridge::StopServer()
{
    if (!bIsRunning)
    {
        return;
    }

    bIsRunning = false;
    bGlobalServerStarted = false; // Reset global flag

    // Clean up thread
    if (ServerThread)
    {
        ServerThread->Kill(true);
        delete ServerThread;
        ServerThread = nullptr;
    }

    // Close sockets
    if (ConnectionSocket.IsValid())
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ConnectionSocket.Get());
        ConnectionSocket.Reset();
    }

    if (ListenerSocket.IsValid())
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenerSocket.Get());
        ListenerSocket.Reset();
    }

    UE_LOG(LogUmgMcp, Display, TEXT("UmgMcpBridge: Server stopped"));
}

// Execute a command received from a client
FString UUmgMcpBridge::ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogUmgMcp, Display, TEXT("UmgMcpBridge: Received command: %s"), *CommandType);

    TSharedPtr<FJsonObject> SafeParams = Params.IsValid() ? Params : MakeShareable(new FJsonObject);
    const bool bAlreadyAdmitted = FMCPBridgeOperationCoordinator::IsCurrentThreadOperationAdmitted();
    TSharedPtr<FMCPBridgeOperationLease> OperationLease;
    if (!bAlreadyAdmitted)
    {
        FMCPBridgeOperationAdmission Admission = FMCPBridgeOperationCoordinator::TryBeginOrReserve(CommandType, SafeParams);
        if (!Admission.bCanExecute)
        {
            return SerializeJsonValue(Admission.Response);
        }
        OperationLease = Admission.Lease;
    }

    // If we are already on the GameThread (e.g. called from FabServer or test), execute directly
    if (IsInGameThread())
    {
        UE_LOG(LogUmgMcp, Verbose, TEXT("UmgMcpBridge: Already on GameThread, executing directly."));
        if (OperationLease.IsValid())
        {
            OperationLease->MarkStarted();
        }
        FString Result = InternalExecuteCommand(CommandType, SafeParams);
        if (OperationLease.IsValid())
        {
            OperationLease->Release();
        }
        return Result;
    }

    // Otherwise, queue execution on Game Thread and wait
    // This ensures thread safety for UObject operations (creating widgets, animations, etc.)
    UE_LOG(LogUmgMcp, Verbose, TEXT("UmgMcpBridge: Dispatching to GameThread..."));

    TPromise<FString> Promise;
    TFuture<FString> Future = Promise.GetFuture();
    TSharedRef<FUmgCommandExecutionState> ExecutionState = MakeShared<FUmgCommandExecutionState>();

    AsyncTask(ENamedThreads::GameThread, [this, CommandType, SafeParams, OperationLease, ExecutionState, Promise = MoveTemp(Promise)]() mutable
    {
        if (ExecutionState->bAbandoned)
        {
            if (OperationLease.IsValid())
            {
                OperationLease->ReleaseIfNeverStarted();
            }
            TSharedPtr<FJsonObject> ErrorResponse = MakeShareable(new FJsonObject);
            ErrorResponse->SetStringField(TEXT("status"), TEXT("error"));
            ErrorResponse->SetStringField(TEXT("error"), TEXT("Game Thread Timeout - command was abandoned before execution."));
            FString AbandonedResult;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&AbandonedResult);
            FJsonSerializer::Serialize(ErrorResponse.ToSharedRef(), Writer);
            Promise.SetValue(AbandonedResult);
            return;
        }

        if (OperationLease.IsValid())
        {
            OperationLease->MarkStarted();
        }
        FString Result = InternalExecuteCommand(CommandType, SafeParams);
        if (OperationLease.IsValid())
        {
            OperationLease->Release();
        }
        Promise.SetValue(Result);
    });

    // Wait for the result with a timeout to prevent infinite hang
    // This ensures we return a proper error to the client instead of letting the socket timeout
    if (Future.WaitFor(FTimespan::FromSeconds(MCP_GAME_THREAD_TIMEOUT_DEFAULT)))
    {
        return Future.Get();
    }
    else
    {
        ExecutionState->bAbandoned = true;
        if (OperationLease.IsValid())
        {
            OperationLease->ReleaseIfNeverStarted();
        }
        UE_LOG(LogUmgMcp, Error, TEXT("UmgMcpBridge: GameThread execution timed out (%.1fs) for command: %s"), MCP_GAME_THREAD_TIMEOUT_DEFAULT, *CommandType);

        TSharedPtr<FJsonObject> ErrorResponse = MakeShareable(new FJsonObject);
        ErrorResponse->SetStringField(TEXT("status"), TEXT("error"));
        ErrorResponse->SetStringField(TEXT("error"), FString::Printf(TEXT("Game Thread Timeout - The editor may be paused or busy (Waited %.1fs)."), MCP_GAME_THREAD_TIMEOUT_DEFAULT));

        FString ResultString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
        FJsonSerializer::Serialize(ErrorResponse.ToSharedRef(), Writer);
        return ResultString;
    }
}

FString UUmgMcpBridge::InternalExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject);
    const FString VerificationKey = GetUmgDesignVerificationKey(Params);
    FUmgDesignVerificationState& VerificationState = GUmgDesignVerificationByAsset.FindOrAdd(VerificationKey);

    if (CommandType == TEXT("save_asset") && VerificationState.bPending && !IsUmgDesignVerificationComplete(VerificationState))
    {
        ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
        ResponseJson->SetStringField(TEXT("error"), TEXT("UMG design verification is pending. Run the missing design verification steps before save_asset."));
        AddUmgDesignVerificationRequirement(ResponseJson, VerificationState, VerificationState.PendingMutationCommand);

        FString ResultString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
        FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
        return ResultString;
    }

    try
    {
        TSharedPtr<FJsonObject> ResultJson;

        if (CommandType == TEXT("ping"))
        {
            ResultJson = MakeShareable(new FJsonObject);
            ResultJson->SetStringField(TEXT("message"), TEXT("pong"));
        }
        // Attention Commands
        else if (CommandType == TEXT("get_last_edited_umg_asset") ||
                 CommandType == TEXT("get_recently_edited_umg_assets") ||
                 CommandType == TEXT("get_target_umg_asset") ||
                 CommandType == TEXT("get_target_widget") ||
                 CommandType == TEXT("set_target_widget") ||
                 CommandType == TEXT("set_target_umg_asset"))
        {
            ResultJson = AttentionCommands->HandleCommand(CommandType, Params);
        }
        else if (CommandType == TEXT("set_target_graph") ||
                 CommandType == TEXT("get_target_graph") ||
                 CommandType == TEXT("set_cursor_node") ||
                 CommandType == TEXT("get_cursor_node") ||
                 CommandType == TEXT("set_edit_function"))
        {
              // Handle new Stateful Attention commands directly via Subsystem
              if (GEditor)
              {
                  UUmgAttentionSubsystem* AttentionSystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>();
                  if (AttentionSystem)
                  {
                       ResultJson = MakeShareable(new FJsonObject);
                       if (CommandType == TEXT("set_target_graph") || CommandType == TEXT("set_edit_function"))
                       {
                           FString GraphName;
                           // Support both 'graph_name' (legacy) and 'function_name' (new)
                           if (!Params->TryGetStringField(TEXT("graph_name"), GraphName))
                           {
                               Params->TryGetStringField(TEXT("function_name"), GraphName);
                           }

                           if (!GraphName.IsEmpty())
                           {
                               // Function Creation / Event Binding Logic
                               UUmgBlueprintFunctionSubsystem* BPSystem = GEditor->GetEditorSubsystem<UUmgBlueprintFunctionSubsystem>();
                               UWidgetBlueprint* TargetBP = AttentionSystem->GetCachedTargetWidgetBlueprint();

                               if (BPSystem && TargetBP)
                               {
                                   FString TargetNodeId;
                                   FString ActualGraphName;
                                   FString FunctionStatus;

                                   FString ComponentName;
                                   FString EventName;

                                   // Check for "Component.Event" syntax
                                   if (GraphName.Split(TEXT("."), &ComponentName, &EventName))
                                   {
                                        // It's a Component Event!
                                        TargetNodeId = BPSystem->EnsureComponentEventExists(TargetBP, ComponentName, EventName, FunctionStatus);
                                        ActualGraphName = TEXT("EventGraph");
                                   }
                                   else
                                   {
                                       // It's a regular Function
                                       // Pass full params to allow signature definition if creation is needed
                                       FString ParamString;
                                       TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ParamString);
                                       FJsonSerializer::Serialize(Params.ToSharedRef(), Writer);

                                       TargetNodeId = BPSystem->EnsureFunctionExists(TargetBP, GraphName, FunctionStatus, ParamString);

                                       // Check if it was resolved to an Event (Custom Event or Component Event)
                                       if (FunctionStatus.Contains(TEXT("Event")))
                                       {
                                           ActualGraphName = TEXT("EventGraph");
                                       }
                                       else
                                       {
                                           ActualGraphName = GraphName;
                                       }
                                   }

                                   // Set Context
                                   AttentionSystem->SetTargetGraph(ActualGraphName);
                                   if (!TargetNodeId.IsEmpty())
                                   {
                                       AttentionSystem->SetCursorNode(TargetNodeId);
                                   }

                                   ResultJson->SetStringField(TEXT("target_graph"), ActualGraphName);
                                   ResultJson->SetStringField(TEXT("cursor_node"), TargetNodeId);
                                   ResultJson->SetStringField(TEXT("status"), FunctionStatus); // Found, Created, Inherited
                                   ResultJson->SetBoolField(TEXT("success"), true);
                               }
                               else
                               {
                                   // Fallback if systems missing (unlikely)
                                   AttentionSystem->SetTargetGraph(GraphName);
                                   ResultJson->SetBoolField(TEXT("success"), true);
                               }
                           }
                           else
                           {
                               ResultJson->SetBoolField(TEXT("success"), false);
                               ResultJson->SetStringField(TEXT("error"), TEXT("Missing function_name or graph_name"));
                           }
                       }
                       else if (CommandType == TEXT("get_target_graph"))
                       {
                            ResultJson->SetStringField(TEXT("target_graph"), AttentionSystem->GetTargetGraph());
                            ResultJson->SetBoolField(TEXT("success"), true);
                       }
                       else if (CommandType == TEXT("set_cursor_node"))
                       {
                           FString NodeId;
                           if (Params->TryGetStringField(TEXT("node_id"), NodeId))
                           {
                               AttentionSystem->SetCursorNode(NodeId);
                               ResultJson->SetStringField(TEXT("cursor_node"), NodeId);
                               ResultJson->SetBoolField(TEXT("success"), true);
                           }
                       }
                       else if (CommandType == TEXT("get_cursor_node"))
                       {
                            ResultJson->SetStringField(TEXT("cursor_node"), AttentionSystem->GetCursorNode());
                            ResultJson->SetBoolField(TEXT("success"), true);
                       }
                  }
              }
        }
        // Widget Commands
        else if (CommandType == TEXT("get_widget_tree") ||
                 CommandType == TEXT("query_widget_properties") ||
                 CommandType == TEXT("get_layout_data") ||
                 CommandType == TEXT("check_widget_overlap") ||
                 CommandType == TEXT("create_widget") ||
                 CommandType == TEXT("set_widget_properties") ||
                 CommandType == TEXT("delete_widget") ||
                 CommandType == TEXT("reparent_widget") ||
                 CommandType == TEXT("move_widget") ||
                 CommandType == TEXT("save_asset") ||
                 CommandType == TEXT("get_widget_schema"))
        {
            ResultJson = WidgetCommands->HandleCommand(CommandType, Params);
        }
        // File Transformation Commands
        else if (CommandType == TEXT("export_umg_to_json") ||
                 CommandType == TEXT("apply_json_to_umg") ||
                 CommandType == TEXT("apply_layout"))
        {
            ResultJson = FileTransformationCommands->HandleCommand(CommandType, Params);
        }
        // Sequencer Commands
        else if (CommandType == TEXT("get_all_animations") ||
                 CommandType == TEXT("create_animation") ||
                 CommandType == TEXT("delete_animation") ||
                 CommandType == TEXT("set_animation_scope") ||
                 CommandType == TEXT("animation_target") ||
                 CommandType == TEXT("set_widget_scope") ||
                 CommandType == TEXT("widget_target") ||
                 CommandType == TEXT("set_property_keys") ||
                 CommandType == TEXT("remove_property_track") ||
                 CommandType == TEXT("remove_keys") ||
                 CommandType == TEXT("get_animation_keyframes") ||
                 CommandType == TEXT("get_animated_widgets") ||
                 CommandType == TEXT("get_animation_full_data") ||
                 CommandType == TEXT("get_widget_animation_data") ||
                 CommandType == TEXT("animation_widget_properties") ||
                 CommandType == TEXT("animation_time_properties") ||
                 CommandType == TEXT("animation_overview") ||
                 CommandType == TEXT("animation_append_widget_tracks") ||
                 CommandType == TEXT("animation_append_time_slice") ||
                 CommandType == TEXT("animation_delete_widget_keys"))
        {
            ResultJson = SequencerCommands->HandleCommand(CommandType, Params);
        }
        // Editor Commands (Actors, Level, etc.)
        else if (CommandType == TEXT("get_actors_in_level") ||
                 CommandType == TEXT("find_actors_by_name") ||
                 CommandType == TEXT("spawn_actor") ||
                 CommandType == TEXT("delete_actor") ||
                 CommandType == TEXT("set_actor_transform") ||
                 CommandType == TEXT("refresh_asset_registry") ||
                 CommandType == TEXT("list_assets"))
        {
            ResultJson = EditorCommands->HandleCommand(CommandType, Params);
        }
        // Blueprint Commands
        else if (CommandType == TEXT("create_blueprint") ||
                 CommandType == TEXT("add_component_to_blueprint") ||
                 CommandType == TEXT("set_physics_properties") ||
                 CommandType == TEXT("compile_blueprint") ||
                 CommandType == TEXT("set_static_mesh_properties") ||
                 CommandType == TEXT("spawn_blueprint_actor") ||
                 CommandType == TEXT("set_mesh_material_color") ||
                 CommandType == TEXT("get_available_materials") ||
                 CommandType == TEXT("apply_material_to_actor") ||
                 CommandType == TEXT("apply_material_to_blueprint") ||
                 CommandType == TEXT("get_actor_material_info"))
        {
            ResultJson = BlueprintCommands->HandleCommand(CommandType, Params);
        }
        // Material Commands (New 5 Pillars)
        else if (CommandType.StartsWith(TEXT("material_")) || CommandType.StartsWith(TEXT("hlsl_")))
        {
             ResultJson = MaterialCommands->HandleCommand(CommandType, Params);
        }
        // Low-level Graph Manipulation (New)
        else if (CommandType == TEXT("manage_blueprint_graph"))
        {
             if (GEditor)
             {
                 UUmgBlueprintFunctionSubsystem* GraphSystem = GEditor->GetEditorSubsystem<UUmgBlueprintFunctionSubsystem>();
                 UUmgAttentionSubsystem* AttentionSystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>();

                 if (GraphSystem && AttentionSystem)
                 {
                     // 1. Resolve Target Blueprint
                     // We *should* use the one from Attention System if not specified?
                     // Or just pass the Attention System's cached one.
                     UWidgetBlueprint* TargetBP = AttentionSystem->GetCachedTargetWidgetBlueprint();

                     // Fallback: Check if payload specifies an asset?
                     // For now, Strict Stateful mode: Must have target set in Attention.

                     if (TargetBP)
                     {
                         // 2. Inject Context (Current Graph) and Auto-Wiring Info
                         FString GraphName;
                         // Helper to get writable copy or modify existing
                         TSharedPtr<FJsonObject> ModifiedParams = MakeShareable(new FJsonObject());
                         // Copy existing fields
                         for (auto& Elem : Params->Values)
                         {
                             ModifiedParams->SetField(Elem.Key, Elem.Value);
                         }

                         FString SubAction;
                         Params->TryGetStringField(TEXT("action"), SubAction);


                          if (SubAction.IsEmpty())
                          {
                              Params->TryGetStringField(TEXT("subAction"), SubAction);
                          }

                          // Ensure "subAction" is present for the subsystem
                          ModifiedParams->SetStringField(TEXT("subAction"), SubAction);

                          if (!Params->HasField(TEXT("graphName")))
                          {
                             ModifiedParams->SetStringField(TEXT("graphName"), AttentionSystem->GetTargetGraph());
                         }

                         // Auto-Layout & Auto-Connect
                         if (SubAction == TEXT("create_node") || SubAction == TEXT("add_node") || SubAction == TEXT("add_param") ||
                             SubAction == TEXT("add_function_step") || SubAction == TEXT("add_step_param"))
                         {
                             if (!Params->HasField(TEXT("x")) || !Params->HasField(TEXT("y")))
                             {
                                 FVector2D Pos = AttentionSystem->GetAndAdvanceCursorPosition();
                                 if (!Params->HasField(TEXT("x"))) ModifiedParams->SetNumberField(TEXT("x"), Pos.X);
                                 if (!Params->HasField(TEXT("y"))) ModifiedParams->SetNumberField(TEXT("y"), Pos.Y);
                             }

                             if (!Params->HasField(TEXT("autoConnectToNodeId")))
                             {
                                 FString CursorNode = AttentionSystem->GetCursorNode();
                                 if (!CursorNode.IsEmpty())
                                 {
                                     ModifiedParams->SetStringField(TEXT("autoConnectToNodeId"), CursorNode);
                                 }
                             }
                         }
                          else if (SubAction == TEXT("delete_node"))
                          {
                              if (!Params->HasField(TEXT("nodeId")))
                              {
                                  FString CursorNode = AttentionSystem->GetCursorNode();
                                  if (!CursorNode.IsEmpty())
                                  {
                                      ModifiedParams->SetStringField(TEXT("nodeId"), CursorNode);
                                  }
                              }
                          }

                         // Serialize Payload
                         FString PayloadString;
                         TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadString);
                         FJsonSerializer::Serialize(ModifiedParams.ToSharedRef(), Writer);

                         FString ResultString = GraphSystem->HandleBlueprintGraphAction(TargetBP, CommandType, PayloadString);

                         // Deserialize result
                         TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResultString);
                         FJsonSerializer::Deserialize(Reader, ResultJson);

                         // 3. Post-Action: Update Attention Context
                         if (ResultJson.IsValid() && ResultJson->GetBoolField(TEXT("success")))
                         {
                             if (SubAction == TEXT("create_node") || SubAction == TEXT("add_node") || SubAction == TEXT("add_param") ||
                                 SubAction == TEXT("add_function_step"))
                             {
                                 FString NewNodeId;
                                 if (ResultJson->TryGetStringField(TEXT("nodeId"), NewNodeId))
                                 {
                                     // Only update Cursor (PC) if it's an Exec node
                                     bool bIsExec = false;
                                     if (ResultJson->TryGetBoolField(TEXT("isExec"), bIsExec))
                                     {
                                         if (bIsExec)
                                         {
                                             AttentionSystem->SetCursorNode(NewNodeId);
                                         }
                                     }
                                     else
                                     {
                                         // Fallback for older/other commands: assume Exec if not specified?
                                         // Or assume Exec for 'add_function_step' specifically.
                                         // But since we updated CreateNodeInstance, it should be there.
                                         AttentionSystem->SetCursorNode(NewNodeId);
                                     }
                                 }
                             }
                             else if (SubAction == TEXT("delete_node"))
                             {
                                 FString NewCursor;
                                 if (ResultJson->TryGetStringField(TEXT("newCursorNode"), NewCursor))
                                 {
                                     AttentionSystem->SetCursorNode(NewCursor);
                                 }
                             }
                         }
                     }
                     else
                     {
                         ResultJson = MakeShareable(new FJsonObject);
                         ResultJson->SetStringField(TEXT("error"), TEXT("No target Blueprint set in Attention Subsystem. Use set_target_umg_asset first."));
                         ResultJson->SetBoolField(TEXT("success"), false);
                     }
                 }
             }
        }
        // --- Material Commands ---
        else if (CommandType.StartsWith(TEXT("material_")) || CommandType.StartsWith(TEXT("hlsl_")))
        {
            ResultJson = MaterialCommands->HandleCommand(CommandType, Params);
        }
        else
        {
            ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
            ResponseJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown command: %s"), *CommandType));

            FString ResultString;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
            FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
            return ResultString;
        }

        // Check if the result contains an error
        bool bSuccess = true;
        FString ErrorMessage;

        // Determine success by checking for "success" bool, or "status" string, or absence of "error"
        if (ResultJson->HasField(TEXT("success")))
        {
            bSuccess = ResultJson->GetBoolField(TEXT("success"));
            if (!bSuccess && ResultJson->HasField(TEXT("error")))
            {
                ErrorMessage = ResultJson->GetStringField(TEXT("error"));
            }
        }
        else if (ResultJson->HasField(TEXT("status")))
        {
            FString InnerStatus;
            ResultJson->TryGetStringField(TEXT("status"), InnerStatus);
            bSuccess = (InnerStatus != TEXT("error"));
            if (!bSuccess && ResultJson->HasField(TEXT("error")))
            {
                ErrorMessage = ResultJson->GetStringField(TEXT("error"));
            }
            else if (!bSuccess && ResultJson->HasField(TEXT("message")))
            {
                ErrorMessage = ResultJson->GetStringField(TEXT("message"));
            }
        }

        if (bSuccess)
        {
            // Flatten: copy all fields from ResultJson directly into ResponseJson (skip internal keys)
            ResponseJson->SetStringField(TEXT("status"), TEXT("success"));
            for (const auto& Field : ResultJson->Values)
            {
                const FString& Key = Field.Key;
                if (Key != TEXT("success") && Key != TEXT("status"))
                {
                    ResponseJson->SetField(Key, Field.Value);
                }
            }

            if (IsUmgDesignVerificationCommand(CommandType) && DidUmgDesignVerificationCommandPass(CommandType, ResultJson))
            {
                VerificationState.CompletedSteps.Add(CommandType);
            }

            if (IsUmgDesignMutationCommand(CommandType))
            {
                VerificationState.bPending = true;
                VerificationState.PendingMutationCommand = CommandType;
                VerificationState.CompletedSteps.Empty();
                AddUmgDesignVerificationRequirement(ResponseJson, VerificationState, CommandType);
            }
            else if (CommandType == TEXT("save_asset") && VerificationState.bPending && IsUmgDesignVerificationComplete(VerificationState))
            {
                VerificationState.bPending = false;
                VerificationState.PendingMutationCommand.Empty();
                VerificationState.CompletedSteps.Empty();
            }
        }
        else
        {
            // Set error status and include the error message
            ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
            ResponseJson->SetStringField(TEXT("error"), ErrorMessage);
        }
    }
    catch (const std::exception& e)
    {
        ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
        ResponseJson->SetStringField(TEXT("error"), UTF8_TO_TCHAR(e.what()));
    }

    FString ResultString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
    FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
    return ResultString;
}
