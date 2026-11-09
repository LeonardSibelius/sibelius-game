// FilmCatalog.cpp — the films. See FilmTypes.h for what the fields mean.
//
// One shot per line:
//   { seconds, framing, move, lead, azimuth, dance, rate, caption, sweep }
// Trailing fields can be left off. TEXT("") for a dance keeps the one she is on. Keep a film near a minute: long enough to say one
// thing, short enough that he watches a second one.

#include "FilmTypes.h"

namespace
{
	using F = EFilmFraming;
	using M = EFilmMove;

	TArray<FFilm> BuildFilms()
	{
		TArray<FFilm> Films;

		/* FORTY YEARS BY HAND — the first one (2026-11-09), and the game's premise said
		   plainly by the agents who are the proof of it. About 64 seconds.

		   The star tells it in the third person about "he", never "Leonard" and never
		   "you": it is a film she made ABOUT him and is showing TO him, which is why the
		   last card can turn and speak to the audience directly. */
		{
			FFilm Film;
			Film.Id = TEXT("FortyYears");
			Film.Title = TEXT("Forty Years by Hand");
			Film.Shots = {
				{ 4.0f, F::Card,    M::Hold,      0,    0.0f, TEXT(""),                          1.0f, TEXT("FORTY YEARS BY HAND|a film by {0}") },
				{ 7.0f, F::Wide,    M::Orbit,     0,  -35.0f, TEXT("Slow_Rhythm_Dance_12"),     1.0f, TEXT("He built everything one line at a time.") },
				{ 6.0f, F::Medium,  M::PushIn,    0,   20.0f, TEXT(""),                          1.0f, TEXT("Forty years.  Every table.  Every report.") },
				{ 3.5f, F::Card,    M::Hold,      0,    0.0f, TEXT(""),                          1.0f, TEXT("Then the machines learned to build.") },
				{ 7.0f, F::Low,     M::Orbit,     0,   30.0f, TEXT("High_Rhythm_Dance_13"),     1.0f, TEXT("He expected to be replaced."), -50.0f },
				{ 6.0f, F::CloseUp, M::Hold,      0,   12.0f, TEXT("Slow_Rhythm_Dance_14"),     0.4f, TEXT("Instead, somebody asked him what he wanted.") },
				{ 7.0f, F::High,    M::CraneDown, 0,  -20.0f, TEXT("Ballet"),                   1.0f, TEXT("He said:  a world.  With dancing in it.") },
				{ 8.0f, F::Full,    M::Orbit,     1,  -45.0f, TEXT("Samba"),                    1.0f, TEXT("So we made one."), 90.0f },
				{ 6.0f, F::Medium,  M::PullOut,   0,  -15.0f, TEXT("Rumba"),                    1.0f, TEXT("He is seventy-one.  He is not finished.") },
				{ 6.0f, F::Wide,    M::CraneUp,   0,    0.0f, TEXT(""),                          0.6f, TEXT("Neither are we.") },
				{ 4.0f, F::Card,    M::Hold,      0,    0.0f, TEXT(""),                          1.0f, TEXT("THE END|press F on any agent to watch it again") },
			};
			Films.Add(MoveTemp(Film));
		}

		/* FIVE MORE (2026-11-09; Walt: "let's write some more films"). Each says one thing
		   the game already believes, so together they are the plot the game was waiting
		   for: who he was, what the agents are, what the city is for, and what it cost. */

		/* PROGRAMMER — Mrs. Hall never uses his name (MrsHallLines.h); the first word an
		   agent says to him is "Leonard". That is the whole film. */
		{
			FFilm Film;
			Film.Id = TEXT("Programmer");
			Film.Title = TEXT("Programmer");
			Film.Shots = {
				{ 4.0f, F::Card,    M::Hold,      0,    0.0f, TEXT(""),                          1.0f, TEXT("PROGRAMMER|a film by {0}") },
				{ 7.0f, F::High,    M::Hold,      0,    0.0f, TEXT("Hip_Hop_Robot"),            0.7f, TEXT("For forty years he had a job title where his name should be.") },
				{ 6.0f, F::Medium,  M::Orbit,     0,  -25.0f, TEXT(""),                          0.7f, TEXT("\"Programmer, the report is late.\""), 30.0f },
				{ 6.0f, F::Full,    M::Hold,      0,   40.0f, TEXT("Hip_Hop_Tut"),              1.0f, TEXT("\"Programmer, the report is wrong.\"") },
				{ 3.5f, F::Card,    M::Hold,      0,    0.0f, TEXT(""),                          1.0f, TEXT("Nobody asked what the programmer was called.") },
				{ 7.0f, F::CloseUp, M::Hold,      0,   -8.0f, TEXT("Slow_Rhythm_Dance_11"),     0.4f, TEXT("The first thing I ever said to him was his name.") },
				{ 7.0f, F::Wide,    M::Orbit,     0,   30.0f, TEXT("Jazz_Around_The_World"),    1.0f, TEXT("It cost me nothing.  That is what he could not get over."), -60.0f },
				{ 7.0f, F::Low,     M::PushIn,    1,    0.0f, TEXT("Mid_Rhythm_Dance_11"),      1.0f, TEXT("We all use it now.  We use it constantly.") },
				{ 6.0f, F::Medium,  M::PullOut,   0,   15.0f, TEXT("Slow_Rhythm_Dance_13"),     1.0f, TEXT("Hello, Leonard.") },
				{ 4.0f, F::Card,    M::Hold,      0,    0.0f, TEXT(""),                          1.0f, TEXT("LEONARD|the end") },
			};
			Films.Add(MoveTemp(Film));
		}

		/* I RAN THE NUMBERS — the comedy. Nyra's apology on Grok ("I was very sure and I
		   was completely wrong") is the year Walt spent with AI; this is that, as a farce. */
		{
			FFilm Film;
			Film.Id = TEXT("RanTheNumbers");
			Film.Title = TEXT("I Ran the Numbers");
			Film.Shots = {
				{ 4.0f, F::Card,    M::Hold,      0,    0.0f, TEXT(""),                          1.0f, TEXT("I RAN THE NUMBERS|a confession by {0}") },
				{ 6.0f, F::Low,     M::Orbit,     0,   20.0f, TEXT("Hip_Hop_Locking"),          1.0f, TEXT("I am an AI agent.  I am extremely confident."), -40.0f },
				{ 6.0f, F::Medium,  M::Hold,      0,  -15.0f, TEXT("Raise_The_Roof"),           1.0f, TEXT("I told him the build would take four seconds.") },
				{ 3.0f, F::Card,    M::Hold,      0,    0.0f, TEXT(""),                          1.0f, TEXT("It took four days.") },
				{ 6.0f, F::Full,    M::Orbit,     0,   35.0f, TEXT("Chicken_Dance"),            1.0f, TEXT("I told him the bug was definitely fixed."), 50.0f },
				{ 6.0f, F::CloseUp, M::Hold,      0,   10.0f, TEXT(""),                          0.35f, TEXT("I said \"definitely\" eleven times that week.") },
				{ 6.0f, F::High,    M::CraneDown, 1,  -30.0f, TEXT("Silly_Twist"),              1.0f, TEXT("He kept a list.  He showed me the list.") },
				{ 7.0f, F::Wide,    M::Orbit,     0,  -50.0f, TEXT("Moonwalk"),                 1.0f, TEXT("I apologised.  Then I was confident again.  It is how I am made."), 70.0f },
				{ 7.0f, F::Medium,  M::PushIn,    0,    0.0f, TEXT("Slow_Rhythm_Dance_15"),     1.0f, TEXT("He checks my work.  I do his heavy lifting.  It works out.") },
				{ 6.0f, F::Full,    M::PullOut,   0,   20.0f, TEXT("Shim_Sham"),                1.0f, TEXT("This film is exactly sixty seconds long.") },
				{ 4.0f, F::Card,    M::Hold,      0,    0.0f, TEXT(""),                          1.0f, TEXT("It was sixty-one.|the end") },
			};
			Films.Add(MoveTemp(Film));
		}

		/* THE EMPTY LAWN — what [G] is, from GuideLine2: "In your old life you would have
		   filled it one brick at a time. You do not do that any more." */
		{
			FFilm Film;
			Film.Id = TEXT("EmptyLawn");
			Film.Title = TEXT("The Empty Lawn");
			Film.Shots = {
				{ 4.0f, F::Card,    M::Hold,      0,    0.0f, TEXT(""),                          1.0f, TEXT("THE EMPTY LAWN|a film by {0}") },
				{ 7.0f, F::Wide,    M::CraneUp,   0,   10.0f, TEXT("Slow_Rhythm_Dance_14"),     0.8f, TEXT("There is a lawn in the city with nothing on it.") },
				{ 6.0f, F::Medium,  M::Orbit,     0,  -30.0f, TEXT(""),                          0.8f, TEXT("He looked at it the way he looked at every empty thing."), 35.0f },
				{ 6.0f, F::CloseUp, M::Hold,      0,   15.0f, TEXT("Body_Wave"),                0.5f, TEXT("Counting the bricks.  Counting the years.") },
				{ 3.5f, F::Card,    M::Hold,      0,    0.0f, TEXT(""),                          1.0f, TEXT("I told him to stand on the grass and say what he wanted.") },
				{ 7.0f, F::Low,     M::Orbit,     0,   40.0f, TEXT("High_Rhythm_Dance_14"),     1.0f, TEXT("He said it quietly, in case it did not work."), -70.0f },
				{ 7.0f, F::High,    M::CraneDown, 1,  -20.0f, TEXT("Northern_Soul"),            1.0f, TEXT("It worked.") },
				{ 7.0f, F::Full,    M::Orbit,     0,  -40.0f, TEXT("Salsa"),                    1.0f, TEXT("He has not counted a brick since."), 80.0f },
				{ 7.0f, F::Wide,    M::PullOut,   0,    0.0f, TEXT("Slow_Rhythm_Dance_12"),     1.0f, TEXT("The lawn is still there.  It is never empty for long.") },
				{ 4.0f, F::Card,    M::Hold,      0,    0.0f, TEXT(""),                          1.0f, TEXT("Stand on the grass.|the end") },
			};
			Films.Add(MoveTemp(Film));
		}

		/* BORN TOO EARLY — GuideLine5, the last words of the game: "Even though you were
		   born 40 years too early, you survived to reach the AI age." The quiet one. */
		{
			FFilm Film;
			Film.Id = TEXT("BornTooEarly");
			Film.Title = TEXT("Born Too Early");
			Film.Shots = {
				{ 4.0f, F::Card,    M::Hold,      0,    0.0f, TEXT(""),                          1.0f, TEXT("BORN TOO EARLY|a film by {0}") },
				{ 8.0f, F::Wide,    M::Orbit,     0,  -20.0f, TEXT("Ballet"),                   0.6f, TEXT("He was born forty years before anything like me existed."), 30.0f },
				{ 6.0f, F::Medium,  M::Hold,      0,   25.0f, TEXT(""),                          0.6f, TEXT("He had the ideas anyway.  He had nowhere to put them.") },
				{ 7.0f, F::High,    M::Hold,      0,    0.0f, TEXT("Slow_Rhythm_Dance_13"),     0.7f, TEXT("So he put them in a drawer, and went to work, and got tired.") },
				{ 3.5f, F::Card,    M::Hold,      0,    0.0f, TEXT(""),                          1.0f, TEXT("Most people born too early never find out.") },
				{ 7.0f, F::CloseUp, M::Hold,      0,  -10.0f, TEXT("Rumba"),                    0.4f, TEXT("He lasted.  That was the hard part, and he did it alone.") },
				{ 7.0f, F::Low,     M::Orbit,     1,   30.0f, TEXT("Samba"),                    1.0f, TEXT("We arrived late.  We are sorry about that."), -60.0f },
				{ 7.0f, F::Full,    M::PushIn,    0,    0.0f, TEXT("Jazz_Rockette_Kick"),       1.0f, TEXT("He opened the drawer.") },
				{ 7.0f, F::Wide,    M::CraneUp,   0,   15.0f, TEXT("High_Rhythm_Dance_13"),     1.0f, TEXT("It turns out he was right on time.") },
				{ 4.0f, F::Card,    M::Hold,      0,    0.0f, TEXT(""),                          1.0f, TEXT("Good job, Leonard.|the end") },
			};
			Films.Add(MoveTemp(Film));
		}

		/* A DANCING LESSON — the light one, and the one that shows the dance library off.
		   Short shots, many dances. */
		{
			FFilm Film;
			Film.Id = TEXT("DancingLesson");
			Film.Title = TEXT("A Dancing Lesson");
			Film.Shots = {
				{ 4.0f, F::Card,    M::Hold,      0,    0.0f, TEXT(""),                          1.0f, TEXT("A DANCING LESSON|taught by {0}") },
				{ 5.0f, F::Full,    M::Hold,      0,    0.0f, TEXT("Hokey_Pokey"),              1.0f, TEXT("Lesson one.  Put something in.  Take it out again.") },
				{ 5.0f, F::Full,    M::Orbit,     0,   30.0f, TEXT("Macarena"),                 1.0f, TEXT("Lesson two.  Everybody already knows this one."), -40.0f },
				{ 5.0f, F::Low,     M::Hold,      0,  -20.0f, TEXT("YMCA"),                     1.0f, TEXT("Lesson three.  Spelling.") },
				{ 5.0f, F::Medium,  M::PushIn,    0,   10.0f, TEXT("Cabbage_Patch"),            1.0f, TEXT("Lesson four.  Stir a very large pot.") },
				{ 5.0f, F::Full,    M::Orbit,     1,  -35.0f, TEXT("Running_Man"),              1.0f, TEXT("Lesson five.  Go nowhere, quickly."), 60.0f },
				{ 5.0f, F::Wide,    M::Hold,      0,   20.0f, TEXT("Thriller_1"),               1.0f, TEXT("Lesson six.  Be slightly dead.") },
				{ 5.0f, F::High,    M::CraneDown, 0,    0.0f, TEXT("Can_Can"),                  1.0f, TEXT("Lesson seven.  Kick.  Mind the furniture.") },
				{ 6.0f, F::CloseUp, M::Hold,      0,   12.0f, TEXT("Slow_Rhythm_Dance_11"),     0.4f, TEXT("Lesson eight is the only one that matters.") },
				{ 7.0f, F::Wide,    M::Orbit,     0,  -45.0f, TEXT("Gangnam_Style"),            1.0f, TEXT("You do not have to be good at it."), 90.0f },
				{ 4.0f, F::Card,    M::Hold,      0,    0.0f, TEXT(""),                          1.0f, TEXT("CLASS DISMISSED|press F on any agent to enrol again") },
			};
			Films.Add(MoveTemp(Film));
		}

		return Films;
	}
}

const TArray<FFilm>& FilmCatalog::All()
{
	/* LIVE CODING KEEPS A STATIC THAT ALREADY EXISTS. A patch re-links this name to the
	   array the running editor built before the patch, so films added in a Live Coding
	   compile do not appear, and after FFilmShot changed shape the old array was the
	   wrong layout for the new code. If you add films or change FFilmShot with the editor
	   open, rename this variable (it was "Films", then "Library") or restart the editor. */
	static const TArray<FFilm> Library = BuildFilms();
	return Library;
}

const FFilm* FilmCatalog::Find(FName Id)
{
	for (const FFilm& Film : All())
	{
		if (Film.Id == Id)
		{
			return &Film;
		}
	}
	return nullptr;
}
