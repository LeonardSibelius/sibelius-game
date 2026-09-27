// TableSounds.h
//
// THE SOUNDS OF THE LIVING-ROOM TABLES — the pinball and the pool table (Walt, 2026-09-27).
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
};

namespace TableSounds
{
	/** Plays one sound, flat (2D): the player is standing at the table either way.
	    Volume 0..1-ish; Pitch 1 is as built, and a little randomness keeps a rack of
	    clicks from sounding like one sample repeated. */
	void Play(const UObject* WorldContext, ETableSound Sound, float Volume, float Pitch = 1.0f);
}
