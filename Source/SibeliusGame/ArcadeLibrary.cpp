// ArcadeLibrary.cpp — Leonard's Arcade, the pure part. See the header.

#include "ArcadeLibrary.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Math/RandomStream.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace SibArcadeDetail
{
	using SibArcade::FPick;

	/* THE PICKS. The worlds are Leonard's: the agents' homes, the city in the sphere, the rooms
	   he plays in. The prompt half of each is written to give a model something to paint. */
	const FPick Worlds[] = {
		{ TEXT("Kaia's alien jungle"), TEXT("Kaia's alien jungle: giant glowing plants under two moons, drifting mist and fireflies"), TEXT("alien-jungle") },
		{ TEXT("Elise's grassland"), TEXT("Elise's endless grassland at sunset, the wind rolling through it like waves"), TEXT("grassland") },
		{ TEXT("Aisling's snowfield"), TEXT("Aisling's snowfield under the northern lights, ice crystals glittering in the air"), TEXT("snowfield") },
		{ TEXT("Isla's space station"), TEXT("Isla's ring-shaped space station orbiting Mars, its windows full of stars"), TEXT("space-station") },
		{ TEXT("The Dyson sphere"), TEXT("inside a Dyson sphere built around a blazing star, its inner shell twinkling with city lights"), TEXT("dyson-sphere") },
		{ TEXT("The stained-glass cathedral"), TEXT("a cathedral of stained glass, light pouring through it in every colour"), TEXT("cathedral") },
		{ TEXT("The midnight pool hall"), TEXT("a smoky pool hall at midnight, green felt, clacking balls and neon signs"), TEXT("pool-hall") },
		{ TEXT("The casino floor"), TEXT("a casino floor of slot machines, flashing reels and spinning lights"), TEXT("casino") },
		{ TEXT("Transhuman City"), TEXT("a city of glass towers where people and machines live side by side"), TEXT("transhuman-city") },
	};

	const FPick Heroes[] = {
		{ TEXT("A tiny rocket"), TEXT("a tiny rocket with a big flame"), TEXT("tiny-rocket") },
		{ TEXT("A robot dog"), TEXT("a loyal robot dog with a wagging antenna"), TEXT("robot-dog") },
		{ TEXT("A glowing jellyfish"), TEXT("a glowing jellyfish that pulses as it moves"), TEXT("jellyfish") },
		{ TEXT("A cat in a spacesuit"), TEXT("a cat in a spacesuit"), TEXT("space-cat") },
		{ TEXT("A cue ball come to life"), TEXT("a pool cue ball that has come to life"), TEXT("cue-ball") },
		{ TEXT("A paper airplane"), TEXT("a paper airplane"), TEXT("paper-plane") },
		{ TEXT("A dragon made of light"), TEXT("a small dragon made of light"), TEXT("light-dragon") },
		{ TEXT("An explorer with a lantern"), TEXT("a tiny explorer carrying a lantern"), TEXT("explorer") },
		{ TEXT("A dancing alien"), TEXT("a small alien who dances when it is happy"), TEXT("dancing-alien") },
	};

	const FPick Games[] = {
		{ TEXT("Collect and dodge"), TEXT("a collect-and-dodge game: grab the shining things, avoid the dangers"), TEXT("collect") },
		{ TEXT("Arcade shooter"), TEXT("an arcade shooter: waves of enemies, power-ups, and a boss every few waves"), TEXT("shooter") },
		{ TEXT("Endless runner"), TEXT("an endless runner: the hero races forward, jumping and sliding past obstacles"), TEXT("runner") },
		{ TEXT("Brick breaker"), TEXT("a brick-breaker: bounce a ball off a paddle to smash walls of bricks, with power-ups"), TEXT("bricks") },
		{ TEXT("Platformer"), TEXT("a platform game: run and jump across levels, collect stars, reach the exit"), TEXT("platformer") },
		{ TEXT("Racer"), TEXT("a top-down racing game against three rivals on a twisting track"), TEXT("racer") },
		{ TEXT("Match-three puzzle"), TEXT("a match-three puzzle game with combos and chain reactions"), TEXT("puzzle") },
		{ TEXT("Tower defence"), TEXT("a tower-defence game: place towers to stop waves marching along a path"), TEXT("towers") },
	};

	const FPick Twists[] = {
		{ TEXT("Gravity flips"), TEXT("gravity flips every ten seconds."), TEXT("flip") },
		{ TEXT("Faster and faster"), TEXT("everything speeds up as the score climbs."), TEXT("faster") },
		{ TEXT("A trail of light"), TEXT("the hero leaves a trail of light that destroys any enemy it touches."), TEXT("trail") },
		{ TEXT("Colour storms"), TEXT("every power-up repaints the whole world in new colours."), TEXT("colours") },
		{ TEXT("Time slows when still"), TEXT("time slows down while the hero stands still."), TEXT("slow-time") },
		{ TEXT("The screen wraps"), TEXT("the screen wraps around: leave one edge and come back at the other."), TEXT("wrap") },
		{ TEXT("Enemies become friends"), TEXT("enemies you beat become friends who fight beside you."), TEXT("friends") },
		{ TEXT("Something huge wakes"), TEXT("halfway through each level, something enormous wakes up."), TEXT("giant") },
		{ TEXT("The music knows"), TEXT("the music, made with Web Audio, speeds up as the danger grows."), TEXT("music") },
		{ TEXT("A pull to the centre"), TEXT("a great force at the centre of the screen pulls the hero in, always."), TEXT("pull") },
	};

	struct FRowTable { const FPick* Picks; int32 Num; const TCHAR* Label; const TCHAR* Hint; };

	FRowTable Table(SibArcade::EPick Row)
	{
		switch (Row)
		{
		case SibArcade::EPick::World: return { Worlds, UE_ARRAY_COUNT(Worlds), TEXT("World"), TEXT("where your game takes place") };
		case SibArcade::EPick::Hero:  return { Heroes, UE_ARRAY_COUNT(Heroes), TEXT("Hero"), TEXT("who you play") };
		case SibArcade::EPick::Game:  return { Games, UE_ARRAY_COUNT(Games), TEXT("Game"), TEXT("what kind of game it is") };
		default:                      return { Twists, UE_ARRAY_COUNT(Twists), TEXT("Twist"), TEXT("the surprise that makes it yours") };
		}
	}

	FString DecodeEntities(FString S)
	{
		S.ReplaceInline(TEXT("&lt;"), TEXT("<"));
		S.ReplaceInline(TEXT("&gt;"), TEXT(">"));
		S.ReplaceInline(TEXT("&quot;"), TEXT("\""));
		S.ReplaceInline(TEXT("&#39;"), TEXT("'"));
		S.ReplaceInline(TEXT("&apos;"), TEXT("'"));
		S.ReplaceInline(TEXT("&nbsp;"), TEXT(" "));
		S.ReplaceInline(TEXT("&amp;"), TEXT("&"));   // last, so "&amp;lt;" stays "&lt;"
		return S;
	}
}

