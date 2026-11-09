// FilmTypes.h
//
// A FILM IS DATA (Walt, 2026-11-09: "can a bunch of original movies be played from a menu,
// like the agent dance animations are?").
//
// Not video. A film here is a screenplay and a shot list, and the game performs it live:
// the agents already standing in the level are the cast, their dances are the acting, a
// spawned camera is the cinematographer, and the words are intertitles in a letterbox, the
// way a silent film says them. UFilmDirectorSubsystem stages it; FilmCatalog.cpp is where
// the films themselves are written.
//
// Nothing here is generated at runtime and nothing calls a service. A film ships inside
// the executable, costs nothing per play, and works in any level that has a dancer in it.
//
// WRITING ONE: add an FFilm to FilmCatalog.cpp. It appears in the F menu by itself.

#pragma once

#include "CoreMinimal.h"

/** How much of her the shot holds. Card is no shot at all: words on black. */
enum class EFilmFraming : uint8
{
	Card,      // a title card. Caption is "BIG LINE|small line"
	Wide,      // her and the room
	Full,      // head to toe
	Medium,    // waist up
	CloseUp,   // her face
	Low,       // from the floor, looking up: the heroic one
	High       // from above, looking down
};

/** What the camera does across the length of the shot. Every move eases in and out. */
enum class EFilmMove : uint8
{
	Hold,       // nearly still: a few degrees of drift, so it never reads as a freeze frame
	Orbit,      // travel round her by Sweep degrees
	PushIn,     // start back, finish close
	PullOut,    // start close, finish back
	CraneUp,    // rise
	CraneDown   // descend
};

struct FFilmShot
{
	float Seconds = 5.0f;
	EFilmFraming Framing = EFilmFraming::Wide;
	EFilmMove Move = EFilmMove::Hold;

	/** Which cast member. 0 is the star (the agent whose menu started the film); 1, 2...
	    are the other agents in the level, nearest first. Wraps, so a film written for
	    three plays with one. */
	int32 Lead = 0;

	/** Where the camera starts, in degrees round her from dead ahead. Positive is her left. */
	float Azimuth = 0.0f;

	/** Part of a dance's asset name ("Ballet", "Slow_Rhythm_Dance_12"). Empty keeps
	    whatever she is already doing. An FString, not a literal's pointer: [G] writes
	    films at runtime (FilmGenerator.h) and a shot has to own what it says. */
	FString Dance;

	/** Playback speed of the dance. 0.5 is slow motion. */
	float Rate = 1.0f;

	/** The intertitle. {0} is the star's name. Empty shows nothing. */
	FString Caption;

	/** Degrees an Orbit travels. Negative goes the other way. */
	float Sweep = 40.0f;
};

struct FFilm
{
	FName Id;
	FString Title;
	TArray<FFilmShot> Shots;

	float Length() const
	{
		float Total = 0.0f;
		for (const FFilmShot& Shot : Shots)
		{
			Total += Shot.Seconds;
		}
		return Total;
	}
};

namespace FilmCatalog
{
	/** Every film, in menu order. */
	SIBELIUSGAME_API const TArray<FFilm>& All();

	/** Null when there is no film of that id. */
	SIBELIUSGAME_API const FFilm* Find(FName Id);
}
