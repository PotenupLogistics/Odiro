#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"

class FDialogHandlers
{
public:
	static void RegisterHandlers(class FMCPHandlerRegistry& Registry);

	// Call once during module startup to hook FCoreDelegates::ModalMessageDialog
	static void InstallDialogHook();

	// Call during module shutdown to restore the original delegate
	static void RemoveDialogHook();

	// Add a default dialog policy (e.g. auto-accept overwrite dialogs)
	static void AddDefaultPolicy(const FString& Pattern, EAppReturnType::Type Response);

	/** Limits automatic dialog policy handling to one editor-automation call on the current thread. */
	class FScopedAutomationDialogPolicy
	{
	public:
		/** Enter a bridge-owned editor automation dialog scope. */
		FScopedAutomationDialogPolicy();

		/** Leave the bridge-owned editor automation dialog scope. */
		~FScopedAutomationDialogPolicy();
	};

private:
	// Dialog policy: pattern -> response mapping
	struct FDialogPolicy
	{
		FString Pattern;
		EAppReturnType::Type Response;
	};

	// Handler implementations
	static TSharedPtr<FJsonValue> SetDialogPolicy(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> ClearDialogPolicy(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> GetDialogPolicy(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> ListDialogs(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonValue> RespondToDialog(const TSharedPtr<FJsonObject>& Params);

	// The hooked dialog handler
	static EAppReturnType::Type HandleModalDialog(EAppMsgType::Type MsgType, const FText& Text, const FText& Title);
	// FCoreDelegates::ModalMessageDialog (UE 5.7) signature includes an EAppMsgCategory.
	static EAppReturnType::Type HandleModalDialogV2(enum EAppMsgCategory Category, EAppMsgType::Type MsgType, const FText& Text, const FText& Title);

	/** Return whether the current thread is executing a bridge-owned editor automation call. */
	static bool IsAutomationDialogPolicyActive();

	/** Show the original Unreal modal dialog without recursively entering this hook. */
	static EAppReturnType::Type OpenUnrealModalDialog(EAppMsgType::Type MsgType, const FText& Text, const FText& Title);

	// Convert string to EAppReturnType
	static EAppReturnType::Type ParseResponseType(const FString& ResponseStr);
	static FString ResponseTypeToString(EAppReturnType::Type Response);
	static FString MsgTypeToString(EAppMsgType::Type MsgType);

	// Active policies
	static TArray<FDialogPolicy> Policies;

	// Original delegate handle (so we can unbind)
	static FDelegateHandle OriginalDelegateHandle;

	// Whether we've installed our hook
	static bool bHookInstalled;
};
