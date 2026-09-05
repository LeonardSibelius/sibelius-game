// Spaceport.h — what Generate makes on the lawn across from Jacob's.
//
// ===========================================================================
// docs/SPACEPORT_PLAN.md, Phase B.
//
// Leonard types "spaceport" and the ground opens. Pad slabs, gantry legs, a tower and
// two fuel tanks rise out of the grass over about eight seconds. Phase C puts a rocket
// on the pad; this class is the thing that receives it.
//
// ---------------------------------------------------------------------------
// WHY IT EXTENDS ABuildSite AND NOT AActor.
//
// Everything UGenerateComponent creates is an ABuildSite, and that is load-bearing
// rather than incidental. The site is what carries:
//
//     IBranchable     Test-Drive can branch a spaceport and DISCARD it for free
//     MarkGenerated   provenance, so Deploy knows which catalog row made it
//     a stable GUID   identity that survives save and re-spawn
//
// The plan's best mechanic — branch before you launch, so a failed launch costs
// nothing — therefore needs no branching code written for it at all. It is inherited.
// A plain AActor would have looked identical and sat outside all three systems.
//
// ---------------------------------------------------------------------------
// THE PARTS ARE DATA, AND THE MESHES ARE VENDOR ASSETS THAT COOK BY DIRECTORY RULE.
//
// ModularSciFiEnv_F and _J are gitignored vendor packs, and a soft path from C++ is not
// a package reference — the cooker does not follow it. Without the
// DirectoriesToAlwaysCook lines added alongside this class, the spaceport assembles
// perfectly in PIE and is INVISIBLE in the shipped build. That is the v0.7.4 bug
// exactly, and it is why those lines exist in DefaultGame.ini.
//
// Parts default to a layout built in the constructor, and every field is EditAnywhere,
// so the shape is tunable on a placed instance without a rebuild.

#pragma once

#include "CoreMinimal.h"
#include "BuildSite.h"
#include "Spaceport.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UNiagaraSystem;
class UNiagaraComponent;

/**
 * One part's own materials, kept so they can be handed back.
 *
 * A USTRUCT wrapper rather than TArray<TArray<>>, because UPROPERTY cannot reflect a
 * nested array and an unreflected TObjectPtr array is a garbage-collection bug waiting
 * for a quiet afternoon.
 */
USTRUCT()
struct FSpaceportPartMaterials
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> Slots;
};

/**
 * One piece of the spaceport: a mesh, where it sits, and when it arrives.
 *
 * RiseAt/RiseFor are FRACTIONS of the whole assembly (0..1), not seconds, so retuning
 * AssemblySeconds re-times every part together instead of desynchronising them.
 */
USTRUCT()
struct FSpaceportPart
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Spaceport")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, Category = "Spaceport")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Spaceport")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, Category = "Spaceport")
	FVector Scale = FVector::OneVector;

	/** When this part starts rising, as a fraction of the whole assembly. */
	UPROPERTY(EditAnywhere, Category = "Spaceport", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RiseAt = 0.0f;

	/** How long it takes to arrive, as a fraction. Clamped so it can never be zero. */
	UPROPERTY(EditAnywhere, Category = "Spaceport", meta = (ClampMin = "0.02", ClampMax = "1.0"))
	float RiseFor = 0.25f;
};

UCLASS()
class SIBELIUSGAME_API ASpaceport : public ABuildSite
{
	GENERATED_BODY()

public:
	ASpaceport();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/* BRANCH STATE — deliberately wider than ABuildSite's built/unbuilt, and deliberately
	   numbered so Phase C can grow into it without changing what a saved 0 or 1 means.

	       0  Empty        the lawn (ABuildSite's "not built")
	       1  Assembled    standing, pad clear (ABuildSite's "built")
	       2  RocketOnPad  Phase C
	       3  Launched     Phase C

	   0 and 1 keep the parent's meaning exactly, so an existing save reading 1 gets a
	   finished spaceport rather than a surprise. */
	virtual uint8 CaptureBranchState() const override;
	virtual void RestoreBranchState(uint8 InState) override;

