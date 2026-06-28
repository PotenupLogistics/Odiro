#include "Scenario/Data/ScenarioCityBlockCatalog.h"

#include "UObject/SoftObjectPath.h"

namespace
{
	const FSoftObjectPath DefaultCityBlockCatalogPath(
		TEXT("/Game/Data/Scenario/DA_ScenarioCityBlockCatalog.DA_ScenarioCityBlockCatalog"));
}

TSoftObjectPtr<UScenarioCityBlockCatalog> UScenarioCityBlockCatalog::MakeDefaultCatalogReference()
{
	return TSoftObjectPtr<UScenarioCityBlockCatalog>(DefaultCityBlockCatalogPath);
}

bool UScenarioCityBlockCatalog::FindBlockEntryById(
	FName blockId,
	FScenarioCityBlockCatalogEntry& outBlockEntry) const
{
	outBlockEntry = FScenarioCityBlockCatalogEntry();
	if (blockId.IsNone())
	{
		return false;
	}

	for (const FScenarioCityBlockCatalogEntry& blockEntry : Entries)
	{
		if (blockEntry.BlockId == blockId)
		{
			outBlockEntry = blockEntry;
			return true;
		}
	}

	return false;
}

void UScenarioCityBlockCatalog::GetEntriesForRole(
	EScenarioCityBlockRole role,
	TArray<FScenarioCityBlockCatalogEntry>& outEntries) const
{
	outEntries.Reset();
	for (const FScenarioCityBlockCatalogEntry& blockEntry : Entries)
	{
		if (blockEntry.Role == role)
		{
			outEntries.Add(blockEntry);
		}
	}
}
