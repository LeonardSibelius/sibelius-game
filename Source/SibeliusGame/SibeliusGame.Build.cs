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
			// GGameThreadTime/GRenderThreadTime/GGPUFrameTime for the swarm ridge probe -
			// the same four numbers `stat unit` shows, logged instead of drawn.
			"RenderCore",
			// GGPUFrameTime is RHI, not RenderCore - the other two are RenderCore.
			"RHI",
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

		// WebBrowserWidget and WebBrowser were here until v1.1.3, for the cathedral
		// cabinet's embedded Celestial Fortune. Removing them is what drops 334 MB of
		// Chromium from the shipped build — the plugin stages libcef.dll and its
		// resources whether or not a single line of code ever calls into them.
		PrivateDependencyModuleNames.AddRange(new string[] {
			// NPC chat step 6: the proxy transport. HTTP for the call, Json for the
			// contract. JsonUtilities is deliberately absent - the bodies are small and
			// hand-built, and a struct reflector would hide the shape from the gate.
			"HTTP",
			"Json",
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

