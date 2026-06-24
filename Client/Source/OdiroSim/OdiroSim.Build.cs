// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OdiroSim : ModuleRules
{
	public OdiroSim(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "SlateCore",
			"CommonUI", "ModelViewViewModel", "FieldNotification", "ChaosVehicles", "Json", "JsonUtilities", "HTTP", "DeveloperSettings" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate" });

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("DesktopPlatform");
		}
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
