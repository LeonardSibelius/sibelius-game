// THE GIST's rules, held to docs/GIST_GENERATOR_PLAN.md section 4. Run by run_gist_tests.ps1,
// which passes the repo root so the real word lists and Mrs. Hall's blocklist are tested.
#include "GistRules.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

using namespace Gist;

static int Failures = 0;
static void Expect(bool bOk, const std::string& What)
{
	if (!bOk) { ++Failures; if (Failures <= 40) std::cout << "FAIL: " << What << "\n"; }
}

static std::string ReadFile(const std::string& Path)
{
	std::ifstream In(Path, std::ios::binary);
	if (!In) { std::cout << "cannot read " << Path << "\n"; std::exit(2); }
	std::stringstream S;
	S << In.rdbuf();
	return S.str();
}

static std::string Serialize(const FRecipe& R)
{
	std::ostringstream S;
	S.setf(std::ios::fixed);
	S.precision(3);
	S << R.bRefused << R.bEmpty << R.bContraption << '|' << R.Archetype << '|' << R.Behaviours << '|' << R.Copies << '|'
	  << R.Scale << '|' << R.Title << '|' << R.Echo << '|' << R.Tagline << '\n';
	for (const FPart& P : R.Parts)
		S << int(P.Shape) << ' ' << int(P.Finish) << ' ' << int(P.Role) << ' ' << P.Colour.R << ' ' << P.Colour.G << ' ' << P.Colour.B
		  << ' ' << P.Location.X << ' ' << P.Location.Y << ' ' << P.Location.Z << ' ' << P.Rotation.X << ' ' << P.Rotation.Y
		  << ' ' << P.Rotation.Z << ' ' << P.Size.X << ' ' << P.Size.Y << ' ' << P.Size.Z << '\n';
	return S.str();
}

static int CodePoints(const std::string& S)
{
	int N = 0;
	for (const char C : S) if ((static_cast<unsigned char>(C) & 0xC0) != 0x80) ++N;
	return N;
}

static int Count(const FRecipe& R, ERole Role)
{
	int N = 0;
	for (const FPart& P : R.Parts) if (P.Role == Role) ++N;
	return N;
}

// The invariants every built gist must keep, whatever was typed.
static void Sound(const FRecipe& R, const std::string& Prompt)
{
	if (R.bRefused || R.bEmpty) return;
	const std::string At = " [" + Prompt + "]";
	Expect(!R.Parts.empty(), "builds something" + At);
	Expect(static_cast<int>(R.Parts.size()) <= MaxParts, "at most 64 parts" + At);
	Expect(R.Copies >= 1 && R.Copies <= MaxCopies, "1 to 12 copies" + At);
	Expect(Count(R, ERole::Eye) >= 2 && Count(R, ERole::Pupil) >= 2, "googly eyes" + At);
	FVec Min, Max;
	Bounds(R.Parts, Min, Max);
	const float Limit = (R.bGiant ? GiantRoom : Room) + 0.5f;
	Expect(Max.X - Min.X <= Limit && Max.Y - Min.Y <= Limit && Max.Z - Min.Z <= Limit, "fits its room" + At);
	Expect(std::fabs(Min.Z) < 0.5f, "stands on the floor" + At);
	Expect(std::fabs(Min.X + Max.X) < 1.0f && std::fabs(Min.Y + Max.Y) < 1.0f, "centred" + At);
	for (const FPart& P : R.Parts)
	{
		const bool bFinite = std::isfinite(P.Location.X) && std::isfinite(P.Location.Y) && std::isfinite(P.Location.Z)
			&& std::isfinite(P.Size.X) && std::isfinite(P.Size.Y) && std::isfinite(P.Size.Z);
		Expect(bFinite && P.Size.X > 0 && P.Size.Y > 0 && P.Size.Z > 0, "finite, positive parts" + At);
		Expect(P.Colour.R >= 0 && P.Colour.R <= 1.01f && P.Colour.G >= 0 && P.Colour.G <= 1.01f && P.Colour.B >= 0 && P.Colour.B <= 1.01f,
			"colours in range" + At);
	}
	Expect(R.Title.find("THE GIST") == 0, "plaque says THE GIST" + At);
	Expect(CodePoints(R.Echo) <= EchoChars, "echo at most 40 characters" + At);
	Expect(!R.Tagline.empty(), "a tagline" + At);
}

