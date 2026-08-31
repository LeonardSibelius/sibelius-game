// SibeliusControls.cpp — see the header for why there is only one of these now.

#include "SibeliusControls.h"

#include "ProgressionSubsystem.h"
#include "ProgressionTypes.h"
#include "SibeliusGameCharacter.h"   // IsCityOpen - the [>] row asks the key itself

namespace SibeliusControls
{

TArray<FControlRow> BuildRows(const UObject* WorldContext)
{
	const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(WorldContext);
	auto Owned = [Progression](EPowerVerb Verb)
	{
		// No subsystem (headless, a smoke test, a commandlet) shows everything rather
		// than hiding everything - a control list that goes blank under a test harness
		// looks broken and teaches nothing.
		return !Progression || Progression->IsUnlocked(Verb);
	};

	const bool bCityOpen = ASibeliusGameCharacter::IsCityOpen(WorldContext);

	/* THE ORDER IS THE ORDER A PLAYER LEARNS THEM, not alphabetical and not the order
	   they were coded: move, then the two verbs every level uses, then the six powers in
	   the sequence the game grants them, then the screens, then the ways out. */
	return {
		{ TEXT("W A S D / mouse"), TEXT("move / look"), true },
		{ TEXT("E"), TEXT("interact — collect, doors, the cauldron, talk to an AI Agent"), true },
		{ TEXT("F"), TEXT("fight a Refuser / change an AI Agent's dance"), true },

		{ TEXT("V (hold)"), TEXT("Code Vision"), Owned(EPowerVerb::CodeVision) },
		{ TEXT("R"), TEXT("Refactor what you're looking at"), Owned(EPowerVerb::Refactor) },
		{ TEXT("C"), TEXT("Compile at a build site"), Owned(EPowerVerb::Compile) },
		{ TEXT("6 / 7 / 8"), TEXT("Test-Drive: branch / merge / discard"), Owned(EPowerVerb::TestDrive) },
		{ TEXT("0"), TEXT("Deploy (persist your edits)"), Owned(EPowerVerb::Deploy) },
		{ TEXT("G"), TEXT("Generate — type a request"), Owned(EPowerVerb::Generate) },

		{ TEXT("M"), TEXT("this menu"), true },
		{ TEXT("J"), TEXT("how to play"), true },
		{ TEXT("H"), TEXT("developer overlay (debug)"), true },

		{ TEXT("N N"), TEXT("new game (erases ALL progress)"), true },
		{ TEXT("O"), TEXT("back to the office (from any other world)"), true },
		{ TEXT(">"), TEXT("go to the city"), bCityOpen,
		  TEXT("a city is waiting - it opens when the Architects fall") },
		{ TEXT("Q Q"), TEXT("quit"), true },
	};
}

FString ComposeAsText(const UObject* WorldContext, int32 KeyWidth)
{
	FString Out = TEXT("\n\n\nEVERY KEY\n\n");

	for (const FControlRow& R : BuildRows(WorldContext))
	{
		FString Keys = R.Keys;
		while (Keys.Len() < KeyWidth)
		{
			Keys.AppendChar(TEXT(' '));
		}
		Out += FString::Printf(TEXT("  %s%s\n"), *Keys, R.bShown ? *R.Action : *R.Locked);
	}

	return Out;
}

}   // namespace SibeliusControls
