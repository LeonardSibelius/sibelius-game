// TableSounds.h
//
// THE SOUNDS OF THE GAMES — the living-room pinball and pool table, and the bowling alley
// (Walt, 2026-09-27).
//
// Synthesised in code at runtime, like the slot and poker machines (ProceduralPcm.h):
// no recordings, so nothing to license and nothing for the Steam AI disclosure to cover.
// Each sound is built once, on first use, and then replayed at whatever volume and pitch
// the hit calls for, so a hard break clacks and a soft kiss ticks.
//
// THE PINBALL IS A 1960s ELECTROMECHANICAL MACHINE, and it sounds like one: every score
// rings a chime - a bell bar struck by a solenoid - once per digit, the way a score motor
// pulses them out. Fifty points is five rings of the 10 chime, not one. APinballTable
// queues them; this file only makes the rings.

#pragma once

#include "CoreMinimal.h"

enum class ETableSound : uint8
{
	// Pinball
	Chime10,
	Chime100,
	Chime1000,
	Bumper,        // the pop bumper's solenoid: a hard, low thock
	Slingshot,     // a sharper snap
	FlipperUp,     // coil and bat: a wooden clack
	FlipperDown,   // the bat falling back to its stop
	Plunger,       // the spring let go
	KickoutIn,     // the ball dropping into a hole
	Drain,         // the ball rolling down under the apron
	ReelClick,     // one step of a score reel resetting

	// Pool
	CueStrike,     // leather tip on the cue ball
	BallClick,     // ball on ball: phenolic, bright, short
	CushionThud,   // ball into rubber
	PocketDrop,    // into the pocket, with a rattle

	// Bowling
	BowlRelease,   // the ball set down on the lane
	BowlRoll,      // its rumble down sixty feet of maple, fading as it reaches the pins
	PinClatter,    // wood on wood: the crash of the rack

	// Chess
	ChessPlace,    // a felted boxwood piece set down on the board: a soft, low tok
	ChessCapture,  // the taker set down hard where the taken piece stood

	// The Transhuman Grand Prix
	KartBeep,      // the countdown's 3, 2, 1
	KartGo,        // and GO, an octave up
	KartBump,      // kart on wall, kart on kart
	KartPickup,    // an item out of a box: two bright notes
	KartBoost,     // a whoosh that climbs
	KartSlap,      // the slap landing
	KartChest,     // Refactor: a shimmer down into a heavy chest
	KartLap,       // a new lap: three rising notes
	KartFinish,    // over the line: the fanfare

	// Go
	GoPlace,       // a slate stone snapped down on a kaya board: a bright click and the wood's ring
	GoCapture,     // the taken stones lifted off, clicking together in the hand

	// Lantern Burst (Kaia's jungle)
	LanternShoot,  // the orb let go: a soft rising whoosh
	LanternStick,  // glass set against glass: a small high tink
	LanternPop,    // a glass bubble bursting: a quick falling bloop and a bright ring
	LanternFall,   // orbs tumbling down: a scatter of tinks
	LanternLower,  // the crown let down a link: chain rattle, then a thud
	LanternSpark,  // the spark going off: a crackle
	LanternOver,   // the lantern on the embers: four notes falling

	Count
};

class USoundWaveProcedural;

namespace TableSounds
{
	/** Plays one sound, flat (2D): the player is standing at the table either way.
	    Volume 0..1-ish; Pitch 1 is as built, and a little randomness keeps a rack of
	    clicks from sounding like one sample repeated. */
	void Play(const UObject* WorldContext, ETableSound Sound, float Volume, float Pitch = 1.0f);

	/** A kart engine that never stops: half a second of two-stroke buzz, every frequency in
	    it a whole number of cycles long, queued again each time it runs dry. Play it on an
	    audio component and ride its pitch with the speed. */
	USoundWaveProcedural* MakeKartEngine();
}
