// FilmGenerator.cpp — see the header.

#include "FilmGenerator.h"
#include "FilmGenRules.h"

#include <string>
#include <vector>

// FilmGenRules.h is engine-free and keeps its own copy of these two lists. Reorder one
// without the other and every generated shot is framed wrong, silently; so, loudly:
static_assert(static_cast<int32>(FilmGen::EFraming::Card) == static_cast<int32>(EFilmFraming::Card)
	&& static_cast<int32>(FilmGen::EFraming::Wide) == static_cast<int32>(EFilmFraming::Wide)
	&& static_cast<int32>(FilmGen::EFraming::Full) == static_cast<int32>(EFilmFraming::Full)
	&& static_cast<int32>(FilmGen::EFraming::Medium) == static_cast<int32>(EFilmFraming::Medium)
	&& static_cast<int32>(FilmGen::EFraming::CloseUp) == static_cast<int32>(EFilmFraming::CloseUp)
	&& static_cast<int32>(FilmGen::EFraming::Low) == static_cast<int32>(EFilmFraming::Low)
	&& static_cast<int32>(FilmGen::EFraming::High) == static_cast<int32>(EFilmFraming::High),
	"FilmGen::EFraming and EFilmFraming must list the same framings in the same order");
static_assert(static_cast<int32>(FilmGen::EMove::Hold) == static_cast<int32>(EFilmMove::Hold)
	&& static_cast<int32>(FilmGen::EMove::Orbit) == static_cast<int32>(EFilmMove::Orbit)
	&& static_cast<int32>(FilmGen::EMove::PushIn) == static_cast<int32>(EFilmMove::PushIn)
	&& static_cast<int32>(FilmGen::EMove::PullOut) == static_cast<int32>(EFilmMove::PullOut)
	&& static_cast<int32>(FilmGen::EMove::CraneUp) == static_cast<int32>(EFilmMove::CraneUp)
	&& static_cast<int32>(FilmGen::EMove::CraneDown) == static_cast<int32>(EFilmMove::CraneDown),
	"FilmGen::EMove and EFilmMove must list the same moves in the same order");

bool FilmGenerator::IsFilmWish(const FString& Wish)
{
	return FilmGen::IsFilmWish(TCHAR_TO_UTF8(*Wish));
}

bool FilmGenerator::Make(const FString& Wish, uint32 Take, const TArray<FString>& Blocklist, FFilm& Out)
{
	std::vector<std::string> Blocked;
	Blocked.reserve(Blocklist.Num());
	for (const FString& Word : Blocklist)
	{
		Blocked.emplace_back(TCHAR_TO_UTF8(*Word.ToLower()));
	}

	FilmGen::FRecipe Recipe;
	if (!FilmGen::Plan(TCHAR_TO_UTF8(*Wish), Take, Blocked, Recipe))
	{
		return false;
	}

	Out = FFilm();
	Out.Id = TEXT("Generated");
	Out.Title = UTF8_TO_TCHAR(Recipe.Title.c_str());
	Out.Shots.Reserve(static_cast<int32>(Recipe.Shots.size()));
	for (const FilmGen::FShot& From : Recipe.Shots)
	{
		FFilmShot Shot;
		Shot.Seconds = From.Seconds;
		Shot.Framing = static_cast<EFilmFraming>(From.Framing);
		Shot.Move = static_cast<EFilmMove>(From.Move);
		Shot.Lead = From.Lead;
		Shot.Azimuth = From.Azimuth;
		Shot.Dance = UTF8_TO_TCHAR(From.Dance.c_str());
		Shot.Rate = From.Rate;
		Shot.Caption = UTF8_TO_TCHAR(From.Caption.c_str());
		Shot.Sweep = From.Sweep;
		Out.Shots.Add(MoveTemp(Shot));
	}
	return true;
}
