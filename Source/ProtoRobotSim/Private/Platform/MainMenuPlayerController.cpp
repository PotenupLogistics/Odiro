#include "Platform/MainMenuPlayerController.h"

#include "DeliveryBot/Actor/DeliveryBot.h"
#include "DeliveryBot/Component/DeliveryBot_HttpPolicyComponent.h"
#include "DeliveryBot/Component/DeliveryBot_PolicyControllerComponent.h"
#include "EngineUtils.h"
#include "Platform/Widget/MainMenuWidget.h"
#include "UObject/SoftObjectPath.h"

DEFINE_LOG_CATEGORY_STATIC(LogMainMenuPlayerController, Log, All);

namespace
{
	const TCHAR* DefaultMainWidgetBlueprintClassPath = TEXT("/Game/Widgets/MainMenu/WBP_MainMenu.WBP_MainMenu_C");
}

void AMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	ShowMainWidget();
}

void AMainMenuPlayerController::EndPlay(const EEndPlayReason::Type endPlayReason)
{
	if (IsValid(ActiveRunPolicyHttpComponent))
	{
		ActiveRunPolicyHttpComponent->OnPolicySpecUpdateResponse.RemoveDynamic(
			this,
			&AMainMenuPlayerController::HandleRunPolicySpecUpdateResponse
		);
		ActiveRunPolicyHttpComponent = nullptr;
	}

	RemoveMainWidget();
	Super::EndPlay(endPlayReason);
}

UMainMenuWidget* AMainMenuPlayerController::ShowMainWidget()
{
	if (IsValid(MainWidget))
	{
		if (!MainWidget->IsInViewport())
		{
			MainWidget->AddToViewport(MainWidgetZOrder);
		}

		ApplyMainMenuInputMode(MainWidget);
		return MainWidget;
	}

	const TSubclassOf<UMainMenuWidget> widgetClass = ResolveMainWidgetClass();
	if (!widgetClass)
	{
		UE_LOG(LogMainMenuPlayerController, Warning, TEXT("MainWidgetClass를 찾을 수 없음"));
		return nullptr;
	}

	MainWidget = CreateWidget<UMainMenuWidget>(this, widgetClass);
	if (!MainWidget)
	{
		UE_LOG(LogMainMenuPlayerController, Warning, TEXT("MainWidget 생성 실패 | Class: %s"), *GetNameSafe(widgetClass.Get()));
		return nullptr;
	}

	MainWidget->AddToViewport(MainWidgetZOrder);
	ApplyMainMenuInputMode(MainWidget);

	UE_LOG(LogMainMenuPlayerController, Log, TEXT("MainWidget 표시 | Class: %s"), *GetNameSafe(MainWidget->GetClass()));
	return MainWidget;
}

void AMainMenuPlayerController::RemoveMainWidget()
{
	if (IsValid(MainWidget))
	{
		MainWidget->RemoveFromParent();
	}

	MainWidget = nullptr;
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
}

TSubclassOf<UMainMenuWidget> AMainMenuPlayerController::ResolveMainWidgetClass() const
{
	if (MainWidgetClass)
	{
		return MainWidgetClass;
	}

	// Blueprint에서 명시한 class가 없을 때만 project 기본 MainMenu WBP를 시도한다.
	const FSoftClassPath defaultMainWidgetClassPath(DefaultMainWidgetBlueprintClassPath);
	if (UClass* defaultMainWidgetClass = defaultMainWidgetClassPath.TryLoadClass<UMainMenuWidget>())
	{
		return TSubclassOf<UMainMenuWidget>(defaultMainWidgetClass);
	}

	UE_LOG(
		LogMainMenuPlayerController,
		Error,
		TEXT("기본 MainMenu widget Blueprint를 찾을 수 없음 | Path: %s"),
		DefaultMainWidgetBlueprintClassPath);
	return nullptr;
}

void AMainMenuPlayerController::ApplyMainMenuInputMode(UMainMenuWidget* widget)
{
	if (!IsValid(widget))
	{
		return;
	}

	widget->SetIsFocusable(true);

	FInputModeGameAndUI inputMode;
	inputMode.SetWidgetToFocus(widget->TakeWidget());
	inputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	inputMode.SetHideCursorDuringCapture(false);
	SetInputMode(inputMode);
	bShowMouseCursor = true;
}

bool AMainMenuPlayerController::StartDeliveryBotRunWithPolicySpecFile(const FString& policySpecFileName)
{
	UDeliveryBot_HttpPolicyComponent* policyHttpComponent = FindPolicyHttpComponent();
	if (!IsValid(policyHttpComponent))
	{
		UE_LOG(LogMainMenuPlayerController, Warning, TEXT("Policy spec run start failed. HttpPolicyComponent is invalid."));
		return false;
	}

	if (IsValid(ActiveRunPolicyHttpComponent))
	{
		ActiveRunPolicyHttpComponent->OnPolicySpecUpdateResponse.RemoveDynamic(this, &AMainMenuPlayerController::HandleRunPolicySpecUpdateResponse);
	}

	ActiveRunPolicyHttpComponent = policyHttpComponent;
	ActiveRunPolicyHttpComponent->OnPolicySpecUpdateResponse.AddUniqueDynamic(this, &AMainMenuPlayerController::HandleRunPolicySpecUpdateResponse);

	const bool bRequestStarted = ActiveRunPolicyHttpComponent->SendPolicySpecUpdateJsonFile(policySpecFileName);
	if (!bRequestStarted)
	{
		ActiveRunPolicyHttpComponent->OnPolicySpecUpdateResponse.RemoveDynamic(this, &AMainMenuPlayerController::HandleRunPolicySpecUpdateResponse);
		ActiveRunPolicyHttpComponent = nullptr;

		UE_LOG(LogMainMenuPlayerController, Warning, TEXT("Policy spec update request failed. File: %s"), *policySpecFileName);
		return false;
	}

	UE_LOG(LogMainMenuPlayerController, Log, TEXT("Policy spec update requested. File: %s"), *policySpecFileName);
	return true;
}

