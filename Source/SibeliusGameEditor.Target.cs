// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class SibeliusGameEditorTarget : TargetRules
{
	public SibeliusGameEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		ExtraModuleNames.Add("SibeliusGame");
		ExtraModuleNames.Add("SibeliusGameEditor");   // SIB-42/PK12: the smoke-test gates
	}
}
