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
class UMaterialInstanceDynamic;
class UStaticMesh;
class UTexture2D;
namespace Gist { struct FRecipe; struct FVec; }

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

	/** The EntryId of a painting on its own (THE GIST, illustrated): the easel and the artist's
	    picture, no clay toy. Walt, 2026-11-01: "can we please not have the clay things? pictures
	    only please". Saved by its words like a gist; restored from the painting kept on disk. */
	static const FName PaintingEntryId;

	/** The rules' answer for these words, or null when they refuse or nothing was typed.
	    Words load once from Content/Data; Blocklist is the one the matcher already uses. */
	static TUniquePtr<Gist::FRecipe> Plan(const FString& Prompt, const TArray<FString>& Blocklist);

	/** Draw a planned gist and present it built; bFreshlyGenerated plays the pop-in. */
	void Author(const FString& InPrompt, const Gist::FRecipe& Recipe, bool bFreshlyGenerated);

	/** A painting on its own: no toy and no plaque, the easel where the toy would stand. The
	    picture is asked for separately (UGistPictureSubsystem) and shown with ShowPicture. */
	void AuthorPainting(const FString& InPrompt);

	/** Re-create a remembered or deployed gist: its words rebuild the same recipe (the rules
	    are deterministic), at the saved pose verbatim, under the saved id. Null if the words
	    no longer build anything. Used by both save paths, so they cannot drift.
	    For PaintingEntryId, an easel with the painting kept on disk - null if it is gone. */
	static AGistSite* Respawn(UWorld* World, const FString& Prompt, const FTransform& Where, const FGuid& Id,
		FName Kind = EntryId);

	const FString& GetPrompt() const { return Prompt; }
	FString GetPlaqueText() const;

	/** THE GIST, illustrated (UGistPictureSubsystem): the easel beside the toy. A blank canvas
	    while the artist paints, the painting when it arrives (fading in when fresh), or nothing. */
	void ShowEasel();
	void ShowPicture(UTexture2D* Picture, bool bFresh);
	void HideEasel();

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

	/** The easel stands beside the toy, not on it: it does not hover, spin or dance with it. */
	UPROPERTY(VisibleAnywhere, Category = "Gist")
	TObjectPtr<USceneComponent> Easel;

	/** Legs, ledge and clamp: engine cubes in clay, like the toy - no new meshes to download. */
	UPROPERTY(VisibleAnywhere, Category = "Gist")
	TObjectPtr<UInstancedStaticMeshComponent> EaselFrame;

	UPROPERTY(VisibleAnywhere, Category = "Gist")
	TObjectPtr<UStaticMeshComponent> Canvas;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CanvasMaterial;

	/** The painting itself, held so it lives as long as the easel shows it. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Painting;

	FString Prompt;
	FString Title, Echo, Tagline;
	TArray<FGistPupil> Pupils;
	uint32 Behaviours = 0;
	float Clock = 0.0f;
	float RevealAge = -1.0f;   // < 0: no pop-in running
	float ToyHeight = 100.0f;
	bool bEaselShown = false;
	bool bPaintingOnly = false;  // AuthorPainting: no toy, no plaque
	float PaintReveal = -1.0f;   // < 0: no fade running

	void SetShown(bool bShown);
	void PlaceEasel(const FVector& At, float Yaw);
	UMaterialInstanceDynamic* CanvasMid();
};
