// VirtualHome.h
//
// AN AGENT'S VIRTUAL HOME - one per level that is one. Walt, 2026-10-03, designing Kaia's:
// "do ai agents even need houses? I could just start with Kaia standing there with a chat
// introducing me to her planet", then: "Welcome to my virtual home, Leonard. I can Refactor
// it as I please" - "and then the player can use the Refactor power, turning the strange
// plants into animals as in the living room without having to earn it at the slot machine."
//
// What placing one does, on arrival:
//
//   HE GOES AS HIMSELF, in first person, as in the office (Walt, 2026-10-04 - see
//   bThirdPerson). He went as Greystone at first ("it will be good to have the third-person
//   deal where our Epic Games character is viewed from behind"): battle form's avatar and
//   camera, with the powers LEFT ON, which SibeliusAim's eye-level rays made possible. That
//   is still one switch away.
//
//   POWERS ARE LENT. ASibeliusGameCharacter::CheckPowerUnlocked asks LendsPower before the
//   progression gate, so R works here whether or not the slot machine has granted it. Lent,
//   not granted: nothing is written to the save, and back in the house it is his again only
//   if he earned it.
//
//   THE WHOLE PLANET IS CLAY. The wild refactor refuses anything over 3.5 m so walls and
//   buildings never turn to gold (URefactorComponent::MaxWildTargetSize). Her coral trees
//   are bigger than that, and in her world reshaping them is the point, so the limit is
//   raised here and nowhere else. Treasure costs sauce here as everywhere (Walt,
//   2026-10-03, when the animals became gems and gold): the power is lent, the wallet is his.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProgressionTypes.h"
#include "VirtualHome.generated.h"

UCLASS()
class SIBELIUSGAME_API AVirtualHome : public AActor
{
	GENERATED_BODY()

public:
	AVirtualHome();

	/** Whose home this is, for the log. (Not "Owner": AActor already has one.) */
	UPROPERTY(EditAnywhere, Category = "Virtual Home")
	FString AgentName = TEXT("Kaia");

	/** Arrive in Greystone's body, seen from behind. OFF since 2026-10-04 - Walt: "Greystone is
	 *  starting to annoy me. I am thinking i want to go into Kaia world as first-person like in
	 *  the office." Kept as a switch: everything it needs (battle form with the powers left
	 *  on, SibeliusAim's eye-level rays) still works if a home wants it. */
	UPROPERTY(EditAnywhere, Category = "Virtual Home")
	bool bThirdPerson = false;

	/** Powers that work here without being earned. */
	UPROPERTY(EditAnywhere, Category = "Virtual Home")
	TArray<EPowerVerb> LentPowers = { EPowerVerb::Refactor };

	/** The wild refactor's size limit here, cm (the house's is 350). A coral tree is ~10 m. */
	UPROPERTY(EditAnywhere, Category = "Virtual Home", meta = (ClampMin = "100.0"))
	float WildRefactorMaxSize = 3000.0f;

	/** No treasure smaller than this here, cm. Her big mushrooms are built in pieces, and R
	 *  mostly meets a stalk: a 77 cm coin seven metres off was lost in her ferns ("refactor
	 *  made mushroom disappear" - Walt, 2026-10-03: "yes do the 2 m minimum in Kaia's home"). */
	UPROPERTY(EditAnywhere, Category = "Virtual Home", meta = (ClampMin = "0.0"))
	float MinTreasureSize = 200.0f;

	/* HAIR STAYS OUT OF RAY TRACING HERE. The first Play in L_KaiaHome crashed the GPU three
	   seconds in: DXGI_ERROR_DEVICE_HUNG with a page fault, the breadcrumbs inside
	   HairCardsInterpolation > CommitHairRayTracingGeometryUpdates - Kaia's MetaHuman hair
	   being handed to ray tracing. Seen from Greystone's third-person distance her groom
	   most likely drops to its cards LOD; in the house she is always close enough for
	   strands. Walt typed r.HairStrands.Raytracing 0 and played again: "it works, no crash".
	   Hair still draws; it only leaves ray-traced reflections and shadows. Put back as it
	   was when the level ends. */
	UPROPERTY(EditAnywhere, Category = "Virtual Home")
	bool bHairOutOfRayTracing = true;

