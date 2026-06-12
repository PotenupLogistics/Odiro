// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ProtoRobotSim : ModuleRules
{
	public ProtoRobotSim(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "SlateCore",
			"ChaosVehicles", "Json", "JsonUtilities", "HTTP", "DeveloperSettings" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
