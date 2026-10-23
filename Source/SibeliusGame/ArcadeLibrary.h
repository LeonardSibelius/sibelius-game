// ArcadeLibrary.h
//
// LEONARD'S ARCADE. Walt, 2026-10-23: "let's do the arcade idea" - then "can the two monitors -
// one in the living room, the other in bedroom - be used instead of cabinets - remove the link
// displayed for my web version of Celestial Fortune and have 'E' open the menu from the TVs".
//
// E at either TV opens a menu (UArcadeMenuWidget) that hands the player an ASSIGNMENT: a prompt,
// built from four picks, to paste into any frontier model - ChatGPT, Claude, Gemini. The model
// answers with a whole game in one HTML file; the player saves it in Documents\Leonard
// Sibelius\Arcade; the TV plays it. The feeling Walt wanted to pass on is his own - "watching AI
// generate tons of code that works really well" - and a one-prompt game is that, small.
//
// THE GAME NEVER TALKS TO A MODEL AND NEVER OPENS A WEBSITE. Steam failed build 25330029 when a
// TV opened the web slot machine ("a game may not send players out of Steam to play somewhere
// else"). Here the player takes a prompt out, by their own hand, and the game plays only what
// they bring back: a local file, offline, the internet blocked (UArcadeMenuWidget's browser).
//
// This file is the pure part - the picks, the prompt, the player's code word, the folder, the
// scan that finds the games, the browser's offline rule. No UI, no world: ArcadeSmokeTest runs
// it headless.

#pragma once

#include "CoreMinimal.h"

struct FRandomStream;

namespace SibArcade
{
	/**
	 * The actor tag that makes a TV an arcade TV - the living room's and the bedroom's in
	 * L_Office_v02. It keeps the name it had when those TVs showed the web slot machine's
	 * address: a tag is map data, and renaming it would mean re-saving the map.
	 */
	inline FName TVTag() { return FName(TEXT("CelestialFortuneWebTV")); }

	/** The four rows of an assignment. */
	enum class EPick : uint8 { World, Hero, Game, Twist, Count };

	struct FPick
	{
		const TCHAR* Label;    // what the menu shows: "Kaia's alien jungle"
		const TCHAR* Prompt;   // what the prompt says: "Kaia's alien jungle: giant glowing plants..."
		const TCHAR* Slug;     // for the suggested file name: "alien-jungle"
	};

	SIBELIUSGAME_API int32 NumPicks(EPick Row);
	/** Any index: it wraps, so the menu's arrows can step past either end. */
	SIBELIUSGAME_API const FPick& GetPick(EPick Row, int32 Index);
	SIBELIUSGAME_API const TCHAR* RowLabel(EPick Row);   // "World", "Hero", "Game", "Twist"
	/**
	 * What a row tells the model - "where your game takes place". Walt, 2026-10-23, at "World <
	 * Kaia's alien jungle": "how is the world used? I don't see it in the game". The picks only
	 * shape the prompt, and so the game a model makes; the menu says so under each one.
	 */
	SIBELIUSGAME_API const TCHAR* RowHint(EPick Row);

	struct FAssignment
	{
		int32 Picks[(int32)EPick::Count] = { 0, 0, 0, 0 };
		int32& operator[](EPick Row) { return Picks[(int32)Row]; }
		int32 operator[](EPick Row) const { return Picks[(int32)Row]; }
	};

	SIBELIUSGAME_API FAssignment Surprise(FRandomStream& Rng);

	/** The player's code word, "LA-4827": proof a game came from one of their assignments. */
	SIBELIUSGAME_API FString CodeWord(int32 PlayerNumber);

	/**
	 * ONE CODE WORD PER PC, kept beside the games: <Folder>/code word.txt. Walt, 2026-10-23: "yes
	 * do the code word file fix". It lived only in the progression save at first - which a New
	 * Game wipes, and which the editor and the packaged game keep in different places - so one
	 * player could have two codes, and his own games would lose their mark. The scan reads only
	 * .html/.htm, so the file never shows up as a game.
	 */
	SIBELIUSGAME_API FString CodeFilePath(const FString& Folder);
	SIBELIUSGAME_API FString CodeFileText(int32 PlayerNumber);
	/** The number in the first "LA-1234" in Text, any case; 0 if there is none (or it is 0000). */
	SIBELIUSGAME_API int32 ParseCodeWord(const FString& Text);