	/* THE ASSEMBLY IS A SHOW, AND A RELOAD IS NOT.

	   Both live generation and re-spawn-on-load funnel through AuthorGeneratedSite ->
	   RestoreBranchState(1), which is correct: from the save's point of view both mean
	   "this spaceport exists". But the player should watch it rise ONCE, when he asks for
	   it — not every time he loads a save.

	   So RestoreBranchState always SNAPS, and this hook (empty on ABuildSite) is what the
	   live path calls to replay the show. It is a virtual on the base rather than a cast
	   in UGenerateComponent so the generator never needs to know this class exists. */
	virtual void OnGeneratedFresh() override;

	virtual FText GetInteractionPrompt_Implementation() const override;

	/** Raise the parts over AssemblySeconds. Safe to call when already assembled. */
	void PlayAssembly();

	/** Fully built, instantly, no animation — the reload path and the smoke tests. */
	void SnapAssembled();

	/** Back to bare lawn: every part component destroyed. */
	void ClearParts();

	bool IsAssembled() const { return bAssembled; }

	/** The world-space point a rocket stands on. Phase C's entry point. */
	UFUNCTION(BlueprintPure, Category = "Spaceport")
	FVector GetPadTopLocation() const;

	UPROPERTY(EditAnywhere, Category = "Spaceport")
	TArray<FSpaceportPart> Parts;

	/** Total seconds for the whole structure to arrive. */
	UPROPERTY(EditAnywhere, Category = "Spaceport", meta = (ClampMin = "0.5"))
	float AssemblySeconds = 8.0f;

	/* PARTS MATERIALISE; THEY NO LONGER CLIMB OUT OF THE GROUND (Walt, 2026-09-01).

	   The first version raised each part from RiseFromBelow centimetres under the lawn.
	   With a greybox layout that read as "columns sliding out of the pavement", which
	   Walt summarised as looking like hell. Rising is kept as an OPTIONAL flourish and
	   defaulted OFF: set it above zero and parts drift up as they form, which reads well
	   once the proportions are real. Zero is the honest default until then. */
	UPROPERTY(EditAnywhere, Category = "Spaceport", meta = (ClampMin = "0.0"))
	float RiseFromBelow = 0.0f;

	/* THE APPARITION MATERIAL EVERY PART WEARS WHILE IT FORMS.

	   Built by Tools/Scripts/build_materialise_material.py, and deliberately NOT loaded
	   in the constructor — GS_Idle_MH crashed the editor on startup that way and the
	   rule stands: a constructor runs before the editor exists, so a bad asset there is
	   a project that will not open. Loaded on demand, in a running world. */
	UPROPERTY(EditAnywhere, Category = "Spaceport")
	TSoftObjectPtr<UMaterialInterface> MaterialiseMaterial;

	/** How brightly a forming part glows. Handed to the material's Glow parameter. */
	UPROPERTY(EditAnywhere, Category = "Spaceport", meta = (ClampMin = "0.0"))
	float MaterialiseGlow = 6.0f;

	/* Height above the actor origin where the rocket stands — Phase C's entry point.

	   1433.2 is not a guess: it is SM_Rocket's own Z in PackDev's showcase map, once the
	   layout is re-centred with the launch pad's base at zero. Phase C's physics body
	   replaces the static rocket at exactly the height the artist stood it at. */
	UPROPERTY(EditAnywhere, Category = "Spaceport")
	float PadTopHeight = 1433.2f;

	/* ===================================================================================
	   BOARDING — "C", the pre-flight compile.  docs/SPACEPORT_PLAN.md Phase C.

	   Walt, 2026-09-03: "why not have Leonard just go to the space port and type 'C' and
	   suddenly he is inside the ship?"

	   That is not a shortcut around the plan; it IS the plan's own row for that key —
	   "C | Compile | Pre-flight.  A design with TWR < 1 or an empty tank is a COMPILE
	   ERROR, read out like any other refusal" — with the failure cases that exist today
	   (no supplies aboard, standing too far away) in place of the ones Phase C's physics
	   will add later.  Compiling is what Leonard does for a living, and in six chapters
	   this game has never once asked him to climb a ladder.

	   AND C IS ALREADY THE COMPILE KEY — SibeliusControls.cpp, the list in the M menu,
	   has said so since the powers were written.  So this adds NO BINDING.  It is one more
	   case in ASibeliusGameCharacter::OnBuildPressed, beside the attic key and the temple
	   sauce bowl, under the rule those two already follow: one verb, disambiguated by
	   state.  A raw BindKey on C would have CONSUMED the press and killed the Compile power
	   outright — every build site, the bowl, the attic orb — which is exactly what the
	   first draft of this feature did before the controls list was read.

	   IT ALL LIVES ON ASpaceport, NOT ON THE CHARACTER, because the crew compartment's
	   coordinates are this class's business and nobody else's.  The character finds a
	   spaceport and asks it; it never learns that the interior is 119 metres up the nose.
	   =================================================================================== */

