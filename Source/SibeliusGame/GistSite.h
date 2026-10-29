// GistSite.h — THE GIST in the world (docs/GIST_GENERATOR_PLAN.md, phase 2).
//
// GistRules.h decides WHAT the gist of a request is; this draws it. One instanced mesh per
// (engine shape, finish), each part an instance coloured by custom data, a box to bump into and
// to aim E at, and a brass plaque. It is an ABuildSite like every other generated object, so
// Branch / Test-Drive / Deploy treat it the same way; only its look and its "put it back" differ.
#pragma once

#include "CoreMinimal.h"
#include "BuildSite.h"
#include "GistSite.generated.h"

class UInstancedStaticMeshComponent;
class UBoxComponent;
class UTextRenderComponent;
class UMaterialInterface;
class UStaticMesh;
namespace Gist { struct FRecipe; }

/** A googly pupil: which instance it is, the eye it rolls around in, and where it looks now. */
struct FGistPupil
{
	int32 Piece = 0;
	int32 Instance = 0;
	FVector Eye = FVector::ZeroVector;     // toy space
	float Reach = 0.0f;                    // how far from the eye's centre it sits
	FVector Offset = FVector::ZeroVector;  // where it is now, eased toward the player
	FVector Scale = FVector::OneVector;
};

UCLASS()
class SIBELIUSGAME_API AGistSite : public ABuildSite
{
	GENERATED_BODY()

public:
	AGistSite();

	/** The EntryId a gist carries in place of a catalog row. */
	static const FName EntryId;

	/** The rules' answer for these words, or null when they refuse or nothing was typed.
	    Words load once from Content/Data; Blocklist is the one the matcher already uses. */
	static TUniquePtr<Gist::FRecipe> Plan(const FString& Prompt, const TArray<FString>& Blocklist);

	/** Draw a planned gist and present it built; bFreshlyGenerated plays the pop-in. */
	void Author(const FString& InPrompt, const Gist::FRecipe& Recipe, bool bFreshlyGenerated);

	/** Re-create a remembered or deployed gist: its words rebuild the same recipe (the rules
	    are deterministic), at the saved pose verbatim, under the saved id. Null if the words
	    no longer build anything. Used by both save paths, so they cannot drift. */
	static AGistSite* Respawn(UWorld* World, const FString& Prompt, const FTransform& Where, const FGuid& Id);

	const FString& GetPrompt() const { return Prompt; }
	FString GetPlaqueText() const;

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnGeneratedFresh() override;
	virtual void RestoreBranchState(uint8 InState) override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

private:
	/** What the behaviours move and the reveal scales; the plaque stays put. */
	UPROPERTY(VisibleAnywhere, Category = "Gist")
	TObjectPtr<USceneComponent> Toy;

	/** Index = shape * 3 + finish (Gist::EShape, Gist::EFinish). */
	UPROPERTY(VisibleAnywhere, Category = "Gist")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Pieces;

	UPROPERTY(VisibleAnywhere, Category = "Gist")
	TObjectPtr<UBoxComponent> Hitbox;

	UPROPERTY(VisibleAnywhere, Category = "Gist")
	TObjectPtr<UStaticMeshComponent> PlaquePlate;

	UPROPERTY(VisibleAnywhere, Category = "Gist")
	TObjectPtr<UTextRenderComponent> Plaque;

	FString Prompt;
	FString Title, Echo, Tagline;
	TArray<FGistPupil> Pupils;
	uint32 Behaviours = 0;
	float Clock = 0.0f;
	float RevealAge = -1.0f;   // < 0: no pop-in running
	float ToyHeight = 100.0f;

	void SetShown(bool bShown);
};