	/** "tiny-rocket-dyson-sphere.html" - only a suggestion; any .html in the folder plays. */
	SIBELIUSGAME_API FString FileNameFor(const FAssignment& A);

	/**
	 * The prompt the player pastes into a model. It names the folder as "Documents\Leonard
	 * Sibelius\Arcade", never the full path: the full path carries the Windows user name, and
	 * this text leaves the machine.
	 */
	SIBELIUSGAME_API FString ComposePrompt(const FAssignment& A, const FString& Code);

	/** Documents/Leonard Sibelius/Arcade/ - absolute, forward slashes, trailing slash. */
	SIBELIUSGAME_API FString ArcadeFolder();

	/** The same folder the way Windows writes it, for the menu (it stays on this machine). */
	SIBELIUSGAME_API FString FolderForDisplay(const FString& Folder);

	/**
	 * THE ARCADE'S OWN GAMES: Content/Arcade, shipped in every build as loose files (DefaultGame.ini,
	 * DirectoriesToAlwaysStageAsNonUFS - Chromium cannot read a .pak). Walt, 2026-10-23: "can we
	 * make seven more sample games to be placed in the arcade?" Eight, one of each kind of game,
	 * each made from one prompt the menu could have handed out. They came with the game, so they
	 * play without the gate.
	 */
	SIBELIUSGAME_API FString SampleFolder();

	struct FGame
	{
		FString Path;        // absolute
		FString FileName;
		FString Title;       // the page's <title>, else the file name made readable
		bool bHasCode = false;
		bool bBuiltIn = false;   // one of the arcade's own (SampleFolder)
		FString Picks;           // the picks it was made from, if the page says (PicksOf)
		FDateTime Modified;
		int64 Bytes = 0;
	};

	/** Every .html/.htm in the folder, newest first. A file over MaxGameBytes is left out. */
	SIBELIUSGAME_API TArray<FGame> FindGames(const FString& Folder, const FString& Code);
	constexpr int64 MaxGameBytes = 20 * 1024 * 1024;

	SIBELIUSGAME_API FString TitleOf(const FString& Html);

	/**
	 * The four picks a sample was made from, from its <meta name="leonards-arcade-picks"
	 * content="..."> - so the menu can show, under each sample, what the four rows do.
	 */
	SIBELIUSGAME_API FString PicksOf(const FString& Html);

	/**
	 * Picks ("World · Hero · Game · Twist", by label) back into an assignment, so a sample's "See
	 * the prompt" can put the prompt it was made from in the menu. Walt, 2026-10-23: "if i want to
	 * see the prompt for each game, how do i do that?" False, and Out untouched, unless all four
	 * labels are picks the menu has.
	 */
	SIBELIUSGAME_API bool AssignmentFromPicks(const FString& Picks, FAssignment& Out);
	SIBELIUSGAME_API FString ReadableFileName(const FString& FileName);

	/** file:///C:/Users/.../Leonard%20Sibelius/Arcade/game.html */
	SIBELIUSGAME_API FString FileUrl(const FString& AbsolutePath);

	/**
	 * THE BROWSER'S ONE RULE: may the page load this? Files on this machine (file:///), and what
	 * a page makes for itself (data:, blob:, about:), yes. Anything with a host - http, https,
	 * ws, ftp, and file://host, a network share - no. So the game
	 * that plays is exactly the file the player saved: it cannot load a script that changes
	 * tomorrow, send anything anywhere, or take the player off to a website.
	 */
	SIBELIUSGAME_API bool IsOffline(const FString& Url);

	/** "cdnjs.cloudflare.com" from "https://cdnjs.cloudflare.com/ajax/x.js" - for the notice. */
	SIBELIUSGAME_API FString HostOf(const FString& Url);
}
