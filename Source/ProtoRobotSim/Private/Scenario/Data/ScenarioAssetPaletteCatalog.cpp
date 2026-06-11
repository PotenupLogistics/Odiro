#include "Scenario/Data/ScenarioAssetPaletteCatalog.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	const FSoftObjectPath DefaultAssetPaletteCatalogPath(
		TEXT("/Game/Data/Episode/DA_EpisodeAssetPaletteCatalog.DA_EpisodeAssetPaletteCatalog"));
}

TSoftObjectPtr<UScenarioAssetPaletteCatalog> UScenarioAssetPaletteCatalog::MakeDefaultCatalogReference()
{
	return TSoftObjectPtr<UScenarioAssetPaletteCatalog>(DefaultAssetPaletteCatalogPath);
}
