#pragma once

#include "CoreMinimal.h"
#include "Shared/Struct/DeliveryBot/Setup/DeliveryBotPythonSettings.h"
#include "Engine/DeveloperSettings.h"
#include "DeliveryBotPythonDeveloperSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "DeliveryBot Python"))
class ODIROSIM_API UDeliveryBotPythonDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override; // Project Settings 상위 카테고리 이름 반환


public:
	UPROPERTY(EditAnywhere, Config, Category = "Python")
	FDeliveryBotPythonSettings PythonSettings;
};