namespace SibArcade
{
	int32 NumPicks(EPick Row)
	{
		return SibArcadeDetail::Table(Row).Num;
	}

	const FPick& GetPick(EPick Row, int32 Index)
	{
		const SibArcadeDetail::FRowTable T = SibArcadeDetail::Table(Row);
		const int32 I = ((Index % T.Num) + T.Num) % T.Num;
		return T.Picks[I];
	}

	const TCHAR* RowLabel(EPick Row)
	{
		return SibArcadeDetail::Table(Row).Label;
	}

	const TCHAR* RowHint(EPick Row)
	{
		return SibArcadeDetail::Table(Row).Hint;
	}

	FAssignment Surprise(FRandomStream& Rng)
	{
		FAssignment A;
		for (int32 Row = 0; Row < (int32)EPick::Count; ++Row)
		{
			A.Picks[Row] = Rng.RandRange(0, NumPicks((EPick)Row) - 1);
		}
		return A;
	}

	FString CodeWord(int32 PlayerNumber)
	{
		return FString::Printf(TEXT("LA-%04d"), FMath::Clamp(PlayerNumber, 0, 9999));
	}

	FString CodeFilePath(const FString& Folder)
	{
		return Folder / TEXT("code word.txt");
	}

	FString CodeFileText(int32 PlayerNumber)
	{
		return CodeWord(PlayerNumber) + TEXT("\r\n\r\n")
			TEXT("Your code word for Leonard's Arcade. Every prompt the arcade gives you asks the model to put it ")
			TEXT("on the game's title screen, and a game that carries it plays without asking first.\r\n")
			TEXT("Keep this file: it is how the arcade knows your games. Delete it and you get a new code word, ")
			TEXT("and the games you have made will ask before they play.\r\n");
	}

