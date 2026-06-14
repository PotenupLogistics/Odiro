#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "ScenarioAssetPaletteCatalog.generated.h"

UCLASS(BlueprintType)
class ODIROSIM_API UScenarioAssetPaletteCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	static TSoftObjectPtr<UScenarioAssetPaletteCatalog> MakeDefaultCatalogReference();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Palette")
	TArray<FScenarioPaletteItemEntry> SpecialEntries;
};
