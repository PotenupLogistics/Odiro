#include "Scenario/Data/ScenarioStaticObstaclePropCatalog.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	const FSoftObjectPath DefaultStaticObstaclePropCatalogPath(
		TEXT("/Game/Data/Scenario/DA_ScenarioStaticObstaclePropCatalog.DA_ScenarioStaticObstaclePropCatalog"));
}

TSoftObjectPtr<UScenarioStaticObstaclePropCatalog> UScenarioStaticObstaclePropCatalog::MakeDefaultCatalogReference()
{
	return TSoftObjectPtr<UScenarioStaticObstaclePropCatalog>(DefaultStaticObstaclePropCatalogPath);
}

bool UScenarioStaticObstaclePropCatalog::FindPropEntryById(
	FName propId,
	FScenarioStaticObstaclePropEntry& outPropEntry) const
{
	if (propId.IsNone()) return false;

	for (const FScenarioStaticObstaclePropEntry& propEntry : Entries)
	{
		if (propEntry.PropId == propId)
		{
			outPropEntry = propEntry;
			return true;
		}
	}

	return false;
}
