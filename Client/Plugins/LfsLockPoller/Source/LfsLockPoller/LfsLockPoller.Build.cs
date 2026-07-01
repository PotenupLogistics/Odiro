using UnrealBuildTool;

public class LfsLockPoller : ModuleRules
{
	public LfsLockPoller(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"DeveloperSettings",
				"Engine",
				"Json"
			}
		);
	}
}