	/* THE WORLD'S OWN DIALS. Etherion's pack keeps its look in a material parameter
	   collection (MPC_AlienJungleAppearance): FakeLighting, which their level blueprint set
	   to 0.3 at the start (their NOTE board explains it), and hue and wetness dials.
	   L_KaiaHome's level blueprint is emptied - it also spawned their demo controller, which
	   moved the player into the lake (see make_kaia_home.py) - so the values it needs are set
	   here instead. Set on the placed actor; the collection is the pack's, which is why
	   nothing is named in C++. */
	UPROPERTY(EditAnywhere, Category = "Virtual Home|Look")
	TObjectPtr<class UMaterialParameterCollection> Appearance;

	UPROPERTY(EditAnywhere, Category = "Virtual Home|Look")
	TMap<FName, float> AppearanceScalars;

	/* THE PLANTS ANSWER HIM. Etherion's foliage only comes alive - mushrooms that boing, ferns
	   that part - near one of their influencer actors, which swaps the painted plants around it
	   for live physics copies. Their demo mannequin carried three; Greystone carried none, so
	   the "balloon mushrooms that boing when he touches them" Kaia talks about did nothing
	   (Walt, 2026-10-03: "Greystone can't interact with the mushrooms in Kaia's world even
	   though she says he can"). One rides on him from arrival. Set to the pack's
	   BP_FoliageInfluencer on the placed actor (make_kaia_home.py); nothing is named in C++,
	   as with Appearance. Its collision is switched off - it works from its radius, not
	   physics, and must never be what an E or R ray meets. */
	UPROPERTY(EditAnywhere, Category = "Virtual Home|Foliage")
	TSubclassOf<AActor> FoliageInfluencer;

	/* HER PLACED PLANTS BOUNCE WHEN SLAPPED. The big balloon mushrooms are plain static
	   meshes the plugin never brings to life, so F squashes and stretches them itself
	   (USlapComponent::SetPlantWobble) and plays these: a boing for mushrooms, a rustle for
	   the rest. The pack's own sounds, set on the placed actor (make_kaia_home.py). */
	UPROPERTY(EditAnywhere, Category = "Virtual Home|Foliage")
	TObjectPtr<class USoundBase> MushroomSound;

	UPROPERTY(EditAnywhere, Category = "Virtual Home|Foliage")
	TObjectPtr<class USoundBase> PlantSound;

	/** How far around him plants come alive, cm; 0 keeps the blueprint's own (200). As far as
	 *  his sword swing reaches - 250 cm ahead plus its 150 cm sweep (USlapComponent) - since
	 *  only a live plant can answer F (Walt: "some kind of noise and movement when he slaps
	 *  them with F"). */
	UPROPERTY(EditAnywhere, Category = "Virtual Home|Foliage", meta = (ClampMin = "0.0"))
	float FoliageInfluencerRadius = 400.0f;

	/** True when a virtual home in this world lends Verb. */
	static bool LendsPower(const UObject* WorldContext, EPowerVerb Verb);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	/** The pawn is possessed a moment after BeginPlay; this waits for it. */
	void Arrive();

	FTimerHandle ArriveTimer;
	int32 ArriveAttempts = 0;

	/** Hang FoliageInfluencer on him. */
	void CarryFoliageInfluencer(class ACharacter* Player);

	UPROPERTY() TObjectPtr<AActor> CarriedInfluencer;

	/** r.HairStrands.Raytracing as it was before this level changed it; INDEX_NONE if untouched. */
	int32 SavedHairRaytracing = INDEX_NONE;
};
