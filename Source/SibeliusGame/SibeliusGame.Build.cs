// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SibeliusGame : ModuleRules
{
	public SibeliusGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// SIB-42 / PK11: the GAME target failed to package with error C2971
		// ("TAtArgPos ... non-static storage duration") — UE's compile-time
		// format-string validator (FormatStringSan) miscompiling checkf() in
		// engine MovieScene headers under MSVC 14.44 + C++20. Known engine/
		// toolchain bug (same family as Inkpot #98). The editor target doesn't
		// trip it. Validation is a dev-time lint, safe to disable for this
		// module; revisit when Epic fixes the header or we bump toolchains.
		bValidateFormatStrings = false;

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
			"SlateCore",
			// SIB-47 PCG spike: real UPCGComponent in the Elsewhere builder (runtime module).
			"PCG",
			// v0.9.7.1: Niagara Fluids sauce (simmer / pour / pool).
			"Niagara"
		});

		// SIB-34 Path A: the cathedral slot cabinet embeds the real Celestial
		// Fortune web build via UE's Chromium widget (WebBrowserWidget plugin).
		PrivateDependencyModuleNames.AddRange(new string[] {
			"WebBrowserWidget",
			"WebBrowser",
			// Travel-door transition: animated loading screen during the blocking level load
			// (registered on PreLoadMap in SibeliusGame.cpp).
			"MoviePlayer",
			// APPEAL-R wild refactor: the Menagerie auto-scans animal-pack folders.
			"AssetRegistry",
			// MetaHuman grooms (Elise's hair blew out when talk-E yawed her).
			"HairStrandsCore",
			// AVideoCue: UMediaPlayer / UMediaTexture / UMediaSoundComponent for the
			// pre-rendered cutscenes (docs/CINEMATICS.md).
			"MediaAssets",
			// ASequenceCue: plays a Level Sequence live instead of a pre-rendered file.
			"LevelSequence",
			"MovieScene"
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

