// GistRules.h — THE GIST: the instant, silly builder behind Generate (docs/GIST_GENERATOR_PLAN.md).
//
// When Generate's closed catalog has no match, the supply room builds the gist of the request
// out of simple shapes: nouns pick a toy-like recipe, modifiers colour, size, animate and decorate
// it, unknown words still build something, and everything gets googly eyes and a plaque.
//
// Engine-free and deterministic, like DragonsWagerRules.h and LanternBurstSim: no RNG but a
// small generator seeded from the words, no clock, no network. The same words always give the
// same recipe, so a save keeps only the prompt, and Tools/Tests/gist_test.cpp can compare
// recipes exactly. The vocabulary is data (Content/Data/GistNouns.csv, GistModifiers.csv);
// what each archetype and modifier DOES is code below.
//
// Units are centimetres. +X is the front (the face looks along it), +Z is up, and a finished
// recipe stands on Z = 0, centred on X = Y = 0. Rotations are Unreal's (pitch, yaw, roll).
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace Gist
{
enum class EShape : uint8_t { Cube, Sphere, Cylinder, Cone };    // the engine's BasicShapes
enum class EFinish : uint8_t { Clay, Gold, Glass };              // M_GistClay, M_WagerGold, M_LanternOrb
enum class ERole : uint8_t { Body, Eye, Pupil, Feature };        // only Body takes colour modifiers

struct FVec { float X = 0, Y = 0, Z = 0; };
struct FColour { float R = 1, G = 1, B = 1; };

struct FPart
{
	EShape Shape = EShape::Cube;
	EFinish Finish = EFinish::Clay;
	ERole Role = ERole::Body;
	FColour Colour;
	FVec Location;   // centre
	FVec Rotation;   // pitch, yaw, roll in degrees
	FVec Size;       // full extent along the part's own axes (the engine shapes are 100 cm)
};

namespace Behaviour
{
	constexpr uint32_t Hover = 1u << 0;
	constexpr uint32_t Spin = 1u << 1;
	constexpr uint32_t Dance = 1u << 2;
	constexpr uint32_t Bounce = 1u << 3;
	constexpr uint32_t Wobble = 1u << 4;
	constexpr uint32_t Sway = 1u << 5;
}

constexpr int MaxParts = 64;
constexpr int MaxCopies = 12;
constexpr float Room = 300.0f;        // every gist fits a 3 m cube...
constexpr float GiantRoom = 600.0f;   // ...or a 6 m one when asked for big
constexpr int EchoChars = 40;         // the player's words on the plaque, at most

struct FRecipe
{
	bool bRefused = false;       // a blocklisted word: nothing is built and nothing is echoed
	bool bEmpty = false;         // no words at all
	bool bContraption = false;   // no known noun: a seeded stack of shapes
	std::string Archetype;       // the main noun's archetype ("" for a contraption)
	std::vector<FPart> Parts;
	uint32_t Behaviours = 0;
	int Copies = 1;
	float Scale = 1.0f;          // already applied to Parts
	bool bGiant = false;         // asked to be big: it gets GiantRoom instead of Room
	std::string Title;           // "THE GIST - Model GFT-4410" (with an em dash)
	std::string Echo;            // the player's words, normalized, at most EchoChars characters
	std::string Tagline;         // "Assembly not required." / "Interpretation 7 of <infinity>"
	uint32_t Seed = 0;
};

struct FVocab
{
	std::map<std::string, std::string> Nouns;       // word -> archetype id
	std::map<std::string, std::string> Modifiers;   // word -> modifier id
	std::vector<std::string> Blocklist;             // the Mrs. Hall blocklist, lowercase words
};

// "Word,Id" rows after a header line; blank lines and '#' comments are skipped, words lowercased.
std::vector<std::pair<std::string, std::string>> ParseWordCsv(const std::string& Text);
// The blocklist CSV is "Name,Word": its second column.
std::vector<std::string> ParseBlocklistCsv(const std::string& Text);
FVocab MakeVocab(const std::string& NounsCsv, const std::string& ModifiersCsv, const std::string& BlocklistCsv);

// Lowercase ASCII, anything not a letter or digit separates words; UTF-8 bytes stay in words.
std::vector<std::string> Tokenize(const std::string& Prompt);
uint32_t Hash(const std::vector<std::string>& Tokens);

FRecipe Build(const FVocab& Vocab, const std::string& Prompt);

// What the code knows how to build, so tests can hold the CSVs to it.
const std::vector<std::string>& ArchetypeIds();
const std::vector<std::string>& ModifierIds();

// Axis-aligned bounds of a part list (rotations included), for placement and tests.
void Bounds(const std::vector<FPart>& Parts, FVec& OutMin, FVec& OutMax);
}
