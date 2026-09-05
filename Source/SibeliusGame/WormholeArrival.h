// WormholeArrival.h — the passage to Grok, which is also the arrival on it.
//
// ===========================================================================
// docs/SEVENTH_POWER.md rev 2-5.
//
// Walt: "a vague idea about approaching a blue ghost and being invited to travel through
// an abstract dreamlike realm of particle art to get to planet Grok, instead of buying a
// spaceship asset and going through all the 40 light year journey routing."
//
// ---------------------------------------------------------------------------
// WHY THE PASSAGE IS NOT ITS OWN LEVEL.
//
// The obvious build is a cinematic level between the city and Grok: travel there, fly a
// Sequencer shot through particles, travel again. That is two loading screens and a hard
// cut into Grok at the end — and the reveal of the planet is the shot the whole ending
// rests on, so a seam is the last thing it can afford.
//
// So there is no passage level. He travels straight to L_Grok and arrives inside the
// effect, which fades away until the landscape is simply there. Rev 2 argued for this
// before anyone knew it was cheaper: "the drift does not cut to a place, it thickens
// into one."
//
// ---------------------------------------------------------------------------
// IT IS NIAGARA NOW, AND THE FIRST VERSION WAS NOT.
//
// The first build made the cloud out of static-mesh cubes wearing M_materialise, because
// that was the part I could author from C++ without touching the editor. Walt's verdict
// was exact: "a flying cube wormhole?" It was. Swapping cubes for spheres would have
// made it a flying sphere wormhole. Meshes with a translucent material are an imitation
// of a particle system, and the imitation has a ceiling.
//
// Portal and SavePoint VFX (Dr.Game, $5) is the thing that unblocked it — 14 real Niagara
// systems whose parameters are EXPOSED as overrides, so they can be driven from C++
// without anyone authoring a node graph. That was the gap.
//
// ---------------------------------------------------------------------------
// TWO THINGS THAT WOULD OTHERWISE BITE.
//
// HARD REFERENCE, NOT A SOFT PATH. Content/PortalVFX/ is gitignored, and a Niagara system
// named only by soft path from C++ is not a package reference — the cooker will not
// follow it. It would work perfectly in PIE and be MISSING from the shipped build, which
// is the v0.7.4 invisible-spaceport bug, in the last scene of the game. The constructor
// hard-references the default via FObjectFinder so the cook has a real edge to follow.
//
// AND THE SYSTEM IS PICKED BY CONSOLE VARIABLE. There are 14 of them and choosing between
// them is an eye judgement, not a code one. `sib.WormholeFX NS_HeavenPath` swaps it at
// runtime; a rebuild per candidate would be fourteen editor restarts to answer a question
// about taste. The boarding lamp learned this the same evening.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WormholeArrival.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;

UCLASS()
class SIBELIUSGAME_API AWormholeArrival : public AActor
{
	GENERATED_BODY()

public:
	AWormholeArrival();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** How long the effect runs before he gets the controls and the planet. */
	UPROPERTY(EditAnywhere, Category = "Wormhole", meta = (ClampMin = "0.5"))
	float PassageSeconds = 7.0f;

	/* WHICH OF THE FOURTEEN. NS_DreamLand is the default because Walt's own words for what
	   he wanted were "an abstract dreamlike realm of particle art", and the pack happens to
	   ship a system with that name. NS_HeavenPath, NS_TeleporterHole and NS_TwistEddy are
	   the other obvious candidates — try them with sib.WormholeFX rather than rebuilding. */
	UPROPERTY(EditAnywhere, Category = "Wormhole")
	TSoftObjectPtr<UNiagaraSystem> PassageFX;

	/* HOW FAR IN FRONT OF HIM IT STANDS, in cm along his facing.

	   NOT zero, which is what the first Niagara build used. These are DOORWAYS - authored
	   to be viewed from a few metres - and centring one on the player put him inside a flat
	   sheet: "just a weird vertical thing with no animation". Live on sib.WormholeFXAhead. */
	UPROPERTY(EditAnywhere, Category = "Wormhole", meta = (ClampMin = "0.0"))
	float FXAhead = 450.0f;

	/** Lifted off the ground so a doorway stands rather than sinks. */
	UPROPERTY(EditAnywhere, Category = "Wormhole")
	float FXUp = 120.0f;

	/** Scale. Authored for a doorway, so 1 is the honest starting point, not 3. */
	UPROPERTY(EditAnywhere, Category = "Wormhole", meta = (ClampMin = "0.1"))
	float FXScale = 1.0f;

	/** Tint, if the chosen system exposes a Color override. Cyan, to match the apparitions. */
	UPROPERTY(EditAnywhere, Category = "Wormhole")
	FLinearColor FXColor = FLinearColor(0.15f, 0.85f, 1.0f, 1.0f);

	/** Any key ends it early. On by default: nobody wants a cutscene twice. */
	UPROPERTY(EditAnywhere, Category = "Wormhole")
	bool bSkippable = true;

private:
	void Finish();
	void HoldPlayer(bool bHold);
	UNiagaraSystem* ResolveFX() const;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> FXComponent;

	float Elapsed = 0.0f;
	bool bRunning = false;
	bool bFinished = false;
};
