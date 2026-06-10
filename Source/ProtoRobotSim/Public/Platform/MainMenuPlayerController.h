#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MainMenuPlayerController.generated.h"

class UMainMenuWidget;

UCLASS(BlueprintType, Blueprintable)
class PROTOROBOTSIM_API AMainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "MainMenu|UI")
	UMainMenuWidget* ShowMainWidget();

	UFUNCTION(BlueprintCallable, Category = "MainMenu|UI")
	void RemoveMainWidget();

	UFUNCTION(BlueprintPure, Category = "MainMenu|UI")
	UMainMenuWidget* GetMainWidget() const { return MainWidget; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu|UI")
	TSubclassOf<UMainMenuWidget> MainWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu|UI")
	int32 MainWidgetZOrder = 0;

private:
	TSubclassOf<UMainMenuWidget> ResolveMainWidgetClass() const;
	void ApplyMainMenuInputMode(UMainMenuWidget* widget);

	UPROPERTY(Transient)
	TObjectPtr<UMainMenuWidget> MainWidget;
};
