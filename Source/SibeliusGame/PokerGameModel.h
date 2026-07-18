// PokerGameModel.h
//
// SIDE_GAMES G5 — video poker (Jacks or Better), the machine with choices.
// A PURE game model in the house pattern (USlotGameModel's sibling): no UI,
// no actors, no timers. Seeded RNG, honest 52-card deck, deterministic and
// headlessly simulatable. The screen (UPokerScreenWidget) and the cabinet
// (APokerMachine) only PRESENT what this class decides.
//
// Rules: deal 5 from a fresh shuffled deck; player holds any subset; the
// draw replaces the rest from the same deck; the final hand pays by the
// FULL-PAY 9/6 paytable (multiples of the bet):
//   Royal Flush 250 · Straight Flush 50 · Four of a Kind 25 · Full House 9
//   Flush 6 · Straight 4 · Three of a Kind 3 · Two Pair 2 · Jacks-or-Better 1
// Full-pay is deliberately generous (~99.5% RTP under perfect play, less in
// practice) — this house pays better than the Strip, same as the slot.
//
// Cards are int32 0..51: rank = Card % 13 (0=Two .. 8=Ten, 9=Jack, 12=Ace),
// suit = Card / 13 (0=Spades, 1=Hearts, 2=Diamonds, 3=Clubs).
//
// R5 note: a single hand CAN pay zero (that's poker); the R5 kindness here is
// the paytable choice, not a consolation — revisit only if Walt asks.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Math/RandomStream.h"
#include "PokerGameModel.generated.h"

UENUM(BlueprintType)
enum class EPokerHandRank : uint8
{
	Nothing        UMETA(DisplayName = "Nothing"),
	JacksOrBetter  UMETA(DisplayName = "Jacks or Better"),
	TwoPair        UMETA(DisplayName = "Two Pair"),
	ThreeOfAKind   UMETA(DisplayName = "Three of a Kind"),
	Straight       UMETA(DisplayName = "Straight"),
	Flush          UMETA(DisplayName = "Flush"),
	FullHouse      UMETA(DisplayName = "Full House"),
	FourOfAKind    UMETA(DisplayName = "Four of a Kind"),
	StraightFlush  UMETA(DisplayName = "Straight Flush"),
	RoyalFlush     UMETA(DisplayName = "Royal Flush")
};

// The settled outcome of one hand (after the draw).
USTRUCT(BlueprintType)
struct FPokerHandResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Poker") TArray<int32> Cards;  // 5 final cards
	UPROPERTY(BlueprintReadOnly, Category = "Poker") EPokerHandRank Rank = EPokerHandRank::Nothing;
	UPROPERTY(BlueprintReadOnly, Category = "Poker") int32 PayMultiplier = 0;   // × the bet
};

UCLASS(BlueprintType)
class SIBELIUSGAME_API UPokerGameModel : public UObject
{
	GENERATED_BODY()

public:
	static constexpr int32 HAND_SIZE = 5;
	static constexpr int32 DECK_SIZE = 52;

	// Must be called before Deal(). Seed makes the whole game deterministic.
	UFUNCTION(BlueprintCallable, Category = "Poker")
	void Init(int32 Seed);

	// Shuffle a fresh deck, deal 5. Model now awaits Draw().
	UFUNCTION(BlueprintCallable, Category = "Poker")
	TArray<int32> Deal();

	// Replace every un-held card (bit i of HoldMask = keep dealt card i) from
	// the same deck, evaluate, settle. Model returns to awaiting Deal().
	UFUNCTION(BlueprintCallable, Category = "Poker")
	FPokerHandResult Draw(int32 HoldMask);

	UFUNCTION(BlueprintPure, Category = "Poker")
	bool IsAwaitingDraw() const { return bAwaitingDraw; }

	// Pure helpers — exposed for the smoke test and the screen's paytable.
	static EPokerHandRank EvaluateHand(const TArray<int32>& Cards);
	static int32 PayMultiplier(EPokerHandRank Rank);
	static const TCHAR* RankDisplayName(EPokerHandRank Rank);

	static int32 RankOf(int32 Card) { return Card % 13; }   // 0=Two .. 9=Jack .. 12=Ace
	static int32 SuitOf(int32 Card) { return Card / 13; }   // 0=♠ 1=♥ 2=♦ 3=♣

private:
	FRandomStream Rng;
	TArray<int32> Deck;        // shuffled 52; Hand = Deck[0..4], draws continue at DeckCursor
	int32 DeckCursor = 0;
	TArray<int32> Hand;
	bool bAwaitingDraw = false;
	bool bInitialized = false;
};