void AMainMenuPlayerController::HandleRunPolicySpecUpdateResponse(bool bWasSuccessful, int32 responseCode, const FString& responseBody)
{
	if (IsValid(ActiveRunPolicyHttpComponent))
	{
		ActiveRunPolicyHttpComponent->OnPolicySpecUpdateResponse.RemoveDynamic(this, &AMainMenuPlayerController::HandleRunPolicySpecUpdateResponse);
		ActiveRunPolicyHttpComponent = nullptr;
	}

	const bool bHttpOk = bWasSuccessful && responseCode >= 200 && responseCode < 300;

	if (!bHttpOk)
	{
		UE_LOG(LogMainMenuPlayerController, Warning, TEXT("Policy spec update failed. Success: %s, Code: %d, Body: %s"),
			bWasSuccessful ? TEXT("true") : TEXT("false"), responseCode, *responseBody);
		return;
	}

	UE_LOG(LogMainMenuPlayerController, Log, TEXT("Policy spec update succeeded. Starting episode."));

	if (!StartEpisodeAfterPolicyConfirmed())
	{
		UE_LOG(LogMainMenuPlayerController, Warning, TEXT("Episode start failed after policy spec update."));
	}
}

bool AMainMenuPlayerController::StartEpisodeAfterPolicyConfirmed()
{
	UDeliveryBot_PolicyControllerComponent* policyController = FindDeliveryBotPolicyController();

	if (!IsValid(policyController))
	{
		UE_LOG(LogMainMenuPlayerController, Warning, TEXT("Episode start failed. PolicyControllerComponent is invalid."));
		return false;
	}

	return policyController->SendEpisodeStartAndStartPolicyLoopOnce();
}

UDeliveryBot_HttpPolicyComponent* AMainMenuPlayerController::FindPolicyHttpComponent() const
{
	if (UDeliveryBot_HttpPolicyComponent* policyHttpComponent = FindComponentByClass<UDeliveryBot_HttpPolicyComponent>())
		return policyHttpComponent;

	UWorld* world = GetWorld();
	if (!IsValid(world))
		return nullptr;

	for (TActorIterator<ADeliveryBot> actorIterator(world); actorIterator; ++actorIterator)
	{
		ADeliveryBot* deliveryBot = *actorIterator;

		if (!IsValid(deliveryBot))
			continue;

		if (UDeliveryBot_HttpPolicyComponent* policyHttpComponent = deliveryBot->FindComponentByClass<UDeliveryBot_HttpPolicyComponent>())
			return policyHttpComponent;
	}
	return nullptr;
}

UDeliveryBot_PolicyControllerComponent* AMainMenuPlayerController::FindDeliveryBotPolicyController() const
{
	UWorld* world = GetWorld();
	if (!IsValid(world))
		return nullptr;

	for (TActorIterator<ADeliveryBot> actorIterator(world); actorIterator; ++actorIterator)
	{
		ADeliveryBot* deliveryBot = *actorIterator;
		if (!IsValid(deliveryBot))
			continue;

		if (UDeliveryBot_PolicyControllerComponent* policyController = deliveryBot->FindComponentByClass<UDeliveryBot_PolicyControllerComponent>())
			return policyController;
	}

	return nullptr;
}

// 사용자가 선택한 정책 파일명을 내부 변수에 저장 /  빈 값이 들어오면 기존 선택값을 유지
void AMainMenuPlayerController::SetSelectedPolicySpecFileName(const FString& policySpecFileName)
{
	const FString trimmedFileName = policySpecFileName.TrimStartAndEnd();

	if (trimmedFileName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("SetSelectedPolicySpecFileName skipped. PolicySpec file name is empty."));
		return;
	}

	SelectedPolicySpecFileName = trimmedFileName;

	UE_LOG(LogTemp, Log, TEXT("Selected PolicySpec file changed: %s"), *SelectedPolicySpecFileName);
}

// 현재 선택된 정책 파일명으로 실행 함수 호출
bool AMainMenuPlayerController::StartDeliveryBotRunWithSelectedPolicySpec()
{
	if (SelectedPolicySpecFileName.TrimStartAndEnd().IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("StartDeliveryBotRunWithSelectedPolicySpec failed. SelectedPolicySpecFileName is empty."));
		return false;
	}

	return StartDeliveryBotRunWithPolicySpecFile(SelectedPolicySpecFileName);
}