	/* Run the pre-flight for this pawn: board, disembark, or refuse with a reason.

	   TRUE only when the press was THIS spaceport's to answer.  C is the Compile key and
	   Compile has other work, so a press from across the lawn returns false and falls
	   through to UBuildComponent — a spaceport standing 160 metres away must not silently
	   eat the verb everywhere in the city. */
	bool PreflightCompile(APawn* Pawn);

	/** True while the player is standing in the crew compartment. Session-only, never saved. */
	bool IsAboard() const { return bAboard; }

	/* THE NEAREST SPACEPORT THE KEY SHOULD TALK TO.

	   Prefers one the player is already ABOARD — because once he is up the nose, plain
	   distance from the actor's base disqualifies the only spaceport that matters, and C
	   would quietly stop being the way back out. */
	static ASpaceport* FindForPlayer(const UObject* WorldContext);

	/* WHERE HE STANDS ONCE ABOARD, in the spaceport's local space.

	   MEASURED, not derived - and the first version was derived, which cost a playtest.

	   That version was the centroid of the seven interior meshes' PLACEMENTS, read off
	   MakeDefaultLayout.  The teleport landed on it to the centimetre and Walt arrived in
	   open sky: "i turned around and saw I was just outside the capsule".  All seven pieces
	   share a Z and sit within 35 cm of each other in XY, which is the shape of props
	   exported from one scene with a COMMON PIVOT - not the shape of a room.  A pivot says
	   nothing about where a mesh's geometry is.

	   Tools/Scripts/dump_rocket_interior_bounds.py reads the bounds instead.  SM_Walls'
	   geometry is 7.4 m x 6.9 m x 3.7 m, its floor at local Z 11868.5, and its centre - once
	   the parts' own -52.28 deg yaw is applied to the pivot-to-geometry offset - is at
	   (15.7, 325.6).

	   AND THE CENTRE OF THE ROOM IS NOT FREE SPACE.  SM_Seats measures 6.4 m x 4.1 m in a
	   7.4 m x 6.9 m room, so the middle of the room is the middle of the seat rig and the
	   teleport refused it outright.  This is the corner clear of the seats, the controls,
	   the interface and the power box by their measured bounds - about 2.7 m from centre,
	   toward the wall none of them reach.  Board() searches outward from it, because bounds
	   give you the room and only the collision world gives you the space in it.

	   Z 11890 is read off what RESTS on the floor (the back wall bottoms out at 11888.7,
	   the power box at 11880.6), not off SM_Walls' lowest vertex at 11868.5 - a wall mesh
	   reaches below the surface people stand on.

	   IT IS THE FLOOR, NOT THE EYE: Board() adds the pawn's own capsule half-height, so
	   there is no 96 buried here to fall out of step with the character. */
	UPROPERTY(EditAnywhere, Category = "Spaceport|Boarding")
	FVector BoardingOffset = FVector(15.7f, 325.6f, 11890.0f);

	/** Which way he faces aboard. The interior's own yaw, so he is looking at the controls. */
	UPROPERTY(EditAnywhere, Category = "Spaceport|Boarding")
	float BoardingYaw = 162.8f;

