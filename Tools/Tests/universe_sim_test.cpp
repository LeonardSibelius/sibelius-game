// universe_sim_test.cpp - standalone tests for Source/SibeliusGame/UniverseSim.
//
// No engine, no editor, no cook. Run it with Tools/Tests/run_universe_sim_tests.ps1.
//
// Each test is a RULE of the universes Leonard grows in the AI city: the same seed grows the
// same universe; every ending can be reached, by the dial it belongs to; a universe stops at
// the stage its ending says; the middle of every dial almost always looks back; the seed moves
// the edges, so a setting near one is a gamble; every line of the chronicle is filled in.

#include "UniverseSim.h"

#include <cstdint>
#include <cstdio>
#include <string>

namespace
{
	int GPasses = 0;
	int GFailures = 0;

	void Check(bool bOk, const char* Name, const std::string& Detail = std::string())
	{
		std::printf("%s  %s%s%s\n", bOk ? "PASS" : "FAIL", Name, Detail.empty() ? "" : "  -- ", Detail.c_str());
		if (bOk)
		{
			++GPasses;
		}
		else
		{
			++GFailures;
		}
	}

	std::string Of(const FUniverseHistory& H)
	{
		char Buffer[256];
		std::snprintf(Buffer, sizeof(Buffer), "%s: %s at stage %d, %d galaxies, %.1f billion years",
			H.Name.c_str(), H.EndingTitle.c_str(), static_cast<int>(H.LastStage), H.Galaxies, H.AgeBillions);
		return Buffer;
	}

	int CountEnding(const FUniverseDials& D, EUniverseEnding Ending, int Seeds)
	{
		int N = 0;
		for (int S = 1; S <= Seeds; ++S)
		{
			N += GrowUniverse(D, static_cast<uint64_t>(S)).Ending == Ending ? 1 : 0;
		}
		return N;
	}
}

