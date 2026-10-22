// UniverseSim.h
//
// LEGACY (2026-10-22): the universe game was dropped (see UniverseRecord.h). Kept for the types a
// saved FUniverseRecord is made of; nothing in the game runs it.
//
// THE UNIVERSE A PLAYER GROWS in the AI city (L_Genesis, AUniverseForge) - Seed 1 (Walt,
// 2026-10-15: "go ahead with growing a universe"). Three dials, a seed, and a history: how far
// the universe got and how it ended. Pure standard C++, no engine: tested standalone by
// Tools/Tests/run_universe_sim_tests.ps1, as the house method has it (BasketballSim, ChessEngine).
//
// THE GAME DECIDES, NEVER A MODEL. Every line of a universe's chronicle is written here, from
// templates and the dials; nothing is asked of the chat server.
//
// THE DIALS, each 0..1:
//   Gravity - how hard matter pulls together. Too little and nothing ever gathers (the cold
//             fog); too much and it all falls back into itself (the crunch).
//   Light   - how fiercely its stars burn. Too weak and they cannot warm a world (the dim);
//             too strong and they burn out before anything can live (the burn). It is also
//             their colour, red to blue.
//   Time    - how long the universe lasts. Too short and it ends before life can look up.
// A seed moves every threshold a little, so a setting near an edge is a gamble and two
// universes from the same dials are never quite the same.
//
// THE STAGES it is shown in, each a part of AUniverseForge's growing:
//   Bang, FirstLight, FirstStars, Galaxies, World, Life, Machines.
// The best ending: its life builds machines, and the machines begin a city round their star -
// the city Leonard is standing in, again, one universe down.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct FUniverseDials
{
	double Gravity = 0.5;
	double Light = 0.5;
	double Time = 0.5;
};

enum class EUniverseStage : int
{
	Bang = 0,
	FirstLight,
	FirstStars,
	Galaxies,
	World,
	Life,
	Machines,
	Count
};

enum class EUniverseEnding : int
{
	ColdFog,      // gravity too weak
	Crunch,       // gravity too strong
	Burn,         // light too strong
	Dim,          // light too weak
	TooYoung,     // time too short
	LookedBack,   // its life built machines
};

struct FUniverseHistory
{
	FUniverseDials Dials;
	uint64_t Seed = 0;
	std::string Name;
	EUniverseEnding Ending = EUniverseEnding::LookedBack;
	EUniverseStage LastStage = EUniverseStage::Bang;   // the last stage it reached, and is shown
	int Galaxies = 0;            // how many it made (0 if none)
	double StarHeat = 0.5;       // its stars' colour, 0 red .. 1 blue
	double AgeBillions = 0.0;    // how old it was when it ended
	bool bWorldThawed = false;   // its one world warmed (false: frozen)
	std::vector<std::string> Chronicle;   // one line per stage reached, then the ending's
	std::string EndingTitle;     // THE COLD FOG ... IT LOOKED BACK
	std::string EndingLine;      // a sentence for the card
};

// Its whole history, from the dials (clamped to 0..1) and a seed. The same pair always grows
// the same universe.
// Exported from the game module for the editor's smoke tests; the standalone test build
// (Tools/Tests) has no SIBELIUSGAME_API and compiles it plain.
#if defined(SIBELIUSGAME_API)
SIBELIUSGAME_API
#endif
FUniverseHistory GrowUniverse(const FUniverseDials& Dials, uint64_t Seed);

// A universe's name: two or three syllables, from the seed alone.
std::string UniverseName(uint64_t Seed);

// "red", "orange", "yellow", "white", "blue" - a star of this heat.
const char* StarColourName(double Heat);

const char* UniverseStageName(EUniverseStage Stage);
const char* UniverseEndingName(EUniverseEnding Ending);
