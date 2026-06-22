// SauceDoor.h
//
// THE SAUCE DOOR — the "Many Worlds" kitchen door. A subclass of AHiddenDoor, so it
// REUSES the game's existing hidden-door reveal exactly: hold Code Vision (V) and the
// panel shimmers into view (custom-depth outline), same as the office "Sauce of All
// Knowledge" door and the attic "Carousel of Fates" door.
//
// TRAVEL: it now behaves as a PLAIN travel door — on reveal + E it OpenLevel()s its
// inherited TravelTargetLevel (set on the placed actor by hand to the fixed Poplar forest
// L_Poplar_Forest), exactly like the office obelisk / its AHiddenDoor parent. The old
// curio / cabinet / AElsewhereBuilder / UElsewhereSubsystem "roll a fresh Elsewhere" flow
// is SET ASIDE — those classes still exist but this door no longer drives them. The only
// ASauceDoor specialisations left are cosmetic: a default doorway slab, the hand-dialed
// "Many Worlds" sign placement, and the "Step through [E]" travel prompt.
//
// Arming == revealed: there is no separate Sauce-completion gate here (the office has no
// Sauce-feed); the door is "armed" whenever Code Vision reveals it, matching every other
// hidden door in the game.

#pragma once

#include "CoreMinimal.h"
#include "HiddenDoor.h"
#include "SauceDoor.generated.h"

UCLASS()
class SIBELIUSGAME_API ASauceDoor : public AHiddenDoor
{
	GENERATED_BODY()

public:
	ASauceDoor();
};
