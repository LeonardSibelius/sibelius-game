// FilmGenRules.h — [G] MAKES A FILM (Walt, 2026-11-09: "I really wish the [G] for generate
// could actually generate something").
//
// He types a wish with the word "film" or "movie" in it - "a film about my old boss" - and
// this turns it into a film nobody has seen before: a shot list and intertitles, which
// UFilmDirectorSubsystem performs with the agents in the level (FilmTypes.h).
//
// HOW IT STAYS A FILM AND NOT A RANSOM NOTE. Lines are not drawn from one big pool. There
// are six STORY SHAPES (a tall tale, a confession, an elegy, a lesson, a letter, a nature
// documentary), each a fixed run of beats, and every beat has a few lines written to do
// that beat's job in that shape. Any choice at one beat follows any choice at the one
// before. His own words go in as the subject, and on the title card.
//
// Engine-free and deterministic, like GistRules.h: no clock, no network, and no RNG but a
// small generator seeded from the words and a take number. The same words and take always
// give the same film, and Tools/Tests/film_gen_test.cpp can read a hundred of them.
//
// Nothing here calls a model. Every line was written in advance and ships in the build.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace FilmGen
{
// The same lists, in the same order, as EFilmFraming and EFilmMove in FilmTypes.h, which
// this file cannot include (it is engine-free). FilmGenerator.cpp static_asserts the match.
enum class EFraming : uint8_t { Card, Wide, Full, Medium, CloseUp, Low, High };
enum class EMove : uint8_t { Hold, Orbit, PushIn, PullOut, CraneUp, CraneDown };

struct FShot
{
	float Seconds = 5.0f;
	EFraming Framing = EFraming::Wide;
	EMove Move = EMove::Hold;
	int Lead = 0;
	float Azimuth = 0.0f;
	std::string Dance;     // part of a dance asset's name; empty keeps the one she is on
	float Rate = 1.0f;
	std::string Caption;   // {0} is still in it: the director fills in the star's name
	float Sweep = 40.0f;
};

struct FRecipe
{
	std::string Shape;     // "TallTale", "Elegy"... for the log and the tests
	std::string Topic;     // what the wish was about, as the captions say it
	std::string Title;     // for a menu: "The True Story of his old boss"
	std::vector<FShot> Shots;
};

constexpr int MaxTopicChars = 32;     // his words in a caption, at most
constexpr int MaxCaptionChars = 110;  // one line of the letterbox bar, with room to spare

/** Lowercase words of the wish: letters, digits and apostrophes; everything else parts them. */
std::vector<std::string> Tokens(const std::string& Wish);

/** True when the wish asks for a film: it has "film", "movie" or "cinema" in it. */
bool IsFilmWish(const std::string& Wish);

/**
 * Write the film. False, with Out untouched, when a word of the wish is on the blocklist
 * (lowercase words, Mrs. Hall's own list) - the caller refuses it as Generate always has.
 * A wish with no subject at all ("film") still gets a film, about something of our choosing.
 */
bool Plan(const std::string& Wish, uint32_t Take, const std::vector<std::string>& Blocklist, FRecipe& Out);
}