int main(int Argc, char** Argv)
{
	const std::string Root = Argc > 1 ? Argv[1] : ".";
	const FVocab V = MakeVocab(ReadFile(Root + "/Content/Data/GistNouns.csv"), ReadFile(Root + "/Content/Data/GistModifiers.csv"),
		ReadFile(Root + "/Content/Data/MrsHallBlocklist.csv"));
	auto B = [&V](const std::string& Prompt) { return Build(V, Prompt); };

	// The word lists and the code agree.
	Expect(V.Nouns.size() > 150 && V.Modifiers.size() > 100, "word lists loaded");
	const std::set<std::string> Archetypes(ArchetypeIds().begin(), ArchetypeIds().end());
	const std::set<std::string> Modifiers(ModifierIds().begin(), ModifierIds().end());
	Expect(Archetypes.size() == 40, "40 archetypes");
	std::set<std::string> NounUsed, ModUsed;
	for (const auto& N : V.Nouns) { Expect(Archetypes.count(N.second) == 1, "noun row names a real archetype: " + N.first); NounUsed.insert(N.second); }
	for (const auto& M : V.Modifiers) { Expect(Modifiers.count(M.second) == 1, "modifier row names a real modifier: " + M.first); ModUsed.insert(M.second); }
	Expect(NounUsed == Archetypes, "every archetype has a word");
	Expect(ModUsed == Modifiers, "every modifier has a word");
	for (const auto& N : V.Nouns) Expect(V.Modifiers.count(N.first) == 0, "a word is a noun or a modifier, not both: " + N.first);
	Expect(!V.Blocklist.empty(), "blocklist loaded");

	// Same words, same gist - including after the tokenizer has tidied them.
	Expect(Serialize(B("giant flying toaster")) == Serialize(B("giant flying toaster")), "deterministic");
	Expect(Serialize(B("Giant  FLYING toaster!!")) == Serialize(B("giant flying toaster")), "case and punctuation do not matter");
	Expect(Serialize(B("giant flying toaster")) != Serialize(B("tiny flying toaster")), "different words, different gist");

	// Every noun word builds its archetype, sound.
	for (const auto& N : V.Nouns)
	{
		const FRecipe R = B(N.first);
		Expect(!R.bContraption && R.Archetype == N.second, "noun builds its archetype: " + N.first);
		Sound(R, N.first);
	}
	for (const auto& M : V.Modifiers)
	{
		const FRecipe R = B(M.first + " cat");
		Expect(!R.bRefused && R.Archetype == "cat", "modifier word builds: " + M.first);
	}
	// Every modifier changes the cat, and every modifier on every archetype stays sound.
	const std::string BaseCat = Serialize(B("cat"));
	for (const std::string& Mod : ModifierIds())
	{
		Expect(Serialize(B(Mod + " cat")) != BaseCat, "modifier changes something: " + Mod);
		for (const std::string& A : ArchetypeIds()) Sound(B(Mod + " " + A), Mod + " " + A);
	}

	// Specific behaviours.
	{
		const FRecipe Toaster = B("giant flying toaster");
		Expect(Toaster.Archetype == "toaster" && (Toaster.Behaviours & Behaviour::Hover) && Toaster.bGiant, "giant flying toaster");
		Expect(Toaster.Title.find("Model GFT-") != std::string::npos, "initials on the plaque: " + Toaster.Title);

		const FRecipe Gold = B("cat made of gold");
		bool bAllGold = true;
		for (const FPart& P : Gold.Parts) if (P.Role == ERole::Body && P.Finish != EFinish::Gold) bAllGold = false;
		Expect(Gold.Archetype == "cat" && bAllGold, "a cat made of gold is gold");

		const FRecipe CakeDragon = B("a dragon made of cake");
		Expect(CakeDragon.Archetype == "dragon" && Serialize(CakeDragon) != Serialize(B("dragon")), "a dragon made of cake");

		const FRecipe Hatted = B("cat with a hat");
		const FRecipe Plain = B("cat");
		FVec MinH, MaxH, MinP, MaxP;
		Bounds(Hatted.Parts, MinH, MaxH);
		Bounds(Plain.Parts, MinP, MaxP);
		Expect(Hatted.Archetype == "cat" && Hatted.Parts.size() > Plain.Parts.size() && MaxH.Z > MaxP.Z, "a cat with a hat wears it on top");

		const FRecipe Hybrid = B("chair lamp");
		Expect(Hybrid.Archetype == "lamp" && Hybrid.Parts.size() > B("lamp").Parts.size(), "a chair lamp is a lamp with a chair on it");

		const FRecipe Invisible = B("invisible cat");
		Expect(Count(Invisible, ERole::Body) == 0 && Count(Invisible, ERole::Eye) >= 2, "an invisible cat is just its eyes");

		const FRecipe Three = B("three cats");
		Expect(Three.Copies == 3 && Three.Parts.size() == 3 * Plain.Parts.size(), "three cats");
		Expect(B("cats").Copies == 3, "a plural with no count makes three");
		const FRecipe Army = B("an army of giant dragons");
		Expect(Army.Archetype == "dragon" && Army.Copies >= 1 && static_cast<int>(Army.Parts.size()) <= MaxParts, "an army is capped by parts");
		Expect(B("99 ducks").Copies <= MaxCopies, "counts are capped");

		const FRecipe Unknown = B("my mother-in-law");
		Expect(Unknown.bContraption && Unknown.Tagline.find("Interpretation") == 0 && Unknown.Parts.size() >= 4, "unknown words build a contraption");
		const FRecipe Quirky = B("fluffy cat");
		Expect(Quirky.Archetype == "cat" && Quirky.Parts.size() > Plain.Parts.size(), "an unknown word beside a noun adds a quirk");

		Expect(B("").bEmpty && B("  ?!  ").bEmpty, "nothing typed builds nothing");
		const FRecipe Long = B("a giant flying golden rainbow dancing royal fancy fiery cat with a hat and a crown made of cake");
		Expect(CodePoints(Long.Echo) == EchoChars, "a long request is cut to 40 characters");
		Sound(Long, "long");
	}

	// Mrs. Hall's blocklist never reaches the builder, alone or hidden in a request.
	for (const std::string& Word : V.Blocklist)
	{
		for (const std::string& Prompt : {Word, "giant " + Word + " cat", "a cat with a " + Word, Word + "s"})
		{
			const FRecipe R = B(Prompt);
			Expect(R.bRefused && R.Parts.empty() && R.Echo.empty() && R.Title.empty(), "blocklisted word refused");
		}
	}

	// Fuzz: requests stitched from the word lists, counts and junk all stay sound.
	std::vector<std::string> Words;
	for (const auto& N : V.Nouns) Words.push_back(N.first);
	for (const auto& M : V.Modifiers) Words.push_back(M.first);
	const char* Extra[] = {"with", "and", "made", "of", "a", "the", "three", "army", "12", "zorp", "blimbo", "of", "from", "wearing"};
	for (const char* E : Extra) Words.push_back(E);
	uint32_t S = 12345;
	int Built = 0;
	for (int I = 0; I < 5000; ++I)
	{
		std::string Prompt;
		S = S * 1664525u + 1013904223u;
		const int N = 1 + static_cast<int>((S >> 16) % 8);
		for (int J = 0; J < N; ++J)
		{
			S = S * 1664525u + 1013904223u;
			Prompt += (J ? " " : "") + Words[(S >> 8) % Words.size()];
		}
		const FRecipe R = B(Prompt);
		Sound(R, Prompt);
		if (!R.bRefused && !R.bEmpty) ++Built;
		else Expect(false, "fuzzed request did not build [" + Prompt + "]");
	}
	Expect(Built == 5000, "every fuzzed request built");

	if (Failures) { std::cout << Failures << " check(s) FAILED.\n"; return 1; }
	std::cout << "THE GIST: word lists, determinism, every noun and modifier, joins, counts, contraptions, caps, "
	             "plaque and blocklist tests passed (5000 fuzzed requests).\n";
	return 0;
}
