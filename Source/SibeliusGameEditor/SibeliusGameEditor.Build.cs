// SibeliusGameEditor.Build.cs — editor-only module (SIB-42 / PK12).
//
// Home of the nine smoke-test commandlets (the gates). They lived in the
// runtime module guarded by #if WITH_EDITOR, which the editor target
// tolerated — but UHT generates reflection code UNCONDITIONALLY, so the
// first-ever GAME-target build (packaging) exploded in the .gen.cpp files.
// Editor-only UCLASSes belong in an editor-only module; now they're in one.

using UnrealBuildTool;

public class SibeliusGameEditor : ModuleRules
{
	public SibeliusGameEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"SibeliusGame"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"UMG",
			"Slate",
			"SlateCore",
			// PK13: the commandlets construct/load these classes directly
			// (UInputAction/UInputMappingContext smoke checks, NavMesh bounds).
			"EnhancedInput",
			"NavigationSystem",
			"AIModule"
		});

		// Same FormatStringSan workaround as the game module (PK11).
		bValidateFormatStrings = false;
	}
}
