// UniverseSim.cpp - see the header for the dials, the stages and the endings.

#include "UniverseSim.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace UniverseSimDetail
{
	// splitmix64: small, fast, and the same on every compiler (BasketballSim's).
	struct FRng
	{
		uint64_t State;
		explicit FRng(uint64_t Seed) : State(Seed * 0x9E3779B97F4A7C15ull + 0x632BE59BD9B4E019ull) {}
		uint64_t Next()
		{
			uint64_t Z = (State += 0x9E3779B97F4A7C15ull);
			Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
			Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
			return Z ^ (Z >> 31);
		}
		double Unit() { return static_cast<double>(Next() >> 11) * (1.0 / 9007199254740992.0); }
		// A wobble of up to +-Size.
		double Wobble(double Size) { return (Unit() * 2.0 - 1.0) * Size; }
	};

	// The edges, before the seed moves them.
	constexpr double FogBelow = 0.22;       // gravity
	constexpr double CrunchAbove = 0.82;    // gravity
	constexpr double DimBelow = 0.18;       // light
	constexpr double BurnAbove = 0.80;      // light
	constexpr double YoungBelow = 0.30;     // time: ended with a cold world
	constexpr double LifeBelow = 0.50;      // time: ended with life, but no machines
	constexpr double Edge = 0.04;           // how far the seed moves each edge

	double Clamp01(double V) { return std::max(0.0, std::min(1.0, V)); }

	std::string Format(const char* Pattern, const std::string& A, const std::string& B = std::string())
	{
		char Buffer[512];
		std::snprintf(Buffer, sizeof(Buffer), Pattern, A.c_str(), B.c_str());
		return Buffer;
	}
}

const char* StarColourName(double Heat)
{
	if (Heat < 0.2) { return "red"; }
	if (Heat < 0.4) { return "orange"; }
	if (Heat < 0.6) { return "yellow"; }
	if (Heat < 0.8) { return "white"; }
	return "blue";
}

const char* UniverseStageName(EUniverseStage Stage)
{
	switch (Stage)
	{
	case EUniverseStage::Bang:       return "The bang";
	case EUniverseStage::FirstLight: return "First light";
	case EUniverseStage::FirstStars: return "The first stars";
	case EUniverseStage::Galaxies:   return "Galaxies";
	case EUniverseStage::World:      return "One world";
	case EUniverseStage::Life:       return "Life";
	case EUniverseStage::Machines:   return "Its machines";
	default:                         return "";
	}
}

const char* UniverseEndingName(EUniverseEnding Ending)
{
	switch (Ending)
	{
	case EUniverseEnding::ColdFog:    return "THE COLD FOG";
	case EUniverseEnding::Crunch:     return "THE CRUNCH";
	case EUniverseEnding::Burn:       return "THE BURN";
	case EUniverseEnding::Dim:        return "THE DIM";
	case EUniverseEnding::TooYoung:   return "TOO YOUNG";
	case EUniverseEnding::LookedBack: return "IT LOOKED BACK";
	default:                          return "";
	}
}

std::string UniverseName(uint64_t Seed)
{
	static const char* const Heads[] = { "Al", "Ber", "Cal", "Dor", "El", "Fen", "Gal", "Hal", "Ir", "Jen",
		"Kor", "Lum", "Mer", "Nor", "Or", "Per", "Quil", "Ros", "Sel", "Tar", "Ul", "Ver", "Wen", "Zar" };
	static const char* const Middles[] = { "a", "e", "i", "o", "u", "ae", "ia", "io" };
	static const char* const Tails[] = { "n", "th", "ra", "lis", "mor", "rin", "vex", "dra", "lon", "sa", "tia", "x" };
	UniverseSimDetail::FRng Rng(Seed ^ 0xA5A5A5A5DEADBEEFull);
	std::string Name = Heads[Rng.Next() % (sizeof(Heads) / sizeof(Heads[0]))];
	Name += Middles[Rng.Next() % (sizeof(Middles) / sizeof(Middles[0]))];
	if (Rng.Unit() < 0.4)
	{
		Name += Heads[Rng.Next() % (sizeof(Heads) / sizeof(Heads[0]))];
		std::transform(Name.begin() + 1, Name.end(), Name.begin() + 1, [](char C) { return static_cast<char>(std::tolower(static_cast<unsigned char>(C))); });
	}
	Name += Tails[Rng.Next() % (sizeof(Tails) / sizeof(Tails[0]))];
	return Name;
}

