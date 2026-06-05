#include "Platform/MainMenuPlayerController.h"

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

	return TSubclassOf<UMainMenuWidget>(UMainMenuWidget::StaticClass());
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