	/* HOW NEAR THE STRUCTURE HE MUST BE FOR C TO MEAN ANYTHING.

	   120 m, and the first two numbers here were both wrong for the same reason: I priced
	   this like a doorway.  It is not one.

	   The catalog gives the spaceport SpawnAhead=16000 — it is planted 160 METRES ahead of
	   wherever the player is standing when he types it, deliberately ("a lamp is happy at
	   250 cm; a launch complex is not, and putting one there trapped Walt inside it"), and
	   the generator's wall clamp is skipped for entries that size.  So it lands where the
	   player was AIMING, not where he can WALK: Walt's went into a fenced meadow, and the
	   nearest he could physically get was 70 m from its outer edge.

	   A game that cannot promise you can reach the spaceport cannot require you to touch it
	   to board.  And nothing is lost by that: compiling yourself aboard from 70 m away is
	   the same fiction as compiling yourself aboard from 3 m away.

	   THE COST OF A RADIUS THIS BIG is that it could shadow an ordinary build site
	   somewhere inside it — so it does not.  OnBuildPressed offers the press here only when
	   UBuildComponent has nothing in reach. */
	UPROPERTY(EditAnywhere, Category = "Spaceport|Boarding", meta = (ClampMin = "100.0"))
	float BoardingRange = 12000.0f;

	/* THE HINT NEEDS A MUCH TIGHTER RADIUS THAN THE KEY, and conflating them was a bug.

	   BoardingRange is 120 m because the spaceport can land somewhere unreachable, so C has
	   to work from wherever a player can actually stand. The HINT has the opposite
	   requirement: it should fire when he arrives at the pad, not when he happens to pass
	   within 120 m of it.

	   Walt, playing a fresh game 2026-09-05: "When I exited uFoods, the C to board message
	   popped up and disappeared too quickly... I had to remember to press C because there
	   was no prompt." The uFoods street is inside 120 m of the lawn, so the one-shot hint
	   spent itself on a doorway two errands early and was never available again at the
	   place it was written for. */
	UPROPERTY(EditAnywhere, Category = "Spaceport|Boarding", meta = (ClampMin = "100.0"))
	float BoardingHintRange = 3000.0f;

	/* AND IT RE-ARMS WHEN HE LEAVES. A prompt shown once, at a glance, while walking past,
	   is a prompt nobody read. If he wanders beyond this it can be offered again. */
	UPROPERTY(EditAnywhere, Category = "Spaceport|Boarding", meta = (ClampMin = "100.0"))
	float BoardingHintRearmRange = 9000.0f;

	/* ===================================================================================
	   LAUNCH CUTSCENE — Unreal plays it, on THIS rocket, with Niagara (2026-09-03).

	   Not a pre-rendered film and not a second rocket in a black void. C while aboard
	   is ignition: cameras cut around the pad that Generate built, a Niagara Fluids
	   plume lights under PackDev's SM_Rocket, and the hull (plus the crew compartment
	   riding in its nose) climbs out of the city. Sequencer can author a matching shot
	   (Tools/Scripts/build_launch_shot.py → LS_Rocket_Launch); the runtime path does
	   not depend on that asset existing, because a missing sequence must never strand
	   him in the cabin.

	   Physics (TWR, fuel, max-Q) is still Phase C's rest. This is the voyage beat Nyra
	   already promised — time compression, skippable, then the pad is empty. */
	bool IsLaunching() const { return bLaunching; }
	bool HasLaunched() const { return bLaunched; }

	/** Soft ref to a Sequencer-authored shot. Empty / failed load = the in-place cameras. */
	UPROPERTY(EditAnywhere, Category = "Spaceport|Launch")
	TSoftObjectPtr<class ULevelSequence> LaunchSequence;

	/** Niagara Fluids gas, pointed down and orange. Constructor hard-refs the plugin
	    template so it cooks; BeginPlay prefers a game copy if build_launch_shot.py made one. */
	UPROPERTY(EditAnywhere, Category = "Spaceport|Launch")
	TSoftObjectPtr<class UNiagaraSystem> PlumeSystem;

	UPROPERTY(EditAnywhere, Category = "Spaceport|Launch")
	FLinearColor PlumeColor = FLinearColor(1.0f, 0.42f, 0.08f);

