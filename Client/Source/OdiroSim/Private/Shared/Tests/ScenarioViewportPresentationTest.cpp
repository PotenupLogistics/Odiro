#if WITH_DEV_AUTOMATION_TESTS

#include "Shared/ScenarioViewportPresentation.h"

#include "Misc/AutomationTest.h"

namespace
{
	// Restores the global grey-background setting after a test mutates it.
	struct FScopedScenarioGreyBackgroundSetting
	{
		// Value observed before the test changed the global viewport presentation flag.
		bool bPreviousValue = true;

		// Captures the current global setting before test-specific overrides.
		FScopedScenarioGreyBackgroundSetting()
			: bPreviousValue(FScenarioViewportPresentation::bUseGreyBackgroundPostProcess)
		{
		}

		// Restores the global setting so later tests and editor code see the original value.
		~FScopedScenarioGreyBackgroundSetting()
		{
			FScenarioViewportPresentation::bUseGreyBackgroundPostProcess = bPreviousValue;
		}
	};

	// Checks whether a preload list contains a specific soft object path.
	bool ContainsPreloadPath(
		const TArray<TSoftObjectPtr<UObject>>& preloadAssets,
		const FSoftObjectPath& expectedPath)
	{
		return preloadAssets.ContainsByPredicate(
			[&expectedPath](const TSoftObjectPtr<UObject>& preloadAsset)
			{
				return preloadAsset.ToSoftObjectPath() == expectedPath;
			});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScenarioViewportPresentationGreyBackgroundToggleTest,
	"OdiroSim.Shared.ViewportPresentation.GreyBackgroundToggle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FScenarioViewportPresentationGreyBackgroundToggleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FScopedScenarioGreyBackgroundSetting scopedSetting;

	FScenarioViewportPresentation::bUseGreyBackgroundPostProcess = true;
	const TSoftObjectPtr<UObject> enabledGreyAsset =
		FScenarioViewportPresentation::MakeGreyBackgroundPreloadAsset();
	const FSoftObjectPath enabledGreyPath = enabledGreyAsset.ToSoftObjectPath();
	TestFalse(TEXT("enabled grey preload asset is configured"), enabledGreyAsset.IsNull());
	TestTrue(
		TEXT("enabled scenario preload assets include grey background"),
		ContainsPreloadPath(
			FScenarioViewportPresentation::MakeScenarioMapPreloadAssets(),
			enabledGreyPath));

	FScenarioViewportPresentation::bUseGreyBackgroundPostProcess = false;
	TestTrue(
		TEXT("disabled grey preload asset is empty"),
		FScenarioViewportPresentation::MakeGreyBackgroundPreloadAsset().IsNull());
	TestFalse(
		TEXT("disabled scenario preload assets exclude grey background"),
		ContainsPreloadPath(
			FScenarioViewportPresentation::MakeScenarioMapPreloadAssets(),
			enabledGreyPath));
	TestNull(
		TEXT("disabled loaded grey material is unavailable"),
		FScenarioViewportPresentation::ResolveLoadedGreyBackgroundPostProcessMaterial());
	TestNull(
		TEXT("disabled grey material does not sync load"),
		FScenarioViewportPresentation::ResolveOrLoadGreyBackgroundPostProcessMaterial());

	return true;
}

#endif
