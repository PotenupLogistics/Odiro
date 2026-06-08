#include "Episode/Data/EpisodeStaticObstaclePropCatalog.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	const FSoftObjectPath DefaultStaticObstaclePropCatalogPath(
		TEXT("/Game/Data/Episode/DA_EpisodeStaticObstaclePropCatalog.DA_EpisodeStaticObstaclePropCatalog"));
}

TSoftObjectPtr<UEpisodeStaticObstaclePropCatalog> UEpisodeStaticObstaclePropCatalog::MakeDefaultCatalogReference()
{
	return TSoftObjectPtr<UEpisodeStaticObstaclePropCatalog>(DefaultStaticObstaclePropCatalogPath);
}

bool UEpisodeStaticObstaclePropCatalog::FindPropEntryById(
	FName propId,
	FEpisodeStaticObstaclePropEntry& outPropEntry) const
{
	if (propId.IsNone()) return false;

	for (const FEpisodeStaticObstaclePropEntry& propEntry : Entries)
	{
		if (propEntry.PropId == propId)
		{
			outPropEntry = propEntry;
			return true;
		}
	}

	return false;
}