	/** Engine bell, relative to SM_Rocket's origin. Negative Z is toward the pad. */
	UPROPERTY(EditAnywhere, Category = "Spaceport|Launch")
	FVector PlumeRelativeOffset = FVector(0.0f, 0.0f, -400.0f);

	/** The gas template rises along its local +Z. Pitch 180 points that column at the ground. */
	UPROPERTY(EditAnywhere, Category = "Spaceport|Launch")
	FRotator PlumeRelativeRotation = FRotator(180.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, Category = "Spaceport|Launch", meta = (ClampMin = "0.2"))
	float PlumeScale = 5.0f;

	/** Whole cutscene, including the ignition hold. Skip ends it early. */
	UPROPERTY(EditAnywhere, Category = "Spaceport|Launch", meta = (ClampMin = "4.0"))
	float LaunchSeconds = 22.0f;

	/** Engines lit, hull not yet moving. */
	UPROPERTY(EditAnywhere, Category = "Spaceport|Launch", meta = (ClampMin = "0.0"))
	float IgnitionHoldSeconds = 2.5f;

	/** How far the hull climbs, in cm. 80 000 = 800 m, enough to leave the skyline. */
	UPROPERTY(EditAnywhere, Category = "Spaceport|Launch", meta = (ClampMin = "1000.0"))
	float ClimbHeight = 80000.0f;

	/** Programmed pitch after clearing the tower, degrees. Negative = gravity turn. */
	UPROPERTY(EditAnywhere, Category = "Spaceport|Launch")
	float GravityTurnDegrees = -12.0f;

private:
	/** Create the component for each part, hidden and sunk. Idempotent. */
	void BuildPartComponents();

	/** Place and shade one part at its progress (0 = not yet there, 1 = solid). */
	void ApplyPartProgress(int32 Index, float Alpha);

	/* PUT THE REAL MATERIALS BACK — the half that would otherwise be forgotten.

	   Taking a mesh's materials is visible instantly; failing to give them back is
	   invisible until someone notices the spaceport is permanently a cyan ghost. The
	   originals are captured the first time a part is dressed and restored the moment it
	   reaches full opacity, so a part that finishes early solidifies while its neighbours
	   are still forming. */
	void DressPart(int32 Index, bool bMaterialise);

	/** Loaded on demand, never in the constructor. Null if the asset is missing. */
	UMaterialInterface* GetMaterialiseMaterial();

	/* Retries the parts that stayed intangible because the player was inside them, and
	   stops itself once every part is solid. A timer rather than Tick: the structure is
	   finished by now and it would be wrong to keep an 8-second assembly's tick alive for
	   the rest of the level just in case somebody loiters under the gantry. */
	void PokeDeferredParts();

	FTimerHandle DeferredCollisionTimer;

	/** True while at least one part is waiting for the player to move. */
	bool bHasDeferredCollision = false;

	/** The authored default layout, filled in the constructor. */
	void MakeDefaultLayout();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PartComponents;

	/** Per-part: the mesh's own materials, captured before the apparition replaces them. */
	UPROPERTY(Transient)
	TArray<FSpaceportPartMaterials> OriginalMaterials;

	/** Per-part: the live apparition instance whose Opacity is being driven. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> MaterialiseMIDs;

	/** Per-part: true while wearing the apparition, so restore happens exactly once. */
	TArray<bool> PartIsMaterialising;

	/** Seconds since PlayAssembly began. Meaningless unless bAssembling. */
	float AssemblyElapsed = 0.0f;

	bool bAssembling = false;
	bool bAssembled = false;

	/* --- boarding, private half ------------------------------------------------------

	   ABOARD IS NOT A BRANCH STATE, and that is deliberate.  States 2 and 3 are reserved
	   for RocketOnPad and Launched — facts about the SPACEPORT, which persist and which
	   Test-Drive can branch and discard.  "Leonard happens to be standing in the nose" is
	   a fact about the PAWN, it lasts as long as he stays there, and writing it into the
	   save would mean a reload could put him inside a rocket he never boarded. */

	/* 2D distance from a point to the ASSEMBLED STRUCTURE, not to the actor origin - which
	   is the launch pad, with the apron 22 m away in +X and the base 24 m out.  Standing on
	   the complex returns 0.  See the definition for why the origin-distance version made
	   both the hint and the key go silent exactly where they were wanted. */
	float DistanceToStructure(const FVector& From) const;