	int32 ParseCodeWord(const FString& Text)
	{
		int32 From = 0;
		while (true)
		{
			const int32 At = Text.Find(TEXT("LA-"), ESearchCase::IgnoreCase, ESearchDir::FromStart, From);
			if (At == INDEX_NONE)
			{
				return 0;
			}
			const FString Digits = Text.Mid(At + 3, 4);
			const bool bFour = Digits.Len() == 4 && FChar::IsDigit(Digits[0]) && FChar::IsDigit(Digits[1])
				&& FChar::IsDigit(Digits[2]) && FChar::IsDigit(Digits[3]);
			const bool bEnds = At + 7 >= Text.Len() || !FChar::IsDigit(Text[At + 7]);   // not "LA-12345"
			if (bFour && bEnds)
			{
				return FCString::Atoi(*Digits);   // 0000 reads as 0: no code
			}
			From = At + 3;
		}
	}

	FString FileNameFor(const FAssignment& A)
	{
		return FString::Printf(TEXT("%s-%s.html"), GetPick(EPick::Hero, A[EPick::Hero]).Slug,
			GetPick(EPick::World, A[EPick::World]).Slug);
	}

	FString ComposePrompt(const FAssignment& A, const FString& Code)
	{
		const FPick& World = GetPick(EPick::World, A[EPick::World]);
		const FPick& Hero = GetPick(EPick::Hero, A[EPick::Hero]);
		const FPick& Game = GetPick(EPick::Game, A[EPick::Game]);
		const FPick& Twist = GetPick(EPick::Twist, A[EPick::Twist]);

		FString P;
		P += TEXT("Make me a complete, polished browser game in ONE self-contained HTML file.\n\n");
		P += TEXT("THE GAME\n");
		P += FString::Printf(TEXT("- Kind: %s.\n"), Game.Prompt);
		P += FString::Printf(TEXT("- Hero: %s.\n"), Hero.Prompt);
		P += FString::Printf(TEXT("- World: %s.\n"), World.Prompt);
		P += FString::Printf(TEXT("- Twist: %s\n\n"), Twist.Prompt);
		P += TEXT("MAKE IT FEEL AMAZING\n");
		P += TEXT("Smooth 60 fps animation, glowing colours, particle effects, screen shake on hits, juicy sound ");
		P += TEXT("effects made with the Web Audio API, a score and a best score, difficulty that keeps rising, ");
		P += TEXT("a title screen with the game's name, and a game-over screen where Enter plays again.\n\n");
		P += TEXT("RULES\n");
		P += TEXT("1. Everything in one HTML file. No images, fonts, libraries or anything else from the internet: ");
		P += TEXT("draw with the canvas and make every sound with Web Audio. The game will be played offline.\n");
		P += TEXT("2. Controls: arrow keys or WASD, Space and Enter, and the mouse if it helps. Never use the Escape key for anything.\n");
		P += TEXT("3. Fill the whole window and adapt when it is resized.\n");
		P += FString::Printf(TEXT("4. On the title screen, show this small in a corner: Leonard's Arcade · %s\n\n"), *Code);
		P += FString::Printf(TEXT("Give me the whole file in one piece. Then tell me, in two or three short steps, how to save it "
			"on Windows as a file called %s in the folder Documents\\Leonard Sibelius\\Arcade."), *FileNameFor(A));
		return P;
	}