int main()
{
	const FUniverseDials Middle{ 0.5, 0.5, 0.75 };

	// --- the same seed grows the same universe -----------------------------------------------
	{
		const FUniverseHistory A = GrowUniverse(Middle, 42);
		const FUniverseHistory B = GrowUniverse(Middle, 42);
		Check(A.Name == B.Name && A.Ending == B.Ending && A.Galaxies == B.Galaxies && A.Chronicle == B.Chronicle,
			"the same dials and seed grow the same universe", Of(A));
		bool bDifferent = false;
		for (uint64_t S = 1; S < 20 && !bDifferent; ++S)
		{
			bDifferent = GrowUniverse(Middle, S).Name != A.Name;
		}
		Check(bDifferent, "another seed is another universe (another name)");
	}

	// --- every ending, by its own dial ------------------------------------------------------------
	{
		const FUniverseHistory Fog = GrowUniverse({ 0.05, 0.5, 0.8 }, 7);
		Check(Fog.Ending == EUniverseEnding::ColdFog && Fog.LastStage == EUniverseStage::FirstLight && Fog.Galaxies == 0,
			"too little gravity: the cold fog, after first light", Of(Fog));
		const FUniverseHistory Crunch = GrowUniverse({ 0.97, 0.5, 0.8 }, 7);
		Check(Crunch.Ending == EUniverseEnding::Crunch && Crunch.LastStage == EUniverseStage::FirstStars && Crunch.Galaxies == 0,
			"too much gravity: the crunch, after the first stars", Of(Crunch));
		const FUniverseHistory Burn = GrowUniverse({ 0.5, 0.97, 0.8 }, 7);
		Check(Burn.Ending == EUniverseEnding::Burn && Burn.LastStage == EUniverseStage::Galaxies && Burn.Galaxies > 0,
			"light too strong: the burn, after the galaxies", Of(Burn));
		const FUniverseHistory Dim = GrowUniverse({ 0.5, 0.03, 0.8 }, 7);
		Check(Dim.Ending == EUniverseEnding::Dim && Dim.LastStage == EUniverseStage::World && !Dim.bWorldThawed,
			"light too weak: the dim, a frozen world", Of(Dim));
		const FUniverseHistory Young = GrowUniverse({ 0.5, 0.5, 0.05 }, 7);
		Check(Young.Ending == EUniverseEnding::TooYoung && Young.LastStage == EUniverseStage::World,
			"too little time: too young, before life", Of(Young));
		const FUniverseHistory Brief = GrowUniverse({ 0.5, 0.5, 0.40 }, 7);
		Check(Brief.Ending == EUniverseEnding::TooYoung && Brief.LastStage == EUniverseStage::Life && Brief.bWorldThawed,
			"a little more time: life, but no machines", Of(Brief));
		const FUniverseHistory Back = GrowUniverse(Middle, 7);
		Check(Back.Ending == EUniverseEnding::LookedBack && Back.LastStage == EUniverseStage::Machines && Back.bWorldThawed,
			"the middle of every dial: it looked back, with machines", Of(Back));
	}

	// --- a universe stops where its ending says, and says so in order ----------------------------
	{
		bool bOk = true;
		std::string Bad;
		for (int G = 0; G <= 10 && bOk; ++G)
		{
			for (int L = 0; L <= 10 && bOk; ++L)
			{
				for (int T = 0; T <= 10 && bOk; ++T)
				{
					const FUniverseHistory H = GrowUniverse({ G / 10.0, L / 10.0, T / 10.0 }, static_cast<uint64_t>(G * 121 + L * 11 + T + 1));
					// One line per stage reached, then the ending's line.
					const bool bLines = static_cast<int>(H.Chronicle.size()) == static_cast<int>(H.LastStage) + 2;
					const bool bFilled = !H.Name.empty() && !H.EndingTitle.empty() && !H.EndingLine.empty()
						&& H.Chronicle.back() == H.EndingLine;
					const bool bMachines = (H.Ending == EUniverseEnding::LookedBack) == (H.LastStage == EUniverseStage::Machines);
					bool bNoHoles = true;
					for (const std::string& Line : H.Chronicle)
					{
						bNoHoles = bNoHoles && !Line.empty() && Line.find("%") == std::string::npos;
					}
					if (!(bLines && bFilled && bMachines && bNoHoles))
					{
						bOk = false;
						Bad = Of(H);
					}
				}
			}
		}
		Check(bOk, "over the whole dial cube: one line a stage, the ending last, machines only when it looked back", Bad);
	}

	// --- the middle almost always looks back; the edges are gambles --------------------------------
	{
		const int Back = CountEnding(Middle, EUniverseEnding::LookedBack, 200);
		Check(Back == 200, "the middle of every dial looks back in 200 seeds of 200", std::to_string(Back));
		const int EdgeFog = CountEnding({ 0.22, 0.5, 0.8 }, EUniverseEnding::ColdFog, 400);
		Check(EdgeFog > 100 && EdgeFog < 300, "gravity on the fog's edge: a gamble, either way", std::to_string(EdgeFog) + " of 400 fogs");
		const int EdgeBurn = CountEnding({ 0.5, 0.80, 0.8 }, EUniverseEnding::Burn, 400);
		Check(EdgeBurn > 100 && EdgeBurn < 300, "light on the burn's edge: a gamble, either way", std::to_string(EdgeBurn) + " of 400 burns");
	}

	// --- the numbers the forge draws -----------------------------------------------------------------
	{
		bool bGalaxies = true;
		for (int G = 3; G <= 7; ++G)
		{
			const FUniverseHistory H = GrowUniverse({ G / 10.0, 0.5, 0.8 }, 3);
			bGalaxies = bGalaxies && H.Galaxies >= 3 && H.Galaxies <= 9;
		}
		Check(bGalaxies, "gravity that makes galaxies makes three to nine of them");
		const FUniverseHistory More = GrowUniverse({ 0.75, 0.5, 0.8 }, 5);
		const FUniverseHistory Fewer = GrowUniverse({ 0.3, 0.5, 0.8 }, 5);
		Check(More.Galaxies > Fewer.Galaxies, "more gravity gathers more galaxies", std::to_string(Fewer.Galaxies) + " vs " + std::to_string(More.Galaxies));
		Check(GrowUniverse({ 0.5, 0.25, 0.8 }, 9).StarHeat < GrowUniverse({ 0.5, 0.75, 0.8 }, 9).StarHeat,
			"more light burns bluer stars");
		Check(GrowUniverse({ 0.5, 0.5, 0.95 }, 9).AgeBillions > GrowUniverse({ 0.5, 0.5, 0.55 }, 9).AgeBillions,
			"more time, an older universe");
		const FUniverseHistory Wild = GrowUniverse({ -3.0, 7.0, 0.5 }, 1);
		Check(Wild.Dials.Gravity == 0.0 && Wild.Dials.Light == 1.0, "dials outside 0..1 are clamped");
	}

	std::printf("\n%d passed, %d failed\n", GPasses, GFailures);
	return GFailures == 0 ? 0 : 1;
}