	/** Lamp brightness: the console override if set, else BoardingLightLumens. */
	float BoardingLampLumens() const;

	/* Index in Parts of SM_Walls - the first of the seven crew-compartment pieces, which
	   MakeDefaultLayout appends last and in one run.  Their union IS the room, and boarding
	   requires the spot to be inside it: empty air beside the hull passes an overlap test
	   perfectly, and that is exactly where three attempts at this kept landing. */
	static constexpr int32 FirstInteriorPart = 7;

	/** Send the VIEW into the compartment. False if the interior is not there to look at. */
	bool Board(APawn* Pawn);

	/** Give him back his eyes and his legs. He never left the lawn. */
	void Disembark(APawn* Pawn);

	/** Fade up from black on the far side of the cut. Harmless without a camera manager. */
	void FadeIn(APawn* Pawn) const;

	bool bAboard = false;

	/* THE CAMERA THAT GOES ABOARD IN HIS PLACE.  Transient and spawned on demand: it exists
	   only while he is looking through it, and a view is not something a save should carry. */
	UPROPERTY(Transient)
	TObjectPtr<class ACameraActor> BoardingView;

	/* THE CABIN LAMP the view carries in with it. A sealed hull 119 m up has no light of
	   its own, and the set was authored to be lit by whatever scene it lands in. */
	UPROPERTY(EditAnywhere, Category = "Spaceport|Boarding", meta = (ClampMin = "0.0"))
	float BoardingLightLumens = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Spaceport|Boarding", meta = (ClampMin = "50.0"))
	float BoardingLightRadius = 700.0f;

	/* ===================================================================================
	   THE WAY TO GROK — the portal that replaces the rocket.

	   Walt, 2026-09-05: "TeleporterHole in the city, with a [C] to enter Portal on it -
	   skip talking to the blue ghosts. Actually, put the portal at the spot that the [G]
	   for spaceport went, after the message fades out after the takeoff."

	   THREE DECISIONS, ALL HIS, ALL SIMPLIFICATIONS.

	   NO GHOST CONVERSATION. docs/SEVENTH_POWER.md rev 2 had a blue ghost invite him
	   through. That needed a new dialogue actor, a new recorded line, and a reason for one
	   particular ghost to break a silence it has kept all game. The portal simply opens.

	   ON THE SPACEPORT'S OWN GROUND. He typed "spaceport" on that lawn and watched a launch
	   complex assemble there; the rocket left from there; and now the way out opens on the
	   same spot. Nothing has to be found.

	   AND ON C, WHICH IS ALREADY THE KEY. C is the Compile verb and already means "the
	   pre-flight" at this actor - it is what boarded the rocket. So the key that put him on
	   the ship is the key that follows it. No new binding, and nothing to teach.

	   AFTER THE TOAST, NOT DURING IT. EndLaunch says "THE SHIP IS AWAY. NYRA WILL CALL FROM
	   GROK." for six seconds. A portal opening under that line would be two things at once;
	   opening as it clears is a beat.
	   =================================================================================== */

	/** True once the portal stands and C will take him through. */
	bool IsGrokPortalOpen() const { return GrokPortal != nullptr; }

	/* WHICH SYSTEM. NS_TeleporterHole is Walt's pick out of the fourteen in Portal and
	   SavePoint VFX, after seeing them on Grok. HARD-REFERENCED in the constructor:
	   Content/PortalVFX/ is gitignored, and a soft path from C++ is not a package
	   reference - it would work in PIE and be missing from the shipped build, which is the
	   v0.7.4 invisible-spaceport bug landing on the way to the last scene. */
	UPROPERTY(EditAnywhere, Category = "Spaceport|Grok")
	TSoftObjectPtr<UNiagaraSystem> GrokPortalSystem;

	/* HIGH ENOUGH TO SEE OVER THE BERM. Walt, 2026-09-05: "that tiny purple portal is at
	   the base of the rocket and I cannot walk there, the fence blocks my way." At 300 cm it
	   sat on the pad behind the launch complex's own retaining wall. Live on
	   sib.GrokPortalUp. */
	UPROPERTY(EditAnywhere, Category = "Spaceport|Grok")
	FVector GrokPortalOffset = FVector(0.0f, 0.0f, 1100.0f);