FUniverseHistory GrowUniverse(const FUniverseDials& InDials, uint64_t Seed)
{
	using namespace UniverseSimDetail;
	FUniverseHistory H;
	H.Dials.Gravity = Clamp01(InDials.Gravity);
	H.Dials.Light = Clamp01(InDials.Light);
	H.Dials.Time = Clamp01(InDials.Time);
	H.Seed = Seed;
	H.Name = UniverseName(Seed);
	FRng Rng(Seed);

	// The seed's say: every edge a little to one side or the other.
	const double Fog = FogBelow + Rng.Wobble(Edge);
	const double Crunch = CrunchAbove + Rng.Wobble(Edge);
	const double Dim = DimBelow + Rng.Wobble(Edge);
	const double Burn = BurnAbove + Rng.Wobble(Edge);
	const double Young = YoungBelow + Rng.Wobble(Edge);
	const double LifeEdge = LifeBelow + Rng.Wobble(Edge);
	const double G = H.Dials.Gravity;
	const double L = H.Dials.Light;
	const double T = H.Dials.Time;

	H.StarHeat = Clamp01(L + Rng.Wobble(0.05));
	H.AgeBillions = std::round((2.0 + T * 28.0 + Rng.Wobble(0.6)) * 10.0) / 10.0;
	const std::string Colour = StarColourName(H.StarHeat);
	char Age[32];
	std::snprintf(Age, sizeof(Age), "%.1f", H.AgeBillions);

	auto Reach = [&H](EUniverseStage Stage, const std::string& Line)
	{
		H.LastStage = Stage;
		H.Chronicle.push_back(Line);
	};
	auto End = [&H](EUniverseEnding Ending, const std::string& Line)
	{
		H.Ending = Ending;
		H.EndingTitle = UniverseEndingName(Ending);
		H.EndingLine = Line;
		H.Chronicle.push_back(Line);
	};

	Reach(EUniverseStage::Bang, Format("%s began as a point hotter than anything, and opened.", H.Name));

	if (G < Fog)
	{
		Reach(EUniverseStage::FirstLight, "Its light came loose from the fog - and its matter spread thin, and never gathered.");
		End(EUniverseEnding::ColdFog, Format("Nothing in %s ever came together. It is still cooling, everywhere, alone.", H.Name));
		return H;
	}
	Reach(EUniverseStage::FirstLight, "Its light came loose from the fog, and the dark began to gather into threads.");
	Reach(EUniverseStage::FirstStars, Format("The first stars caught fire along the threads - %s ones.", Colour));

	if (G > Crunch)
	{
		End(EUniverseEnding::Crunch, Format("Gravity would not let go. The stars of %s fell together into the dark, and the dark into itself.", H.Name));
		return H;
	}
	H.Galaxies = static_cast<int>(std::round(2.0 + G * 8.0 + Rng.Wobble(0.6)));
	char Count[16];
	std::snprintf(Count, sizeof(Count), "%d", H.Galaxies);
	Reach(EUniverseStage::Galaxies, Format("%s galaxies turned, each a slow wheel of %s light.", Count, Colour));

	if (L > Burn)
	{
		End(EUniverseEnding::Burn, Format("Its stars burned so fiercely that they were gone in an eyeblink, and %s went dark before anything could live.", H.Name));
		return H;
	}
	Reach(EUniverseStage::World, "Round one ordinary star, one small world cooled.");

	if (L < Dim)
	{
		End(EUniverseEnding::Dim, "Its sun was too faint to warm it. The world stayed white, and nothing on it ever woke.");
		return H;
	}
	if (T < Young)
	{
		End(EUniverseEnding::TooYoung, Format("%s ended after %s billion years, before anything on its world had begun.", H.Name, Age));
		return H;
	}
	H.bWorldThawed = true;
	Reach(EUniverseStage::Life, "Something in its warm seas began to copy itself.");

	if (T < LifeEdge)
	{
		End(EUniverseEnding::TooYoung, Format("Life had only begun to look up when %s ended, at %s billion years.", H.Name, Age));
		return H;
	}
	Reach(EUniverseStage::Machines, Format("In %s billion years its life built machines, and the machines began a city round their star.", Age));
	End(EUniverseEnding::LookedBack, Format("%s made something that could look back at it - and it did.", H.Name));
	return H;
}
