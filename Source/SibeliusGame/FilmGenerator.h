// FilmGenerator.h
//
// The engine's side of [G]'s film generator. FilmGenRules.h writes a film from a wish in
// plain C++ that a test can run without the engine; this turns that recipe into the FFilm
// UFilmDirectorSubsystem plays. Nothing else lives here: every line, shape, dance and
// camera choice is in FilmGenRules.cpp.

#pragma once

#include "CoreMinimal.h"
#include "FilmTypes.h"

namespace FilmGenerator
{
	/** True when the wish asks for a film ("a film about...", "movie"). */
	SIBELIUSGAME_API bool IsFilmWish(const FString& Wish);

	/**
	 * Write a film about the wish. Take picks which of the many films those words could
	 * be: the same wish and take always give the same film. False when a word of the wish
	 * is on the blocklist (Mrs. Hall's, as UGenerateComponent loads it).
	 */
	SIBELIUSGAME_API bool Make(const FString& Wish, uint32 Take, const TArray<FString>& Blocklist, FFilm& Out);
}
