#include "Episode/Data/EpisodeAssetPaletteCatalog.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	const FSoftObjectPath DefaultAssetPaletteCatalogPath(
		TEXT("/Game/Data/Episode/DA_EpisodeAssetPaletteCatalog.DA_EpisodeAssetPaletteCatalog"));
}

TSoftObjectPtr<UEpisodeAssetPaletteCatalog> UEpisodeAssetPaletteCatalog::MakeDefaultCatalogReference()
{
	return TSoftObjectPtr<UEpisodeAssetPaletteCatalog>(DefaultAssetPaletteCatalogPath);
}