	/* AND MUCH BIGGER. These systems are authored as person-sized doorways; this one has to
	   read from across a field, against a 120 m rocket complex. Live on sib.GrokPortalScale. */
	UPROPERTY(EditAnywhere, Category = "Spaceport|Grok", meta = (ClampMin = "0.1"))
	float GrokPortalScale = 14.0f;

	/* THE SAME RANGE AS BOARDING, AND FOR THE SAME REASON.

	   25 m was the second time I priced a spaceport interaction like a doorway. The catalog
	   plants this thing 160 m ahead of the player and skips the wall clamp doing it, so it
	   lands where he was AIMING and not where he can WALK - his went behind a fence. If C
	   could board a rocket from 120 m it can enter the door that replaced it from 120 m. */
	UPROPERTY(EditAnywhere, Category = "Spaceport|Grok", meta = (ClampMin = "100.0"))
	float GrokPortalRange = 25000.0f;

	/** Seconds after the launch before it opens — long enough for the toast to clear. */
	UPROPERTY(EditAnywhere, Category = "Spaceport|Grok", meta = (ClampMin = "0.0"))
	float GrokPortalDelay = 7.0f;

	UPROPERTY(EditAnywhere, Category = "Spaceport|Grok")
	FName GrokLevelName = TEXT("L_Grok");

	/* TELLING HIM THE KEY EXISTS — once, when he first walks up to the pad able to use it.

	   Nyra's stage 3 sends him back here ("we will do the boarding procedures") and then
	   the game goes quiet: he is standing on the pad with supplies bought and nothing on
	   screen saying which of his six powers to try.  A mechanic nobody can find is not
	   built.

	   A one-second timer rather than Tick, for PokeDeferredParts' reason exactly: this is
	   a proximity poll on a finished building, and holding an eight-second assembly's tick
	   open for the rest of the level to run it would be wrong. */
	void StartBoardingHintWatch();
	void PollBoardingHint();

	/** Open the portal on the pad. bImmediate on a reload: never replay the wait. */
	void OpenGrokPortal(bool bImmediate);
	void ClearGrokPortal();

	UPROPERTY(Transient)
	TObjectPtr<class UNiagaraComponent> GrokPortal;

	FTimerHandle GrokPortalTimer;

	FTimerHandle BoardingHintTimer;
	bool bBoardingHintShown = false;

	/** Throttles the "still too far" log to one line in five. Diagnostic only. */
	int32 PollsWhileFar = 0;

	/* --- launch cutscene ---------------------------------------------------------- */

	/** C while aboard. False only if there is no rocket mesh to fly. */
	bool BeginLaunch(APawn* Pawn);

	void TickLaunch(float DeltaSeconds);
	void EndLaunch();
	void SkipLaunch();

	void AttachFlightPartsToRocket();
	void AttachPlume();
	void SpawnLaunchCameras();
	void DestroyLaunchRig();
	void CutToLaunchShot(int32 Shot);
	void HideFlightParts();
	void BindLaunchSkip(bool bBind);

	/** Hull + crew compartment (Meshes/Rocket/*). The pad stays. */
	bool IsFlightPart(int32 Index) const;
	class UStaticMeshComponent* FindRocketMesh() const;

	bool bLaunching = false;
	bool bLaunched = false;
	float LaunchElapsed = 0.0f;
	int32 LaunchShot = INDEX_NONE;
	bool bLaunchSkipBound = false;

	/** Assembled relative transform of SM_Rocket, captured at ignition so climb is a delta. */
	FTransform RocketRestRelative = FTransform::Identity;

	UPROPERTY(Transient)
	TObjectPtr<class UNiagaraComponent> Plume;

	UPROPERTY(Transient)
	TObjectPtr<class UPointLightComponent> PlumeLight;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class ACameraActor>> LaunchCams;

	UPROPERTY(Transient)
	TObjectPtr<APawn> LaunchPawn;
};