	FString ArcadeFolder()
	{
		FString Folder = FPaths::Combine(FPlatformProcess::UserDir(), TEXT("Leonard Sibelius/Arcade/"));
		FPaths::NormalizeDirectoryName(Folder);
		return Folder + TEXT("/");
	}

	FString SampleFolder()
	{
		FString Folder = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Arcade"));
		FPaths::NormalizeDirectoryName(Folder);
		return Folder + TEXT("/");
	}

	FString FolderForDisplay(const FString& Folder)
	{
		FString Out = Folder;
		Out.RemoveFromEnd(TEXT("/"));
		Out.ReplaceInline(TEXT("/"), TEXT("\\"));
		return Out;
	}

	FString ReadableFileName(const FString& FileName)
	{
		FString Base = FPaths::GetBaseFilename(FileName);
		Base.ReplaceInline(TEXT("-"), TEXT(" "));
		Base.ReplaceInline(TEXT("_"), TEXT(" "));
		Base.TrimStartAndEndInline();
		if (Base.IsEmpty())
		{
			return TEXT("Untitled game");
		}
		Base[0] = FChar::ToUpper(Base[0]);
		return Base;
	}

	FString TitleOf(const FString& Html)
	{
		const int32 Open = Html.Find(TEXT("<title"), ESearchCase::IgnoreCase);
		if (Open == INDEX_NONE)
		{
			return FString();
		}
		const int32 Start = Html.Find(TEXT(">"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open);
		const int32 End = Start == INDEX_NONE ? INDEX_NONE
			: Html.Find(TEXT("</title"), ESearchCase::IgnoreCase, ESearchDir::FromStart, Start);
		if (Start == INDEX_NONE || End == INDEX_NONE || End - Start > 400)
		{
			return FString();
		}
		FString Title = SibArcadeDetail::DecodeEntities(Html.Mid(Start + 1, End - Start - 1));
		Title.ReplaceInline(TEXT("\r"), TEXT(" "));
		Title.ReplaceInline(TEXT("\n"), TEXT(" "));
		Title.TrimStartAndEndInline();
		if (Title.Len() > 60)
		{
			Title = Title.Left(57).TrimEnd() + TEXT("...");
		}
		return Title;
	}

	FString PicksOf(const FString& Html)
	{
		const int32 Name = Html.Find(TEXT("name=\"leonards-arcade-picks\""), ESearchCase::IgnoreCase);
		if (Name == INDEX_NONE)
		{
			return FString();
		}
		const int32 TagEnd = Html.Find(TEXT(">"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Name);
		const int32 Content = Html.Find(TEXT("content=\""), ESearchCase::IgnoreCase, ESearchDir::FromStart, Name);
		if (Content == INDEX_NONE || (TagEnd != INDEX_NONE && Content > TagEnd))
		{
			return FString();
		}
		const int32 Start = Content + 9;
		const int32 End = Html.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
		if (End == INDEX_NONE || End - Start > 200)
		{
			return FString();
		}
		return SibArcadeDetail::DecodeEntities(Html.Mid(Start, End - Start)).TrimStartAndEnd();
	}

	bool AssignmentFromPicks(const FString& Picks, FAssignment& Out)
	{
		TArray<FString> Labels;
		Picks.ParseIntoArray(Labels, TEXT(" · "));
		if (Labels.Num() != (int32)EPick::Count)
		{
			return false;
		}
		FAssignment A;
		for (int32 Row = 0; Row < (int32)EPick::Count; ++Row)
		{
			int32 Found = INDEX_NONE;
			for (int32 i = 0; i < NumPicks((EPick)Row) && Found == INDEX_NONE; ++i)
			{
				if (Labels[Row].TrimStartAndEnd().Equals(GetPick((EPick)Row, i).Label, ESearchCase::IgnoreCase))
				{
					Found = i;
				}
			}
			if (Found == INDEX_NONE)
			{
				return false;
			}
			A.Picks[Row] = Found;
		}
		Out = A;
		return true;
	}

	TArray<FGame> FindGames(const FString& Folder, const FString& Code)
	{
		TArray<FGame> Games;
		IFileManager& FM = IFileManager::Get();
		TArray<FString> Names;
		FM.FindFiles(Names, *(Folder / TEXT("*.html")), /*Files=*/true, /*Directories=*/false);
		TArray<FString> Htm;
		FM.FindFiles(Htm, *(Folder / TEXT("*.htm")), true, false);
		for (const FString& N : Htm)
		{
			Names.AddUnique(N);   // on Windows *.htm also matches .html
		}

		for (const FString& Name : Names)
		{
			const FString Ext = FPaths::GetExtension(Name).ToLower();
			if (Ext != TEXT("html") && Ext != TEXT("htm"))
			{
				continue;
			}
			FGame G;
			G.FileName = Name;
			G.Path = Folder / Name;
			G.Bytes = FM.FileSize(*G.Path);
			if (G.Bytes <= 0 || G.Bytes > MaxGameBytes)
			{
				continue;
			}
			G.Modified = FM.GetTimeStamp(*G.Path);
			FString Html;
			if (!FFileHelper::LoadFileToString(Html, *G.Path))
			{
				continue;
			}
			G.Title = TitleOf(Html);
			if (G.Title.IsEmpty())
			{
				G.Title = ReadableFileName(Name);
			}
			G.bHasCode = !Code.IsEmpty() && Html.Contains(Code, ESearchCase::IgnoreCase);
			G.Picks = PicksOf(Html);
			Games.Add(MoveTemp(G));
		}
		Games.Sort([](const FGame& L, const FGame& R) { return L.Modified > R.Modified; });
		return Games;
	}

	FString FileUrl(const FString& AbsolutePath)
	{
		FString Path = AbsolutePath;
		Path.ReplaceInline(TEXT("\\"), TEXT("/"));
		FString Out = TEXT("file:///");
		const FTCHARToUTF8 Utf8(*Path);
		for (int32 i = 0; i < Utf8.Length(); ++i)
		{
			const uint8 C = (uint8)Utf8.Get()[i];
			const bool bPlain = (C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') || (C >= '0' && C <= '9')
				|| C == '-' || C == '_' || C == '.' || C == '~' || C == '/' || C == ':';
			Out += bPlain ? FString::Chr((TCHAR)C) : FString::Printf(TEXT("%%%02X"), C);
		}
		return Out;
	}

	bool IsOffline(const FString& Url)
	{
		/* "file:///", never "file://": a file URL with a host is a Windows network share. A
		   game's "//cdn.example.com/x.js", resolved against its file:// page, becomes exactly
		   that, and opening a share sends the player's Windows sign-in to whoever runs it. */
		return Url.StartsWith(TEXT("file:///"), ESearchCase::IgnoreCase)
			|| Url.StartsWith(TEXT("data:"), ESearchCase::IgnoreCase)
			|| Url.StartsWith(TEXT("blob:"), ESearchCase::IgnoreCase)
			|| Url.StartsWith(TEXT("about:"), ESearchCase::IgnoreCase);
	}

	FString HostOf(const FString& Url)
	{
		int32 Start = Url.Find(TEXT("://"));
		if (Start == INDEX_NONE)
		{
			return Url.Left(40);
		}
		Start += 3;
		int32 End = Start;
		while (End < Url.Len() && Url[End] != TEXT('/') && Url[End] != TEXT('?') && Url[End] != TEXT('#') && Url[End] != TEXT(':'))
		{
			++End;
		}
		return Url.Mid(Start, End - Start);
	}
}
