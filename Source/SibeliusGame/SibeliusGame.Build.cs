// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SibeliusGame : ModuleRules
{
	public SibeliusGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore"
		});

		// SIB-34 Path A: the cathedral slot cabinet embeds the real Celestial
		// Fortune web build via UE's Chromium widget (WebBrowserWidget plugin).
		PrivateDependencyModuleNames.AddRange(new string[] {
			"WebBrowserWidget",
			"WebBrowser"
		});

// SIB-19 smoke-test commandlet uses UEditorLoadingAndSavingUtils (editor-only)
if (Target.bBuildEditor)
{
PrivateDependencyModuleNames.Add("UnrealEd");
}

		PublicIncludePaths.AddRange(new string[] {
			"SibeliusGame",
			"SibeliusGame/Variant_Horror",
			"SibeliusGame/Variant_Horror/UI",
			"SibeliusGame/Variant_Shooter",
			"SibeliusGame/Variant_Shooter/AI",
			"SibeliusGame/Variant_Shooter/UI",
			"SibeliusGame/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}

