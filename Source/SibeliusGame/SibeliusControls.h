// SibeliusControls.h — EVERY KEY THE GAME BINDS, IN ONE PLACE.
//
// ===========================================================================
// WHY THIS FILE EXISTS.
//
// There used to be two lists of the same facts: UGameMenuWidget::BuildControlsTab built
// one from a row table, and Content/Journal/HOW_TO_PLAY.md carried another as prose. By
// the time anyone checked they had drifted in BOTH directions:
//
//     N N   journal missing   menu has it
//     >     journal missing   menu has it
//     H     journal has it    menu missing
//
// Walt found the first two by playing the shipped 0.9.9.2 and noticing that [>] was in
// the menu and not the guide. The third had been wrong for longer and nobody had looked.
//
// Two sources of truth for one set of facts drift every single time. This is the one
// source; the menu and the journal are two VIEWS of it.
//
// ---------------------------------------------------------------------------
// WHAT EACH VIEW CAN DO THAT THE OTHER CANNOT.
//
// The MENU greys out what the player has not earned and says what is missing - a power
// they do not have yet, or a city that opens when the Architects fall. Prose cannot do
// that, which is why the journal's copy was always going to be the poorer relation.
//
// The JOURNAL is a flat printed list a stuck player can read end to end without the
// game telling them what they are allowed to know. Both are worth having. Neither is
// worth maintaining twice.
//
// ---------------------------------------------------------------------------
// ADDING A KEY. Add one row to BuildRows() in the .cpp. Both screens pick it up. There
// is no second place, and that is the entire point of the file.

#pragma once

#include "CoreMinimal.h"

namespace SibeliusControls
{
	/** One line of the control list. */
	struct FControlRow
	{
		/** As printed: "W A S D / mouse", "V (hold)", "N N". */
		FString Keys;

		/** What it does, when the player is allowed to know. */
		FString Action;

		/** False = the player has not earned this yet; show Locked instead of Action. */
		bool bShown = true;

		/* WHAT A LOCKED ROW SAYS. Defaulted, because for a power the honest answer has
		   always been the same one - but the city is not a power, and "a power you have
		   not earned yet" would be a lie about it. Walt asked that the player KNOW a
		   city is waiting; a greyed row that says so from the first minute is how a
		   control list makes a promise rather than just refusing. */
		FString Locked = TEXT("(a power you have not earned yet)");
	};

	/**
	 * Every bound key, with the earned/unearned state resolved for the current save.
	 *
	 * Needs a world context because two rows depend on progression: the six power verbs
	 * read UProgressionSubsystem, and [>] asks ASibeliusGameCharacter::IsCityOpen - the
	 * same call the key itself makes, so the list cannot claim something the key would
	 * refuse.
	 */
	SIBELIUSGAME_API TArray<FControlRow> BuildRows(const UObject* WorldContext);

	/**
	 * The same rows as a monospaced block for the Journal, key column padded to KeyWidth.
	 *
	 * The journal has no columns - it is one text block - so alignment has to be spaces.
	 * 19 matches the widest entry ("W A S D + mouse") with room to breathe.
	 */
	SIBELIUSGAME_API FString ComposeAsText(const UObject* WorldContext, int32 KeyWidth = 19);
}
