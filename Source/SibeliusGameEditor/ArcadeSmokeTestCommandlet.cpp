// ArcadeSmokeTestCommandlet.cpp — headless gate for Leonard's Arcade. See header.
//
// Pure library calls and a scratch folder under Saved/ that it makes and removes itself. NAMED
// namespace + function-scoped `using` (unity-build safe), no variable shadowing.

#include "ArcadeSmokeTestCommandlet.h"
#include "ArcadeLibrary.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Math/RandomStream.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcadeSmoke, Log, All);

namespace ArcadeSmokeTestNS
{
	struct FResult
	{
		int32 Failures = 0;
		void Check(bool bCondition, const FString& Label)
		{
			if (bCondition)
			{
				UE_LOG(LogArcadeSmoke, Display, TEXT("  [PASS] %s"), *Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogArcadeSmoke, Error, TEXT("  [FAIL] %s"), *Label);
			}
		}
	};

	bool IsSlug(const TCHAR* Slug)
	{
		const FString S(Slug);
		if (S.IsEmpty())
		{
			return false;
		}
		for (const TCHAR C : S)
		{
			if (!((C >= 'a' && C <= 'z') || (C >= '0' && C <= '9') || C == '-'))
			{
				return false;
			}
		}
		return true;
	}
}

UArcadeSmokeTestCommandlet::UArcadeSmokeTestCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UArcadeSmokeTestCommandlet::Main(const FString& Params)
{
	using namespace ArcadeSmokeTestNS;
	using SibArcade::EPick;

	UE_LOG(LogArcadeSmoke, Display, TEXT("=== Leonard's Arcade smoke test ==="));
	FResult R;

	// --- 1) The picks are whole: enough of each, every label and slug filled in.
	for (int32 Row = 0; Row < (int32)EPick::Count; ++Row)
	{
		const EPick P = (EPick)Row;
		const int32 N = SibArcade::NumPicks(P);
		R.Check(N >= 6, FString::Printf(TEXT("%s has at least 6 picks (got %d)"), SibArcade::RowLabel(P), N));
		bool bWhole = true;
		for (int32 i = 0; i < N; ++i)
		{
			const SibArcade::FPick& Pick = SibArcade::GetPick(P, i);
			bWhole &= FCString::Strlen(Pick.Label) > 0 && FCString::Strlen(Pick.Prompt) > 0 && IsSlug(Pick.Slug);
		}
		R.Check(bWhole, FString::Printf(TEXT("every %s pick has a label, a prompt and a lowercase slug"), SibArcade::RowLabel(P)));
		R.Check(&SibArcade::GetPick(P, -1) == &SibArcade::GetPick(P, N - 1) && &SibArcade::GetPick(P, N) == &SibArcade::GetPick(P, 0),
			FString::Printf(TEXT("%s picks wrap at both ends"), SibArcade::RowLabel(P)));
	}

	// --- 2) The code word.
	R.Check(SibArcade::CodeWord(4827) == TEXT("LA-4827"), TEXT("CodeWord(4827) is LA-4827"));
	R.Check(SibArcade::CodeWord(7) == TEXT("LA-0007"), TEXT("CodeWord(7) pads to LA-0007"));
	R.Check(SibArcade::ParseCodeWord(SibArcade::CodeFileText(8214)) == 8214, TEXT("the code word file reads back its own number"));
	R.Check(SibArcade::ParseCodeWord(TEXT("my code is la-0042 ok")) == 42, TEXT("ParseCodeWord finds la-0042, any case"));
	R.Check(SibArcade::ParseCodeWord(TEXT("LA-12 then LA-12345 then nothing")) == 0, TEXT("ParseCodeWord wants exactly four digits"));
	R.Check(SibArcade::ParseCodeWord(TEXT("LA-0000")) == 0 && SibArcade::ParseCodeWord(TEXT("")) == 0, TEXT("LA-0000 and empty are no code"));
	R.Check(SibArcade::CodeFilePath(TEXT("C:/x/Arcade/")) == TEXT("C:/x/Arcade/code word.txt"), TEXT("the code word file sits in the folder"));

	// --- 3) The prompt: the picks, the code word, the Escape rule, offline, the folder by its
	//        Documents name - and never the full path, which carries the Windows user name.
	SibArcade::FAssignment A;
	A[EPick::World] = 4; A[EPick::Hero] = 0; A[EPick::Game] = 0; A[EPick::Twist] = 1;
	const FString Prompt = SibArcade::ComposePrompt(A, TEXT("LA-4827"));
	R.Check(Prompt.Contains(TEXT("Leonard's Arcade \u00B7 LA-4827")), TEXT("the prompt carries the code word line"));
	R.Check(Prompt.Contains(TEXT("Never use the Escape key")), TEXT("the prompt keeps the Escape key for the arcade"));
	R.Check(Prompt.Contains(TEXT("ONE self-contained HTML file")) && Prompt.Contains(TEXT("played offline")),
		TEXT("the prompt asks for one offline file"));
	R.Check(Prompt.Contains(SibArcade::GetPick(EPick::World, 4).Prompt) && Prompt.Contains(SibArcade::GetPick(EPick::Hero, 0).Prompt)
		&& Prompt.Contains(SibArcade::GetPick(EPick::Game, 0).Prompt) && Prompt.Contains(SibArcade::GetPick(EPick::Twist, 1).Prompt),
		TEXT("the prompt carries all four picks"));
	R.Check(Prompt.Contains(TEXT("Documents\\Leonard Sibelius\\Arcade")), TEXT("the prompt names the folder from Documents"));
	R.Check(Prompt.Contains(SibArcade::FileNameFor(A)), TEXT("the prompt names the suggested file"));
	FString UserDir = FPlatformProcess::UserDir();
	UserDir.RemoveFromEnd(TEXT("/"));
	const FString Who = FPlatformProcess::UserName();
	R.Check(!Prompt.Contains(UserDir) && (Who.Len() < 4 || !Prompt.Contains(Who)),   // a 2-letter name is in any text
		TEXT("the prompt never carries the user's folder or name"));
	R.Check(SibArcade::FileNameFor(A) == TEXT("tiny-rocket-dyson-sphere.html"),
		FString::Printf(TEXT("FileNameFor is hero-world.html (got %s)"), *SibArcade::FileNameFor(A)));

	// --- 4) The offline rule.
	R.Check(SibArcade::IsOffline(TEXT("file:///C:/Users/x/Documents/Leonard%20Sibelius/Arcade/a.html")), TEXT("a local file is offline"));
	R.Check(SibArcade::IsOffline(TEXT("data:image/png;base64,AAAA")) && SibArcade::IsOffline(TEXT("blob:null/1234"))
		&& SibArcade::IsOffline(TEXT("about:blank")), TEXT("data:, blob: and about: are offline"));
	R.Check(!SibArcade::IsOffline(TEXT("https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js")), TEXT("https is not"));
	R.Check(!SibArcade::IsOffline(TEXT("http://example.com/")) && !SibArcade::IsOffline(TEXT("ws://example.com/socket")),
		TEXT("http and ws are not"));
	R.Check(!SibArcade::IsOffline(TEXT("file://cdn.example.com/x.js")), TEXT("file://host - a network share - is not"));
	R.Check(SibArcade::HostOf(TEXT("https://cdnjs.cloudflare.com/ajax/x.js")) == TEXT("cdnjs.cloudflare.com"), TEXT("HostOf finds the host"));
	R.Check(SibArcade::HostOf(TEXT("http://localhost:8080/x")) == TEXT("localhost"), TEXT("HostOf drops the port"));

	// --- 5) File URLs.
	R.Check(SibArcade::FileUrl(TEXT("C:/Users/x y/Arcade/a b.html")) == TEXT("file:///C:/Users/x%20y/Arcade/a%20b.html"),
		TEXT("FileUrl encodes spaces"));
	R.Check(SibArcade::FileUrl(TEXT("C:\\Users\\x\\a.html")) == TEXT("file:///C:/Users/x/a.html"), TEXT("FileUrl turns backslashes"));
	R.Check(SibArcade::FileUrl(TEXT("C:/Caf\u00E9/a#1.html")) == TEXT("file:///C:/Caf%C3%A9/a%231.html"),
		TEXT("FileUrl encodes UTF-8 and #"));

	// --- 6) Titles and names.
	R.Check(SibArcade::TitleOf(TEXT("<html><head><TITLE>Corona &amp; Drift</TITLE></head>")) == TEXT("Corona & Drift"),
		TEXT("TitleOf reads <title>, any case, entities decoded"));
	R.Check(SibArcade::TitleOf(TEXT("<html><body>no title</body></html>")).IsEmpty(), TEXT("no <title>, no title"));
	R.Check(SibArcade::TitleOf(FString(TEXT("<title>")) + FString::ChrN(90, 'x') + TEXT("</title>")).Len() == 60,
		TEXT("a long title is cut to 60"));
	R.Check(SibArcade::ReadableFileName(TEXT("tiny-rocket_dyson-sphere.html")) == TEXT("Tiny rocket dyson sphere"),
		TEXT("ReadableFileName"));
	R.Check(SibArcade::PicksOf(TEXT("<meta name=\"leonards-arcade-picks\" content=\"Kaia&#39;s jungle · A cat\">")) == TEXT("Kaia's jungle · A cat"),
		TEXT("PicksOf reads the picks tag"));
	R.Check(SibArcade::PicksOf(TEXT("<meta name=\"viewport\" content=\"x\">")).IsEmpty(), TEXT("no picks tag, no picks"));
	bool bHinted = true;
	for (int32 Row = 0; Row < (int32)EPick::Count; ++Row)
	{
		bHinted &= FCString::Strlen(SibArcade::RowHint((EPick)Row)) > 0;
	}
	R.Check(bHinted, TEXT("every row says what it tells the model"));

	// --- 7) The folder scan, in a scratch folder.
	const FString Scratch = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ArcadeSmokeTest/"));
	IFileManager& FM = IFileManager::Get();
	FM.DeleteDirectory(*Scratch, false, true);
	FM.MakeDirectory(*Scratch, true);
	FFileHelper::SaveStringToFile(TEXT("<title>Made Here</title><p>Leonard's Arcade \u00B7 la-4827</p>"), *(Scratch / TEXT("made.html")));
	FFileHelper::SaveStringToFile(TEXT("<p>brought from elsewhere</p>"), *(Scratch / TEXT("other-game.htm")));
	FFileHelper::SaveStringToFile(TEXT("not a game"), *(Scratch / TEXT("notes.txt")));
	FFileHelper::SaveStringToFile(SibArcade::CodeFileText(4827), *SibArcade::CodeFilePath(Scratch));
	FM.SetTimeStamp(*(Scratch / TEXT("made.html")), FDateTime(2026, 10, 1));
	FM.SetTimeStamp(*(Scratch / TEXT("other-game.htm")), FDateTime(2026, 10, 20));
	const TArray<SibArcade::FGame> Games = SibArcade::FindGames(Scratch, TEXT("LA-4827"));
	R.Check(Games.Num() == 2, FString::Printf(TEXT("the scan finds the .html and the .htm, not the .txt or the code word file (got %d)"), Games.Num()));
	if (Games.Num() == 2)
	{
		R.Check(Games[0].FileName == TEXT("other-game.htm") && Games[1].FileName == TEXT("made.html"), TEXT("newest first"));
		R.Check(Games[1].bHasCode && !Games[0].bHasCode, TEXT("the code word marks the assignment's game, any case"));
		R.Check(Games[1].Title == TEXT("Made Here") && Games[0].Title == TEXT("Other game"), TEXT("titles: <title>, else the file name"));
	}
	R.Check(SibArcade::FindGames(Scratch / TEXT("missing"), TEXT("LA-4827")).Num() == 0, TEXT("a missing folder is an empty arcade"));
	FM.DeleteDirectory(*Scratch, false, true);

	// --- 8) The arcade's own games: all there, all titled, none asking the internet for anything
	//        (the TVs answer such a request with nothing, so a sample that made one would break).
	{
		const TArray<SibArcade::FGame> Samples = SibArcade::FindGames(SibArcade::SampleFolder(), FString());
		R.Check(Samples.Num() >= 8, FString::Printf(TEXT("Content/Arcade holds at least 8 sample games (got %d)"), Samples.Num()));
		bool bTitled = true, bOffline = true, bPicked = true;
		for (const SibArcade::FGame& G : Samples)
		{
			FString Html;
			FFileHelper::LoadFileToString(Html, *G.Path);
			bTitled &= !SibArcade::TitleOf(Html).IsEmpty();
			TArray<FString> Parts;
			G.Picks.ParseIntoArray(Parts, TEXT(" · "));
			SibArcade::FAssignment Back;
			bPicked &= Parts.Num() == 4 && SibArcade::AssignmentFromPicks(G.Picks, Back);   // "See the prompt" can show it
			const bool bReaches = Html.Contains(TEXT("http:")) || Html.Contains(TEXT("https:")) || Html.Contains(TEXT("src=")) || Html.Contains(TEXT("<link"));
			if (bReaches)
			{
				UE_LOG(LogArcadeSmoke, Error, TEXT("  %s names something outside its own file"), *G.FileName);
			}
			bOffline &= !bReaches;
		}
		R.Check(bTitled, TEXT("every sample has a <title>"));
		R.Check(bPicked, TEXT("every sample names four picks the menu has, so its prompt can be shown"));
		R.Check(bOffline, TEXT("no sample loads anything from outside its file"));
	}

	// --- 9) The real folder, and Surprise.
	R.Check(SibArcade::ArcadeFolder().EndsWith(TEXT("/Leonard Sibelius/Arcade/")), TEXT("the folder is Documents/Leonard Sibelius/Arcade/"));
	FRandomStream Rng(42);
	bool bInRange = true;
	for (int32 i = 0; i < 200; ++i)
	{
		const SibArcade::FAssignment S = SibArcade::Surprise(Rng);
		for (int32 Row = 0; Row < (int32)EPick::Count; ++Row)
		{
			bInRange &= S.Picks[Row] >= 0 && S.Picks[Row] < SibArcade::NumPicks((EPick)Row);
		}
	}
	R.Check(bInRange, TEXT("Surprise stays in range"));

	const int32 ExitCode = R.Failures;
	if (R.Failures == 0)
	{
		UE_LOG(LogArcadeSmoke, Display, TEXT("=== ARCADE SMOKE TEST PASSED ==="));
	}
	else
	{
		UE_LOG(LogArcadeSmoke, Error, TEXT("=== ARCADE SMOKE TEST FAILED: %d assertion(s). ==="), R.Failures);
	}
	if (FApp::IsUnattended())
	{
		RequestEngineExit(TEXT("ArcadeSmokeTest complete"));
	}
	return ExitCode;
}
