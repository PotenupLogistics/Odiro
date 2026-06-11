#include "Scenario/Data/ScenarioAssetPaletteCatalog.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	const FSoftObjectPath DefaultAssetPaletteCatalogPath(
		TEXT("/Game/Data/Scenario/DA_ScenarioAssetPaletteCatalog.DA_ScenarioAssetPaletteCatalog"));
}

TSoftObjectPtr<UScenarioAssetPaletteCatalog> UScenarioAssetPaletteCatalog::MakeDefaultCatalogReference()
{
	return TSoftObjectPtr<UScenarioAssetPaletteCatalog>(DefaultAssetPaletteCatalogPath);
}
